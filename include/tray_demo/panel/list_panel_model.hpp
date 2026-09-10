#pragma once

/**
 * @file list_panel_model.hpp
 * @brief 原生列表面板的业务数据源（框架只负责宿主与过滤 UI）。
 *
 * CUSTOMIZE: 业务 module 实现本接口，经 IPanelContentUi::BindListModel 绑定。
 */

#include "tray_demo/panel/list_item.hpp"

#include <vector>

namespace tray_demo {

class IListPanelModel {
public:
  virtual ~IListPanelModel() {}

  /// @brief 复制当前全部条目到 out（宿主再做模糊搜索 / 分类过滤）
  virtual void CopyItems(std::vector<PanelListItem>* out) const = 0;
};

}  // namespace tray_demo
