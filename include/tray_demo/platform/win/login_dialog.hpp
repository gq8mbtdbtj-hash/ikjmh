#pragma once

/**
 * @file login_dialog.hpp
 * @brief Windows 登录对话框（LDAP 账号密码 → SessionService::Login）。
 *
 * @customize 改控件布局 / 文案见 ShowLoginDialog 内 CreateWindow 调用。
 */

#include "tray_demo/net/session.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tray_demo {
namespace win {

#ifdef _WIN32

/**
 * @brief 模态登录窗
 * @return true 表示登录成功
 *
 * @customize 完整界面定制示例：本文件 + WinPanelRenderer::RenderNavigation。
 */
bool ShowLoginDialog(HWND owner, SessionService* session);

#endif

}  // namespace win
}  // namespace tray_demo
