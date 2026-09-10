#pragma once

/**
 * @file demo_catalog_model.hpp
 * @brief Demo 列表数据源（IListPanelModel）。
 */

#include "tray_demo/panel/list_panel_model.hpp"

#include <vector>

namespace tray_demo {
namespace modules {

class DemoCatalogModel : public IListPanelModel {
public:
  /// @brief 重新从 BuildDemoCatalog 装载
  void Refresh();

  virtual void CopyItems(std::vector<PanelListItem>* out) const;

private:
  std::vector<PanelListItem> items_;
};

}  // namespace modules
}  // namespace tray_demo
