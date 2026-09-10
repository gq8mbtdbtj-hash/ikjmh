#pragma once

/**
 * @file win_webview_panel.hpp
 * @brief Windows WebView2 浮动面板（远程 URL）。
 * @platform Windows
 */

#include "tray_demo/render/webview_panel.hpp"
#include "tray_demo/platform/win/win_panel_placement.hpp"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl.h>
#include <wrl/client.h>

#include "WebView2.h"
#endif

namespace tray_demo {
namespace win {

#ifdef _WIN32

/**
 * @class WinWebViewPanel
 * @brief 嵌入 Edge WebView2，加载 panel_webview_url。
 * @customize Configure / InjectAuth 脚本键名与消息格式。
 */
class WinWebViewPanel : public IWebViewPanel {
public:
  WinWebViewPanel();
  virtual ~WinWebViewPanel();

  bool CreateHost();
  void Destroy();

  virtual void Show();
  virtual void Hide();
  virtual bool IsVisible() const;
  virtual void SetFullscreen(bool enabled);
  virtual bool IsFullscreen() const;
  virtual void RenderNavigation(const PageNavigator& navigator);
  virtual void InvalidateCurrentPage();

  virtual void Configure(const std::string& url, const std::string& token_key);
  virtual void InjectAuth(const std::string& token, const std::string& username);
  virtual void ClearAuth();
  virtual bool IsReady() const;
  virtual bool FailedMissingRuntime() const;
  virtual std::string LastError() const;

  HWND hwnd() const { return hwnd_; }

  void SetNotifyIconAnchor(HWND notify_hwnd, UINT notify_id);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void PositionNearTray();
  void ApplyWindowPlacement();
  void ResizeWebView();
  void EnsureEnvironment();
  void NavigateIfReady();
  void ApplyPendingAuth();
  void RunInjectScript(bool clear);
  std::wstring Utf8ToWide(const std::string& utf8) const;
  std::string EscapeJsString(const std::string& s) const;

  HWND hwnd_;
  bool visible_;
  bool fullscreen_;
  bool ready_;
  bool creating_;
  bool missing_runtime_;
  bool pending_show_;
  bool pending_auth_;
  bool pending_clear_auth_;
  int normal_w_;
  int normal_h_;
  NotifyIconAnchor tray_anchor_;
  std::string url_;
  std::string token_key_;
  std::string token_;
  std::string username_;
  std::string last_error_;

  Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
};

#endif  // _WIN32

}  // namespace win
}  // namespace tray_demo
