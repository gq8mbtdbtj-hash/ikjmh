#include "tray_demo/modules/demo/demo_catalog_model.hpp"

#include "tray_demo/modules/demo/demo_catalog.hpp"

namespace tray_demo {
namespace modules {

void DemoCatalogModel::Refresh() {
  items_ = BuildDemoCatalog();
}

void DemoCatalogModel::CopyItems(std::vector<PanelListItem>* out) const {
  if (!out) {
    return;
  }
  *out = items_;
}

}  // namespace modules
}  // namespace tray_demo
