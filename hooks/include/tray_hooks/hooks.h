/**
 * @file hooks.h
 * @ingroup tray_hooks_api
 * @brief 跨平台 PLT/IAT hook 门面（API 形态对齐 bytehook / xhook）。
 *
 * ## 设计要点
 * - **Caller 侧**改写：改的是调用者模块的 GOT/IAT 槽，不是 callee 代码段。
 * - 稳定 **C ABI**：平台差异收敛在 `CreateBackend()`；业务与 memprobe 只依赖本头文件。
 * - 与采集解耦：本头文件只管 hook；打点用 @ref collector.h ，采栈用 @ref backtrace.h 。
 *
 * ## 调用顺序
 * @code{.unparsed}
 *   tray_hooks_init(MODE)
 *     → tray_hooks_hook_all / hook_single / hook_partial
 *     → Proxy 内：tray_hooks_get_prev / TRAY_HOOKS_CALL_PREV
 *     →（可选）tray_hooks_unhook
 *     → tray_hooks_uninit
 * @endcode
 *
 * ## 后端
 * - Windows：真 IAT 改写（`win_iat_*`）
 * - Android/Linux/OHOS/QNX：自研 ELF GOT/JUMP_SLOT（`elf_plt_*`）
 *
 * @customize{Android 量产可在 CreateBackend() 内转调 bytehook；此处保持稳定 C ABI。}
 * @see docs/ARCHITECTURE.md
 * @see docs/API.md
 * @see hook_whitebox.cpp
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  ifdef TRAY_HOOKS_EXPORTS
#    define TRAY_HOOKS_API __declspec(dllexport)
#  else
#    define TRAY_HOOKS_API
#  endif
#else
#  define TRAY_HOOKS_API __attribute__((visibility("default")))
#endif

/**
 * @brief 初始化模式（对齐 BYTEHOOK_MODE_*）
 *
 * - AUTOMATIC：hook LoadLibrary / dlopen（及 Android `android_dlopen_ext`），
 *   新模块加载后自动重放活跃 stub。
 * - MANUAL：仅改写 init / hook 调用时已映射的模块。
 *
 * @note init 幂等：已初始化则忽略新 mode，直接返回 OK。换模式须先 uninit。
 */
typedef enum tray_hooks_mode {
  TRAY_HOOKS_MODE_AUTOMATIC = 0,
  TRAY_HOOKS_MODE_MANUAL = 1
} tray_hooks_mode_t;

/** @brief 门面返回的状态码（hook_* 失败时另返回 NULL） */
typedef enum tray_hooks_status {
  TRAY_HOOKS_OK = 0,           /**< 成功 */
  TRAY_HOOKS_ERR_INIT = 1,     /**< 初始化失败 */
  TRAY_HOOKS_ERR_NOT_IMPL = 2, /**< 功能未实现 */
  TRAY_HOOKS_ERR_PARAM = 3,    /**< 参数非法 */
  TRAY_HOOKS_ERR_PLATFORM = 4, /**< 无后端 / 平台失败 */
  TRAY_HOOKS_ERR_EXISTS = 5    /**< 冲突（预留） */
} tray_hooks_status_t;

/**
 * @brief hook 任务存根（传给 unhook；内部为 C++ Stub*）
 * @note 调用方勿解引用；生命周期由门面管理至 unhook/uninit。
 */
typedef struct tray_hooks_stub tray_hooks_stub_t;

/**
 * @brief 单个符号 hook 完成回调（可空）
 * @param stub         本次任务存根
 * @param status       通常为 TRAY_HOOKS_OK
 * @param caller_path  触发改写时的 caller 路径提示（可空）
 * @param sym_name     符号名
 * @param new_func     代理函数
 * @param prev_func    改写前原地址（供对照；运行时请用 get_prev）
 * @param arg          用户参数
 */
typedef void (*tray_hooks_hooked_t)(tray_hooks_stub_t* stub,
                                    int status,
                                    const char* caller_path,
                                    const char* sym_name,
                                    void* new_func,
                                    void* prev_func,
                                    void* arg);

/**
 * @brief 初始化 hook 引擎（幂等）
 * @param mode AUTOMATIC 或 MANUAL
 * @return TRAY_HOOKS_OK 或错误码
 */
TRAY_HOOKS_API int tray_hooks_init(tray_hooks_mode_t mode);

/** @brief 恢复全部改写并销毁后端；之后须重新 init 才能 hook */
TRAY_HOOKS_API void tray_hooks_uninit(void);

/**
 * @brief 当前后端名称
 * @return 如 `"win_iat(auto)"` / `"elf_plt(android,auto)"`；未 init 时 `"none"`
 */
TRAY_HOOKS_API const char* tray_hooks_backend_name(void);

/**
 * @brief Hook 单个 caller 模块中的符号
 * @param caller_path 调用者 so/dll 路径或 basename 子串；NULL=不限（视后端）
 * @param callee_path 被调用者路径过滤；Win 下 NULL=任意导入 DLL（推荐）
 * @param sym_name    符号名（如 `"malloc"`）
 * @param new_func    代理函数地址（亦为 get_prev 的 key）
 * @param hooked      完成回调，可为 NULL
 * @param hooked_arg  回调用户参数
 * @return stub；失败返回 NULL（未 init / 参数非法 / 无匹配槽等）
 */
TRAY_HOOKS_API tray_hooks_stub_t* tray_hooks_hook_single(
    const char* caller_path,
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg);

/**
 * @brief 按 caller 白名单回调 hook
 * @param caller_allow 返回非 0 表示允许改写该 caller；**不可为 NULL**
 * @note 真正按模块过滤（不再回退为 hook_all）
 */
TRAY_HOOKS_API tray_hooks_stub_t* tray_hooks_hook_partial(
    int (*caller_allow)(const char* caller_path, void* arg),
    void* caller_allow_arg,
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg);

/**
 * @brief Hook 当前进程内所有已映射模块对该符号的导入
 * @param callee_path 可为 NULL（见 hook_single）
 */
TRAY_HOOKS_API tray_hooks_stub_t* tray_hooks_hook_all(
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg);

/**
 * @brief 恢复该 stub 改写过的槽位并释放 stub
 * @return TRAY_HOOKS_OK 或 TRAY_HOOKS_ERR_PARAM
 */
TRAY_HOOKS_API int tray_hooks_unhook(tray_hooks_stub_t* stub);

/**
 * @brief 在 proxy 中取「原实现」函数指针
 * @param proxy 必须是 hook 时传入的 new_func 地址
 * @return 原函数指针；未登记时为 NULL
 * @warning 在 proxy 内直接调用同名 libc 符号会死递归。
 */
TRAY_HOOKS_API void* tray_hooks_get_prev(void* proxy);

/**
 * @brief 便捷：按 proxy 取 prev 并直接调用
 * @param proxy 代理函数地址（通常传本函数符号）
 * @param type  函数指针类型，如 `malloc_fn`
 * @param ...   传给原函数的参数
 */
#define TRAY_HOOKS_CALL_PREV(proxy, type, ...) \
  (((type)tray_hooks_get_prev((void*)(proxy)))(__VA_ARGS__))

#ifdef __cplusplus
}
#endif
