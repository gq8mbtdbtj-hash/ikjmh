/**
 * @file backend.hpp
 * @brief tray_hooks 平台后端抽象（内部，不对外）。
 *
 * ## 在架构中的位置
 * 门面 `hooks_api.cpp` 只依赖本头文件；真正改写发生在：
 * - Windows: `win_iat_backend` → `win_iat_patch`（IAT 槽）
 * - ELF:     `elf_plt_backend` → `elf_plt_patch`（GOT / JUMP_SLOT）
 *
 * ## 扩展点
 * @customize{量产 Android 若改用 bytehook，只需换 CreateBackend() 返回值，
 * 对外 C ABI（tray_hooks_*）保持不变。见 docs/ANDROID.md。}
 *
 * @internal 本头文件不得被 include/tray_hooks 对外头文件包含。
 * @see docs/ARCHITECTURE.md
 * @see docs/PLATFORM.md
 */

#pragma once

#include "tray_hooks/hooks.h"

#include <string>
#include <vector>

namespace tray_hooks {
namespace detail {

/**
 * @brief 一次 hook 任务的运行时描述
 *
 * 生命周期：Install 分配 → Backend::hook 填充 prev_func → unhook/uninit 释放。
 * scope：0=single，1=partial，2=all。
 */
struct Stub {
  std::string caller_path;  /**< 调用者 so/dll 过滤；空=不限（ELF 用 pathname 子串） */
  std::string callee_path;  /**< 被导入 DLL/so；空=按符号名在任意导入表匹配（Win 重要） */
  std::string sym_name;     /**< 如 "malloc" */
  void* new_func;           /**< 代理函数地址 */
  void* prev_func;          /**< 改写前原实现，供 tray_hooks_get_prev */
  tray_hooks_hooked_t hooked; /**< 可选完成回调 */
  void* hooked_arg;
  int scope;  /**< 0=single 1=partial 2=all */
  /** partial：非空时按模块路径回调过滤（返回非 0=允许改写） */
  int (*caller_allow)(const char* caller_path, void* arg);
  void* caller_allow_arg;
};

/**
 * @brief 平台后端虚接口
 *
 * 实现类负责：枚举模块、改写 GOT/IAT、AUTOMATIC 下拦截加载器并重放 stub。
 */
struct Backend {
  virtual ~Backend() {}
  /** @brief 后端名，如 `elf_plt(android,auto)` / `win_iat` */
  virtual const char* name() const = 0;
  virtual int init(tray_hooks_mode_t mode) = 0;
  /** @brief 恢复全部改写 */
  virtual void uninit() = 0;
  /**
   * @brief 执行改写并填充 stub->prev_func
   * @return stub 自身；失败返回 NULL（调用方 delete stub）
   */
  virtual Stub* hook(Stub* stub) = 0;
  virtual int unhook(Stub* stub) = 0;
  /** @brief 解析「真实」符号地址（CALL_PREV / 统计用） */
  virtual void* resolve_sym(const char* module, const char* sym) = 0;
};

/**
 * @brief 各平台 .cpp 提供唯一实现
 * @customize{可返回 BytehookBackend 适配器而不改门面。}
 */
Backend* CreateBackend();

}  // namespace detail
}  // namespace tray_hooks
