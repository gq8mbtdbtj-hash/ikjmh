#pragma once

/**
 * @file customize.hpp
 * @brief 定制约定：业务用 IAppModule + IListPanelModel，勿改框架分发逻辑。
 *
 * @par 检索
 * - @c CUSTOMIZE: — 业务 / 配置入口
 * - @c PLATFORM:  — Win / mac / linux 平台实现
 * - @c INTERNAL:  — 框架内部
 *
 * @par 推荐定制顺序
 * 1. 登录：保留框架 @ref tray_demo::AuthModule（auth.login / auth.logout）
 * 2. 实现业务 @ref tray_demo::IAppModule（参考 DemoModule / AboutModule）
 * 3. 动作 id 使用 @c MakeActionId(module_id, "…")；框架按 OwnsAction 前缀路由
 * 4. 用 @c menu_order() 控制多 module 菜单合并顺序（Auth 默认 10）
 * 5. 列表数据实现 @ref tray_demo::IListPanelModel，经 BindListModel 交给面板宿主
 * 6. 在 @c src/app/main.cpp 中 RegisterModule（Auth + 业务）
 * 7. settings.ini — HTTP / panel_ui / WebView URL
 * 8. 平台：Win 完整；mac/linux 见 platform/* 桩；矩阵见 docs/platform.md
 * 9. Go：@c framework.Module + 可选 @c ACLProvider / Admin.RegisterNav
 * 10. @c server/acl.yaml — 同 id 优先；缺失 id 由 module 碎片合并
 */

namespace tray_demo {

#ifndef TRAY_DEMO_CUSTOMIZE_MARK
#define TRAY_DEMO_CUSTOMIZE_MARK 1
#endif

}  // namespace tray_demo
