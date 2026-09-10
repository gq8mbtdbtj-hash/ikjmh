#pragma once

/**
 * @file panel_renderer.hpp
 * @brief 左键面板「如何显示」——定制 UI 内容时改这里的实现。
 */

#include "tray_demo/page/page_navigator.hpp"

namespace tray_demo {

/**
 * @class IPanelRenderer
 * @brief 浮动面板窗口与页面绘制。
 *
 * @platform Windows：`WinPanelRenderer`（demo 为空窗口）
 * @customize 在自己的实现里重写 @ref RenderNavigation：
 * 读取 `navigator.Current()` / 面包屑，绘制列表、版本号、按钮等。
 */
class IPanelRenderer {
public:
  virtual ~IPanelRenderer() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() const = 0;

  /**
   * @brief 可选：面板铺满当前显示器工作区（非强制；由 settings / 菜单切换）
   * @customize settings.ini：panel_allow_fullscreen / panel_fullscreen
   */
  virtual void SetFullscreen(bool enabled) { (void)enabled; }
  virtual bool IsFullscreen() const { return false; }

  /**
   * @brief 根据当前页面栈刷新面板内容
   * @customize **画业务 UI 的主入口**。
   */
  virtual void RenderNavigation(const PageNavigator& navigator) = 0;

  /**
   * @brief 栈未变、仅数据变时刷新
   * @customize 拉取 JSON 完成后调用。
   */
  virtual void InvalidateCurrentPage() {}
};

}  // namespace tray_demo
