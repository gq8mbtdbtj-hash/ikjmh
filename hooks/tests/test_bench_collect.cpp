/**
 * @file test_bench_collect.cpp
 * @brief collector / backtrace 热路径粗测：打印 ns/op，异常慢则失败。
 */

#include "tray_hooks/backtrace.h"
#include "tray_hooks/collector.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;
volatile int g_sink_sink = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

void Sink(const tray_hooks_event_t* ev, void*) {
  if (ev) {
    g_sink_sink += ev->nframes;
  }
}

double NsPerOp(int iters, void (*fn)(int)) {
  using clock = std::chrono::steady_clock;
  fn(iters / 10 > 0 ? iters / 10 : 1);
  const auto t0 = clock::now();
  fn(iters);
  const auto t1 = clock::now();
  const double ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  return ns / static_cast<double>(iters);
}

void RecNoFilter(int n) {
  for (int i = 0; i < n; ++i) {
    tray_hooks_collector_record("bench", NULL, 0, static_cast<uint64_t>(i));
  }
}

void RecWithSample(int n) {
  for (int i = 0; i < n; ++i) {
    tray_hooks_collector_record("bench", NULL, 0, static_cast<uint64_t>(i));
  }
}

void RecEmptyFrames(int n) {
  tray_hooks_frame_t empty;
  std::memset(&empty, 0, sizeof(empty));
  for (int i = 0; i < n; ++i) {
    tray_hooks_collector_record("bench", &empty, 0, static_cast<uint64_t>(i));
  }
}

void BtOnly(int n) {
  tray_hooks_frame_t frames[32];
  for (int i = 0; i < n; ++i) {
    g_sink_sink += tray_hooks_backtrace(frames, 32, 1);
  }
}

}  // namespace

int main() {
  std::printf("=== test_bench_collect ===\n");
  const int iters = 20000;

  tray_hooks_collector_set_sink(Sink, NULL);
  tray_hooks_collector_set_filter(NULL);
  const double ns_plain = NsPerOp(iters, RecNoFilter);
  std::printf("  record(no filter, auto bt): %.1f ns/op\n", ns_plain);

  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.sample_n = 4;
  f.min_arg0 = 0;
  tray_hooks_collector_set_filter(&f);
  const double ns_sample = NsPerOp(iters, RecWithSample);
  std::printf("  record(sample_n=4):         %.1f ns/op\n", ns_sample);

  tray_hooks_collector_set_filter(NULL);
  const double ns_empty = NsPerOp(iters, RecEmptyFrames);
  std::printf("  record(nframes=0 passed):   %.1f ns/op\n", ns_empty);

  const double ns_bt = NsPerOp(iters, BtOnly);
  std::printf("  backtrace(32):              %.1f ns/op\n", ns_bt);

  // Soft ceilings for CI VMs (catch pathological regressions only).
  CHECK(ns_plain < 5e6);
  CHECK(ns_sample < 5e6);
  CHECK(ns_empty < 2e6);
  CHECK(ns_bt < 5e6);
  CHECK(ns_plain > 0.0);

  if (g_fail) {
    std::fprintf(stderr, "BENCH FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
