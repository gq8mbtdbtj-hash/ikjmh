#pragma once

/**
 * @file elf_plt_patch.hpp
 * @brief 自研 ELF PLT/GOT 改写（思路对齐 xhook/bhook，**不链接**二者）。
 *
 * ## 模型
 * 调用者 so 通过 GOT 中的 JUMP_SLOT 调用外部符号。改写该槽 → proxy，
 * 即可实现「无改业务编译」的采集（配合 LD_PRELOAD / patchelf --add-needed）。
 *
 * ## 支持架构
 * x86_64 / aarch64 / arm / i386 的 `R_*_JUMP_SLOT`；其它架构需扩展宏。
 *
 * AUTOMATIC 模式下新 dlopen 的 so 由 `elf_plt_backend` 再次 `PatchSymbol`。
 *
 * ## 失败语义
 * FULL RELRO / 加固可能导致 `mprotect` 失败；通过 `fail_out` 累计次数，
 * stderr 打印诊断。量产覆盖率监控应读取该计数。
 *
 * @platform Android / Linux / QNX / HarmonyOS(OHOS)
 * @see docs/PLATFORM.md
 * @see docs/ANDROID.md
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
 * @param caller_substr 仅处理 pathname 包含该子串的模块；NULL=全部（可与 allow 并用）
 * @param allow         可选回调；返回 0 跳过该 caller
 * @param fail_out      可选；累计 WriteSlot 失败次数（RELRO/加固诊断）
 */
int PatchSymbol(const char* caller_substr,
                const char* sym_name,
                void* new_fn,
                std::vector<Patch>* out_patches,
                int (*allow)(const char* caller_path, void* arg) = 0,
                void* allow_arg = 0,
                int* fail_out = 0);

bool Restore(const Patch& p);

}  // namespace elfplt
}  // namespace tray_hooks

#endif
