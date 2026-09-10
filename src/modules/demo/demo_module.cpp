#include "tray_demo/modules/demo/demo_module.hpp"

#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/modules/demo/demo_catalog.hpp"

namespace tray_demo {
namespace modules {

const char* DemoModule::module_id() const { return "demo"; }

int DemoModule::menu_order() const { return 100; }

void DemoModule::BuildMenu(AppController& app, MenuModel& menu) {
  // =========================================================================
  // CUSTOMIZE: demo 业务菜单（登录已迁至 AuthModule：auth.login / auth.logout）
  // =========================================================================
  AppSettings& s = app.settings();
  SessionService& sess = app.session();

  if (sess.IsLoggedIn()) {
    menu.AddAction(MakeActionId(module_id(), "fetch_config"), "拉取配置 JSON");
  }
  menu.AddAction(MakeActionId(module_id(), "open_panel"), "打开面板");
  if (s.IsWebViewPanel()) {
    menu.AddAction(MakeActionId(module_id(), "panel_ui_native"),
                   "面板：切换到原生");
  } else {
    menu.AddAction(MakeActionId(module_id(), "panel_ui_webview"),
                   "面板：切换到 WebView");
  }
  if (s.panel_allow_fullscreen) {
    if (s.panel_fullscreen) {
      menu.AddAction(MakeActionId(module_id(), "panel_windowed"),
                     "面板：退出全屏");
    } else {
      menu.AddAction(MakeActionId(module_id(), "panel_fullscreen"),
                     "面板：全屏显示");
    }
  }
}

bool DemoModule::HandleAction(AppController& app, const std::string& action_id) {
  PlatformServices& p = app.platform();
  AppSettings& s = app.settings();

  if (action_id == MakeActionId(module_id(), "fetch_config")) {
    std::string err;
    if (!app.session().FetchConfigJson(&err)) {
      if (p.panel_content_ui) {
        p.panel_content_ui->SetHeader("拉取失败", s.http_base_url, err, "");
      }
    } else {
      app.RefreshPanelUi();
    }
    if (!s.IsWebViewPanel()) {
      IPanelRenderer* panel = app.ActivePanel();
      if (panel) {
        if (!panel->IsVisible()) {
          panel->Show();
        }
        panel->InvalidateCurrentPage();
      }
    }
    return true;
  }
  if (action_id == MakeActionId(module_id(), "open_panel")) {
    app.ShowActivePanel();
    return true;
  }
  if (action_id == MakeActionId(module_id(), "panel_ui_webview")) {
    s.panel_ui = "webview";
    app.SaveSettings();
    app.RebuildMenuForSession();
    if (!app.EnsureWebViewOrFallback()) {
      return true;
    }
    app.ShowActivePanel();
    return true;
  }
  if (action_id == MakeActionId(module_id(), "panel_ui_native")) {
    s.panel_ui = "native";
    app.SaveSettings();
    app.RebuildMenuForSession();
    app.ShowActivePanel();
    return true;
  }
  if (action_id == MakeActionId(module_id(), "panel_fullscreen")) {
    if (!s.panel_allow_fullscreen) {
      return true;
    }
    s.panel_fullscreen = true;
    app.SaveSettings();
    app.RebuildMenuForSession();
    IPanelRenderer* panel = app.ActivePanel();
    if (panel) {
      panel->SetFullscreen(true);
      if (!panel->IsVisible()) {
        app.ShowActivePanel();
      }
      panel->InvalidateCurrentPage();
    }
    return true;
  }
  if (action_id == MakeActionId(module_id(), "panel_windowed")) {
    s.panel_fullscreen = false;
    app.SaveSettings();
    app.RebuildMenuForSession();
    IPanelRenderer* panel = app.ActivePanel();
    if (panel) {
      panel->SetFullscreen(false);
      panel->InvalidateCurrentPage();
    }
    return true;
  }
  if (action_id == MakeActionId(module_id(), "refresh")) {
    app.RefreshPanelUi();
    IPanelRenderer* panel = app.ActivePanel();
    if (panel) {
      panel->InvalidateCurrentPage();
    }
    return true;
  }
  return false;
}

void DemoModule::RefreshPanel(AppController& app) {
  IPanelContentUi* ui = app.platform().panel_content_ui;
  if (!ui) {
    return;
  }
  const AppSettings& s = app.settings();
  const SessionService& sess = app.session();
  const std::string title = "tray_demo 面板定制示例";
  const std::string line1 = "HTTP: " + s.http_base_url;
  const std::string line2 = sess.IsLoggedIn()
                                ? ("状态: " + sess.status_text())
                                : "状态: 未登录（右键 → 登录，AuthModule）";
  std::string body = sess.last_config_json();
  if (body.empty()) {
    body = "列表数据来自 DemoCatalogModel（IListPanelModel）。\n"
           "框架宿主只做搜索/分类过滤。";
  } else if (body.size() > 400) {
    body = body.substr(0, 400) + "\n…";
  }
  ui->SetHeader(title, line1, line2, body);
  catalog_.Refresh();
  ui->BindListModel(&catalog_);
  ui->ReloadList();
}

void DemoModule::OnSessionChanged(AppController& app) {
  RefreshPanel(app);
}

}  // namespace modules
}  // namespace tray_demo
