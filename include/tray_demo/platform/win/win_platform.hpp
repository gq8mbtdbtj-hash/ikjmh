#pragma once

/**
 * @file win_platform.hpp
 * @brief Windows 平台实现：托盘 / 原生菜单 / 面板 / 单实例 / 消息循环。
 *
 * @platform 业务定制一般不用改本文件；画面板内容时扩展 @ref WinPanelRenderer。
 */

#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/render/webview_panel.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/ui/auth_ui.hpp"
#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/panel/list_item.hpp"
#include "tray_demo/panel/list_panel_model.hpp"
#include "tray_demo/platform/win/win_panel_placement.hpp"

#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace tray_demo {

struct PlatformServices;

namespace win {

#ifdef _WIN32

/**
 * @class WinTrayHost
 * @brief Shell_NotifyIcon 托盘宿主。
 * @platform Windows
 */
class WinTrayHost : public ITrayHost {
public:
  WinTrayHost();
  virtual ~WinTrayHost();

  virtual bool Create();
  virtual void Destroy();
  virtual void SetEventHandler(ITrayEventHandler* handler);
  virtual void SetTooltip(const std::string& text);
  virtual void SetIcon(const std::string& icon_key);
  virtual void RequestQuit();

  HWND hwnd() const { return hwnd_; }
  /// @brief 与 Shell_NotifyIcon 的 uID 一致，供面板锚定
  UINT notify_icon_id() const { return 1; }

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  bool AddNotifyIcon();
  void RemoveNotifyIcon();

  HWND hwnd_;
  NOTIFYICONDATAW nid_;
  ITrayEventHandler* handler_;
  std::wstring tooltip_;
  bool icon_added_;
};

/**
 * @class WinMenuRenderer
 * @brief 原生右键菜单（含二级 SubMenu）。
 * @platform Windows
 * @customize 菜单内容请改 MenuModel，不要在此写死项。
 */
class WinMenuRenderer : public IMenuRenderer {
public:
  WinMenuRenderer();
  virtual void PresentContextMenu(const MenuModel& model,
                                  IMenuActionHandler* handler);
  virtual void Dismiss();

  void SetOwnerHwnd(HWND hwnd) { owner_ = hwnd; }

private:
  HMENU BuildMenu(const std::vector<MenuItem>& items, UINT* next_id);
  void MapCommand(UINT cmd, const std::string& action_id);
  void ClearMaps();

  HWND owner_;
  IMenuActionHandler* handler_;
  std::vector<std::string> id_by_cmd_;
};

/**
 * @class WinPanelRenderer
 * @brief 面板宿主：页眉 + 搜索/分类过滤；列表数据来自 IListPanelModel。
 * @platform Windows
 */
class WinPanelRenderer : public IPanelRenderer, public IPanelContentUi {
public:
  WinPanelRenderer();
  virtual ~WinPanelRenderer();

  virtual void Show();
  virtual void Hide();
  virtual bool IsVisible() const;
  virtual void SetFullscreen(bool enabled);
  virtual bool IsFullscreen() const;
  virtual void RenderNavigation(const PageNavigator& navigator);
  virtual void InvalidateCurrentPage();

  virtual void SetHeader(const std::string& title,
                         const std::string& line1,
                         const std::string& line2,
                         const std::string& body);
  virtual void BindListModel(IListPanelModel* model);
  virtual void ReloadList();
  virtual void SetListItems(const std::vector<PanelListItem>& items);

  bool Create();
  void Destroy();
  HWND hwnd() const { return hwnd_; }

  void SetNotifyIconAnchor(HWND notify_hwnd, UINT notify_id);

private:
  enum {
    kIdSearch = 1001,
    kIdFilter = 1002,
    kIdList = 1003,
    kIdHint = 1004
  };

  static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void PositionNearTray();
  void ApplyWindowPlacement();
  void EnsureChildControls();
  void LayoutControls();
  void RebuildFilterCombo();
  void ApplyFilter();
  void Paint(HDC hdc);
  std::wstring Utf8ToWide(const std::string& utf8) const;
  std::string WideToUtf8(const std::wstring& wide) const;

  HWND hwnd_;
  HWND search_;
  HWND filter_;
  HWND list_;
  HWND hint_;
  bool visible_;
  bool fullscreen_;
  int normal_w_;
  int normal_h_;
  NotifyIconAnchor tray_anchor_;
  IListPanelModel* list_model_;
  std::wstring title_;
  std::wstring line1_;
  std::wstring line2_;
  std::wstring body_;
  std::vector<PanelListItem> all_items_;
  std::vector<int> filtered_index_;
};

/**
 * @class WinAuthUi
 * @brief 弹出登录对话框。
 */
class WinAuthUi : public IAuthUi {
public:
  explicit WinAuthUi(HWND owner);
  void SetOwner(HWND owner) { owner_ = owner; }
  virtual bool PromptLogin(SessionService* session);

private:
  HWND owner_;
};

void BindWinPlatform(PlatformServices* out,
                     WinTrayHost* tray,
                     WinMenuRenderer* menu,
                     WinPanelRenderer* panel,
                     WinAuthUi* auth,
                     IWebViewPanel* webview = 0);

int RunMessageLoop();

/**
 * @brief 单实例互斥
 * @customize 互斥名在 win_platform.cpp：Local\\tray_demo.single_instance
 */
bool TryAcquireSingleInstance();
void ReleaseSingleInstance();

#endif  // _WIN32

}  // namespace win
}  // namespace tray_demo
