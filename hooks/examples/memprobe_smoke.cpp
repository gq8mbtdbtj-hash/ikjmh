/**
 * @file memprobe_smoke.cpp
 * @brief 验证分配统计；Windows 链 tray_memprobe.dll，Linux 可用 LD_PRELOAD。
 */

#include "tray_hooks/memprobe.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
  std::printf("memprobe_smoke: allocating...\n");
  // 若未自动 install（极端情况），手动补一次
  tray_memprobe_start();

  std::vector<void*> ptrs;
  for (int i = 0; i < 50; ++i) {
    ptrs.push_back(std::malloc(static_cast<size_t>(64 + i * 16)));
  }
  for (size_t i = 0; i < ptrs.size(); i += 2) {
    std::free(ptrs[i]);
    ptrs[i] = 0;
  }
  void* leak = std::malloc(1024 * 64);
  (void)leak;

  tray_memprobe_stats_t st;
  tray_memprobe_get_stats(&st);
  std::printf("heap live_bytes=%llu peak=%llu cpu%%=%.2f\n",
              static_cast<unsigned long long>(st.live_bytes),
              static_cast<unsigned long long>(st.peak_bytes),
              st.cpu.process_cpu_percent);

  tray_memprobe_dump(0);
  std::printf("memprobe_smoke: done\n");
  return 0;
}
