#pragma once

/**
 * @file null_platform.hpp
 * @brief 空平台：不弹真实托盘，用于单元测试 / 无 GUI 冒烟。
 * @internal 正式 Windows 工具请用 win/win_platform.hpp。
 */

#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/menu/menu_model.hpp"
#include "tray_demo/page/basic_page.hpp"
#include "tray_demo/page/page_navigator.hpp"
#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/thread/job_queue.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/ui/auth_ui.hpp"

namespace tray_demo {

/// @brief 空托盘宿主（不创建真实图标）
class NullTrayHost : public ITrayHost {
public:
  NullTrayHost();
  virtual bool Create();
  virtual void Destroy();
  virtual void SetEventHandler(ITrayEventHandler* handler);
  virtual void SetTooltip(const std::string& text);
  virtual void SetIcon(const std::string& icon_key);

  ITrayEventHandler* handler() const { return handler_; }
  const std::string& tooltip() const { return tooltip_; }

private:
  ITrayEventHandler* handler_;
  std::string tooltip_;
  std::string icon_key_;
  bool created_;
};

class NullMenuRenderer : public IMenuRenderer {
public:
  NullMenuRenderer();
  virtual void PresentContextMenu(const MenuModel& model,
                                  IMenuActionHandler* handler);
  virtual void Dismiss();

  int present_count() const { return present_count_; }
  const MenuModel& last_model() const { return last_model_; }

  /// 测试辅助：模拟用户点选某 action_id
  void SimulateClick(const std::string& action_id);

private:
  MenuModel last_model_;
  IMenuActionHandler* handler_;
  int present_count_;
};

class NullPanelRenderer : public IPanelRenderer {
public:
  NullPanelRenderer();
  virtual void Show();
  virtual void Hide();
  virtual bool IsVisible() const;
  virtual void RenderNavigation(const PageNavigator& navigator);
  virtual void InvalidateCurrentPage();

  int render_count() const { return render_count_; }
  std::string last_page_id() const { return last_page_id_; }

private:
  bool visible_;
  int render_count_;
  std::string last_page_id_;
};

/// @brief 空登录 UI（PromptLogin 恒失败；测试用）
class NullAuthUi : public IAuthUi {
public:
  virtual bool PromptLogin(SessionService* session);
};

void BindNullPlatform(PlatformServices* out,
                      NullTrayHost* tray,
                      NullMenuRenderer* menu,
                      NullPanelRenderer* panel,
                      NullAuthUi* auth = 0);

}  // namespace tray_demo
