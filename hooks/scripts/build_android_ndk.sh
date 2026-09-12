#!/usr/bin/env bash
# Cross-build tray_hooks / tray_memprobe / android_ndk_smoke with Android NDK.
#
# Usage:
#   export ANDROID_NDK_HOME=~/android-ndk-r26b
#   ./hooks/scripts/build_android_ndk.sh [abi...]
#
# Default ABI: arm64-v8a
# Env:
#   ANDROID_PLATFORM   (default android-24)
#   ANDROID_BUILD_ROOT (default <repo>/build_android)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK:-${NDK:-}}}"
if [[ -z "$NDK" || ! -f "$NDK/build/cmake/android.toolchain.cmake" ]]; then
  echo "ERROR: set ANDROID_NDK_HOME to an NDK with build/cmake/android.toolchain.cmake" >&2
  exit 1
fi

if [[ "$#" -gt 0 ]]; then
  ABIS=("$@")
else
  ABIS=(arm64-v8a)
fi
API_LEVEL="${ANDROID_PLATFORM:-android-24}"
OUT_ROOT="${ANDROID_BUILD_ROOT:-$ROOT/build_android}"
READELF="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
[[ -x "$READELF" ]] || READELF="$(command -v llvm-readelf || command -v readelf || true)"

echo "== NDK: $NDK"
echo "== API: $API_LEVEL"
echo "== ABIs: ${ABIS[*]}"
echo "== OUT: $OUT_ROOT"

ok=0
fail=0
for abi in "${ABIS[@]}"; do
  bdir="$OUT_ROOT/$abi"
  echo
  echo "---- configure $abi ----"
  cmake -S "$ROOT/hooks" -B "$bdir" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="$API_LEVEL" \
    -DANDROID_STL=c++_static \
    -DTRAY_HOOKS_BUILD_SAMPLE=OFF \
    -DTRAY_HOOKS_BUILD_TESTS=ON \
    -DTRAY_HOOKS_BUILD_MEMPROBE=ON \
    -DTRAY_HOOKS_BUILD_ANDROID_SMOKE=ON

  echo "---- build $abi ----"
  cmake --build "$bdir" --target \
    tray_hooks tray_memprobe android_ndk_smoke tray_wb_lib \
    -j"$(nproc 2>/dev/null || echo 4)"

  echo "---- verify artifacts $abi ----"
  smoke="$bdir/android_ndk_smoke"
  so="$bdir/libtray_memprobe.so"
  wb="$bdir/libtray_wb_lib.so"
  missing=0
  for f in "$smoke" "$so" "$wb"; do
    if [[ ! -f "$f" ]]; then
      echo "MISSING: $f" >&2
      missing=1
      continue
    fi
    file "$f" || true
    if [[ -n "$READELF" ]]; then
      "$READELF" -h "$f" | sed -n '1,20p' || true
    fi
  done
  if [[ "$missing" -ne 0 ]]; then
    fail=$((fail + 1))
    continue
  fi

  if [[ "$abi" == "arm64-v8a" && -n "$READELF" ]]; then
    if ! "$READELF" -h "$smoke" | grep -qiE 'AArch64|aarch64'; then
      echo "FAIL: expected AArch64 machine type for $smoke" >&2
      fail=$((fail + 1))
      continue
    fi
  fi
  echo "OK: $abi artifacts present"
  ok=$((ok + 1))
done

echo
echo "==== Android NDK smoke build summary: ok=$ok fail=$fail ===="
[[ "$fail" -eq 0 && "$ok" -gt 0 ]]
