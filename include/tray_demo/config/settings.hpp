#pragma once

/**
 * @file settings.hpp
 * @brief 本地配置（默认 HTTP 服务地址等）。
 *
 * @customize 修改 DefaultSettings() 中的默认 URL，或编辑用户目录下 settings.ini。
 */

#include <string>

namespace tray_demo {

/**
 * @struct AppSettings
 * @brief 运行时配置。
 */
struct AppSettings {
  std::string http_base_url;   ///< 如 http://127.0.0.1:8080
  std::string username;        ///< 最近登录用户（可选缓存）
  std::string auth_token;      ///< 登录后 Bearer token
  std::string config_json_path;///< 相对服务端：/api/v1/config.json
  std::string update_manifest_path; ///< /api/v1/update/manifest

  /// @brief 是否启用「面板全屏」可选功能（菜单显示切换项）
  bool panel_allow_fullscreen;
  /// @brief 当前是否以全屏工作区打开面板（可菜单切换）
  bool panel_fullscreen;

  /// @brief 面板 UI：native | webview（Windows WebView2）
  std::string panel_ui;
  /// @brief WebView 远程前端 URL
  std::string panel_webview_url;
  /// @brief 注入 localStorage 的 token 键名
  std::string panel_webview_token_key;

  AppSettings()
      : panel_allow_fullscreen(true), panel_fullscreen(false) {}

  bool IsWebViewPanel() const {
    return panel_ui == "webview";
  }

  static AppSettings DefaultSettings();
};

/**
 * @class SettingsStore
 * @brief 读写 %APPDATA%/tray_demo/settings.ini（简单 key=value）。
 */
class SettingsStore {
public:
  /// @brief 默认配置文件完整路径
  static std::string DefaultFilePath();

  bool Load(const std::string& path, AppSettings* out);
  bool Save(const std::string& path, const AppSettings& in);
};

}  // namespace tray_demo
