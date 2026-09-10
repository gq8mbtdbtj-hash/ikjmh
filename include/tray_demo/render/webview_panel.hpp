#pragma once

/**
 * @file webview_panel.hpp
 * @brief 可选 WebView 面板接口（平台实现，勿把 COM 泄漏到共享核心）。
 */

#include "tray_demo/render/panel_renderer.hpp"

#include <string>

namespace tray_demo {

/**
 * @class IWebViewPanel
 * @brief Windows WebView2 远程站点面板。
 * @customize settings：panel_ui=webview、panel_webview_url、panel_webview_token_key
 */
class IWebViewPanel : public IPanelRenderer {
public:
  virtual ~IWebViewPanel() {}

  /// @brief 设置要 Navigate 的远程 URL 与 localStorage token 键
  virtual void Configure(const std::string& url, const std::string& token_key) = 0;

  /// @brief 登录成功后注入 Bearer（localStorage + postMessage）
  virtual void InjectAuth(const std::string& token, const std::string& username) = 0;

  /// @brief 登出时清除页面侧会话
  virtual void ClearAuth() = 0;

  /// @brief 异步创建是否已完成且可用
  virtual bool IsReady() const = 0;

  /// @brief 是否因缺少 WebView2 Runtime 创建失败
  virtual bool FailedMissingRuntime() const = 0;

  /// @brief 最近一次错误说明（UTF-8）
  virtual std::string LastError() const = 0;
};

}  // namespace tray_demo
