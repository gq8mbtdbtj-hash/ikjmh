#pragma once

/**
 * @file mac_platform.hpp
 * @brief macOS 平台桩：接口对齐 Win，尚未接 NSStatusItem / NSMenu / NSPanel。
 *
 * @platform macOS — 当前实现为可编译桩，行为接近 Null；真机托盘后续替换本文件。
 */

#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/menu/menu_model.hpp"
#include "tray_demo/page/page_navigator.hpp"
#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/ui/auth_ui.hpp"

namespace tray_demo {
namespace mac {

/// @brief macOS 状态栏图标桩（未来：NSStatusItem）
class MacTrayHost : public ITrayHost {
public:
  MacTrayHost();
  virtual bool Create();
  virtual void Destroy();
  virtual void SetEventHandler(ITrayEventHandler* handler);
  virtual void SetTooltip(const std::string& text);
  virtual void SetIcon(const std::string& icon_key);
  virtual void RequestQuit();

  ITrayEventHandler* handler() const { return handler_; }
  bool created() const { return created_; }
  bool quit_requested() const { return quit_requested_; }

private:
  ITrayEventHandler* handler_;
  std::string tooltip_;
  std::string icon_key_;
  bool created_;
  bool quit_requested_;
};

/// @brief macOS 右键菜单桩（未来：NSMenu）
class MacMenuRenderer : public IMenuRenderer {
public:
  MacMenuRenderer();
  virtual void PresentContextMenu(const MenuModel& model,
                                  IMenuActionHandler* handler);
  virtual void Dismiss();

  const MenuModel& last_model() const { return last_model_; }

private:
  MenuModel last_model_;
  IMenuActionHandler* handler_;
};

/// @brief macOS 面板桩（未来：NSPanel / NSPopover）
class MacPanelRenderer : public IPanelRenderer {
public:
  MacPanelRenderer();
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

/// @brief macOS 登录 UI 桩（未来：NSAlert / 自定义窗）
class MacAuthUi : public IAuthUi {
public:
  MacAuthUi();
  virtual bool PromptLogin(SessionService* session);
};

void BindMacPlatform(PlatformServices* out,
                     MacTrayHost* tray,
                     MacMenuRenderer* menu,
                     MacPanelRenderer* panel,
                     MacAuthUi* auth);

}  // namespace mac
}  // namespace tray_demo
