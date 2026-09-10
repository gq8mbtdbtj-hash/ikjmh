#include "tray_demo/menu/menu_model.hpp"

namespace tray_demo {

void MenuModel::Clear() { items_.clear(); }

void MenuModel::Add(const MenuItem& item) { items_.push_back(item); }

MenuItem& MenuModel::AddAction(const std::string& id, const std::string& title) {
  items_.push_back(MenuItem::Action(id, title));
  return items_.back();
}

MenuItem& MenuModel::AddSeparator() {
  items_.push_back(MenuItem::Separator());
  return items_.back();
}

MenuItem& MenuModel::AddSubMenu(const std::string& id, const std::string& title) {
  items_.push_back(MenuItem::SubMenu(id, title));
  return items_.back();
}

const MenuItem* MenuModel::FindById(const std::string& id) const {
  return FindInList(items_, id);
}

const MenuItem* MenuModel::FindInList(const std::vector<MenuItem>& list,
                                      const std::string& id) {
  for (std::size_t i = 0; i < list.size(); ++i) {
    const MenuItem& it = list[i];
    if (it.kind != kMenuSeparator && it.id == id) {
      return &it;
    }
    if (it.kind == kMenuSubMenu) {
      const MenuItem* found = FindInList(it.children, id);
      if (found) {
        return found;
      }
    }
  }
  return 0;
}

}  // namespace tray_demo
