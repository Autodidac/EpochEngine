#!/bin/bash
# Usage: ./build.sh [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- cmake args]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MINIMUM_CMAKE_VERSION="3.28.0"

USE_VCPKG=1
UPDATER_SHELL_BUILD=0

version_at_least() {
  local actual=$1
  local required=$2
  [[ "$(printf '%s\n%s\n' "$required" "$actual" | sort -V | head -n1)" == "$required" ]]
}

resolve_cmake() {
  local candidate
  local candidate_version
  local -a candidates=()

  if [[ -n "${EPOCH_CMAKE:-}" ]]; then
    candidates+=("${EPOCH_CMAKE}")
  fi

  if [[ -x "${HOME}/.local/bin/cmake" ]]; then
    candidates+=("${HOME}/.local/bin/cmake")
  fi

  if [[ -x "/usr/local/bin/cmake" ]]; then
    candidates+=("/usr/local/bin/cmake")
  fi

  if command -v cmake >/dev/null 2>&1; then
    candidates+=("$(command -v cmake)")
  fi

  for candidate in "${candidates[@]}"; do
    if [[ ! -x "${candidate}" ]]; then
      continue
    fi

    candidate_version="$("${candidate}" --version 2>/dev/null | awk 'NR==1 { print $3 }')"
    if [[ -z "${candidate_version}" ]]; then
      continue
    fi

    if version_at_least "${candidate_version}" "${MINIMUM_CMAKE_VERSION}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

resolve_ninja() {
  local candidate
  local candidate_version
  local -a candidates=()

  if [[ -n "${EPOCH_NINJA:-}" ]]; then
    candidates+=("${EPOCH_NINJA}")
  fi

  if [[ -x "${HOME}/.local/bin/ninja" ]]; then
    candidates+=("${HOME}/.local/bin/ninja")
  fi

  if [[ -x "/usr/local/bin/ninja" ]]; then
    candidates+=("/usr/local/bin/ninja")
  fi

  if command -v ninja >/dev/null 2>&1; then
    candidates+=("$(command -v ninja)")
  fi

  for candidate in "${candidates[@]}"; do
    if [[ ! -x "${candidate}" ]]; then
      continue
    fi

    candidate_version="$("${candidate}" --version 2>/dev/null | awk 'NR==1 { print $1 }')"
    if [[ -z "${candidate_version}" ]]; then
      continue
    fi

    if version_at_least "${candidate_version}" "1.11.0"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

resolve_first_program() {
  local candidate
  for candidate in "$@"; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      command -v "${candidate}"
      return 0
    fi
  done

  return 1
}

compiler_version() {
  local compiler=$1

  if [[ "${compiler}" == *clang* ]]; then
    "${compiler}" --version 2>/dev/null | awk 'NR==1 { for(i=1; i<=NF; ++i) if ($i == "version") { print $(i+1); exit } }'
  else
    "${compiler}" -dumpfullversion -dumpversion 2>/dev/null | head -n1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-vcpkg)
      USE_VCPKG=0
      shift
      ;;
    --updater-shell)
      UPDATER_SHELL_BUILD=1
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
      exit 0
      ;;
    gcc|clang)
      break
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
  exit 1
fi

COMPILER_CHOICE=$1
shift

if [[ $# -gt 0 && $1 != "--" ]]; then
  BUILD_TYPE=$1
  shift
else
  BUILD_TYPE=Debug
fi

if [[ $# -gt 0 && $1 == "--" ]]; then
  shift
  EXTRA_CMAKE_ARGS=("$@")
else
  EXTRA_CMAKE_ARGS=()
fi

case "$COMPILER_CHOICE" in
  gcc)
    if ! COMPILER_C="$(resolve_first_program gcc-14 gcc-13 gcc-12 gcc)"; then
      echo "Unable to locate a GCC compiler." >&2
      exit 1
    fi

    if ! COMPILER_CXX="$(resolve_first_program g++-14 g++-13 g++-12 g++)"; then
      echo "Unable to locate a G++ compiler." >&2
      exit 1
    fi

    if ! version_at_least "$(compiler_version "${COMPILER_CXX}")" "14.0.0"; then
      echo "GCC 14+ is required for the module-based Linux build. Install a newer GCC or use clang." >&2
      exit 1
    fi

    COMPILER_NAME="GCC"
    ;;
  clang)
    if ! COMPILER_C="$(resolve_first_program clang-20 clang-19 clang-18 clang-17 clang-16 clang-15 clang-14 clang)"; then
      echo "Unable to locate a Clang compiler." >&2
      exit 1
    fi

    if ! COMPILER_CXX="$(resolve_first_program clang++-20 clang++-19 clang++-18 clang++-17 clang++-16 clang++-15 clang++-14 clang++)"; then
      echo "Unable to locate a Clang++ compiler." >&2
      exit 1
    fi

    if ! version_at_least "$(compiler_version "${COMPILER_CXX}")" "18.0.0"; then
      echo "Clang 18+ is required for the module-based Linux build. Install a newer Clang toolchain." >&2
      exit 1
    fi

    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER_CHOICE'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

INSTALL_PREFIX="${SCRIPT_DIR}/built"
BUILD_VARIANT_SUFFIX=""
if [[ $UPDATER_SHELL_BUILD -ne 0 ]]; then
  BUILD_VARIANT_SUFFIX="-UpdaterShell"
fi
HAS_VERSION_OVERRIDES=0
OVERRIDE_VERSION_MAJOR=""
OVERRIDE_VERSION_MINOR=""
OVERRIDE_VERSION_REVISION=""
PACKAGED_VERSION_MAJOR=""
PACKAGED_VERSION_MINOR=""
PACKAGED_VERSION_REVISION=""

for extra_arg in "${EXTRA_CMAKE_ARGS[@]}"; do
  case "$extra_arg" in
    -DEPOCH_VERSION_OVERRIDE_MAJOR=*)
      HAS_VERSION_OVERRIDES=1
      OVERRIDE_VERSION_MAJOR="${extra_arg#*=}"
      ;;
    -DEPOCH_VERSION_OVERRIDE_MINOR=*)
      HAS_VERSION_OVERRIDES=1
      OVERRIDE_VERSION_MINOR="${extra_arg#*=}"
      ;;
    -DEPOCH_VERSION_OVERRIDE_REVISION=*)
      HAS_VERSION_OVERRIDES=1
      OVERRIDE_VERSION_REVISION="${extra_arg#*=}"
      ;;
    -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR=*)
      HAS_VERSION_OVERRIDES=1
      PACKAGED_VERSION_MAJOR="${extra_arg#*=}"
      ;;
    -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR=*)
      HAS_VERSION_OVERRIDES=1
      PACKAGED_VERSION_MINOR="${extra_arg#*=}"
      ;;
    -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION=*)
      HAS_VERSION_OVERRIDES=1
      PACKAGED_VERSION_REVISION="${extra_arg#*=}"
      ;;
  esac
done

BUILD_VERSION_SUFFIX=""
if [[ $HAS_VERSION_OVERRIDES -ne 0 ]]; then
  if [[ -n "$OVERRIDE_VERSION_MAJOR" && -n "$OVERRIDE_VERSION_MINOR" && -n "$OVERRIDE_VERSION_REVISION" ]]; then
    BUILD_VERSION_SUFFIX="-v${OVERRIDE_VERSION_MAJOR}.${OVERRIDE_VERSION_MINOR}.${OVERRIDE_VERSION_REVISION}"
  elif [[ -n "$PACKAGED_VERSION_MAJOR" && -n "$PACKAGED_VERSION_MINOR" && -n "$PACKAGED_VERSION_REVISION" ]]; then
    BUILD_VERSION_SUFFIX="-v${PACKAGED_VERSION_MAJOR}.${PACKAGED_VERSION_MINOR}.${PACKAGED_VERSION_REVISION}"
  else
    BUILD_VERSION_SUFFIX="-Versioned"
  fi
fi

BUILD_DIR="${SCRIPT_DIR}/Bin/${COMPILER_NAME}-${BUILD_TYPE}${BUILD_VARIANT_SUFFIX}${BUILD_VERSION_SUFFIX}"
GENERATOR_NAME="${EPOCH_CMAKE_GENERATOR:-Ninja}"

if ! CMAKE_BIN="$(resolve_cmake)"; then
  echo "CMake ${MINIMUM_CMAKE_VERSION}+ is required." >&2
  echo "Install a newer CMake in WSL, add it to PATH, or set EPOCH_CMAKE to its full path." >&2
  echo "Ubuntu's stock /usr/bin/cmake is often too old for the module-based Epoch build." >&2
  exit 1
fi

cmake_args=(
  -S "$SCRIPT_DIR"
  -B "$BUILD_DIR"
  -G "$GENERATOR_NAME"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DCMAKE_C_COMPILER="$COMPILER_C"
  -DCMAKE_CXX_COMPILER="$COMPILER_CXX"
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON
  -DCMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP=ON
  -DEPOCH_VERSION_OVERRIDE_MAJOR=
  -DEPOCH_VERSION_OVERRIDE_MINOR=
  -DEPOCH_VERSION_OVERRIDE_REVISION=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION=
)

if [[ "$(uname -s)" == "Linux" ]]; then
  cmake_args+=(-DEPOCH_ENABLE_VULKAN=OFF)
  cmake_args+=(-DEPOCH_ENABLE_SFML=OFF)
fi

if [[ $UPDATER_SHELL_BUILD -ne 0 ]]; then
  cmake_args+=(
    -DEPOCH_UPDATER_SHELL_BUILD=ON
    -DEPOCH_ENABLE_RAYLIB=OFF
    -DEPOCH_ENABLE_SDL=OFF
    -DEPOCH_ENABLE_SFML=OFF
    -DEPOCH_ENABLE_VULKAN=OFF
    -DEPOCH_ENABLE_OPENGL=ON
    -DEPOCH_ENABLE_SOFTWARE_RENDERER=ON
  )
else
  cmake_args+=(-DEPOCH_UPDATER_SHELL_BUILD=OFF)
fi

if [[ "${GENERATOR_NAME}" == "Ninja" ]]; then
  if ! NINJA_BIN="$(resolve_ninja)"; then
    echo "Ninja 1.11+ is required when using the Ninja generator with C++ modules." >&2
    echo "Install a newer Ninja in WSL, add it to PATH, or set EPOCH_NINJA to its full path." >&2
    exit 1
  fi

  cmake_args+=(-DCMAKE_MAKE_PROGRAM="$NINJA_BIN")
fi

if [[ $USE_VCPKG -ne 0 ]]; then
  detect_vcpkg_root() {
    if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${VCPKG_ROOT}"
      return 0
    fi

    local candidate

    candidate="${SCRIPT_DIR}/../../vcpkg"
    if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi

    if command -v vcpkg >/dev/null 2>&1; then
      local executable
      executable="$(command -v vcpkg)"

      if command -v realpath >/dev/null 2>&1; then
        executable="$(realpath "$executable")"
      elif command -v readlink >/dev/null 2>&1; then
        executable="$(readlink -f "$executable" 2>/dev/null || echo "$executable")"
      fi

      candidate="$(cd "$(dirname "$executable")" && pwd)"
      if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
        printf '%s\n' "${candidate}"
        return 0
      fi
    fi

    return 1
  }

  if ! VCPKG_ROOT="$(detect_vcpkg_root)"; then
    echo "vcpkg installation not found." >&2
    echo "Set VCPKG_ROOT to the root of your vcpkg checkout, install vcpkg and ensure it is on your PATH, or rerun with --no-vcpkg to rely on system packages." >&2
    exit 1
  fi

  export VCPKG_ROOT

  VCPKG_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  if [[ ! -f "${VCPKG_TOOLCHAIN_FILE}" ]]; then
    echo "Unable to locate vcpkg toolchain file at '${VCPKG_TOOLCHAIN_FILE}'." >&2
    exit 1
  fi

  if [[ -z "${VCPKG_FEATURE_FLAGS:-}" ]]; then
    export VCPKG_FEATURE_FLAGS=manifests
  fi

  cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE")
else
  echo "[build.sh] Proceeding without vcpkg integration; system-installed dependencies will be used." >&2
fi

cmake_args+=("${EXTRA_CMAKE_ARGS[@]}")

"$CMAKE_BIN" "${cmake_args[@]}"

"$CMAKE_BIN" --build "$BUILD_DIR" --verbose

if "$CMAKE_BIN" -LA -N "$BUILD_DIR" | grep -q "DOXYGEN_FOUND:BOOL=1"; then
  echo "Generating Epoch API documentation..."
  if "$CMAKE_BIN" --build "$BUILD_DIR" --target docs; then
    echo "API reference available under $(pwd)/docs/api/html/index.html"
  else
    echo "Doxygen reported an error while generating documentation." >&2
  fi
else
  echo "Skipping API documentation generation (Doxygen not detected during configure)."
fi
