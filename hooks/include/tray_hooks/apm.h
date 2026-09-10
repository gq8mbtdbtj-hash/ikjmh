/**
 * @file apm.h
 * @ingroup tray_hooks_collect
 * @brief Collector → 持续观测：NDJSON 落盘 / HTTP POST 批量上报。
 *
 * ## 职责
 * 将 collector 事件缓冲后：
 * - 追加写入 NDJSON 文件；和/或
 * - 定时 HTTP POST 批次（`Content-Type: application/x-ndjson`）
 *
 * ## 环境变量（`tray_hooks_apm_start_from_env`）
 * | 变量 | 含义 | 默认 |
 * |------|------|------|
 * | `TRAY_HOOKS_APM_FILE` | NDJSON 路径 | 无则不启文件 |
 * | `TRAY_HOOKS_APM_URL` | HTTP 端点 | 无 |
 * | `TRAY_HOOKS_APM_INTERVAL_MS` | 刷新间隔 | 2000 |
 * | `TRAY_HOOKS_APM_BATCH` | 每批最多事件 | 64 |
 * | `TRAY_HOOKS_APM_STACKS` | `0`=不上报 frames | 开 |
 *
 * ## 使用注意
 * - start 会 **注册为 collector sink**（覆盖先前 `set_sink`）
 * - 建议先 `tray_hooks_collector_apply_env_filter()` 再 start
 * - POSIX 当前以 HTTP 为主；HTTPS 见 docs/ROADMAP.md
 *
 * @see docs/DATAFLOW.md
 * @see test_apm_unit.cpp
 */

#pragma once

#include "tray_hooks/collector.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief APM 启动配置；file_path 与 url 可只填其一 */
typedef struct tray_hooks_apm_config {
  const char* file_path;     /**< NDJSON 路径，可为 NULL */
  const char* url;           /**< HTTP 端点，可为 NULL */
  int interval_ms;           /**< <=0 用默认 2000 */
  int batch_max;             /**< <=0 用默认 64 */
  int include_stacks;        /**< 0=事件不含 frames */
} tray_hooks_apm_config_t;

/**
 * @brief 注册为 collector sink，并启动刷盘/上报线程（interval>0 时）
 * @return 0 成功；非 0 失败
 */
TRAY_HOOKS_API int tray_hooks_apm_start(const tray_hooks_apm_config_t* cfg);

/**
 * @brief 读环境变量并 start
 * @return 0 成功或未配置 FILE/URL（不启动）；非 0 失败
 */
TRAY_HOOKS_API int tray_hooks_apm_start_from_env(void);

/** @brief 停止工作线程并注销 sink */
TRAY_HOOKS_API void tray_hooks_apm_stop(void);

/** @brief 立即刷出缓冲（文件 / HTTP） */
TRAY_HOOKS_API void tray_hooks_apm_flush(void);

/**
 * @brief 写入一行自定义 JSON（如 memprobe 周期统计）
 * @note 须已 start；线程安全
 */
TRAY_HOOKS_API void tray_hooks_apm_emit_raw(const char* json_line);

#ifdef __cplusplus
}
#endif
