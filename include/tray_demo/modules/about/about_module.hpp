#pragma once

/**
 * @file about_module.hpp
 * @brief 第二业务 module 示例：证明多 module 菜单合并与前缀分发。
 *
 * CUSTOMIZE: 可删除本模块；不影响 demo / 框架。
 */

#include "tray_demo/plugin/app_module.hpp"

namespace tray_demo {
namespace modules {

class AboutModule : public IAppModule {
public:
  virtual const char* module_id() const;
  virtual int menu_order() const;
  virtual void BuildMenu(AppController& app, MenuModel& menu);
  virtual bool HandleAction(AppController& app, const std::string& action_id);
};

}  // namespace modules
}  // namespace tray_demo
