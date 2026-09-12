/**
 * @file backtrace.h
 * @ingroup tray_hooks_collect
 * @brief 跨平台调用栈采集（可在 hook proxy / OnAlloc 内调用）。
 *
 * ## 平台实现（backtrace_all.cpp）
 * | 平台 | 抓栈 | 符号化 |
 * |------|------|--------|
 * | Windows | CaptureStackBackTrace | DbgHelp SymFromAddr |
 * | Android / OHOS | _Unwind_Backtrace | dladdr |
 * | 其它 POSIX | backtrace(3) | dladdr |
 *
 * ## 约定
 * - `skip`：跳过本 API / proxy 入口等无关帧（memprobe 常用 skip=2）
 * - 符号可空（剥离 so）；保留 module+offset 供离线 addr2line
 * - 实现侧帧数上限 64
 *
 * @see docs/PLATFORM.md
 * @see tray_hooks_collector_record
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "tray_hooks/hooks.h"

#include <stddef.h>
#include <stdint.h>

/** @brief 单帧；module/symbol 可空 */
typedef struct tray_hooks_frame {
  void* pc;           /**< 返回地址 / PC */
  char  module[256];  /**< 所属模块路径或名（可空） */
  char  symbol[256];  /**< 符号名（可空） */
  uintptr_t offset;   /**< 相对模块基址偏移 */
} tray_hooks_frame_t;

/**
 * @brief 采集当前线程调用栈
 * @param out        输出帧数组
 * @param max_frames 容量（实现侧上限 64）
 * @param skip       跳过顶层帧数
 * @return 实际写入帧数；参数非法返回 0
 */
TRAY_HOOKS_API int tray_hooks_backtrace(tray_hooks_frame_t* out,
                                        int max_frames,
                                        int skip);

/**
 * @brief 将栈格式化为多行文本（`#NN symbol (module+0xoff)`）
 * @return 写入字节数（不含 NUL）；缓冲不够时截断
 */
TRAY_HOOKS_API int tray_hooks_backtrace_format(const tray_hooks_frame_t* frames,
                                               int nframes,
                                               char* buf,
                                               size_t buf_size);

#ifdef __cplusplus
}
#endif
