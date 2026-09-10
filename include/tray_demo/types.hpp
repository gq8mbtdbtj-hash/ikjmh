#pragma once

/**
 * @file types.hpp
 * @brief 公共小类型（C++11，无 string_view）。
 */

#include <string>

namespace tray_demo {

/**
 * @class StringRef
 * @brief 非拥有字符串引用，避免在接口中强制拷贝。
 * @internal 框架辅助类型，业务可选用。
 */
class StringRef {
public:
  StringRef() : data_(""), size_(0) {}
  StringRef(const char* s) : data_(s ? s : ""), size_(s ? std::char_traits<char>::length(s) : 0) {}
  StringRef(const std::string& s) : data_(s.data()), size_(s.size()) {}

  const char* data() const { return data_; }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  std::string to_string() const { return std::string(data_, size_); }

private:
  const char* data_;
  std::size_t size_;
};

}  // namespace tray_demo
