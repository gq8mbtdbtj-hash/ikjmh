#include "tray_demo/platform/linux/linux_platform.hpp"

namespace tray_demo {
namespace linux_plat {

LinuxTrayHost::LinuxTrayHost()
    : handler_(0), created_(false), quit_requested_(false) {}

bool LinuxTrayHost::Create() {
  // PLATFORM: stub — 接入 StatusNotifierItem / AppIndicator
  created_ = true;
  return true;
}

void LinuxTrayHost::Destroy() { created_ = false; }

void LinuxTrayHost::SetEventHandler(ITrayEventHandler* handler) {
  handler_ = handler;
}

void LinuxTrayHost::SetTooltip(const std::string& text) { tooltip_ = text; }

void LinuxTrayHost::SetIcon(const std::string& icon_key) { icon_key_ = icon_key; }

void LinuxTrayHost::RequestQuit() { quit_requested_ = true; }

LinuxMenuRenderer::LinuxMenuRenderer() : handler_(0) {}

void LinuxMenuRenderer::PresentContextMenu(const MenuModel& model,
                                           IMenuActionHandler* handler) {
  last_model_ = model;
  handler_ = handler;
  (void)handler_;
}

void LinuxMenuRenderer::Dismiss() {}

LinuxPanelRenderer::LinuxPanelRenderer()
    : visible_(false), fullscreen_(false) {}

void LinuxPanelRenderer::Show() { visible_ = true; }

void LinuxPanelRenderer::Hide() { visible_ = false; }

bool LinuxPanelRenderer::IsVisible() const { return visible_; }

void LinuxPanelRenderer::SetFullscreen(bool enabled) { fullscreen_ = enabled; }

bool LinuxPanelRenderer::IsFullscreen() const { return fullscreen_; }

void LinuxPanelRenderer::RenderNavigation(const PageNavigator& navigator) {
  IPage* cur = navigator.Current();
  last_page_id_ = cur ? cur->id() : std::string();
}

void LinuxPanelRenderer::InvalidateCurrentPage() {}

bool LinuxAuthUi::PromptLogin(SessionService* /*session*/) {
  // PLATFORM: stub — 无图形登录框；可用环境变量日后接 CLI
  return false;
}

void BindLinuxPlatform(PlatformServices* out,
                       LinuxTrayHost* tray,
                       LinuxMenuRenderer* menu,
                       LinuxPanelRenderer* panel,
                       LinuxAuthUi* auth) {
  if (!out) {
    return;
  }
  out->tray = tray;
  out->menu_renderer = menu;
  out->panel_renderer = panel;
  out->webview_panel = 0;
  out->auth_ui = auth;
  out->panel_content_ui = 0;
}

}  // namespace linux_plat
}  // namespace tray_demo
