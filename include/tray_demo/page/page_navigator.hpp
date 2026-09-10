#pragma once

/**
 * @file page_navigator.hpp
 * @brief 左键面板的多级页面栈（根页 + 二级/更深）。
 */

#include <string>
#include <vector>

namespace tray_demo {

/**
 * @class IPage
 * @brief 面板中的一页；内容如何绘制由 @ref IPanelRenderer 决定。
 *
 * @customize 实现自己的页面类（或用 @ref BasicPage），
 * 在需要进入详情时 `navigator.Push(&page)`。
 */
class IPage {
public:
  virtual ~IPage() {}

  virtual std::string id() const = 0;     ///< 稳定页面 ID
  virtual std::string title() const = 0;  ///< 标题（面包屑/标题栏可用）

  /**
   * @brief 可选工具栏动作 ID 列表
   * @customize 若面板要画「刷新」等按钮，在此返回动作 id，并在 HandleMenuAction 同类逻辑中处理。
   */
  virtual void GetToolbarActionIds(std::vector<std::string>* out) const {
    if (out) {
      out->clear();
    }
  }
};

/**
 * @class IPageNavigationListener
 * @brief 页面栈变化通知（@ref AppController 用于刷新面板）。
 */
class IPageNavigationListener {
public:
  virtual ~IPageNavigationListener() {}
  virtual void OnPageStackChanged() = 0;
};

/**
 * @class PageNavigator
 * @brief 页面导航：SetRoot / Push / Pop。
 *
 * @customize
 * - 首页：`SetRoot(&home)`
 * - 二级：`Push(&detail)`（须先有 root）
 * - 返回：`Pop()` / `PopToRoot()`
 */
class PageNavigator {
public:
  PageNavigator();

  void SetListener(IPageNavigationListener* listener);

  /// @brief 设置根页（清空后压入）
  void SetRoot(IPage* page);

  /**
   * @brief 压入二级或更深页面
   * @note page 生命周期由调用方保证
   */
  bool Push(IPage* page);

  bool Pop();
  void PopToRoot();
  void Clear();

  IPage* Current() const;  ///< 栈顶；空栈返回 NULL
  IPage* Root() const;
  bool CanGoBack() const;
  std::size_t Depth() const;

  /// @brief 自根到栈顶的标题列表（面包屑）
  void GetBreadcrumbTitles(std::vector<std::string>* out) const;

private:
  void NotifyChanged();

  std::vector<IPage*> stack_;
  IPageNavigationListener* listener_;
};

}  // namespace tray_demo
