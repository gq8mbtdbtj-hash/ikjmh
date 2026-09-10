/**
 * @file collector.h
 * @ingroup tray_hooks_collect
 * @brief Hook 命中事件 + 堆栈采集汇聚（APM / 诊断用）。
 *
 * ## 在架构中的位置
 * - **memprobe**：按域记账 live/peak（见 @ref memprobe.h）
 * - **collector**：通用「tag + arg0 + 栈」打点，可接任意 sink / APM
 * - 同一 proxy 内两者可同时调用；互不替代
 *
 * ## 数据流（简）
 * @code{.unparsed}
 *   record(tag, frames?, n, arg0)
 *     → 组装 tray_hooks_event_t
 *     → filter（deny > allow > tid/process/modules/min_arg0/sample）
 *     → sink 或丢弃（dropped++）
 * @endcode
 *
 * 完整说明见 docs/DATAFLOW.md。
 *
 * @see tray_hooks_apm_start
 * @see test_collector_unit.cpp
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "tray_hooks/backtrace.h"

#include <stdint.h>

/**
 * @brief 一次采集事件（sink 回调入参）
 * @note frames 最多 64；超出在 record 侧截断。
 */
typedef struct tray_hooks_event {
  int64_t timestamp_ms;       /**< Unix 纪元毫秒 */
  char tag[64];               /**< 如符号名 "malloc" */
  uint64_t arg0;              /**< 业务附加（如 size） */
  uint32_t pid;
  uint32_t tid;
  char process[96];           /**< 主模块 basename，可空 */
  int nframes;
  tray_hooks_frame_t frames[64];
} tray_hooks_event_t;

/**
 * @brief 用户 sink
 * @warning 勿在回调里长时间持锁或再次 uninit；可异步投递。
 */
typedef void (*tray_hooks_sink_fn)(const tray_hooks_event_t* ev, void* user);

/**
 * @brief 过滤规则（全 0 / 空串 = 不限制该项）
 *
 * 判定顺序概念上：
 * 1. deny_tags 命中 → 丢弃
 * 2. allow_tags 非空且未命中 → 丢弃
 * 3. tid / process / modules / min_arg0 / sample_n
 *
 * allow_* 与 deny_*：逗号分隔；tag 大小写敏感；module 对栈帧做子串匹配。
 */
typedef struct tray_hooks_filter {
  const char* allow_tags;     /**< 仅这些 tag；空=全放行 */
  const char* deny_tags;      /**< 黑名单优先于白名单 */
  const char* allow_tids;     /**< 如 "1234,5678"；空=全 tid */
  const char* allow_process;  /**< 进程 basename 子串，空=不限 */
  const char* allow_modules;  /**< 栈中至少一帧 module 包含其一 */
  uint64_t min_arg0;          /**< arg0 下限（如最小分配尺寸） */
  unsigned sample_n;          /**< 每 N 次通过 1 次；0/1=全过 */
} tray_hooks_filter_t;

/** @brief 设置事件 sink；传 NULL 清空（仍会计 total） */
TRAY_HOOKS_API void tray_hooks_collector_set_sink(tray_hooks_sink_fn sink,
                                                  void* user);

/** @brief 拷贝规则；传 NULL 清空过滤 */
TRAY_HOOKS_API void tray_hooks_collector_set_filter(const tray_hooks_filter_t* f);

/**
 * @brief 从环境变量装载过滤
 *
 * 变量：
 * - `TRAY_HOOKS_FILTER_TAGS` / `DENY_TAGS`
 * - `TRAY_HOOKS_FILTER_TIDS`
 * - `TRAY_HOOKS_FILTER_PROCESS`
 * - `TRAY_HOOKS_FILTER_MODULES`
 * - `TRAY_HOOKS_FILTER_MIN_ARG0`
 * - `TRAY_HOOKS_FILTER_SAMPLE`
 *
 * @note 可与 set_filter 叠加；memprobe/APM 启动路径会调用。
 */
TRAY_HOOKS_API void tray_hooks_collector_apply_env_filter(void);

/**
 * @brief 记录一次采集（通常在 proxy 开头调用）
 * @param tag     事件标签
 * @param frames  可为 NULL：内部自动 backtrace
 * @param nframes frames 有效长度；frames==NULL 时忽略
 * @param arg0    附加整数（如分配 size）
 * @note 未通过 filter 时不调用 sink，计入 dropped；通过则 total++。
 */
TRAY_HOOKS_API void tray_hooks_collector_record(const char* tag,
                                                const tray_hooks_frame_t* frames,
                                                int nframes,
                                                uint64_t arg0);

/** @brief 进程内累计成功 record 次数（含无 sink 时） */
TRAY_HOOKS_API uint64_t tray_hooks_collector_total(void);

/** @brief 被 filter 丢掉的次数 */
TRAY_HOOKS_API uint64_t tray_hooks_collector_dropped(void);

#ifdef __cplusplus
}
#endif
