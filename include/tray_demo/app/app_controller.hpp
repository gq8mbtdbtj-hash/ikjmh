#pragma once

/**
 * @file app_controller.hpp
 * @brief 框架编排中枢：托盘 / 面板宿主 / module 分发（不含具体业务菜单逻辑）。
 */

#include "tray_demo/config/settings.hpp"
#include "tray_demo/menu/menu_model.hpp"
#include "tray_demo/net/session.hpp"
#include "tray_demo/page/page_navigator.hpp"
#include "tray_demo/panel/panel_content_ui.hpp"
#include "tray_demo/plugin/app_module.hpp"
#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/render/webview_panel.hpp"
#include "tray_demo/thread/job_queue.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/ui/auth_ui.hpp"

#include <string>
#include <vector>

namespace tray_demo {

struct PlatformServices {
  ITrayHost* tray;
  IMenuRenderer* menu_renderer;
  IPanelRenderer* panel_renderer;
  IWebViewPanel* webview_panel;
  IAuthUi* auth_ui;
  IPanelContentUi* panel_content_ui;  ///< 原生面板内容宿主（非业务）

  PlatformServices()
      : tray(0),
        menu_renderer(0),
        panel_renderer(0),
        webview_panel(0),
        auth_ui(0),
        panel_content_ui(0) {}
};

/**
 * @class AppController
 * @brief 框架控制器。业务请实现 @ref IAppModule 并 @ref RegisterModule。
 */
class AppController : public ITrayEventHandler,
                      public IMenuActionHandler,
                      public IPageNavigationListener {
public:
  AppController();
  ~AppController();

  void SetPlatform(const PlatformServices& platform);
  PlatformServices& platform() { return platform_; }
  const PlatformServices& platform() const { return platform_; }

  AppSettings& settings() { return settings_; }
  const AppSettings& settings() const { return settings_; }
  SessionService& session() { return session_; }
  const SessionService& session() const { return session_; }

  MenuModel& menu_model() { return menu_model_; }
  PageNavigator& navigator() { return navigator_; }
  JobQueue& jobs() { return jobs_; }

  /// @brief 注册业务模块（不取得所有权；生命周期由调用方保证）
  void RegisterModule(IAppModule* module);

  void LoadSettings();
  void SaveSettings();

  void BuildQuitOnlyMenu();
  void RebuildMenuForSession();

  bool Start();
  void Shutdown();
  bool is_started() const { return started_; }

  void SimulateLeftClick();
  void SimulateRightClick();

  bool OpenSecondaryPage(IPage* page);
  bool GoBack();

  virtual void OnTrayLeftClick();
  virtual void OnTrayRightClick();
  virtual void OnMenuAction(const std::string& action_id);
  virtual void OnPageStackChanged();
  virtual void HandleMenuAction(const std::string& action_id);

  /// @brief 通知各 module 刷新面板
  void RefreshPanelUi();

  IPanelRenderer* ActivePanel();
  void ShowActivePanel();
  void SyncWebViewAuth();
  bool EnsureWebViewOrFallback();
  void NotifySessionChanged();

private:
  void HideInactivePanels();
  bool DispatchToModules(const std::string& action_id);
  /// @brief 按 menu_order 排序后的 module 列表（稳定：同 order 保注册序）
  void ModulesInMenuOrder(std::vector<IAppModule*>* out) const;

  PlatformServices platform_;
  AppSettings settings_;
  SessionService session_;
  std::string settings_path_;
  MenuModel menu_model_;
  PageNavigator navigator_;
  JobQueue jobs_;
  std::vector<IAppModule*> modules_;
  bool started_;
};

}  // namespace tray_demo
