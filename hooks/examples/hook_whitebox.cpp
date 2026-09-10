/**
 * @file hook_whitebox.cpp
 * @brief 白盒 Hook：真 IAT/PLT、CALL_PREV、hook_partial；可选晚加载。
 */

#include "tray_hooks/hooks.h"
#include "wb_lib.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

int g_fail = 0;
int g_proxy_hits = 0;
int g_prev_ok = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      std::fflush(stderr);                                                     \
      ++g_fail;                                                                \
    } else {                                                                   \
      std::printf("  ok: %s\n", #cond);                                        \
      std::fflush(stdout);                                                     \
    }                                                                          \
  } while (0)

typedef int (*wb_fn)(int, int);
typedef int (*late_fn)(int, int);

int ProxyWbTarget(int a, int b) {
  ++g_proxy_hits;
  wb_fn prev = reinterpret_cast<wb_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(ProxyWbTarget)));
  if (!prev || prev == reinterpret_cast<wb_fn>(ProxyWbTarget)) {
    std::fprintf(stderr, "FAIL: invalid prev=%p\n",
                 reinterpret_cast<void*>(prev));
    ++g_fail;
    return -1;
  }
  const int orig = prev(a, b);
  if (orig == a + b + 1000) {
    ++g_prev_ok;
  }
  return a + b + 2000;
}

int AllowMainOrWb(const char* caller_path, void*) {
  if (!caller_path || !*caller_path) {
    return 1;
  }
  if (std::strstr(caller_path, "hook_whitebox")) {
    return 1;
  }
  if (std::strstr(caller_path, "tray_wb")) {
    return 1;
  }
  return 0;
}

int AllowNobody(const char*, void*) { return 0; }

}  // namespace

int main() {
  std::printf("=== hook_whitebox ===\n");
  std::fflush(stdout);
  std::printf("wb_lib_version=%s\n", wb_lib_version());

  CHECK(wb_target(1, 2) == 1003);

  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_MANUAL) == TRAY_HOOKS_OK);
  std::printf("backend=%s\n", tray_hooks_backend_name());
  std::fflush(stdout);

  tray_hooks_stub_t* stub = tray_hooks_hook_all(
      NULL, "wb_target", reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(stub != 0);

  g_proxy_hits = 0;
  g_prev_ok = 0;
  CHECK(wb_target(1, 2) == 2003);
  CHECK(g_proxy_hits == 1);
  CHECK(g_prev_ok == 1);

  for (int i = 0; i < 5; ++i) {
    CHECK(wb_target(3, 4) == 2007);
  }
  CHECK(g_proxy_hits == 6);

  CHECK(tray_hooks_unhook(stub) == TRAY_HOOKS_OK);
  CHECK(wb_target(1, 2) == 1003);
  tray_hooks_uninit();

  // hook_partial
  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_MANUAL) == TRAY_HOOKS_OK);
  tray_hooks_stub_t* deny = tray_hooks_hook_partial(
      AllowNobody, NULL, NULL, "wb_target",
      reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(deny != 0);
  g_proxy_hits = 0;
  CHECK(wb_target(1, 2) == 1003);
  CHECK(g_proxy_hits == 0);
  tray_hooks_unhook(deny);

  tray_hooks_stub_t* allow = tray_hooks_hook_partial(
      AllowMainOrWb, NULL, NULL, "wb_target",
      reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(allow != 0);
  g_proxy_hits = 0;
  CHECK(wb_target(1, 2) == 2003);
  CHECK(g_proxy_hits == 1);
  tray_hooks_unhook(allow);
  CHECK(wb_target(1, 2) == 1003);
  tray_hooks_uninit();

  // AUTOMATIC：LdrRegisterDllNotification（或回退 LoadLibrary IAT）补晚加载 DLL
  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC) == TRAY_HOOKS_OK);
  std::printf("backend(auto)=%s\n", tray_hooks_backend_name());
  std::fflush(stdout);
  tray_hooks_stub_t* auto_stub = tray_hooks_hook_all(
      NULL, "wb_target", reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(auto_stub != 0);
  g_proxy_hits = 0;
#if defined(_WIN32)
  HMODULE late = LoadLibraryA("tray_wb_late.dll");
#else
  void* late = dlopen("libtray_wb_late.so", RTLD_NOW);
  if (!late) {
    late = dlopen("./libtray_wb_late.so", RTLD_NOW);
  }
#endif
  CHECK(late != 0);
  late_fn invoke = 0;
#if defined(_WIN32)
  if (late) {
    invoke = reinterpret_cast<late_fn>(GetProcAddress(late, "wb_late_invoke"));
  }
#else
  if (late) {
    invoke = reinterpret_cast<late_fn>(dlsym(late, "wb_late_invoke"));
  }
#endif
  CHECK(invoke != 0);
  if (invoke) {
    CHECK(invoke(1, 2) == 2003);
    CHECK(g_proxy_hits >= 1);
  }
#if defined(_WIN32)
  if (late) {
    FreeLibrary(late);
  }
#else
  if (late) {
    dlclose(late);
  }
#endif
  tray_hooks_unhook(auto_stub);
  tray_hooks_uninit();

  if (g_fail) {
    std::fprintf(stderr, "WHITEBOX FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("WHITEBOX ALL PASS\n");
  return 0;
}
