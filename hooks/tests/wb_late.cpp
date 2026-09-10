/**
 * @file wb_late.cpp
 * @brief 晚加载 victim：导入 wb_target，供 AUTOMATIC 补 hook 验证。
 */
#include "wb_lib.h"

#if defined(_WIN32)
#  define TRAY_WB_LATE_API __declspec(dllexport)
#else
#  define TRAY_WB_LATE_API __attribute__((visibility("default")))
#endif

extern "C" TRAY_WB_LATE_API int wb_late_invoke(int a, int b) {
  return wb_target(a, b);
}
