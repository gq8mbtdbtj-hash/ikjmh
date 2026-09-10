/**
 * @file sample_collect.cpp
 * @brief 演示：init → 登记 hook 符号 → 手动触发采集 + backtrace。
 *
 * 注意：当前后端为桩，不会改写 GOT/IAT；采集与堆栈 API 已可用。
 */

#include "tray_hooks/backtrace.h"
#include "tray_hooks/collector.h"
#include "tray_hooks/hooks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void OnEvent(const tray_hooks_event_t* ev, void* /*user*/) {
  char stack[2048];
  tray_hooks_backtrace_format(ev->frames, ev->nframes, stack, sizeof(stack));
  std::printf("[sink] tag=%s arg0=%llu frames=%d\n%s", ev->tag,
              static_cast<unsigned long long>(ev->arg0), ev->nframes, stack);
}

typedef void* (*malloc_fn)(size_t);

static void* proxy_malloc(size_t n) {
  tray_hooks_collector_record("malloc", NULL, 0, static_cast<uint64_t>(n));
  malloc_fn prev = (malloc_fn)tray_hooks_get_prev((void*)proxy_malloc);
  if (prev && prev != (malloc_fn)proxy_malloc) {
    return prev(n);
  }
  return std::malloc(n);
}

int main() {
  std::printf("backend=%s\n", tray_hooks_backend_name());
  if (tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC) != TRAY_HOOKS_OK) {
    std::fprintf(stderr, "init failed\n");
    return 1;
  }
  std::printf("backend after init=%s\n", tray_hooks_backend_name());

  tray_hooks_collector_set_sink(OnEvent, NULL);

  tray_hooks_stub_t* stub =
      tray_hooks_hook_all(NULL, "malloc", (void*)proxy_malloc, NULL, NULL);
  if (!stub) {
    std::fprintf(stderr, "hook_all failed\n");
    tray_hooks_uninit();
    return 1;
  }

  // 桩未改 GOT：直接调 proxy 演示采集路径
  void* p = proxy_malloc(128);
  std::free(p);

  tray_hooks_frame_t frames[16];
  const int n = tray_hooks_backtrace(frames, 16, 0);
  char text[1024];
  tray_hooks_backtrace_format(frames, n, text, sizeof(text));
  std::printf("--- direct backtrace (%d) ---\n%s", n, text);
  std::printf("collector total=%llu\n",
              static_cast<unsigned long long>(tray_hooks_collector_total()));

  tray_hooks_unhook(stub);
  tray_hooks_uninit();
  return 0;
}
