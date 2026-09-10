#pragma once

/**
 * @file menu_renderer.hpp
 * @brief 右键菜单「如何显示」——平台实现，业务一般只改 MenuModel。
 */

#include "tray_demo/menu/menu_model.hpp"

namespace tray_demo {

/**
 * @class IMenuRenderer
 * @brief 将 @ref MenuModel 呈现为原生上下文菜单。
 *
 * @platform Windows：`WinMenuRenderer`（支持二级 HMENU）
 * @customize 除非要换皮肤/非原生菜单，否则不必实现此类；改 @ref MenuModel 即可。
 */
class IMenuRenderer {
public:
  virtual ~IMenuRenderer() {}

  /**
   * @brief 弹出菜单；点选后回调 handler
   * @param model 菜单数据
   * @param handler 动作接收者（通常为 AppController）
   */
  virtual void PresentContextMenu(const MenuModel& model,
                                  IMenuActionHandler* handler) = 0;

  virtual void Dismiss() {}
};

}  // namespace tray_demo
