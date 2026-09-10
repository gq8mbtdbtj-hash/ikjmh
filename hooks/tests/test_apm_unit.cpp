/**
 * @file test_apm_unit.cpp
 * @brief APM 落盘冒烟：写临时 NDJSON，确认 emit / flush。
 */

#include "tray_hooks/apm.h"
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
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

std::string TempPath() {
#if defined(_WIN32)
  char dir[MAX_PATH];
  GetTempPathA(MAX_PATH, dir);
  return std::string(dir) + "tray_apm_unit.ndjson";
#else
  return "/tmp/tray_apm_unit.ndjson";
#endif
}

}  // namespace

int main() {
  std::printf("=== test_apm_unit ===\n");
  const std::string path = TempPath();
  std::remove(path.c_str());

  tray_hooks_apm_config_t cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.file_path = path.c_str();
  cfg.interval_ms = 100;
  cfg.batch_max = 8;
  cfg.include_stacks = 0;
  CHECK(tray_hooks_apm_start(&cfg) == 0);

  tray_hooks_collector_record("ut_malloc", NULL, 0, 128);
  tray_hooks_collector_record("ut_mmap", NULL, 0, 4096);
  tray_hooks_apm_emit_raw("{\"type\":\"unit\",\"ok\":1}");
  tray_hooks_apm_flush();
  tray_hooks_apm_stop();

  FILE* fp = std::fopen(path.c_str(), "r");
  CHECK(fp != 0);
  char buf[4096];
  size_t n = 0;
  if (fp) {
    n = std::fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    std::fclose(fp);
  }
  CHECK(n > 0);
  CHECK(std::strstr(buf, "ut_malloc") != 0);
  CHECK(std::strstr(buf, "unit") != 0);
  std::printf("  wrote %zu bytes -> %s\n", n, path.c_str());

  if (g_fail) {
    std::fprintf(stderr, "APM UNIT FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
