/**
 * @file memprobe_module.cpp
 * @brief 托盘菜单：dump / start / stop / APM / 查看统计。
 */

#include "tray_demo/modules/memprobe/memprobe_module.hpp"

#include "tray_demo/app/app_controller.hpp"
#include "tray_hooks/memprobe.h"
#include "tray_hooks/memprobe_install.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tray_demo {
namespace modules {
namespace {

std::string DefaultDumpPath() {
#if defined(_WIN32)
  char dir[MAX_PATH];
  DWORD n = GetTempPathA(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH) {
    return "tray_memprobe_dump.txt";
  }
  return std::string(dir) + "tray_memprobe_dump.txt";
#else
  return "/tmp/tray_memprobe_dump.txt";
#endif
}

std::string DefaultApmPath() {
#if defined(_WIN32)
  char dir[MAX_PATH];
  DWORD n = GetTempPathA(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH) {
    return "tray_apm.ndjson";
  }
  return std::string(dir) + "tray_apm.ndjson";
#else
  return "/tmp/tray_apm.ndjson";
#endif
}

void ShowStats(AppController& app) {
  tray_memprobe_stats_t st;
  std::memset(&st, 0, sizeof(st));
  tray_memprobe_get_stats(&st);
  char line1[160];
  char line2[160];
  char body[256];
  std::snprintf(line1, sizeof(line1),
                "heap live=%llu peak=%llu",
                static_cast<unsigned long long>(st.live_bytes),
                static_cast<unsigned long long>(st.peak_bytes));
  std::snprintf(line2, sizeof(line2), "rss=%llu cpu%%=%.2f",
                static_cast<unsigned long long>(st.rss_bytes),
                st.cpu.process_cpu_percent);
  std::snprintf(body, sizeof(body),
                "alloc=%llu free=%llu sampled=%llu\n"
                "菜单: dump / start / stop / start_apm",
                static_cast<unsigned long long>(st.alloc_calls),
                static_cast<unsigned long long>(st.free_calls),
                static_cast<unsigned long long>(st.sampled_allocs));
  IPanelContentUi* ui = app.platform().panel_content_ui;
  if (ui) {
    ui->SetHeader("Memprobe", line1, line2, body);
  }
  app.ShowActivePanel();
}

}  // namespace

const char* MemprobeModule::module_id() const { return "memprobe"; }

int MemprobeModule::menu_order() const { return 850; }

void MemprobeModule::BuildMenu(AppController& /*app*/, MenuModel& menu) {
  MenuItem& sub =
      menu.AddSubMenu(MakeActionId(module_id(), "menu"), "Memprobe 采集");
  sub.AddChild(MenuItem::Action(MakeActionId(module_id(), "stats"), "查看统计"));
  sub.AddChild(MenuItem::Action(MakeActionId(module_id(), "dump"), "导出 dump"));
  sub.AddChild(MenuItem::Action(MakeActionId(module_id(), "start"), "开始采集"));
  sub.AddChild(MenuItem::Action(MakeActionId(module_id(), "stop"), "停止采集"));
  sub.AddChild(
      MenuItem::Action(MakeActionId(module_id(), "start_apm"), "启动 APM 落盘"));
}

bool MemprobeModule::HandleAction(AppController& app,
                                  const std::string& action_id) {
  const std::string stats_id = MakeActionId(module_id(), "stats");
  const std::string dump_id = MakeActionId(module_id(), "dump");
  const std::string start_id = MakeActionId(module_id(), "start");
  const std::string stop_id = MakeActionId(module_id(), "stop");
  const std::string apm_id = MakeActionId(module_id(), "start_apm");

  if (action_id == stats_id) {
    ShowStats(app);
    return true;
  }
  if (action_id == dump_id) {
    const std::string path = DefaultDumpPath();
    tray_memprobe_dump(path.c_str());
    IPanelContentUi* ui = app.platform().panel_content_ui;
    if (ui) {
      ui->SetHeader("Memprobe", "已导出 dump", path, "可用记事本打开查看 Top live");
    }
    app.ShowActivePanel();
    return true;
  }
  if (action_id == start_id) {
    tray_memprobe_start();
    tray_memprobe_install_hooks();
    ShowStats(app);
    return true;
  }
  if (action_id == stop_id) {
    tray_memprobe_stop();
    IPanelContentUi* ui = app.platform().panel_content_ui;
    if (ui) {
      ui->SetHeader("Memprobe", "已停止采集", "tray_memprobe_stop()", "");
    }
    app.ShowActivePanel();
    return true;
  }
  if (action_id == apm_id) {
    const std::string path = DefaultApmPath();
#if defined(_WIN32)
    SetEnvironmentVariableA("TRAY_HOOKS_APM_FILE", path.c_str());
    SetEnvironmentVariableA("TRAY_MEMPROBE_COLLECT", "1");
#else
    setenv("TRAY_HOOKS_APM_FILE", path.c_str(), 1);
    setenv("TRAY_MEMPROBE_COLLECT", "1", 1);
#endif
    tray_memprobe_start_apm();
    IPanelContentUi* ui = app.platform().panel_content_ui;
    if (ui) {
      ui->SetHeader("Memprobe APM", "持续落盘已启动", path,
                    "TRAY_MEMPROBE_COLLECT=1");
    }
    app.ShowActivePanel();
    return true;
  }
  return false;
}

}  // namespace modules
}  // namespace tray_demo
