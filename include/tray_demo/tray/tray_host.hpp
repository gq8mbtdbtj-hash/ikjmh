#pragma once

/**
 * @file tray_host.hpp
 * @brief 托盘图标宿主与点击事件接口。
 */

#include <string>

namespace tray_demo {

class ITrayEventHandler;

/**
 * @class ITrayHost
 * @brief 系统托盘 / 菜单栏状态项抽象。
 *
 * @platform Windows：`WinTrayHost`（Shell_NotifyIcon）
 * @customize 改提示文案/图标：`SetTooltip` / `SetIcon`（在 main 或 Start 后调用）。
 */
class ITrayHost {
public:
  virtual ~ITrayHost() {}

  virtual bool Create() = 0;
  virtual void Destroy() = 0;

  virtual void SetEventHandler(ITrayEventHandler* handler) = 0;

  /// @brief 鼠标悬停提示
  virtual void SetTooltip(const std::string& text) = 0;
  /**
   * @brief 设置图标
   * @param icon_key 路径或资源键，由平台解释；空表示默认
   * @customize 换成自己的 .ico / 资源 ID。
   */
  virtual void SetIcon(const std::string& icon_key) = 0;

  /// @brief 请求退出消息循环
  virtual void RequestQuit() {}
};

/**
 * @class ITrayEventHandler
 * @brief 托盘点击回调（默认由 @ref AppController 实现）。
 */
class ITrayEventHandler {
public:
  virtual ~ITrayEventHandler() {}
  virtual void OnTrayLeftClick() = 0;
  virtual void OnTrayRightClick() = 0;
  /// @customize 若需要双击行为，覆盖此方法。
  virtual void OnTrayDoubleClick() {}
};

}  // namespace tray_demo
