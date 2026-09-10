#include "tray_demo/panel/list_item.hpp"

#include <set>

namespace tray_demo {

std::vector<std::string> CollectCategories(const std::vector<PanelListItem>& items) {
  std::set<std::string> uniq;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (!items[i].category.empty()) {
      uniq.insert(items[i].category);
    }
  }
  std::vector<std::string> out;
  out.push_back("全部");
  for (std::set<std::string>::const_iterator it = uniq.begin(); it != uniq.end(); ++it) {
    out.push_back(*it);
  }
  return out;
}

}  // namespace tray_demo
