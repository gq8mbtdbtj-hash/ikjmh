#pragma once

/**
 * @file menu_model.hpp
 * @brief 右键菜单数据模型（与如何绘制无关）。
 */

#include <string>
#include <vector>

namespace tray_demo {

/**
 * @enum MenuItemKind
 * @brief 菜单项类型。
 */
enum MenuItemKind {
  kMenuAction = 0,      ///< 可点击动作（触发 @ref IMenuActionHandler）
  kMenuSeparator = 1,   ///< 分隔线
  kMenuSubMenu = 2      ///< 子菜单容器（二级及更深）
};

/**
 * @struct MenuItem
 * @brief 单个菜单项；`kMenuSubMenu` 时通过 @ref children 嵌套。
 *
 * @customize 增加菜单项时优先改 @ref MenuModel
 *（`AppController::BuildQuitOnlyMenu` / `BuildDefaultMenuSkeleton`），
 * 不要在 Win 渲染层写死文案。
 */
struct MenuItem {
  std::string id;                 ///< 稳定动作 ID，点选回调用（如 `"quit"`）
  std::string title;              ///< 显示文案
  MenuItemKind kind;              ///< 类型
  bool enabled;                   ///< 是否可点
  bool checked;                   ///< 勾选态（预留）
  std::string shortcut;           ///< 快捷键文案（预留，可空）
  std::vector<MenuItem> children; ///< 仅 SubMenu 有效

  /// @brief 构造动作项
  static MenuItem Action(const std::string& id, const std::string& title) {
    MenuItem m;
    m.id = id;
    m.title = title;
    m.kind = kMenuAction;
    m.enabled = true;
    m.checked = false;
    return m;
  }

  /// @brief 构造分隔线
  static MenuItem Separator() {
    MenuItem m;
    m.kind = kMenuSeparator;
    m.enabled = true;
    m.checked = false;
    return m;
  }

  /// @brief 构造子菜单（二级入口）
  static MenuItem SubMenu(const std::string& id, const std::string& title) {
    MenuItem m;
    m.id = id;
    m.title = title;
    m.kind = kMenuSubMenu;
    m.enabled = true;
    m.checked = false;
    return m;
  }

  /// @brief 向子菜单追加子项，返回新子项引用便于继续链式添加
  MenuItem& AddChild(const MenuItem& child) {
    children.push_back(child);
    return children.back();
  }
};

/**
 * @class MenuModel
 * @brief 托盘右键菜单的完整模型。
 *
 * @customize 典型写法：
 * @code
 * menu.Clear();
 * menu.AddAction("refresh", "刷新");
 * MenuItem& more = menu.AddSubMenu("more", "更多");
 * more.AddChild(MenuItem::Action("about", "关于"));
 * menu.AddAction("quit", "退出");
 * @endcode
 */
class MenuModel {
public:
  void Clear();
  void Add(const MenuItem& item);
  MenuItem& AddAction(const std::string& id, const std::string& title);
  MenuItem& AddSeparator();
  MenuItem& AddSubMenu(const std::string& id, const std::string& title);

  /// @brief 只读访问根级项
  const std::vector<MenuItem>& items() const { return items_; }
  /// @brief 可写访问（高级定制）
  std::vector<MenuItem>& mutable_items() { return items_; }

  /**
   * @brief 按 id 深度优先查找
   * @return 找到返回指针，否则 NULL
   */
  const MenuItem* FindById(const std::string& id) const;

private:
  static const MenuItem* FindInList(const std::vector<MenuItem>& list,
                                    const std::string& id);
  std::vector<MenuItem> items_;
};

/**
 * @class IMenuActionHandler
 * @brief 菜单动作回调；由 @ref AppController 实现，业务可重写 HandleMenuAction。
 */
class IMenuActionHandler {
public:
  virtual ~IMenuActionHandler() {}
  /**
   * @brief 用户点选某动作项
   * @param action_id 对应 @ref MenuItem::id
   */
  virtual void OnMenuAction(const std::string& action_id) = 0;
};

}  // namespace tray_demo
