# Android 采集落地设计

**状态：** v1.0  
**结论：** Hook 方案在 Android 上 **可行**；仓库已具备 ELF GOT 自研路径与统一门面。量产建议按风险矩阵验证，必要时后端切换为 **bytehook**。

相关：[ARCHITECTURE.md](ARCHITECTURE.md) · [PLATFORM.md](PLATFORM.md) · [API.md](API.md)

---

## 1. 目标场景

| 场景 | 做法 | 是否改业务工程 |
|------|------|----------------|
| A. SDK 链入 | App/so 链接 `tray_hooks`，主动 `tray_hooks_hook_*` | 改 CMake/代码 |
| B. 无编译探针 | 打入 `libtray_memprobe.so`，启动加载 | 通常只改打包/启动 |
| C. 量产加固 | 门面不变，`CreateBackend()` → bytehook | 仅 hooks 内部 |

合规：仅 **自有 App 进程** 诊断 / APM。

---

## 2. 技术选型

### 2.1 为何用 PLT/GOT（Caller 侧）

- 与 xhook / ByteHook 同模型，生态验证充分。
- 不改 `libc`/callee 代码，规避部分 inline hook 与代码段限制。
- 统一 API 可同时服务 Linux / OHOS / QNX。

### 2.2 本仓库 Android 能力清单

| 能力 | 状态 |
|------|------|
| `elf_plt_patch` JUMP_SLOT 改写 | ✅ 自研 |
| aarch64 / arm `clear_cache` | ✅ |
| `android_dlopen_ext` AUTOMATIC | ✅ |
| `_Unwind_Backtrace` + `dladdr` | ✅ |
| Memprobe `constructor(101)` 自启 | ✅ |
| NDK CMake 说明 | ✅（`hooks/README.md`） |
| CI 真机 / 模拟器矩阵 | ❌ 待补 |
| bytehook 适配后端 | 📐 `CreateBackend()` 扩展点已预留 |

---

## 3. 架构在 Android 上的映射

```text
  APK
  ├─ lib/<abi>/libbiz.so
  ├─ lib/<abi>/libtray_memprobe.so
  └─ wrap.sh 或 Java System.loadLibrary("tray_memprobe")
           │
           ▼
  constructor(101)
    tray_memprobe_start()
    tray_memprobe_install_hooks()
      → tray_hooks_init(AUTOMATIC)   # backend: elf_plt(android,auto)
      → PatchSymbol(malloc/…)        # 各 caller so 的 GOT
      → hook android_dlopen_ext/dlopen
           │
           ▼
  Proxy → backtrace / collector / OnAlloc → APM_FILE（设备路径）
```

Linker namespace（App 隔离命名空间）下：

- 验证 `dl_iterate_phdr` 是否枚举到目标 so；
- 探针 so 需与业务处于可互相可见的 namespace（通常同 App `lib/<abi>/` 即可）；
- 失败时 stderr 有 `fails=`（RELRO / `mprotect` 失败）。

---

## 4. 构建

### 4.1 NDK 交叉编译

```bash
cmake -S hooks -B build_hooks \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
cmake --build build_hooks --target tray_hooks tray_memprobe
```

建议 ABI：`arm64-v8a` 必做；`armeabi-v7a` 按产品矩阵。

### 4.2 链入业务 so

```cmake
add_library(biz SHARED …)
target_link_libraries(biz PRIVATE tray_hooks log)
```

在 `JNI_OnLoad` 或早期 native init 中调用 `tray_hooks_init` + `tray_hooks_hook_*`。

---

## 5. 无编译注入步骤

1. 将 `libtray_memprobe.so` 拷入 APK `lib/arm64-v8a/`（及需要的其它 ABI）。
2. 启动方式任选：
   - **Java：** 尽量早 `System.loadLibrary("tray_memprobe");`
   - **wrap.sh：** `LD_PRELOAD` 指向探针 so（debuggable / 特定渠道）
   - **patchelf：** 对主 ELF / 关键 so `--add-needed libtray_memprobe.so`
3. 环境变量：普通 App 对 `adb setprop` 有限；更常见 wrap.sh、debug 脚本、或 load 前 `Os.setenv`。

最小验证：

```text
TRAY_MEMPROBE_DOMAINS=heap
TRAY_MEMPROBE_LOG=/data/local/tmp/memprobe.txt
TRAY_MEMPROBE_COLLECT=1
TRAY_HOOKS_APM_FILE=/data/local/tmp/tray.ndjson
```

用 `adb shell run-as <pkg>` 或可写目录读取 dump。

---

## 6. 量产建议：bytehook 适配

替换点：`tray_hooks::detail::CreateBackend()`（见 `src/internal/backend.hpp`）。

```text
tray_hooks_* C ABI（不变）
        │
        ▼
BytehookBackend : Backend
  init     → bytehook_init
  hook     → bytehook_hook_all / partial / single
  get_prev → 与 BYTEHOOK_CALL_PREV 语义对齐（经门面 `tray_hooks_get_prev` 表）
```

**何时切换：** 自研 GOT 在目标机型/加固包上 `fails` 偏高；需要 bytehook 已覆盖的 linker 边界。  
**何时可继续自研：** 可控 ROM / 未加固 Debug；希望无第三方 hook 依赖。

参见 [PLATFORM.md](PLATFORM.md)「Android 量产建议」。

---

## 7. 风险矩阵与验收

| 风险 | 影响 | 缓解 / 验收 |
|------|------|-------------|
| FULL RELRO / 只读 GOT | 写槽失败 | 查 stderr `fails=`；换 bytehook 或缩小 hook 范围 |
| Linker namespace | 枚举不到 / 改不到 | 确认 so 路径与 namespace；钩自有可见库 |
| 静态链接分配器 | 无 PLT | 改包装层或插桩编译选项 |
| 加固 / 壳 | 映射异常 | 壳解密后安装；或仅 hook 自有 so |
| 多线程早初始化 | 竞态 | constructor(101) + AUTOMATIC 重放 |
| 采栈质量 | 无符号 | Debug 未剥离对照；生产用 module+offset 离线符号化 |
| 性能 | 热分配卡顿 | `MIN_SIZE` / `SAMPLE` / `FILTER_*` / `APM_STACKS=0` |
| 合规 | 商店政策 | 仅自有进程；隐私协议覆盖诊断数据 |

**验收清单：**

- [ ] arm64 真机：等价 whitebox（自建 so 导出符号，验证 CALL_PREV）
- [ ] Debug APK：memprobe dump 非空，peak 随分配变化
- [ ] 晚加载 so：AUTOMATIC 后出现 re-patched 日志
- [ ] Release：记录 fails 与覆盖率
- [ ] 过滤后 APM 体积与主线程耗时可接受

---

## 8. 鸿蒙（OHOS）提示

- 后端名：`elf_plt(ohos[,auto])`；同样尝试 `android_dlopen_ext` 与 `dlopen`。
- 优先验证 `dl_iterate_phdr` 与命名空间；策略同 Android。

---

## 9. 明确不做

- 未授权注入其它 App  
- 绕过 SELinux / 沙箱的利用链  
- 内核模块级 hook  
- 保证所有加固厂商包可写 GOT  

---

## 10. 参考

- 实现：`src/plat/elf_plt_patch.cpp`、`elf_plt_backend.cpp`
- 注入：`../memprobe/README.md`
- 数据流：`DATAFLOW.md`
