#pragma once

/**
 * @file demo_module.hpp
 * @brief Demo 业务模块：拉配置 / 面板数据源 / WebView 切换（登录见 AuthModule）。
 *
 * CUSTOMIZE: 新产品请复制本模块或另写 IAppModule，在 main 里 RegisterModule。
 * 登录/登出请用框架 @ref tray_demo::AuthModule，勿再写 demo.login。
 */

#include "tray_demo/modules/demo/demo_catalog_model.hpp"
#include "tray_demo/plugin/app_module.hpp"

namespace tray_demo {
namespace modules {

class DemoModule : public IAppModule {
public:
  virtual const char* module_id() const;
  virtual int menu_order() const;
  virtual void BuildMenu(AppController& app, MenuModel& menu);
  virtual bool HandleAction(AppController& app, const std::string& action_id);
  virtual void RefreshPanel(AppController& app);
  virtual void OnSessionChanged(AppController& app);

private:
  DemoCatalogModel catalog_;
};

}  // namespace modules
}  // namespace tray_demo
