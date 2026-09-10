#pragma once

/**
 * @file auth_ui.hpp
 * @brief 登录 UI 抽象（平台实现弹窗）。
 */

#include "tray_demo/net/session.hpp"

namespace tray_demo {

class IAuthUi {
public:
  virtual ~IAuthUi() {}
  /// @return true 登录成功
  virtual bool PromptLogin(SessionService* session) = 0;
};

}  // namespace tray_demo
