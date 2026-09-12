/**
 * @file android_ndk_smoke.cpp
 * @brief Android NDK 交叉编译冒烟：真 PLT hook + CALL_PREV + collector。
 *
 * 编包门槛（无真机也可跑通「交叉编译 + 产物检查」）：
 * 1. `__ANDROID__` 下链接 tray_hooks + tray_wb_lib
 * 2. init 后 backend 名包含 `elf_plt(android`
 * 3. hook `wb_target` 后调用走 proxy，且 CALL_PREV 结果正确
 * 4. collector 至少记录 1 次事件
 *
 * 真机 / 模拟器：
 *   adb push android_ndk_smoke libtray_wb_lib.so /data/local/tmp/
 *   adb shell 'cd /data/local/tmp && chmod +x android_ndk_smoke && \
 *              LD_LIBRARY_PATH=. ./android_ndk_smoke'
 */

#include "tray_hooks/collector.h"
#include "tray_hooks/hooks.h"
#include "wb_lib.h"

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;
int g_proxy_hits = 0;
int g_prev_ok = 0;
int g_sink_hits = 0;

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

void OnEvent(const tray_hooks_event_t* ev, void*) {
  if (ev && std::strcmp(ev->tag, "wb_target") == 0) {
    ++g_sink_hits;
  }
}

int ProxyWbTarget(int a, int b) {
  ++g_proxy_hits;
  tray_hooks_collector_record("wb_target", NULL, 0,
                              static_cast<uint64_t>(a + b));
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

}  // namespace

int main() {
  std::printf("=== android_ndk_smoke ===\n");
#if defined(__ANDROID__)
  std::printf("build=__ANDROID__ api=%d\n", __ANDROID_API__);
#else
  std::printf("build=HOST (not Android NDK)\n");
#endif
  std::printf("wb_lib_version=%s\n", wb_lib_version());

  CHECK(wb_target(1, 2) == 1003);

  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_MANUAL) == TRAY_HOOKS_OK);
  const char* backend = tray_hooks_backend_name();
  std::printf("backend=%s\n", backend ? backend : "(null)");
#if defined(__ANDROID__)
  CHECK(backend && std::strstr(backend, "elf_plt(android") != NULL);
#else
  CHECK(backend && std::strstr(backend, "elf_plt") != NULL);
#endif

  tray_hooks_collector_set_sink(OnEvent, NULL);

  tray_hooks_stub_t* stub = tray_hooks_hook_all(
      NULL, "wb_target", reinterpret_cast<void*>(ProxyWbTarget), NULL, NULL);
  CHECK(stub != 0);

  g_proxy_hits = 0;
  g_prev_ok = 0;
  g_sink_hits = 0;
  CHECK(wb_target(1, 2) == 2003);
  CHECK(g_proxy_hits == 1);
  CHECK(g_prev_ok == 1);
  CHECK(g_sink_hits >= 1);
  CHECK(tray_hooks_collector_total() >= 1);

  CHECK(tray_hooks_unhook(stub) == TRAY_HOOKS_OK);
  CHECK(wb_target(1, 2) == 1003);
  tray_hooks_uninit();

  if (g_fail) {
    std::fprintf(stderr, "ANDROID_NDK_SMOKE FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("ANDROID_NDK_SMOKE ALL PASS\n");
  return 0;
}
