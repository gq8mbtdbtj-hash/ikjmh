#pragma once

/**
 * @file list_item.hpp
 * @brief 原生面板列表条目（框架类型；内容由业务 module 填充）。
 */

#include <string>
#include <vector>

namespace tray_demo {

struct PanelListItem {
  std::string id;
  std::string name;
  std::string category;
  std::string detail;
};

typedef PanelListItem DemoListItem;  ///< 兼容旧名

std::vector<std::string> CollectCategories(const std::vector<PanelListItem>& items);

}  // namespace tray_demo
