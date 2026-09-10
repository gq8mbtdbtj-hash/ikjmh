/**
 * @file memprobe_module.hpp
 * @brief 托盘联动：菜单控制 tray_memprobe 采集 / dump / APM。
 *
 * 需链接 tray_memprobe；DllMain/constructor 会自动 start，菜单可手动干预。
 */

#pragma once

#include "tray_demo/plugin/app_module.hpp"

namespace tray_demo {
namespace modules {

class MemprobeModule : public IAppModule {
public:
  virtual const char* module_id() const;
  virtual int menu_order() const;
  virtual void BuildMenu(AppController& app, MenuModel& menu);
  virtual bool HandleAction(AppController& app, const std::string& action_id);
};

}  // namespace modules
}  // namespace tray_demo
