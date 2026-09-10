#include "tray_demo/app/app_controller.hpp"

namespace tray_demo {

AppController::AppController()
    : session_(&settings_), started_(false) {
  settings_ = AppSettings::DefaultSettings();
  settings_path_ = SettingsStore::DefaultFilePath();
  navigator_.SetListener(this);
}

AppController::~AppController() { Shutdown(); }

void AppController::SetPlatform(const PlatformServices& platform) {
  platform_ = platform;
}

void AppController::RegisterModule(IAppModule* module) {
  if (!module) {
    return;
  }
  for (std::size_t i = 0; i < modules_.size(); ++i) {
    if (modules_[i] == module) {
      return;
    }
  }
  modules_.push_back(module);
}

void AppController::LoadSettings() {
  SettingsStore store;
  if (!store.Load(settings_path_, &settings_)) {
    settings_ = AppSettings::DefaultSettings();
    store.Save(settings_path_, settings_);
  }
  if (settings_.panel_ui != "webview") {
    settings_.panel_ui = "native";
  }
  session_.SyncStatusFromSettings();
}

void AppController::SaveSettings() {
  SettingsStore store;
  store.Save(settings_path_, settings_);
}

void AppController::BuildQuitOnlyMenu() {
  menu_model_.Clear();
  menu_model_.AddAction("quit", "退出");
}

void AppController::RebuildMenuForSession() {
  menu_model_.Clear();
  std::vector<IAppModule*> ordered;
  ModulesInMenuOrder(&ordered);
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    const std::size_t before = menu_model_.items().size();
    ordered[i]->BuildMenu(*this, menu_model_);
    if (before > 0 && menu_model_.items().size() > before) {
      const MenuItem& prev = menu_model_.items()[before - 1];
      if (prev.kind != kMenuSeparator) {
        menu_model_.mutable_items().insert(
            menu_model_.mutable_items().begin() +
                static_cast<std::ptrdiff_t>(before),
            MenuItem::Separator());
      }
    }
  }
  if (!menu_model_.FindById("quit")) {
    if (!menu_model_.items().empty()) {
      const MenuItem& last = menu_model_.items().back();
      if (last.kind != kMenuSeparator) {
        menu_model_.AddSeparator();
      }
    }
    menu_model_.AddAction("quit", "退出");
  }
}

IPanelRenderer* AppController::ActivePanel() {
  if (settings_.IsWebViewPanel() && platform_.webview_panel) {
    return platform_.webview_panel;
  }
  return platform_.panel_renderer;
}

void AppController::HideInactivePanels() {
  if (settings_.IsWebViewPanel()) {
    if (platform_.panel_renderer) {
      platform_.panel_renderer->Hide();
    }
  } else if (platform_.webview_panel) {
    platform_.webview_panel->Hide();
  }
}

void AppController::SyncWebViewAuth() {
  if (!platform_.webview_panel) {
    return;
  }
  platform_.webview_panel->Configure(settings_.panel_webview_url,
                                     settings_.panel_webview_token_key);
  if (session_.IsLoggedIn() && !settings_.auth_token.empty()) {
    platform_.webview_panel->InjectAuth(settings_.auth_token, settings_.username);
  } else {
    platform_.webview_panel->ClearAuth();
  }
}

bool AppController::EnsureWebViewOrFallback() {
  if (!settings_.IsWebViewPanel()) {
    return true;
  }
  if (!platform_.webview_panel) {
    settings_.panel_ui = "native";
    SaveSettings();
    RebuildMenuForSession();
    return false;
  }
  SyncWebViewAuth();
  if (platform_.webview_panel->FailedMissingRuntime()) {
    settings_.panel_ui = "native";
    SaveSettings();
    RebuildMenuForSession();
    return false;
  }
  return true;
}

void AppController::ShowActivePanel() {
  HideInactivePanels();
  if (!EnsureWebViewOrFallback()) {
    HideInactivePanels();
  }
  IPanelRenderer* panel = ActivePanel();
  if (!panel) {
    return;
  }
  if (settings_.panel_allow_fullscreen) {
    panel->SetFullscreen(settings_.panel_fullscreen);
  }
  RefreshPanelUi();
  panel->Show();
  panel->RenderNavigation(navigator_);
}

void AppController::NotifySessionChanged() {
  for (std::size_t i = 0; i < modules_.size(); ++i) {
    modules_[i]->OnSessionChanged(*this);
  }
  RebuildMenuForSession();
  RefreshPanelUi();
  SyncWebViewAuth();
}

void AppController::RefreshPanelUi() {
  for (std::size_t i = 0; i < modules_.size(); ++i) {
    modules_[i]->RefreshPanel(*this);
  }
  if (platform_.webview_panel) {
    SyncWebViewAuth();
  }
  IPanelRenderer* panel = ActivePanel();
  if (panel && settings_.panel_allow_fullscreen) {
    panel->SetFullscreen(settings_.panel_fullscreen);
  }
}

bool AppController::Start() {
  if (started_) {
    return true;
  }
  if (!platform_.tray || !platform_.menu_renderer || !platform_.panel_renderer) {
    return false;
  }
  LoadSettings();
  platform_.tray->SetEventHandler(this);
  if (!platform_.tray->Create()) {
    return false;
  }
  platform_.tray->SetTooltip("tray_demo");
  if (!jobs_.Start()) {
    platform_.tray->Destroy();
    return false;
  }
  RebuildMenuForSession();
  started_ = true;
  RefreshPanelUi();
  OnPageStackChanged();
  return true;
}

void AppController::Shutdown() {
  if (started_) {
    SaveSettings();
  }
  if (platform_.panel_renderer) {
    platform_.panel_renderer->Hide();
  }
  if (platform_.webview_panel) {
    platform_.webview_panel->Hide();
  }
  if (!started_) {
    if (platform_.tray) {
      platform_.tray->Destroy();
    }
    return;
  }
  if (platform_.menu_renderer) {
    platform_.menu_renderer->Dismiss();
  }
  jobs_.Stop();
  if (platform_.tray) {
    platform_.tray->Destroy();
  }
  started_ = false;
}

void AppController::SimulateLeftClick() { OnTrayLeftClick(); }
void AppController::SimulateRightClick() { OnTrayRightClick(); }

bool AppController::OpenSecondaryPage(IPage* page) {
  return navigator_.Push(page);
}
bool AppController::GoBack() { return navigator_.Pop(); }

void AppController::OnTrayLeftClick() {
  IPanelRenderer* panel = ActivePanel();
  if (!panel) {
    return;
  }
  if (panel->IsVisible()) {
    panel->Hide();
  } else {
    ShowActivePanel();
  }
}

void AppController::OnTrayRightClick() {
  if (!platform_.menu_renderer) {
    return;
  }
  RebuildMenuForSession();
  platform_.menu_renderer->PresentContextMenu(menu_model_, this);
}

void AppController::OnMenuAction(const std::string& action_id) {
  HandleMenuAction(action_id);
}

void AppController::OnPageStackChanged() {
  IPanelRenderer* panel = ActivePanel();
  if (panel && panel->IsVisible()) {
    panel->RenderNavigation(navigator_);
  }
}

bool AppController::DispatchToModules(const std::string& action_id) {
  // 1) 仅认领者（前缀匹配）按菜单顺序尝试
  std::vector<IAppModule*> ordered;
  ModulesInMenuOrder(&ordered);
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i]->OwnsAction(action_id) &&
        ordered[i]->HandleAction(*this, action_id)) {
      return true;
    }
  }
  return false;
}

void AppController::ModulesInMenuOrder(std::vector<IAppModule*>* out) const {
  out->clear();
  out->reserve(modules_.size());
  for (std::size_t i = 0; i < modules_.size(); ++i) {
    out->push_back(modules_[i]);
  }
  // 稳定插入排序：menu_order 升序，同 order 保注册序
  for (std::size_t i = 1; i < out->size(); ++i) {
    IAppModule* key = (*out)[i];
    const int key_order = key->menu_order();
    std::size_t j = i;
    while (j > 0 && (*out)[j - 1]->menu_order() > key_order) {
      (*out)[j] = (*out)[j - 1];
      --j;
    }
    (*out)[j] = key;
  }
}

void AppController::HandleMenuAction(const std::string& action_id) {
  // 框架动作
  if (action_id == "quit") {
    Shutdown();
    if (platform_.tray) {
      platform_.tray->RequestQuit();
    }
    return;
  }
  // 业务 module（按 OwnsAction 前缀路由）
  if (DispatchToModules(action_id)) {
    return;
  }
  (void)action_id;
}

}  // namespace tray_demo
