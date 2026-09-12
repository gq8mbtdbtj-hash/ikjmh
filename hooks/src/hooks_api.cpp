/**
 * @file hooks_api.cpp
 * @brief tray_hooks 对外 C API 实现（线程安全门面）。
 *
 * ## 职责
 * 1. 懒创建平台 Backend（IAT / PLT）
 * 2. 维护 proxy → prev 映射，供 `TRAY_HOOKS_CALL_PREV`
 * 3. 把 hook_single / partial / all 统一转成 Stub 交给后端
 *
 * ## 不在本文件
 * PE/ELF 解析与槽位改写；细节见 plat/*_patch.*。
 *
 * ## 线程与幂等
 * - 全局 mutex 保护 backend 与 proxy 表
 * - init 幂等；重复 hook 同一 proxy 时不覆盖已正确的 prev（防 CALL_PREV 死递归）
 *
 * @see docs/ARCHITECTURE.md
 * @see docs/API.md
 */

#include "tray_hooks/hooks.h"

#include "internal/backend.hpp"

#include <map>
#include <mutex>
#include <new>

namespace {

tray_hooks::detail::Backend* g_backend = 0;
tray_hooks_mode_t g_mode = TRAY_HOOKS_MODE_AUTOMATIC;
std::mutex g_mu;
/** proxy 函数地址 → 改写前原函数；proxy 内 get_prev 查表 */
std::map<void*, void*> g_proxy_to_prev;

}  // namespace

extern "C" int tray_hooks_init(tray_hooks_mode_t mode) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_backend) {
    return TRAY_HOOKS_OK;  // 幂等
  }
  g_backend = tray_hooks::detail::CreateBackend();
  if (!g_backend) {
    return TRAY_HOOKS_ERR_PLATFORM;
  }
  g_mode = mode;
  const int rc = g_backend->init(mode);
  if (rc != TRAY_HOOKS_OK) {
    delete g_backend;
    g_backend = 0;
    return rc;
  }
  return TRAY_HOOKS_OK;
}

extern "C" void tray_hooks_uninit(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_proxy_to_prev.clear();
  if (g_backend) {
    g_backend->uninit();  // 恢复所有 IAT/GOT 槽位
    delete g_backend;
    g_backend = 0;
  }
}

extern "C" const char* tray_hooks_backend_name(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_backend ? g_backend->name() : "none";
}

/** 把已填好的 Stub 交给后端；成功则登记 proxy→prev */
static tray_hooks_stub_t* Install(tray_hooks::detail::Stub* s) {
  if (!g_backend || !s || !s->sym_name.size() || !s->new_func) {
    delete s;
    return 0;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  tray_hooks::detail::Stub* out = g_backend->hook(s);
  if (!out) {
    delete s;
    return 0;
  }
  // 0 槽重入 hook 时 prev 可能被填成 new_func；勿覆盖已有正确映射（否则 CALL_PREV 死递归）
  if (out->prev_func && out->prev_func != out->new_func) {
    g_proxy_to_prev[out->new_func] = out->prev_func;
  } else if (g_proxy_to_prev.find(out->new_func) == g_proxy_to_prev.end()) {
    g_proxy_to_prev[out->new_func] = out->prev_func;
  }
  if (out->hooked) {
    out->hooked(reinterpret_cast<tray_hooks_stub_t*>(out), TRAY_HOOKS_OK,
                out->caller_path.c_str(), out->sym_name.c_str(), out->new_func,
                out->prev_func, out->hooked_arg);
  }
  return reinterpret_cast<tray_hooks_stub_t*>(out);
}

extern "C" tray_hooks_stub_t* tray_hooks_hook_single(
    const char* caller_path,
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg) {
  if (!g_backend) {
    return 0;
  }
  tray_hooks::detail::Stub* s = new (std::nothrow) tray_hooks::detail::Stub();
  if (!s) {
    return 0;
  }
  if (caller_path) {
    s->caller_path = caller_path;
  }
  if (callee_path) {
    s->callee_path = callee_path;
  }
  s->sym_name = sym_name ? sym_name : "";
  s->new_func = new_func;
  s->prev_func = 0;
  s->hooked = hooked;
  s->hooked_arg = hooked_arg;
  s->scope = 0;
  s->caller_allow = 0;
  s->caller_allow_arg = 0;
  return Install(s);
}

extern "C" tray_hooks_stub_t* tray_hooks_hook_partial(
    int (*caller_allow)(const char* caller_path, void* arg),
    void* caller_allow_arg,
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg) {
  if (!g_backend || !caller_allow || !new_func || !sym_name || !*sym_name) {
    return 0;
  }
  tray_hooks::detail::Stub* s = new (std::nothrow) tray_hooks::detail::Stub();
  if (!s) {
    return 0;
  }
  if (callee_path) {
    s->callee_path = callee_path;
  }
  s->sym_name = sym_name;
  s->new_func = new_func;
  s->prev_func = 0;
  s->hooked = hooked;
  s->hooked_arg = hooked_arg;
  s->scope = 1;
  s->caller_allow = caller_allow;
  s->caller_allow_arg = caller_allow_arg;
  return Install(s);
}

extern "C" tray_hooks_stub_t* tray_hooks_hook_all(
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg) {
  if (!g_backend) {
    return 0;
  }
  tray_hooks::detail::Stub* s = new (std::nothrow) tray_hooks::detail::Stub();
  if (!s) {
    return 0;
  }
  if (callee_path) {
    s->callee_path = callee_path;
  }
  s->sym_name = sym_name ? sym_name : "";
  s->new_func = new_func;
  s->prev_func = 0;
  s->hooked = hooked;
  s->hooked_arg = hooked_arg;
  s->scope = 2;
  s->caller_allow = 0;
  s->caller_allow_arg = 0;
  return Install(s);
}

extern "C" int tray_hooks_unhook(tray_hooks_stub_t* stub) {
  if (!stub || !g_backend) {
    return TRAY_HOOKS_ERR_PARAM;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  tray_hooks::detail::Stub* s =
      reinterpret_cast<tray_hooks::detail::Stub*>(stub);
  g_proxy_to_prev.erase(s->new_func);
  const int rc = g_backend->unhook(s);
  delete s;
  return rc;
}

extern "C" void* tray_hooks_get_prev(void* proxy) {
  if (!proxy) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  std::map<void*, void*>::iterator it = g_proxy_to_prev.find(proxy);
  if (it != g_proxy_to_prev.end()) {
    return it->second;
  }
  return 0;
}
