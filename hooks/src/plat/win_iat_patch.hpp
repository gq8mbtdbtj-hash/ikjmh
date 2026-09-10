#pragma once

/**
 * @file win_iat_patch.hpp
 * @brief Windows 进程内 IAT（Import Address Table）改写原语。
 *
 * ## 背景
 * ELF 上 xhook/bhook 改的是「调用者」GOT；Windows 对应物是 PE 的 IAT：
 * 调用者模块通过 IAT 槽间接调用 `ucrtbase!malloc` 等。把槽改成 proxy，
 * 即可在不改业务源码、不链 Detours 的情况下拦截 API。
 *
 * ## 使用方
 * - @ref tray_hooks::detail::WinIatBackend （统一 hook API）
 * - tray_memprobe.dll 的 DllMain → install_hooks
 *
 * @platform Windows only
 */

#ifdef _WIN32

#include <windows.h>

#include <string>
#include <vector>

namespace tray_hooks {
namespace iat {

/**
 * @brief 单次成功改写的记录，用于 unhook 时恢复原指针。
 */
struct Patch {
  void** slot;       ///< IAT 中存放函数指针的地址
  void* original;    ///< 改写前的真实 API 地址
  void* replaced;    ///< 写入的 proxy
  HMODULE caller;    ///< 被改写的调用者模块
  std::string dll_name;  ///< 导入描述里的 DLL 名（可能是 api-ms-win-crt-*）
  std::string sym_name;  ///< 符号名，如 "malloc"
};

/**
 * @brief 在单个模块的导入表中查找并改写符号。
 * @param caller      调用者 HMODULE（其 IAT 将被修改）
 * @param import_dll  限定来自哪个 DLL 的导入；NULL/空 = 任意 DLL（推荐：
 *                    现代 MSVC 常从 api-ms-win-crt-*.dll 导入而非 ucrtbase）
 * @param sym_name    导入符号名（按名称，不支持纯序号导入）
 * @param new_fn      proxy 函数地址
 * @param out_patches 非空则追加 Patch 记录
 * @return 本模块内成功改写的槽位数
 */
int PatchModule(HMODULE caller,
                const char* import_dll,
                const char* sym_name,
                void* new_fn,
                std::vector<Patch>* out_patches);

/**
 * @brief 枚举当前进程所有已加载模块并 PatchModule（含 Delay-Load 导入表）。
 * @param skip_self 跳过探针自身模块，避免改到自己的导入造成递归
 * @param allow     可选；非空时对每个模块路径（UTF-8）回调，返回 0 则跳过
 */
int PatchAllModules(const char* import_dll,
                    const char* sym_name,
                    void* new_fn,
                    std::vector<Patch>* out_patches,
                    HMODULE skip_self,
                    int (*allow)(const char* caller_path, void* arg) = 0,
                    void* allow_arg = 0);

/** @brief 把槽写回 original */
bool Restore(const Patch& p);

}  // namespace iat
}  // namespace tray_hooks

#endif
