#pragma once

/**
 * @file demo_catalog.hpp
 * @brief Demo 业务列表示例数据。
 * @customize 替换 BuildDemoCatalog 或解析远端 config.json。
 */

#include "tray_demo/panel/list_item.hpp"

#include <vector>

namespace tray_demo {
namespace modules {

std::vector<PanelListItem> BuildDemoCatalog();

}  // namespace modules
}  // namespace tray_demo
