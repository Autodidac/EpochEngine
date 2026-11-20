#!/usr/bin/env bash
# EPOCH_MANAGED_GENERATED_FILE: linux_build_v2
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${EPOCH_REPO_ROOT:-$project_dir/../../..}" && pwd)"
engine_dir="$repo_root/Engine"
python3 "$repo_root/Tools/build/verify_epoch_project_source.py" --root "$repo_root" --edition 'epoch.public.github.v1'
artifact_stem='BasicSoftwareExampleCalculator'
configuration="${1:-Release}"
case "$configuration" in Debug|Release) ;; *) echo "Configuration must be Debug or Release." >&2; exit 2 ;; esac
log_dir="$project_dir/Artifacts/Reports"
build_dir="$project_dir/Artifacts/Intermediate/cmake/linux/Clang-$configuration"
output_dir="$project_dir/Artifacts/Runtime/linux/$configuration/x64"
log_path="$log_dir/build-linux-${configuration,,}.log"
tool_cache_root="${EPOCH_TOOL_CACHE_ROOT:-${XDG_CACHE_HOME:-${HOME}/.cache}/epoch/tools}"
mkdir -p "$log_dir" "$output_dir"
exec > >(tee "$log_path") 2>&1

echo "[INFO] Epoch generated project build"
echo "[INFO] Project root: $project_dir"
echo "[INFO] Repo root: $repo_root"
echo "[INFO] Configuration: $configuration"
if [[ ! -x "$engine_dir/build.sh" || ! -f "$engine_dir/unix/current_toolchain.env" ]]; then
  echo "[ERROR] Epoch build policy files are missing under $engine_dir." >&2
  exit 3
fi

"$engine_dir/build.sh" --bootstrap-current-toolchain --tool-cache-root "$tool_cache_root" --check-toolchain clang "$configuration"
source <(sed 's/\r$//' "$engine_dir/unix/current_toolchain.env")

resolve_tool() {
  local candidate
  for candidate in "$@"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then printf '%s\n' "$candidate"; return 0; fi
  done
  return 1
}

cmake_bin="$(resolve_tool "${EPOCH_CMAKE:-}" "$HOME/.local/bin/cmake" "$tool_cache_root/cmake-$EPOCH_CMAKE_VERSION/bin/cmake" "$(command -v cmake 2>/dev/null || true)")" || { echo "[ERROR] Verified CMake $EPOCH_CMAKE_VERSION was not found." >&2; exit 4; }
ninja_bin="$(resolve_tool "${EPOCH_NINJA:-}" "$HOME/.local/bin/ninja" "$tool_cache_root/ninja-$EPOCH_NINJA_VERSION/ninja" "$(command -v ninja 2>/dev/null || true)")" || { echo "[ERROR] Verified Ninja $EPOCH_NINJA_VERSION was not found." >&2; exit 4; }
clang_c="$(resolve_tool "${CC:-}" "$HOME/.local/bin/clang-$EPOCH_LLVM_MAJOR" "$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang" "$(command -v clang-$EPOCH_LLVM_MAJOR 2>/dev/null || true)" "$(command -v clang 2>/dev/null || true)")" || { echo "[ERROR] Verified Clang $EPOCH_LLVM_VERSION C compiler was not found." >&2; exit 4; }
clang_cxx="$(resolve_tool "${CXX:-}" "$HOME/.local/bin/clang++-$EPOCH_LLVM_MAJOR" "$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang++" "$(command -v clang++-$EPOCH_LLVM_MAJOR 2>/dev/null || true)" "$(command -v clang++ 2>/dev/null || true)")" || { echo "[ERROR] Verified Clang $EPOCH_LLVM_VERSION C++ compiler was not found." >&2; exit 4; }
clang_scan_deps="$(resolve_tool "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS:-}" "$HOME/.local/bin/clang-scan-deps-$EPOCH_LLVM_MAJOR" "$HOME/.local/bin/clang-scan-deps" "$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang-scan-deps" "$(command -v clang-scan-deps-$EPOCH_LLVM_MAJOR 2>/dev/null || true)" "$(command -v clang-scan-deps 2>/dev/null || true)")" || { echo "[ERROR] Matching clang-scan-deps was not found." >&2; exit 4; }

resolve_vcpkg_root() {
  local candidate cache line toolchain
  local -a candidates=("${EPOCH_VCPKG_ROOT:-}" "${VCPKG_ROOT:-}" "$repo_root/vcpkg" "$(dirname "$repo_root")/vcpkg" "$HOME/vcpkg" "$HOME/Documents/repos/vcpkg" "$HOME/source/repos/vcpkg" /opt/vcpkg)
  for cache in "$repo_root"/Artifacts/Intermediate/cmake/linux/*/Clang-*/CMakeCache.txt "$repo_root"/build/cmake/linux/*/Clang-*/CMakeCache.txt "$engine_dir"/Bin/Clang-*/CMakeCache.txt; do
    [[ -f "$cache" ]] || continue
    toolchain=""
    while IFS= read -r line; do
      case "$line" in CMAKE_TOOLCHAIN_FILE:FILEPATH=*) toolchain="${line#*=}"; break ;; esac
    done < "$cache"
    if [[ -n "$toolchain" ]]; then candidates+=("$(cd "$(dirname "$toolchain")/../.." 2>/dev/null && pwd || true)"); fi
  done
  if command -v vcpkg >/dev/null 2>&1; then candidates+=("$(cd "$(dirname "$(command -v vcpkg)")" && pwd)"); fi
  for candidate in "${candidates[@]}"; do
    if [[ -n "$candidate" && -f "$candidate/scripts/buildsystems/vcpkg.cmake" ]]; then printf '%s\n' "$candidate"; return 0; fi
  done
  return 1
}

vcpkg_root="$(resolve_vcpkg_root)" || { echo "[ERROR] vcpkg was not found. Set EPOCH_VCPKG_ROOT or VCPKG_ROOT." >&2; exit 5; }
vcpkg_baseline="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["builtin-baseline"])' "$engine_dir/vcpkg.json")"
if [[ ! "$vcpkg_baseline" =~ ^[0-9a-f]{40}$ ]]; then
  echo "[ERROR] Engine manifest contains an invalid vcpkg baseline." >&2
  exit 5
fi
if ! git -C "$vcpkg_root" cat-file -e "${vcpkg_baseline}^{commit}" 2>/dev/null; then
  echo "[ERROR] Selected vcpkg registry does not contain locked baseline $vcpkg_baseline: $vcpkg_root" >&2
  exit 5
fi
echo "[INFO] CMake: $cmake_bin"
echo "[INFO] Ninja: $ninja_bin"
echo "[INFO] Clang: $clang_cxx"
echo "[INFO] vcpkg: $vcpkg_root"

export CC="$clang_c" CXX="$clang_cxx" VCPKG_ROOT="$vcpkg_root" VCPKG_FORCE_SYSTEM_BINARIES=1 VCPKG_FEATURE_FLAGS=manifests
"$cmake_bin" --fresh -S "$project_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE="$configuration" \
  -DEPOCH_REPO_ROOT="$repo_root" \
  -DCMAKE_C_COMPILER="$clang_c" \
  -DCMAKE_CXX_COMPILER="$clang_cxx" \
  -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS="$clang_scan_deps" \
  -DCMAKE_MAKE_PROGRAM="$ninja_bin" \
  -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_DIR="$engine_dir" \
  -DVCPKG_INSTALLED_DIR="$build_dir/vcpkg_installed" \
  -DVCPKG_TARGET_TRIPLET=x64-linux-epoch \
  -DVCPKG_OVERLAY_TRIPLETS="$engine_dir/cmake/triplets" \
  -DEPOCH_BUILD_STATIC_RUNTIME=ON \
  -DBUILD_TESTING=OFF
"$cmake_bin" --build "$build_dir" --target epoch_project_runtime --verbose
runtime_path="$output_dir/$artifact_stem"
if [[ ! -x "$runtime_path" ]]; then echo "[ERROR] Build completed without executable: $runtime_path" >&2; exit 6; fi
echo "[INFO] Output: $runtime_path"
