/**
 * @file win_panel_placement.cpp
 * @brief 托盘角锚定实现。
 */

#include "tray_demo/platform/win/win_panel_placement.hpp"

#ifdef _WIN32

#include <shellapi.h>

#include <cstring>

namespace tray_demo {
namespace win {
namespace {

RECT WorkAreaFromPoint(POINT pt) {
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi;
  std::memset(&mi, 0, sizeof(mi));
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(mon, &mi)) {
    return mi.rcWork;
  }
  RECT work;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  return work;
}

UINT TaskbarEdge() {
  APPBARDATA abd;
  std::memset(&abd, 0, sizeof(abd));
  abd.cbSize = sizeof(abd);
  if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
    return abd.uEdge;
  }
  return ABE_BOTTOM;
}

void ClampToWork(int* x, int* y, int w, int h, const RECT& work) {
  if (*x + w > work.right) {
    *x = work.right - w;
  }
  if (*y + h > work.bottom) {
    *y = work.bottom - h;
  }
  if (*x < work.left) {
    *x = work.left;
  }
  if (*y < work.top) {
    *y = work.top;
  }
}

bool TryIconRect(const NotifyIconAnchor& anchor, RECT* out) {
  if (!anchor.valid || !anchor.hwnd || !out) {
    return false;
  }
  NOTIFYICONIDENTIFIER nii;
  std::memset(&nii, 0, sizeof(nii));
  nii.cbSize = sizeof(nii);
  nii.hWnd = anchor.hwnd;
  nii.uID = anchor.id;
  const HRESULT hr = Shell_NotifyIconGetRect(&nii, out);
  return SUCCEEDED(hr) && (out->right > out->left) && (out->bottom > out->top);
}

}  // namespace

void PositionPanelNearTray(HWND panel_hwnd, const NotifyIconAnchor& anchor) {
  if (!panel_hwnd) {
    return;
  }

  RECT panel_rc;
  GetWindowRect(panel_hwnd, &panel_rc);
  const int w = panel_rc.right - panel_rc.left;
  const int h = panel_rc.bottom - panel_rc.top;
  const int gap = 8;

  RECT icon = {0, 0, 0, 0};
  const bool have_icon = TryIconRect(anchor, &icon);

  POINT ref;
  if (have_icon) {
    ref.x = (icon.left + icon.right) / 2;
    ref.y = (icon.top + icon.bottom) / 2;
  } else {
    RECT work0;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work0, 0);
    // 无图标时用任务栏所在角的工作区角落作为参考点
    const UINT edge = TaskbarEdge();
    if (edge == ABE_TOP) {
      ref.x = work0.right - 1;
      ref.y = work0.top;
    } else if (edge == ABE_LEFT) {
      ref.x = work0.left;
      ref.y = work0.bottom - 1;
    } else if (edge == ABE_RIGHT) {
      ref.x = work0.right - 1;
      ref.y = work0.bottom - 1;
    } else {
      ref.x = work0.right - 1;
      ref.y = work0.bottom - 1;
    }
  }

  const RECT work = WorkAreaFromPoint(ref);
  const UINT edge = TaskbarEdge();

  int x = 0;
  int y = 0;

  if (have_icon) {
    // 相对托盘图标：贴在任务栏内侧
    if (edge == ABE_TOP) {
      x = icon.right - w;
      y = icon.bottom + gap;
    } else if (edge == ABE_LEFT) {
      x = icon.right + gap;
      y = icon.bottom - h;
    } else if (edge == ABE_RIGHT) {
      x = icon.left - w - gap;
      y = icon.bottom - h;
    } else {
      // ABE_BOTTOM（默认）
      x = icon.right - w;
      y = icon.top - h - gap;
    }
  } else {
    // 无图标：贴工作区角落（托盘所在角）
    if (edge == ABE_TOP) {
      x = work.right - w - gap;
      y = work.top + gap;
    } else if (edge == ABE_LEFT) {
      x = work.left + gap;
      y = work.bottom - h - gap;
    } else if (edge == ABE_RIGHT) {
      x = work.right - w - gap;
      y = work.bottom - h - gap;
    } else {
      x = work.right - w - gap;
      y = work.bottom - h - gap;
    }
  }

  ClampToWork(&x, &y, w, h, work);
  SetWindowPos(panel_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

}  // namespace win
}  // namespace tray_demo

#endif
