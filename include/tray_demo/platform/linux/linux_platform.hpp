#pragma once

/**
 * @file linux_platform.hpp
 * @brief Linux 平台桩：接口对齐 Win/mac；尚未接 StatusNotifierItem / GTK/Qt 菜单。
 *
 * @platform Linux — 可编译桩，行为接近 Null；桌面托盘后续替换本文件。
 */

#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/menu/menu_model.hpp"
#include "tray_demo/page/page_navigator.hpp"
#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/ui/auth_ui.hpp"

namespace tray_demo {
namespace linux_plat {

/// @brief Linux 托盘桩（未来：StatusNotifierItem / AppIndicator）
class LinuxTrayHost : public ITrayHost {
public:
  LinuxTrayHost();
  virtual bool Create();
  virtual void Destroy();
  virtual void SetEventHandler(ITrayEventHandler* handler);
  virtual void SetTooltip(const std::string& text);
  virtual void SetIcon(const std::string& icon_key);
  virtual void RequestQuit();

  bool created() const { return created_; }
  bool quit_requested() const { return quit_requested_; }

private:
  ITrayEventHandler* handler_;
  std::string tooltip_;
  std::string icon_key_;
  bool created_;
  bool quit_requested_;
};

class LinuxMenuRenderer : public IMenuRenderer {
public:
  LinuxMenuRenderer();
  virtual void PresentContextMenu(const MenuModel& model,
                                  IMenuActionHandler* handler);
  virtual void Dismiss();

private:
  MenuModel last_model_;
  IMenuActionHandler* handler_;
};

class LinuxPanelRenderer : public IPanelRenderer {
public:
  LinuxPanelRenderer();
  virtual void Show();
  virtual void Hide();
  virtual bool IsVisible() const;
  virtual void SetFullscreen(bool enabled);
  virtual bool IsFullscreen() const;
  virtual void RenderNavigation(const PageNavigator& navigator);
  virtual void InvalidateCurrentPage();

private:
  bool visible_;
  bool fullscreen_;
  std::string last_page_id_;
};

class LinuxAuthUi : public IAuthUi {
public:
  virtual bool PromptLogin(SessionService* session);
};

void BindLinuxPlatform(PlatformServices* out,
                       LinuxTrayHost* tray,
                       LinuxMenuRenderer* menu,
                       LinuxPanelRenderer* panel,
                       LinuxAuthUi* auth);

}  // namespace linux_plat
}  // namespace tray_demo
