/**
 * @file hooks.h
 * @brief 跨平台 PLT/IAT hook 门面（API 形态对齐 bytehook / xhook）。
 *
 * 调用顺序：
 *   tray_hooks_init(MODE) →
 *   tray_hooks_hook_all(NULL, "malloc", Proxy, ...) →
 *   在 Proxy 内用 tray_hooks_get_prev / TRAY_HOOKS_CALL_PREV 调原实现 →
 *   tray_hooks_unhook / tray_hooks_uninit
 *
 * 后端：
 *   - Windows：真 IAT 改写（win_iat_*）
 *   - Android/Linux/OHOS/QNX：自研 ELF GOT/JUMP_SLOT（elf_plt_*）
 *
 * CUSTOMIZE: Android 量产可在 CreateBackend() 内转调 bytehook；
 * 此处保持稳定 C ABI，业务与 memprobe 无需改代码。
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
 * 初始化模式（对齐 BYTEHOOK_MODE_*）
 * AUTOMATIC：hook LoadLibrary / dlopen，新模块加载后自动补用户 hook
 * MANUAL：仅改写 init / hook 调用时已映射的模块
 */
typedef enum tray_hooks_mode {
  TRAY_HOOKS_MODE_AUTOMATIC = 0,
  TRAY_HOOKS_MODE_MANUAL = 1
} tray_hooks_mode_t;

typedef enum tray_hooks_status {
  TRAY_HOOKS_OK = 0,
  TRAY_HOOKS_ERR_INIT = 1,
  TRAY_HOOKS_ERR_NOT_IMPL = 2,
  TRAY_HOOKS_ERR_PARAM = 3,
  TRAY_HOOKS_ERR_PLATFORM = 4,
  TRAY_HOOKS_ERR_EXISTS = 5
} tray_hooks_status_t;

/** hook 任务存根（传给 unhook；内部为 C++ Stub*） */
typedef struct tray_hooks_stub tray_hooks_stub_t;

/** 单个符号 hook 完成回调（可空） */
typedef void (*tray_hooks_hooked_t)(tray_hooks_stub_t* stub,
                                    int status,
                                    const char* caller_path,
                                    const char* sym_name,
                                    void* new_func,
                                    void* prev_func,
                                    void* arg);

/**
 * @brief 初始化 hook 引擎（幂等）
 * @return TRAY_HOOKS_OK 或错误码
 */
TRAY_HOOKS_API int tray_hooks_init(tray_hooks_mode_t mode);

/** 恢复全部改写并销毁后端 */
TRAY_HOOKS_API void tray_hooks_uninit(void);

/** 如 "win_iat" / "elf_plt(android)"；未 init 时 "none" */
TRAY_HOOKS_API const char* tray_hooks_backend_name(void);

/**
 * @brief Hook 单个 caller 模块中的符号
 * @param caller_path 调用者 so/dll 路径或 basename；NULL 表示不限（部分后端）
 * @param callee_path 被调用者路径过滤；Win 下 NULL=任意导入 DLL（推荐）
 * @param sym_name    符号名（如 "malloc"）
 * @param new_func    代理函数
 * @param hooked      完成回调，可为 NULL
 */
TRAY_HOOKS_API tray_hooks_stub_t* tray_hooks_hook_single(
    const char* caller_path,
    const char* callee_path,
    const char* sym_name,
    void* new_func,
    tray_hooks_hooked_t hooked,
    void* hooked_arg);

/**
 * @brief 按 caller 白名单 hook（当前实现回退为 hook_all）
 * @param caller_allow 返回非 0 表示允许该 caller_path
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

TRAY_HOOKS_API int tray_hooks_unhook(tray_hooks_stub_t* stub);

/**
 * @brief 在 proxy 中取「原实现」函数指针
 * @param proxy 必须是 hook 时传入的 new_func 地址
 */
TRAY_HOOKS_API void* tray_hooks_get_prev(void* proxy);

/** 便捷：按 proxy 取 prev 并直接调用（type 为函数指针类型） */
#define TRAY_HOOKS_CALL_PREV(proxy, type, ...) \
  (((type)tray_hooks_get_prev((void*)(proxy)))(__VA_ARGS__))

#ifdef __cplusplus
}
#endif
