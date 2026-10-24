#!/bin/bash
# Usage: ./install.sh [gcc|clang] [Debug|Release]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EPOCH_REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EPOCH_OUTPUT_EDITION="development"
if grep -q '^#define EPOCH_SOURCE_EDITION_PUBLIC 1[[:space:]]*$' "${SCRIPT_DIR}/include/epoch.edition.hpp"; then
  EPOCH_OUTPUT_EDITION="public"
fi
EPOCH_DEFAULT_BUILD_ROOT="${EPOCH_REPOSITORY_ROOT}/build/cmake/linux/${EPOCH_OUTPUT_EDITION}"


if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [gcc|clang] [Debug|Release]" >&2
  exit 1
fi

COMPILER_CHOICE=$1
shift

if [[ $# -gt 0 ]]; then
  BUILD_TYPE=$1
  shift
else
  BUILD_TYPE=Debug
fi

if [[ $# -gt 0 ]]; then
  echo "Unexpected arguments: $*" >&2
  exit 1
fi

case "$COMPILER_CHOICE" in
  gcc)
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER_CHOICE'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

BUILD_DIR="${EPOCH_BUILD_ROOT:-${EPOCH_DEFAULT_BUILD_ROOT}}/${COMPILER_NAME}-${BUILD_TYPE}"
INSTALL_PREFIX="${EPOCH_REPOSITORY_ROOT}/build/install/linux/${EPOCH_OUTPUT_EDITION}/${COMPILER_NAME}-${BUILD_TYPE}"

cmake --install "$BUILD_DIR" --config "$BUILD_TYPE" --prefix "$INSTALL_PREFIX"
