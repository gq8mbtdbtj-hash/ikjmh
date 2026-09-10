/**
 * @file wb_lib.h
 * @brief 白盒 victim：导出可被 IAT/PLT hook 的已知符号。
 */
#pragma once

#if defined(_WIN32)
#  if defined(TRAY_WB_LIB_EXPORTS)
#    define TRAY_WB_API __declspec(dllexport)
#  else
#    define TRAY_WB_API __declspec(dllimport)
#  endif
#else
#  define TRAY_WB_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** 纯函数：返回 a+b+magic，便于断言 hook 前后行为 */
TRAY_WB_API int wb_target(int a, int b);

/** 返回库内版本字符串，确认已加载正确模块 */
TRAY_WB_API const char* wb_lib_version(void);

#ifdef __cplusplus
}
#endif
