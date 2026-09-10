/**
 * @file memprobe_install.h
 * @ingroup tray_memprobe_api
 * @brief 安装真 hook（IAT/PLT）入口，通常由 so/dll 加载时自动调用。
 *
 * 典型路径：`constructor(101)` / `DllMain` → `tray_memprobe_start` →
 * `tray_memprobe_install_hooks`（本 API）。
 *
 * 业务一般不直接调用；需要「延后安装」时可在 `tray_memprobe_start` 之后手动调用。
 *
 * @see docs/ARCHITECTURE.md （生命周期）
 * @see docs/DATAFLOW.md
 */

#pragma once

#include "tray_hooks/memprobe.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 tray_hooks（AUTOMATIC）并 hook 采集符号
 *
 * 默认安装：malloc 族、mmap/VirtualAlloc，以及环境变量指定的 GPU/NPU/NEON 符号。
 * 幂等：重复调用不会重复登记同一 proxy。
 */
TRAY_MEMPROBE_API void tray_memprobe_install_hooks(void);

#ifdef __cplusplus
}
#endif
