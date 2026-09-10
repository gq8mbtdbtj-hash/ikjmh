/**
 * @file test_collector_unit.cpp
 * @brief collector 过滤 / 计数单元自测（无第三方测试框架）。
 *
 * 运行：hooks_unit_tests
 * 期望：全部 PASS，exit 0
 */

#include "tray_hooks/collector.h"
#include "tray_hooks/hooks.h"

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;
int g_sink_hits = 0;
char g_last_tag[64];

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

void Sink(const tray_hooks_event_t* ev, void*) {
  ++g_sink_hits;
  if (ev) {
    std::strncpy(g_last_tag, ev->tag, sizeof(g_last_tag) - 1);
  }
}

void Reset() {
  g_sink_hits = 0;
  g_last_tag[0] = '\0';
  tray_hooks_collector_set_filter(NULL);
  tray_hooks_collector_set_sink(Sink, NULL);
}

void TestAllowTags() {
  Reset();
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.allow_tags = "malloc,mmap";
  tray_hooks_collector_set_filter(&f);

  const uint64_t before_drop = tray_hooks_collector_dropped();
  tray_hooks_collector_record("calloc", NULL, 0, 16);
  tray_hooks_collector_record("malloc", NULL, 0, 32);
  CHECK(g_sink_hits == 1);
  CHECK(std::strcmp(g_last_tag, "malloc") == 0);
  CHECK(tray_hooks_collector_dropped() > before_drop);
  std::printf("[PASS] allow_tags\n");
}

void TestDenyTags() {
  Reset();
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.deny_tags = "calloc";
  tray_hooks_collector_set_filter(&f);
  tray_hooks_collector_record("calloc", NULL, 0, 8);
  tray_hooks_collector_record("malloc", NULL, 0, 8);
  CHECK(g_sink_hits == 1);
  CHECK(std::strcmp(g_last_tag, "malloc") == 0);
  std::printf("[PASS] deny_tags\n");
}

void TestMinArg0() {
  Reset();
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.min_arg0 = 100;
  tray_hooks_collector_set_filter(&f);
  tray_hooks_collector_record("malloc", NULL, 0, 50);
  tray_hooks_collector_record("malloc", NULL, 0, 200);
  CHECK(g_sink_hits == 1);
  std::printf("[PASS] min_arg0\n");
}

void TestSample() {
  Reset();
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.sample_n = 3;
  tray_hooks_collector_set_filter(&f);
  for (int i = 0; i < 9; ++i) {
    tray_hooks_collector_record("malloc", NULL, 0, 1);
  }
  // 每 3 次过 1 次 → 约 3 次；允许实现从 seq%3==0 起步
  CHECK(g_sink_hits >= 2 && g_sink_hits <= 4);
  std::printf("[PASS] sample_n (hits=%d)\n", g_sink_hits);
}

void MetaSink(const tray_hooks_event_t* ev, void* user) {
  tray_hooks_event_t* snap = static_cast<tray_hooks_event_t*>(user);
  if (ev && snap) {
    *snap = *ev;
  }
}

void TestEventMeta() {
  Reset();
  tray_hooks_event_t snap;
  std::memset(&snap, 0, sizeof(snap));
  tray_hooks_collector_set_sink(MetaSink, &snap);
  tray_hooks_collector_record("meta2", NULL, 0, 42);
  CHECK(snap.pid != 0);
  CHECK(snap.tid != 0);
  CHECK(snap.arg0 == 42);
  CHECK(snap.process[0] != '\0');
  std::printf("[PASS] event pid/tid/process\n");
}

void TestHooksInitSmoke() {
  CHECK(tray_hooks_init(TRAY_HOOKS_MODE_MANUAL) == TRAY_HOOKS_OK);
  const char* name = tray_hooks_backend_name();
  CHECK(name && name[0]);
  std::printf("[PASS] hooks_init backend=%s\n", name);
  tray_hooks_uninit();
}

}  // namespace

int main() {
  std::printf("=== hooks_unit_tests ===\n");
  TestAllowTags();
  TestDenyTags();
  TestMinArg0();
  TestSample();
  TestEventMeta();
  TestHooksInitSmoke();
  if (g_fail) {
    std::fprintf(stderr, "FAILED checks: %d\n", g_fail);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
