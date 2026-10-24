#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EDITION="development"
if grep -q '^#define EPOCH_SOURCE_EDITION_PUBLIC 1[[:space:]]*$' "${SCRIPT_DIR}/include/epoch.edition.hpp"; then
  EDITION="public"
fi

# Only this helper's fixed owned output roots. Never delete the caller's cwd,
# a custom EPOCH_BUILD_ROOT, retained legacy outputs, projects or other editions.
for relative in "build/cmake/linux/${EDITION}" "build/install/linux/${EDITION}"; do
  target="${REPOSITORY_ROOT}/${relative}"
  if [[ -e "$target" || -L "$target" ]]; then
    physical="$(realpath -e -- "$target")"
    if [[ "$physical" != "$target" || ! -d "$target" ]]; then
      echo "Refusing redirected output root: $target" >&2
      exit 1
    fi
    rm -rf -- "$target"
    echo "Cleaned $target"
  fi
done
