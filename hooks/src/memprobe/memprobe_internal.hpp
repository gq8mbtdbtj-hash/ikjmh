/**
 * @file memprobe_internal.hpp
 * @brief memprobe 内部入口：hook proxy / 符号插桩共用。
 *
 * OnAlloc/OnFree 负责：
 *   - 域开关（TRAY_MEMPROBE_DOMAINS）
 *   - 重入保护（采集栈/格式化时可能再次 malloc）
 *   - 采样堆栈（SAMPLE / MIN_SIZE）
 *   - 按指针记账 live_bytes / peak
 */

#pragma once

#include "tray_hooks/backtrace.h"
#include "tray_hooks/memprobe.h"

#include <stddef.h>

namespace tray_memprobe {
namespace detail {

/** 分配成功后记账；p==NULL 忽略 */
void OnAlloc(tray_mem_domain_t dom, void* p, size_t size);
/** 释放前记账；未知指针只增加 free_calls（若域开启） */
void OnFree(tray_mem_domain_t dom, void* p);

/** 当前进程是否采集该域（start 时读环境变量） */
bool DomainEnabled(tray_mem_domain_t dom);

}  // namespace detail

namespace gpu {
/** 安装 CUDA / OpenCL / Vulkan 精确尺寸 hook */
void InstallPreciseGpuHooks();
}  // namespace gpu

}  // namespace tray_memprobe
