#include "tray_demo/platform/null_platform.hpp"

namespace tray_demo {

NullTrayHost::NullTrayHost()
    : handler_(0), created_(false) {}

bool NullTrayHost::Create() {
  created_ = true;
  return true;
}

void NullTrayHost::Destroy() {
  created_ = false;
}

void NullTrayHost::SetEventHandler(ITrayEventHandler* handler) {
  handler_ = handler;
}

void NullTrayHost::SetTooltip(const std::string& text) { tooltip_ = text; }

void NullTrayHost::SetIcon(const std::string& icon_key) { icon_key_ = icon_key; }

NullMenuRenderer::NullMenuRenderer()
    : handler_(0), present_count_(0) {}

void NullMenuRenderer::PresentContextMenu(const MenuModel& model,
                                          IMenuActionHandler* handler) {
  last_model_ = model;
  handler_ = handler;
  ++present_count_;
}

void NullMenuRenderer::Dismiss() {}

void NullMenuRenderer::SimulateClick(const std::string& action_id) {
  if (handler_) {
    handler_->OnMenuAction(action_id);
  }
}

NullPanelRenderer::NullPanelRenderer()
    : visible_(false), render_count_(0) {}

void NullPanelRenderer::Show() { visible_ = true; }

void NullPanelRenderer::Hide() { visible_ = false; }

bool NullPanelRenderer::IsVisible() const { return visible_; }

void NullPanelRenderer::RenderNavigation(const PageNavigator& navigator) {
  ++render_count_;
  IPage* cur = navigator.Current();
  last_page_id_ = cur ? cur->id() : std::string();
}

void NullPanelRenderer::InvalidateCurrentPage() { ++render_count_; }

bool NullAuthUi::PromptLogin(SessionService* /*session*/) { return false; }

void BindNullPlatform(PlatformServices* out,
                      NullTrayHost* tray,
                      NullMenuRenderer* menu,
                      NullPanelRenderer* panel,
                      NullAuthUi* auth) {
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

}  // namespace tray_demo
