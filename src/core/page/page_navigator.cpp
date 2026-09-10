#include "tray_demo/page/page_navigator.hpp"

namespace tray_demo {

PageNavigator::PageNavigator() : listener_(0) {}

void PageNavigator::SetListener(IPageNavigationListener* listener) {
  listener_ = listener;
}

void PageNavigator::SetRoot(IPage* page) {
  stack_.clear();
  if (page) {
    stack_.push_back(page);
  }
  NotifyChanged();
}

bool PageNavigator::Push(IPage* page) {
  if (!page || stack_.empty()) {
    return false;
  }
  stack_.push_back(page);
  NotifyChanged();
  return true;
}

bool PageNavigator::Pop() {
  if (stack_.size() <= 1) {
    return false;
  }
  stack_.pop_back();
  NotifyChanged();
  return true;
}

void PageNavigator::PopToRoot() {
  if (stack_.empty()) {
    return;
  }
  IPage* root = stack_.front();
  stack_.clear();
  stack_.push_back(root);
  NotifyChanged();
}

void PageNavigator::Clear() {
  stack_.clear();
  NotifyChanged();
}

IPage* PageNavigator::Current() const {
  if (stack_.empty()) {
    return 0;
  }
  return stack_.back();
}

IPage* PageNavigator::Root() const {
  if (stack_.empty()) {
    return 0;
  }
  return stack_.front();
}

bool PageNavigator::CanGoBack() const { return stack_.size() > 1; }

std::size_t PageNavigator::Depth() const { return stack_.size(); }

void PageNavigator::GetBreadcrumbTitles(std::vector<std::string>* out) const {
  if (!out) {
    return;
  }
  out->clear();
  for (std::size_t i = 0; i < stack_.size(); ++i) {
    if (stack_[i]) {
      out->push_back(stack_[i]->title());
    }
  }
}

void PageNavigator::NotifyChanged() {
  if (listener_) {
    listener_->OnPageStackChanged();
  }
}

}  // namespace tray_demo
