#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL="${1:-gcc}"
CONFIG="${2:-Debug}"
RUN="${3:-run}"

case "$TOOL" in
  gcc) CXX="${CXX:-g++}" ;;
  clang) CXX="${CXX:-clang++}" ;;
  *) echo "Compiler must be gcc or clang" >&2; exit 2 ;;
esac

case "$CONFIG" in
  Debug|debug) CONFIG=Debug; OPT=(-O0 -g) ;;
  Release|release) CONFIG=Release; OPT=(-O3 -DNDEBUG) ;;
  *) echo "Configuration must be Debug or Release" >&2; exit 2 ;;
esac

UNAME="$(uname -s)"
OUT="$ROOT/out/direct-${TOOL}-${CONFIG}"
OBJ_STATIC="$OUT/obj-static"
OBJ_SHARED="$OUT/obj-shared"
mkdir -p "$OBJ_STATIC" "$OBJ_SHARED"
rm -f "$OBJ_STATIC"/*.o "$OBJ_SHARED"/*.o "$OUT"/*.a "$OUT"/*.so "$OUT"/*.dylib "$OUT"/EpochPlatform*

COMMON=(-std=c++23 -pthread -I"$ROOT/include" -Wall -Wextra -Wpedantic -Wconversion -Wshadow "${OPT[@]}")
SOURCES=(
  "$ROOT/src/application.cpp"
  "$ROOT/src/runtime.cpp"
  "$ROOT/src/runtime_host.cpp"
  "$ROOT/src/types.cpp"
  "$ROOT/src/drivers/headless_driver.cpp"
)
PLATFORM_LIBS=()
PLATFORM_DEFS=()
SHARED_EXT=so
SHARED_LINK=(-shared -Wl,-soname,libEpochPlatformEngine.so)

if [[ "$UNAME" == "Linux" ]]; then
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists x11; then
    SOURCES+=("$ROOT/src/drivers/x11_driver.cpp")
    PLATFORM_DEFS+=(-DEPOCH_PLATFORM_HAS_X11=1)
    read -r -a X11_CFLAGS <<<"$(pkg-config --cflags x11)"
    read -r -a X11_LIBS <<<"$(pkg-config --libs x11)"
    COMMON+=("${X11_CFLAGS[@]}")
    PLATFORM_LIBS+=("${X11_LIBS[@]}")
  fi
elif [[ "$UNAME" == "Darwin" ]]; then
  SOURCES+=("$ROOT/src/drivers/cocoa_driver.mm")
  PLATFORM_LIBS+=(-framework Cocoa)
  SHARED_EXT=dylib
  SHARED_LINK=(-dynamiclib -Wl,-install_name,@rpath/libEpochPlatformEngine.dylib)
else
  echo "Unsupported POSIX platform: $UNAME" >&2
  exit 3
fi

compile_set() {
  local outdir="$1"; shift
  local -a defs=("$@")
  for source in "${SOURCES[@]}"; do
    local base
    base="$(basename "${source%.*}")"
    "$CXX" "${COMMON[@]}" "${PLATFORM_DEFS[@]}" "${defs[@]}" -fPIC -c "$source" -o "$outdir/$base.o"
  done
}

echo "[1/9] Static library"
compile_set "$OBJ_STATIC" -DEPOCH_PLATFORM_ENGINE_STATIC=1
ar rcs "$OUT/libEpochPlatformEngineStatic.a" "$OBJ_STATIC"/*.o

echo "[2/9] Shared library"
compile_set "$OBJ_SHARED" -DEPOCH_PLATFORM_ENGINE_EXPORTS=1 -fvisibility=hidden -fvisibility-inlines-hidden
"$CXX" "${SHARED_LINK[@]}" -o "$OUT/libEpochPlatformEngine.$SHARED_EXT" "$OBJ_SHARED"/*.o "${PLATFORM_LIBS[@]}" -pthread

echo "[3/9] Internal entrypoint archives"
"$CXX" "${COMMON[@]}" -DEPOCH_PLATFORM_ENGINE_STATIC=1 -c "$ROOT/src/entrypoint/entrypoint.cpp" -o "$OBJ_STATIC/entrypoint.o"
ar rcs "$OUT/libEpochPlatformEntrypointConsoleStatic.a" "$OBJ_STATIC/entrypoint.o"
"$CXX" "${COMMON[@]}" -c "$ROOT/src/entrypoint/entrypoint.cpp" -o "$OBJ_SHARED/entrypoint.o"
ar rcs "$OUT/libEpochPlatformEntrypointConsoleShared.a" "$OBJ_SHARED/entrypoint.o"

echo "[4/9] Static validation example"
"$CXX" "${COMMON[@]}" -DEPOCH_PLATFORM_ENGINE_STATIC=1 -c "$ROOT/examples/basic_app.cpp" -o "$OBJ_STATIC/basic_app.o"
"$CXX" -o "$OUT/EpochPlatformExampleStatic" "$OBJ_STATIC/basic_app.o" \
  "$OUT/libEpochPlatformEntrypointConsoleStatic.a" "$OUT/libEpochPlatformEngineStatic.a" \
  "${PLATFORM_LIBS[@]}" -pthread

echo "[5/9] Shared validation example"
"$CXX" "${COMMON[@]}" -c "$ROOT/examples/basic_app.cpp" -o "$OBJ_SHARED/basic_app.o"
if [[ "$UNAME" == "Darwin" ]]; then
  "$CXX" -o "$OUT/EpochPlatformExampleShared" "$OBJ_SHARED/basic_app.o" \
    "$OUT/libEpochPlatformEntrypointConsoleShared.a" -L"$OUT" -lEpochPlatformEngine \
    -Wl,-rpath,@loader_path "${PLATFORM_LIBS[@]}" -pthread
else
  "$CXX" -o "$OUT/EpochPlatformExampleShared" "$OBJ_SHARED/basic_app.o" \
    "$OUT/libEpochPlatformEntrypointConsoleShared.a" -L"$OUT" -lEpochPlatformEngine \
    -Wl,-rpath,'$ORIGIN' "${PLATFORM_LIBS[@]}" -pthread
fi

echo "[6/9] Static release example"
"$CXX" "${COMMON[@]}" -DEPOCH_PLATFORM_ENGINE_STATIC=1 -c "$ROOT/examples/release_app.cpp" -o "$OBJ_STATIC/release_app.o"
"$CXX" -o "$OUT/EpochPlatformReleaseExampleStatic" "$OBJ_STATIC/release_app.o" \
  "$OUT/libEpochPlatformEntrypointConsoleStatic.a" "$OUT/libEpochPlatformEngineStatic.a" \
  "${PLATFORM_LIBS[@]}" -pthread

echo "[7/9] Shared release example"
"$CXX" "${COMMON[@]}" -c "$ROOT/examples/release_app.cpp" -o "$OBJ_SHARED/release_app.o"
if [[ "$UNAME" == "Darwin" ]]; then
  "$CXX" -o "$OUT/EpochPlatformReleaseExampleShared" "$OBJ_SHARED/release_app.o" \
    "$OUT/libEpochPlatformEntrypointConsoleShared.a" -L"$OUT" -lEpochPlatformEngine \
    -Wl,-rpath,@loader_path "${PLATFORM_LIBS[@]}" -pthread
else
  "$CXX" -o "$OUT/EpochPlatformReleaseExampleShared" "$OBJ_SHARED/release_app.o" \
    "$OUT/libEpochPlatformEntrypointConsoleShared.a" -L"$OUT" -lEpochPlatformEngine \
    -Wl,-rpath,'$ORIGIN' "${PLATFORM_LIBS[@]}" -pthread
fi

echo "[8/9] Tests"
"$CXX" "${COMMON[@]}" -DEPOCH_PLATFORM_ENGINE_STATIC=1 -c "$ROOT/tests/platform_tests.cpp" -o "$OBJ_STATIC/platform_tests.o"
"$CXX" -o "$OUT/EpochPlatformEngineTests" "$OBJ_STATIC/platform_tests.o" \
  "$OUT/libEpochPlatformEngineStatic.a" "${PLATFORM_LIBS[@]}" -pthread

echo "[9/9] Validation"
if [[ "$RUN" != "no-run" ]]; then
  "$OUT/EpochPlatformEngineTests"
  "$OUT/EpochPlatformExampleStatic"
  "$OUT/EpochPlatformExampleShared"
fi

echo "Built: $OUT"
