#pragma once

/**
 * @file auth_module.hpp
 * @brief 框架级登录模块：菜单登录/登出，不属产品业务。
 *
 * CUSTOMIZE: 一般保留；产品勿再在 DemoModule 里重复登录项。
 */

#include "tray_demo/plugin/app_module.hpp"

namespace tray_demo {

class AuthModule : public IAppModule {
public:
  virtual const char* module_id() const;
  virtual int menu_order() const;
  virtual void BuildMenu(AppController& app, MenuModel& menu);
  virtual bool HandleAction(AppController& app, const std::string& action_id);
};

}  // namespace tray_demo
