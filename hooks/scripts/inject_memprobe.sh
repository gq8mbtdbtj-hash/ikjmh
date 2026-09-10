#!/usr/bin/env bash
# 将 libtray_memprobe.so 注入 ELF 可执行文件（patchelf --add-needed）
#
# 用法:
#   ./inject_memprobe.sh /path/to/app [/path/to/libtray_memprobe.so]
#
# 说明:
#   - add-needed 保证进程启动时加载探针（constructor 会 start）
#   - 若目标已把 malloc 绑定到 libc，插桩可能不生效 → 改用 LD_PRELOAD（见 README）
#   - 需要系统已安装 patchelf

set -euo pipefail

APP="${1:-}"
LIB="${2:-}"

if [[ -z "$APP" ]]; then
  echo "usage: $0 <elf-binary> [libtray_memprobe.so]" >&2
  exit 1
fi

if [[ -z "$LIB" ]]; then
  # 常见构建产物位置
  for c in \
    "./libtray_memprobe.so" \
    "./build/hooks/libtray_memprobe.so" \
    "$(dirname "$0")/../../build/hooks/libtray_memprobe.so"
  do
    if [[ -f "$c" ]]; then LIB="$c"; break; fi
  done
fi

if [[ -z "$LIB" || ! -f "$LIB" ]]; then
  echo "libtray_memprobe.so not found; pass path as 2nd arg" >&2
  exit 1
fi

if ! command -v patchelf >/dev/null 2>&1; then
  echo "patchelf not found. Install: apt install patchelf / brew install patchelf" >&2
  exit 1
fi

LIB_ABS="$(cd "$(dirname "$LIB")" && pwd)/$(basename "$LIB")"
LIBDIR="$(dirname "$LIB_ABS")"
LIBNAME="$(basename "$LIB_ABS")"

# 备份
cp -a "$APP" "${APP}.bak.memprobe"
echo "backup: ${APP}.bak.memprobe"

# 写入 NEEDED + 扩展 rpath，便于找到 so
patchelf --add-needed "$LIBNAME" "$APP"
# 已有 rpath 则追加
OLD_RPATH="$(patchelf --print-rpath "$APP" 2>/dev/null || true)"
if [[ -n "$OLD_RPATH" ]]; then
  patchelf --set-rpath "${LIBDIR}:${OLD_RPATH}" "$APP"
else
  patchelf --set-rpath "$LIBDIR" "$APP"
fi

echo "patched: $APP"
echo "  NEEDED += $LIBNAME"
echo "  RPATH  += $LIBDIR"
echo
echo "run with stacks:"
echo "  TRAY_MEMPROBE_SAMPLE=1 TRAY_MEMPROBE_LOG=/tmp/memprobe.txt \"$APP\""
echo
echo "if malloc not intercepted, use:"
echo "  LD_PRELOAD=\"$LIB_ABS\" \"$APP\""
