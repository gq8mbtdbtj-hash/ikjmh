#pragma once

/**
 * @file win_panel_placement.hpp
 * @brief 面板锚定托盘图标 / 任务栏角落（非光标）。
 * @platform Windows
 */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace tray_demo {
namespace win {

struct NotifyIconAnchor {
  HWND hwnd;
  UINT id;
  bool valid;

  NotifyIconAnchor() : hwnd(NULL), id(0), valid(false) {}
};

/**
 * @brief 将面板放到托盘图标旁，并钳制在工作区内。
 *
 * 优先 Shell_NotifyIconGetRect；失败则按任务栏边（SHAppBarMessage）贴角落。
 */
void PositionPanelNearTray(HWND panel_hwnd, const NotifyIconAnchor& anchor);

}  // namespace win
}  // namespace tray_demo

#endif
