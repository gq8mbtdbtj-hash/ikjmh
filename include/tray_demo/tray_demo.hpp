#pragma once

/**
 * @file tray_demo.hpp
 * @brief 伞头：一次包含框架公共 API。
 *
 * @customize 新工具工程建议只依赖本头文件 + 平台头
 * （Windows：`tray_demo/platform/win/win_platform.hpp`）。
 *
 * 定制入口一览见 @ref customize.hpp 。
 */

#include "tray_demo/customize.hpp"
#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/auth/auth_module.hpp"
#include "tray_demo/config/settings.hpp"
#include "tray_demo/menu/menu_model.hpp"
#include "tray_demo/net/http_client.hpp"
#include "tray_demo/net/session.hpp"
#include "tray_demo/page/basic_page.hpp"
#include "tray_demo/page/page_navigator.hpp"
#include "tray_demo/panel/list_item.hpp"
#include "tray_demo/panel/list_panel_model.hpp"
#include "tray_demo/panel/panel_content_ui.hpp"
#include "tray_demo/plugin/app_module.hpp"
#include "tray_demo/platform/null_platform.hpp"
#include "tray_demo/render/menu_renderer.hpp"
#include "tray_demo/render/panel_renderer.hpp"
#include "tray_demo/thread/job_queue.hpp"
#include "tray_demo/tray/tray_host.hpp"
#include "tray_demo/types.hpp"
#include "tray_demo/ui/auth_ui.hpp"
