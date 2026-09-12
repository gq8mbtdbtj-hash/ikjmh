/**
 * @file test_filter_fuzz.cpp
 * @brief FILTER_* 畸形环境变量 / 畸形规则：不崩溃、可恢复。
 */

#include "tray_hooks/collector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

int g_fail = 0;
int g_hits = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

void Sink(const tray_hooks_event_t*, void*) { ++g_hits; }

#if defined(_WIN32)
void SetEnv(const char* k, const char* v) {
  if (v) {
    SetEnvironmentVariableA(k, v);
  } else {
    SetEnvironmentVariableA(k, NULL);
  }
}
#else
void SetEnv(const char* k, const char* v) {
  if (v) {
    setenv(k, v, 1);
  } else {
    unsetenv(k);
  }
}
#endif

void ClearFilterEnv() {
  SetEnv("TRAY_HOOKS_FILTER_TAGS", NULL);
  SetEnv("TRAY_HOOKS_FILTER_DENY_TAGS", NULL);
  SetEnv("TRAY_HOOKS_FILTER_TIDS", NULL);
  SetEnv("TRAY_HOOKS_FILTER_PROCESS", NULL);
  SetEnv("TRAY_HOOKS_FILTER_MODULES", NULL);
  SetEnv("TRAY_HOOKS_FILTER_MIN_ARG0", NULL);
  SetEnv("TRAY_HOOKS_FILTER_SAMPLE", NULL);
}

void ApplyAndRecord(const char* tag) {
  tray_hooks_collector_apply_env_filter();
  tray_hooks_collector_record(tag, NULL, 0, 16);
}

void TestMalformedEnv() {
  ClearFilterEnv();
  tray_hooks_collector_set_sink(Sink, NULL);
  tray_hooks_collector_set_filter(NULL);
  g_hits = 0;

  const char* cases[] = {
      "",
      ",",
      ",,,",
      "  ,  , ",
      "malloc,",
      ",mmap",
      "a,b,,c,",
      "\t\tmalloc\t",
      ";;;;",
      "x\"y,z\\w",
      "very_long_tag_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    SetEnv("TRAY_HOOKS_FILTER_TAGS", cases[i]);
    SetEnv("TRAY_HOOKS_FILTER_DENY_TAGS", cases[i]);
    SetEnv("TRAY_HOOKS_FILTER_MODULES", cases[i]);
    SetEnv("TRAY_HOOKS_FILTER_PROCESS", cases[i]);
    ApplyAndRecord("malloc");
  }

  const char* nums[] = {"", "0", "-1", "999999999999999999999", "abc", "1e9",
                        " 42 ", "0x10"};
  for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); ++i) {
    ClearFilterEnv();
    SetEnv("TRAY_HOOKS_FILTER_MIN_ARG0", nums[i]);
    SetEnv("TRAY_HOOKS_FILTER_SAMPLE", nums[i]);
    SetEnv("TRAY_HOOKS_FILTER_TIDS", nums[i]);
    ApplyAndRecord("malloc");
  }

  std::string huge;
  huge.reserve(64 * 1024);
  for (int i = 0; i < 2000; ++i) {
    if (i) {
      huge += ',';
    }
    huge += "t";
    huge += std::to_string(i);
  }
  ClearFilterEnv();
  SetEnv("TRAY_HOOKS_FILTER_TAGS", huge.c_str());
  ApplyAndRecord("t42");
  ApplyAndRecord("nope");

  ClearFilterEnv();
  tray_hooks_collector_set_filter(NULL);
  g_hits = 0;
  tray_hooks_collector_record("malloc", NULL, 0, 1);
  CHECK(g_hits == 1);
  std::printf("[PASS] malformed FILTER_* env (no crash)\n");
}

void TestDirectMalformedFilter() {
  tray_hooks_collector_set_sink(Sink, NULL);
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.allow_tags = ",,,";
  f.deny_tags = "  ";
  f.allow_tids = "not-a-tid,0,-3";
  f.allow_process = "";
  f.allow_modules = ",";
  f.min_arg0 = 0;
  f.sample_n = 0;
  tray_hooks_collector_set_filter(&f);
  g_hits = 0;
  tray_hooks_collector_record("malloc", NULL, 0, 8);
  tray_hooks_collector_set_filter(NULL);
  std::printf("[PASS] direct malformed filter_t\n");
}

}  // namespace

int main() {
  std::printf("=== test_filter_fuzz ===\n");
  TestMalformedEnv();
  TestDirectMalformedFilter();
  if (g_fail) {
    std::fprintf(stderr, "FILTER FUZZ FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
