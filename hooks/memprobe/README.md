# tray_memprobe — 无编译注入采集

## 目标

**不改业务工程、不参与其编译**，通过注入本仓库产出的动态库，采集：

| 能力 | 说明 |
|------|------|
| 堆栈 | 分配点 backtrace |
| CPU | 进程 user/sys 时间与占用率估算 |
| 内存域 | `heap` / `mmap` / `gpu` / `npu` / `neon` / `custom` |

Hook 实现：

- **Windows**：自研 **IAT 改写**（`win_iat_patch`）
- **Android / Linux / OHOS / QNX**：自研 **ELF PLT/GOT**（`elf_plt_patch`，思路参考 xhook/bhook，**不链接**它们）

## 无编译注入

### Linux / Android / 鸿蒙

```bash
# 构建
cmake -S . -B build_linux -DTRAY_DEMO_BUILD_HOOKS=ON -DTRAY_DEMO_BUILD_APP=OFF
cmake --build build_linux --target tray_memprobe

# A) LD_PRELOAD（立刻生效：符号插桩 + GOT hook）
LD_PRELOAD=./build_linux/hooks/libtray_memprobe.so \
  TRAY_MEMPROBE_DOMAINS=heap,mmap,gpu \
  TRAY_MEMPROBE_LOG=/tmp/memprobe.txt \
  ./your_app

# B) patchelf 追加 NEEDED（持久）
./hooks/scripts/inject_memprobe.sh ./your_app
```

Android 可把 `libtray_memprobe.so` 打进 APK 的 `lib/<abi>/`，并用 wrap.sh / 启动时 `System.loadLibrary` **仅加载探针**（业务 APK 可不改 CMake）。

### Windows

```bat
cmake --build build --target tray_memprobe
REM 产物: build\hooks\tray_memprobe.dll

REM 运行时注入（任选工具 LoadLibrary）：
REM   将 tray_memprobe.dll 注入目标进程后，DllMain 自动 IAT hook

set TRAY_MEMPROBE_DOMAINS=heap,mmap
set TRAY_MEMPROBE_LOG=%TEMP%\memprobe.txt
```

PE 持久依赖可用：

```powershell
# 等价 patchelf --add-needed（写导入表 + 拷贝 DLL 到 EXE 旁）
.\hooks\scripts\inject_memprobe.ps1 -Target C:\path\app.exe
# 或: python hooks\scripts\inject_memprobe_pe.py app.exe build\hooks\tray_memprobe.dll
# 还原: .\hooks\scripts\inject_memprobe.ps1 -Target app.exe -Restore
```

## 环境变量

| 变量 | 含义 |
|------|------|
| `TRAY_MEMPROBE_DOMAINS` | `heap,mmap,gpu,npu,neon,all` |
| `TRAY_MEMPROBE_SAMPLE` | 每 N 次分配采栈 |
| `TRAY_MEMPROBE_MIN_SIZE` | 小于此字节不采栈 |
| `TRAY_MEMPROBE_LOG` | dump 路径 |
| `TRAY_MEMPROBE_COLLECT` | `1`=分配事件进 collector（配合 APM） |
| `TRAY_HOOKS_APM_FILE` | NDJSON 持续落盘 |
| `TRAY_HOOKS_APM_URL` | HTTP POST 批次上报 |
| `TRAY_HOOKS_APM_INTERVAL_MS` | 刷新间隔 |
| `TRAY_HOOKS_APM_STACKS` | `0`=上报不含栈 |
| `TRAY_HOOKS_FILTER_TAGS` | tag 白名单（如 `malloc,mmap`） |
| `TRAY_HOOKS_FILTER_DENY_TAGS` | tag 黑名单 |
| `TRAY_HOOKS_FILTER_TIDS` | 线程 ID 白名单 |
| `TRAY_HOOKS_FILTER_PROCESS` | 进程名子串 |
| `TRAY_HOOKS_FILTER_MODULES` | 栈模块子串（降噪大进程） |
| `TRAY_HOOKS_FILTER_MIN_ARG0` | 最小 size |
| `TRAY_HOOKS_FILTER_SAMPLE` | 事件采样 N |
| `TRAY_MEMPROBE_GPU_SYMS` | 额外 GPU 符号（占位 ABI，size=0）；精确符号见下表默认已装 |
| `TRAY_MEMPROBE_NPU_SYMS` | 额外 NPU 占位符号；默认已装 Ascend `aclrtMalloc/Free` |
| `TRAY_MEMPROBE_NEON_SYMS` | 自定义/加速缓冲符号（neno→neon） |

### 持续观测示例

```bat
set TRAY_MEMPROBE_COLLECT=1
set TRAY_HOOKS_APM_FILE=%TEMP%\tray_apm.ndjson
set TRAY_HOOKS_FILTER_MIN_ARG0=4096
set TRAY_HOOKS_FILTER_MODULES=myapp,plugin
app.exe
```

### GPU 精确 hook（默认，`domains` 含 `gpu` 时）

| API | 分配 | 释放 | 记账 key / size |
|-----|------|------|----------------|
| CUDA Driver | `cuMemAlloc` / `_v2` | `cuMemFree` / `_v2` | device ptr / `bytesize` |
| CUDA Runtime | `cudaMalloc` | `cudaFree` | `*devPtr` / `size` |
| OpenCL | `clCreateBuffer` | `clReleaseMemObject` | `cl_mem` / `size` |
| Vulkan | `vkAllocateMemory` | `vkFreeMemory` | `VkDeviceMemory` / `allocationSize` |

晚加载的 CUDA/Vulkan DLL 依赖 `tray_hooks_init(AUTOMATIC)`（memprobe 默认已开）：
Windows 补 `LoadLibrary*`，ELF 补 `dlopen` / `android_dlopen_ext`。

NPU 或未知 ABI 请在 SDK 包装层：

```c
tray_memprobe_domain_alloc(TRAY_MEM_NPU, ptr, bytes);
tray_memprobe_domain_free(TRAY_MEM_NPU, ptr);
```

## 合规

仅用于自有进程诊断 / APM；勿未授权注入第三方进程。
