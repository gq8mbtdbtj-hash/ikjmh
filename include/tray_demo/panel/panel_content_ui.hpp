#pragma once

/**
 * @file panel_content_ui.hpp
 * @brief 原生面板宿主接口：页眉文案 + 列表数据源绑定。
 *
 * 框架（WinPanelRenderer）实现；业务只提供 IListPanelModel 与文案。
 */

#include "tray_demo/panel/list_item.hpp"
#include "tray_demo/panel/list_panel_model.hpp"

#include <string>
#include <vector>

namespace tray_demo {

class IPanelContentUi {
public:
  virtual ~IPanelContentUi() {}

  /// @brief 顶部状态区文案
  virtual void SetHeader(const std::string& title,
                         const std::string& line1,
                         const std::string& line2,
                         const std::string& body) = 0;

  /// @brief 绑定列表数据源（不取得所有权；生命周期由 module 保证）
  virtual void BindListModel(IListPanelModel* model) { (void)model; }

  /// @brief 从已绑定 model 重新拉取并刷新列表控件
  virtual void ReloadList() {}

  /// @brief 兼容旧推送式 API（内部快照，无 model 时可用）
  virtual void SetListItems(const std::vector<PanelListItem>& items) {
    (void)items;
  }

  // ---- 兼容旧名 ----
  virtual void UpdateDemoContent(const std::string& title,
                                 const std::string& line1,
                                 const std::string& line2,
                                 const std::string& body) {
    SetHeader(title, line1, line2, body);
  }
  virtual void SetDemoList(const std::vector<PanelListItem>& items) {
    SetListItems(items);
  }
};

typedef IPanelContentUi IDemoPanelUi;

}  // namespace tray_demo
