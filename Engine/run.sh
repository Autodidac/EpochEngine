#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EPOCH_REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EPOCH_OUTPUT_EDITION="development"
if grep -q '^#define EPOCH_SOURCE_EDITION_PUBLIC 1[[:space:]]*$' "${SCRIPT_DIR}/include/epoch.edition.hpp"; then
  EPOCH_OUTPUT_EDITION="public"
fi
EPOCH_DEFAULT_BUILD_ROOT="${EPOCH_REPOSITORY_ROOT}/build/cmake/linux/${EPOCH_OUTPUT_EDITION}"


COMPILER="gcc"
BUILD_TYPE="Debug"

if [[ $# -gt 0 && $1 != "--" ]]; then
  COMPILER=$1
  shift
fi

if [[ $# -gt 0 && $1 != "--" ]]; then
  BUILD_TYPE=$1
  shift
fi

if [[ $# -gt 0 && $1 == "--" ]]; then
  shift
  APP_ARGS=("$@")
else
  APP_ARGS=()
fi

case "$COMPILER" in
  gcc)
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

EXECUTABLE="${EPOCH_BUILD_ROOT:-${EPOCH_DEFAULT_BUILD_ROOT}}/${COMPILER_NAME}-${BUILD_TYPE}/epoch"

if [[ ! -x "$EXECUTABLE" ]]; then
  echo "Executable not found: $EXECUTABLE" >&2
  echo "Build the project first with ./build.sh ${COMPILER} ${BUILD_TYPE}" >&2
  exit 1
fi

cd "$(dirname "$EXECUTABLE")"
"$EXECUTABLE" "${APP_ARGS[@]}"
