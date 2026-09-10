# pthreads-w32 / pthreads4w（Windows）

当前已就绪的最小文件集（MSVC x64）：

| 文件 | 用途 |
|------|------|
| `pthread.h` / `sched.h` / `semaphore.h` | 头文件 |
| `pthreadVC2.lib` | MSVC 导入库 |
| `pthreadVC2.dll` | 运行时（构建后会复制到 `build/` 与 exe 同目录） |

可选：`libpthreadGC2.a` / `pthreadGC2.dll` 供 MinGW 使用。

## 自测编译

在「x64 Native Tools」或先 `vcvars64` 后：

```bat
scripts\build_and_run.bat
```

或：

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\tray_demo.exe
```
