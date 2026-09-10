#pragma once

/**
 * @file app_module.hpp
 * @brief 业务扩展点：实现本接口并在 main 中 RegisterModule。
 *
 * CUSTOMIZE: 新产品 = 新 IAppModule，不要改 AppController 业务分支。
 * 动作 id 必须使用「module_id.动作」前缀（见 OwnsAction），避免多 module 冲突。
 */

#include "tray_demo/menu/menu_model.hpp"

#include <string>

namespace tray_demo {

class AppController;

/**
 * @class IAppModule
 * @brief 业务模块插件接口。
 */
class IAppModule {
public:
  virtual ~IAppModule() {}

  /// @brief 模块 id；动作前缀与此一致（如 demo → demo.fetch_config）
  virtual const char* module_id() const = 0;

  /**
   * @brief 菜单合并顺序（升序；同 order 按注册先后）
   * @details 业务模块建议 100–800；靠后的「关于」类可用 900。
   */
  virtual int menu_order() const { return 100; }

  /// @brief 向菜单追加本业务项（不要添加 quit；框架会追加）
  virtual void BuildMenu(AppController& app, MenuModel& menu) = 0;

  /**
   * @brief 是否认领该动作 id（默认：以 module_id. 为前缀）
   */
  virtual bool OwnsAction(const std::string& action_id) const {
    const char* id = module_id();
    if (!id || !*id) {
      return false;
    }
    const std::string prefix = std::string(id) + ".";
    return action_id.size() >= prefix.size() &&
           action_id.compare(0, prefix.size(), prefix) == 0;
  }

  /**
   * @brief 处理菜单/动作 id
   * @return true 表示已处理
   */
  virtual bool HandleAction(AppController& app, const std::string& action_id) = 0;

  /// @brief 刷新原生面板内容（WebView 配置可由 module 调 app.SyncWebViewAuth）
  virtual void RefreshPanel(AppController& app) { (void)app; }

  /// @brief 登录态变化后回调
  virtual void OnSessionChanged(AppController& app) { (void)app; }

  /// @brief 拼接带命名空间的动作 id
  static std::string MakeActionId(const char* module_id, const char* action) {
    if (!module_id || !*module_id) {
      return action ? std::string(action) : std::string();
    }
    if (!action || !*action) {
      return std::string(module_id);
    }
    return std::string(module_id) + "." + action;
  }
};

}  // namespace tray_demo
