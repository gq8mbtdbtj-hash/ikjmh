/**
 * @file apm.h
 * @ingroup tray_hooks_collect
 * @brief Collector → 持续观测：NDJSON 落盘 / HTTP POST 批量上报。
 *
 * 环境变量（tray_hooks_apm_start_from_env）：
 *   TRAY_HOOKS_APM_FILE=path.ndjson     追加写入每行一个 JSON 事件
 *   TRAY_HOOKS_APM_URL=http://host/v1/  定时 POST 批次（Content-Type: application/x-ndjson）
 *   TRAY_HOOKS_APM_INTERVAL_MS=2000     刷新间隔（默认 2000）
 *   TRAY_HOOKS_APM_BATCH=64             每批最多事件数
 *   TRAY_HOOKS_APM_STACKS=0             不上报 stacks（降噪/带宽）
 *
 * 与过滤配合：先 tray_hooks_collector_apply_env_filter()，再 start APM。
 *
 * @see test_apm_unit.cpp
 */

#pragma once

#include "tray_hooks/collector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tray_hooks_apm_config {
  const char* file_path;     /**< NDJSON 路径，可为 NULL */
  const char* url;           /**< HTTP 端点，可为 NULL */
  int interval_ms;           /**< <=0 用默认 2000 */
  int batch_max;             /**< <=0 用默认 64 */
  int include_stacks;        /**< 0=事件不含 frames 文本 */
} tray_hooks_apm_config_t;

/** 注册为 collector sink，启动刷盘/上报线程（若 interval>0） */
TRAY_HOOKS_API int tray_hooks_apm_start(const tray_hooks_apm_config_t* cfg);

/** 读环境变量并 start；未配置 FILE/URL 时返回 0 且不启动 */
TRAY_HOOKS_API int tray_hooks_apm_start_from_env(void);

TRAY_HOOKS_API void tray_hooks_apm_stop(void);

/** 立即刷出缓冲 */
TRAY_HOOKS_API void tray_hooks_apm_flush(void);

/**
 * @brief 写入一行自定义 JSON（如 memprobe 周期统计）
 * @note 须已 start；线程安全
 */
TRAY_HOOKS_API void tray_hooks_apm_emit_raw(const char* json_line);

#ifdef __cplusplus
}
#endif
