#pragma once

/**
 * @file session.hpp
 * @brief 登录会话与拉取配置的薄封装。
 *
 * 登录响应字段契约见 docs/api-contract.md（token / username / roles / expires_in）。
 */

#include "tray_demo/config/settings.hpp"
#include "tray_demo/net/http_client.hpp"

#include <string>

namespace tray_demo {

/**
 * @class SessionService
 * @brief 对接 Go 简易服务器的登录 / 拉 JSON。
 *
 * @customize API 路径与 JSON 字段解析可按服务端约定修改；稳定键见 api-contract。
 */
class SessionService {
public:
  explicit SessionService(AppSettings* settings);

  AppSettings* settings() { return settings_; }

  bool IsLoggedIn() const;

  /**
   * @brief LDAP/mock 登录（服务端校验）
   * @return true 表示拿到 token
   */
  bool Login(const std::string& user, const std::string& password, std::string* err);

  void Logout();

  /// @brief GET config JSON，写入 last_config_json_
  bool FetchConfigJson(std::string* err);

  const std::string& last_config_json() const { return last_config_json_; }
  const std::string& status_text() const { return status_text_; }
  const std::string& last_roles_json() const { return last_roles_json_; }
  int last_expires_in() const { return last_expires_in_; }

  /// @brief 从已加载的 settings 同步状态文案
  void SyncStatusFromSettings();

private:
  AppSettings* settings_;
  std::string last_config_json_;
  std::string status_text_;
  std::string last_roles_json_;
  int last_expires_in_;
};

}  // namespace tray_demo
