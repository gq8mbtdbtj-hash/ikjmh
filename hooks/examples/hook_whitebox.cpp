/**
 * @file hook_whitebox.cpp
 * @brief 白盒 Hook 测试：自建 victim 库 → 真 IAT/PLT 改写 → 验证拦截与 CALL_PREV。
 *
 * 验证点：
 *  1. 未 hook 时 wb_target(1,2)==1003
 *  2. hook 后走 proxy，返回 2003（proxy 标记）
 *  3. proxy 内 CALL_PREV 得到原 1003
 *  4. 命中计数 == 调用次数
 *  5. unhook 后恢复原行为
 *  6. 幂等：重复 hook_all 不崩溃
 *
 * @example
 *   cmake --build build --target hook_whitebox
 *   ./build/hooks/hook_whitebox
 */

#include "tray_hooks/hooks.h"
#include "wb_lib.h"

#include <cstdio>

namespace {

int g_fail = 0;
int g_proxy_hits = 0;
int g_prev_ok = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    } else {                                                                   \
      std::printf("  ok: %s\n", #cond);                                        \
    }                                                                          \
  } while (0)

typedef int (*wb_fn)(int, int);

int ProxyWbTarget(int a, int b) {
  ++g_proxy_hits;
  wb_fn prev = reinterpret_cast<wb_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(ProxyWbTarget)));
  if (!prev || prev == reinterpret_cast<wb_fn>(ProxyWbTarget)) {
    std::fprintf(stderr, "FAIL: invalid prev=%p\n", reinterpret_cast<void*>(prev));
    ++g_fail;
    return -1;
  }
  const int orig = prev(a, b);
  if (orig == a + b + 1000) {
    ++g_prev_ok;
  }
  return a + b + 2000;
}

}  // namespace

int main() {
  std::printf("=== hook_whitebox ===\n");
  std::printf("wb_lib_version=%s\n", wb_lib_version());

  const int base = wb_target(1, 2);
  CHECK(base == 1003);

  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC) == TRAY_HOOKS_OK);
  std::printf("backend=%s\n", tray_hooks_backend_name());

  tray_hooks_stub_t* stub = tray_hooks_hook_all(
      NULL, "wb_target", reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(stub != 0);

  g_proxy_hits = 0;
  g_prev_ok = 0;
  const int hooked = wb_target(1, 2);
  CHECK(hooked == 2003);
  CHECK(g_proxy_hits == 1);
  CHECK(g_prev_ok == 1);

  for (int i = 0; i < 5; ++i) {
    CHECK(wb_target(3, 4) == 2007);
  }
  CHECK(g_proxy_hits == 6);

  tray_hooks_stub_t* stub2 = tray_hooks_hook_all(
      NULL, "wb_target", reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(stub2 != 0);
  CHECK(wb_target(0, 0) == 2000);

  CHECK(tray_hooks_unhook(stub) == TRAY_HOOKS_OK);
  CHECK(tray_hooks_unhook(stub2) == TRAY_HOOKS_OK);

  const int restored = wb_target(1, 2);
  CHECK(restored == 1003);

  tray_hooks_uninit();

  if (g_fail) {
    std::fprintf(stderr, "WHITEBOX FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("WHITEBOX ALL PASS\n");
  return 0;
}
