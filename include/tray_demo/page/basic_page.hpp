#pragma once

/**
 * @file basic_page.hpp
 * @brief 开箱即用的简单 @ref IPage 实现。
 */

#include "tray_demo/page/page_navigator.hpp"

#include <string>
#include <vector>

namespace tray_demo {

/**
 * @class BasicPage
 * @brief 带 id/title/可选 payload 的页面，适合快速定制。
 *
 * @customize
 * @code
 * BasicPage home("home", "首页");
 * home.SetPayload("{ \"version\": \"1.0\" }");  // 渲染层自行解析
 * navigator.SetRoot(&home);
 * @endcode
 */
class BasicPage : public IPage {
public:
  BasicPage(const std::string& id, const std::string& title)
      : id_(id), title_(title) {}

  virtual std::string id() const { return id_; }
  virtual std::string title() const { return title_; }

  void SetToolbarActionIds(const std::vector<std::string>& ids) {
    toolbar_ids_ = ids;
  }

  virtual void GetToolbarActionIds(std::vector<std::string>* out) const {
    if (out) {
      *out = toolbar_ids_;
    }
  }

  /**
   * @brief 自定义载荷（如 JSON 字符串）
   * @customize 面板渲染时读取 payload() 来填充 UI。
   */
  void SetPayload(const std::string& payload) { payload_ = payload; }
  const std::string& payload() const { return payload_; }

private:
  std::string id_;
  std::string title_;
  std::string payload_;
  std::vector<std::string> toolbar_ids_;
};

}  // namespace tray_demo
