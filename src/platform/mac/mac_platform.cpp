#include "tray_demo/platform/mac/mac_platform.hpp"

#include "tray_demo/page/page_navigator.hpp"

namespace tray_demo {
namespace mac {

MacTrayHost::MacTrayHost()
    : handler_(0), created_(false), quit_requested_(false) {}

bool MacTrayHost::Create() {
  // PLATFORM: stub — 接入 NSStatusItem 时在此创建状态栏项
  created_ = true;
  return true;
}

void MacTrayHost::Destroy() { created_ = false; }

void MacTrayHost::SetEventHandler(ITrayEventHandler* handler) {
  handler_ = handler;
}

void MacTrayHost::SetTooltip(const std::string& text) { tooltip_ = text; }

void MacTrayHost::SetIcon(const std::string& icon_key) { icon_key_ = icon_key; }

void MacTrayHost::RequestQuit() { quit_requested_ = true; }

MacMenuRenderer::MacMenuRenderer() : handler_(0) {}

void MacMenuRenderer::PresentContextMenu(const MenuModel& model,
                                         IMenuActionHandler* handler) {
  // PLATFORM: stub — 接入 NSMenu 后在此弹出并回传 action
  last_model_ = model;
  handler_ = handler;
  (void)handler_;
}

void MacMenuRenderer::Dismiss() {}

MacPanelRenderer::MacPanelRenderer()
    : visible_(false), fullscreen_(false) {}

void MacPanelRenderer::Show() { visible_ = true; }

void MacPanelRenderer::Hide() { visible_ = false; }

bool MacPanelRenderer::IsVisible() const { return visible_; }

void MacPanelRenderer::SetFullscreen(bool enabled) { fullscreen_ = enabled; }

bool MacPanelRenderer::IsFullscreen() const { return fullscreen_; }

void MacPanelRenderer::RenderNavigation(const PageNavigator& navigator) {
  IPage* cur = navigator.Current();
  last_page_id_ = cur ? cur->id() : std::string();
}

void MacPanelRenderer::InvalidateCurrentPage() {}

MacAuthUi::MacAuthUi() {}

bool MacAuthUi::PromptLogin(SessionService* /*session*/) {
  // PLATFORM: stub — 尚未实现登录对话框；返回 false
  return false;
}

void BindMacPlatform(PlatformServices* out,
                     MacTrayHost* tray,
                     MacMenuRenderer* menu,
                     MacPanelRenderer* panel,
                     MacAuthUi* auth) {
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

}  // namespace mac
}  // namespace tray_demo
