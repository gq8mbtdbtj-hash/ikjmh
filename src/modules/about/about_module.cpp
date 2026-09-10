#include "tray_demo/modules/about/about_module.hpp"

#include "tray_demo/app/app_controller.hpp"

namespace tray_demo {
namespace modules {

const char* AboutModule::module_id() const { return "about"; }

int AboutModule::menu_order() const { return 900; }

void AboutModule::BuildMenu(AppController& /*app*/, MenuModel& menu) {
  menu.AddAction(MakeActionId(module_id(), "info"), "关于 tray_demo…");
}

bool AboutModule::HandleAction(AppController& app, const std::string& action_id) {
  if (action_id != MakeActionId(module_id(), "info")) {
    return false;
  }
  IPanelContentUi* ui = app.platform().panel_content_ui;
  if (ui) {
    ui->SetHeader(
        "关于 tray_demo",
        "薄框架 + 可插拔 IAppModule",
        "本项来自 AboutModule（menu_order=900）",
        "动作 id 使用 about.info 前缀；与 demo.* 互不抢占。");
  }
  app.ShowActivePanel();
  return true;
}

}  // namespace modules
}  // namespace tray_demo
