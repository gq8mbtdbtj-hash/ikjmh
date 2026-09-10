#include "tray_demo/auth/auth_module.hpp"

#include "tray_demo/app/app_controller.hpp"

namespace tray_demo {

const char* AuthModule::module_id() const { return "auth"; }

int AuthModule::menu_order() const { return 10; }

void AuthModule::BuildMenu(AppController& app, MenuModel& menu) {
  const AppSettings& s = app.settings();
  if (app.session().IsLoggedIn()) {
    menu.AddAction(MakeActionId(module_id(), "logout"),
                   "登出 (" + s.username + ")");
  } else {
    menu.AddAction(MakeActionId(module_id(), "login"), "登录…");
  }
}

bool AuthModule::HandleAction(AppController& app, const std::string& action_id) {
  PlatformServices& p = app.platform();

  if (action_id == MakeActionId(module_id(), "login")) {
    if (p.auth_ui && p.auth_ui->PromptLogin(&app.session())) {
      app.SaveSettings();
      app.NotifySessionChanged();
      IPanelRenderer* panel = app.ActivePanel();
      if (panel && panel->IsVisible()) {
        panel->InvalidateCurrentPage();
      }
    }
    return true;
  }
  if (action_id == MakeActionId(module_id(), "logout")) {
    app.session().Logout();
    app.SaveSettings();
    app.NotifySessionChanged();
    return true;
  }
  return false;
}

}  // namespace tray_demo
