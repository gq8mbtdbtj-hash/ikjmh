/**
 * @file memprobe_install.h
 * @brief 安装真 hook（IAT/PLT）入口，通常由 so/dll 加载时自动调用。
 *
 * 业务一般不直接调用；需要「延后安装」时可在 tray_memprobe_start 之后手动调用。
 */

#pragma once

#include "tray_hooks/memprobe.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 tray_hooks 并 hook malloc/mmap（及环境变量指定的 GPU/NPU 符号） */
TRAY_MEMPROBE_API void tray_memprobe_install_hooks(void);

#ifdef __cplusplus
}
#endif
