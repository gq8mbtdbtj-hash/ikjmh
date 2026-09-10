#pragma once

/**
 * @file elf_plt_patch.hpp
 * @brief 自研 ELF PLT/GOT 改写（思路对齐 xhook/bhook，不链接二者）。
 *
 * ## 模型
 * 调用者 so 通过 GOT 中的 JUMP_SLOT 调用外部符号。改写该槽 → proxy，
 * 即可实现「无改业务编译」的采集（配合 LD_PRELOAD / patchelf --add-needed）。
 *
 * ## 支持架构
 * x86_64 / aarch64 / arm / i386 的 R_*_JUMP_SLOT；其它架构需扩展宏。
 *
 * AUTOMATIC 模式下新 dlopen 的 so 由 elf_plt_backend 再次 PatchSymbol。
 *
 * @platform Android / Linux / QNX / HarmonyOS(OHOS)
 */

#if !defined(_WIN32)

#include <link.h>

#include <string>
#include <vector>

namespace tray_hooks {
namespace elfplt {

/** 单次 GOT 改写记录，供 Restore / unhook */
struct Patch {
  void** slot;
  void* original;
  void* replaced;
  std::string caller_path;  ///< dl_iterate_phdr 给出的 pathname（主程序可能为空串）
  std::string sym_name;
};

/**
 * @brief 遍历已加载 ELF，改写对 sym_name 的 PLT GOT 槽。
 * @param caller_substr 仅处理 pathname 包含该子串的模块；NULL=全部
 * @param sym_name      如 "malloc" / "mmap"
 * @param new_fn        proxy
 * @param out_patches   可选，收集 Patch
 * @return 成功改写槽位数（同一符号可能在多个 so 各有一条）
 */
int PatchSymbol(const char* caller_substr,
                const char* sym_name,
                void* new_fn,
                std::vector<Patch>* out_patches);

bool Restore(const Patch& p);

}  // namespace elfplt
}  // namespace tray_hooks

#endif
