/**
 * @file collector.h
 * @ingroup tray_hooks_collect
 * @brief Hook 命中事件 + 堆栈采集汇聚（APM / 诊断用）。
 *
 * 与 memprobe 独立：memprobe 做内存域记账；collector 做通用「打点+栈」。
 * 可在同一 proxy 里两者都调。
 *
 * 降噪：tray_hooks_filter_t / apply_env_filter（进程名、TID、tag、模块、采样）。
 *
 * @see test_collector_unit.cpp
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "tray_hooks/backtrace.h"

#include <stdint.h>

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

/** 用户 sink：勿在回调里长时间持锁；可异步投递 */
typedef void (*tray_hooks_sink_fn)(const tray_hooks_event_t* ev, void* user);

/**
 * 过滤规则（全 0 / 空串 = 不限制该项）。
 * allow_* 与 deny_*：逗号分隔；tag 大小写敏感；module 子串匹配栈帧。
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

TRAY_HOOKS_API void tray_hooks_collector_set_sink(tray_hooks_sink_fn sink,
                                                  void* user);

/** 拷贝规则；传 NULL 清空过滤 */
TRAY_HOOKS_API void tray_hooks_collector_set_filter(const tray_hooks_filter_t* f);

/**
 * 从环境变量装过滤（可与 set_filter 叠加，env 在 start 时调用即可）：
 *   TRAY_HOOKS_FILTER_TAGS / DENY_TAGS
 *   TRAY_HOOKS_FILTER_TIDS
 *   TRAY_HOOKS_FILTER_PROCESS
 *   TRAY_HOOKS_FILTER_MODULES
 *   TRAY_HOOKS_FILTER_MIN_ARG0
 *   TRAY_HOOKS_FILTER_SAMPLE
 */
TRAY_HOOKS_API void tray_hooks_collector_apply_env_filter(void);

/**
 * @brief 记录一次采集（通常在 proxy 开头调用）
 * @param tag     事件标签
 * @param frames  可为 NULL：内部自动 backtrace
 * @param nframes frames 有效长度；frames==NULL 时忽略
 * @param arg0    附加整数（如分配 size）
 * @note 未通过 filter 时不调用 sink，也不计入 total
 */
TRAY_HOOKS_API void tray_hooks_collector_record(const char* tag,
                                                const tray_hooks_frame_t* frames,
                                                int nframes,
                                                uint64_t arg0);

/** 进程内累计成功 record 次数（含无 sink 时） */
TRAY_HOOKS_API uint64_t tray_hooks_collector_total(void);

/** 被 filter 丢掉的次数 */
TRAY_HOOKS_API uint64_t tray_hooks_collector_dropped(void);

#ifdef __cplusplus
}
#endif
