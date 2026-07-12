#!/bin/bash
# Usage: ./build.sh [options] [gcc|clang] [Debug|Release] [-- cmake args]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_LOCK="${SCRIPT_DIR}/unix/current_toolchain.env"
if [[ ! -f "${TOOLCHAIN_LOCK}" ]]; then
  echo "Epoch toolchain lock is missing: ${TOOLCHAIN_LOCK}" >&2
  exit 1
fi
# shellcheck source=unix/current_toolchain.env
source "${TOOLCHAIN_LOCK}"

MINIMUM_CMAKE_VERSION="${EPOCH_CMAKE_VERSION}"

USE_VCPKG=1
UPDATER_SHELL_BUILD=0
VCPKG_ROOT_OVERRIDE=""
VCPKG_OVERLAY_PORTS_OVERRIDE=""
BOOTSTRAP_CURRENT_TOOLCHAIN=0
TOOL_CACHE_ROOT_OVERRIDE=""
CHECK_TOOLCHAIN_ONLY=0
CANCEL_FILE=""

version_at_least() {
  local actual=$1
  local required=$2
  [[ "$(printf '%s\n%s\n' "$required" "$actual" | sort -V | head -n1)" == "$required" ]]
}

tool_cache_root() {
  printf '%s\n' "${TOOL_CACHE_ROOT_OVERRIDE:-${XDG_CACHE_HOME:-${HOME}/.cache}/epoch/tools}"
}

sha256_file() {
  local path=$1

  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${path}" | awk '{ print $1 }'
    return 0
  fi

  if command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 "${path}" | awk '{ print $NF }'
    return 0
  fi

  return 1
}

verify_sha256() {
  local path=$1
  local expected=$2
  local actual

  actual="$(sha256_file "${path}" 2>/dev/null || true)"
  [[ -n "${actual}" && "${actual,,}" == "${expected,,}" ]]
}

download_verified() {
  local url=$1
  local destination=$2
  local expected_sha=$3
  local expected_size=$4
  local progress_start=$5
  local progress_span=$6
  local label=$7
  local partial="${destination}.part"
  local downloader_pid
  local downloaded
  local stage_progress
  local exit_code

  mkdir -p "$(dirname "${destination}")"
  if [[ -f "${destination}" ]] && verify_sha256 "${destination}" "${expected_sha}"; then
    echo "[EPOCH_PROGRESS] $((progress_start + progress_span))% ${label} ready from verified cache." >&2
    printf '%s\n' "${destination}"
    return 0
  fi

  rm -f "${destination}"
  if [[ -f "${partial}" ]] && (( $(wc -c < "${partial}") > expected_size )); then
    rm -f "${partial}"
  fi
  echo "[build.sh] Downloading ${url}" >&2
  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --retry 3 --retry-delay 2 --continue-at - --output "${partial}" "${url}" &
    downloader_pid=$!
  elif command -v wget >/dev/null 2>&1; then
    wget --continue --output-document="${partial}" "${url}" &
    downloader_pid=$!
  else
    echo "curl or wget is required to prepare the managed build toolchain." >&2
    return 1
  fi

  while kill -0 "${downloader_pid}" >/dev/null 2>&1; do
    if [[ -n "${CANCEL_FILE}" && -f "${CANCEL_FILE}" ]]; then
      kill "${downloader_pid}" >/dev/null 2>&1 || true
      wait "${downloader_pid}" 2>/dev/null || true
      rm -f "${partial}"
      echo "[build.sh] Current toolchain download canceled before build replacement." >&2
      return 130
    fi

    downloaded=0
    if [[ -f "${partial}" ]]; then
      downloaded=$(wc -c < "${partial}")
    fi
    if (( expected_size > 0 )); then
      stage_progress=$((progress_start + (downloaded * progress_span / expected_size)))
      if (( stage_progress > progress_start + progress_span )); then
        stage_progress=$((progress_start + progress_span))
      fi
      echo "[EPOCH_PROGRESS] ${stage_progress}% ${label} (${downloaded}/${expected_size} bytes)." >&2
    fi
    sleep 2
  done

  set +e
  wait "${downloader_pid}"
  exit_code=$?
  set -e
  if (( exit_code != 0 )); then
    if command -v curl >/dev/null 2>&1; then
      rm -f "${partial}"
      curl --fail --location --retry 3 --retry-delay 2 --output "${partial}" "${url}"
    else
      return "${exit_code}"
    fi
  fi

  if ! verify_sha256 "${partial}" "${expected_sha}"; then
    echo "Checksum verification failed for ${url}." >&2
    rm -f "${partial}"
    return 1
  fi

  mv -f "${partial}" "${destination}"
  echo "[EPOCH_PROGRESS] $((progress_start + progress_span))% ${label} downloaded and verified." >&2
  printf '%s\n' "${destination}"
}

run_cancellable() {
  local child_pid
  local exit_code

  if [[ -z "${CANCEL_FILE}" ]]; then
    "$@"
    return $?
  fi

  if command -v setsid >/dev/null 2>&1; then
    setsid "$@" &
  else
    "$@" &
  fi
  child_pid=$!

  while kill -0 "${child_pid}" >/dev/null 2>&1; do
    if [[ -f "${CANCEL_FILE}" ]]; then
      kill -- "-${child_pid}" >/dev/null 2>&1 || kill "${child_pid}" >/dev/null 2>&1 || true
      wait "${child_pid}" 2>/dev/null || true
      echo "[build.sh] Build canceled before runtime replacement." >&2
      return 130
    fi
    sleep 1
  done

  set +e
  wait "${child_pid}"
  exit_code=$?
  set -e
  return "${exit_code}"
}

write_tool_provenance() {
  local destination=$1
  local component=$2
  local version=$3
  local url=$4
  local sha=$5

  {
    printf 'Component: %s\n' "${component}"
    printf 'Version: %s\n' "${version}"
    printf 'Source: %s\n' "${url}"
    printf 'SHA-256: %s\n' "${sha}"
    printf 'Prepared by: Engine/build.sh\n'
  } > "${destination}/EPOCH_TOOL_PROVENANCE.txt"
}

bootstrap_cmake() {
  local cache_root
  local install_root
  local archive
  local staging
  local extracted

  cache_root="$(tool_cache_root)"
  install_root="${cache_root}/cmake-${EPOCH_CMAKE_VERSION}"
  if [[ -x "${install_root}/bin/cmake" && -f "${install_root}/LICENSE.rst" ]]; then
    printf '%s\n' "${install_root}/bin/cmake"
    return 0
  fi

  archive="$(download_verified \
    "${EPOCH_CMAKE_LINUX_X64_URL}" \
    "${cache_root}/downloads/cmake-${EPOCH_CMAKE_VERSION}-linux-x86_64.tar.gz" \
    "${EPOCH_CMAKE_LINUX_X64_SHA256}" \
    "${EPOCH_CMAKE_LINUX_X64_SIZE}" 34 4 "Managed CMake ${EPOCH_CMAKE_VERSION}")" || return $?
  staging="${cache_root}/staging/cmake-${EPOCH_CMAKE_VERSION}"
  rm -rf "${staging}"
  mkdir -p "${staging}"
  tar -xzf "${archive}" -C "${staging}"
  extracted="$(find "${staging}" -mindepth 1 -maxdepth 1 -type d -print -quit)"
  if [[ -z "${extracted}" || ! -x "${extracted}/bin/cmake" ]]; then
    echo "The verified CMake archive did not contain bin/cmake." >&2
    return 1
  fi
  if [[ ! -f "${extracted}/doc/cmake/LICENSE.rst" ]]; then
    echo "The verified CMake archive is missing its license; refusing the managed tool." >&2
    return 1
  fi
  cp "${extracted}/doc/cmake/LICENSE.rst" "${extracted}/LICENSE.rst"

  rm -rf "${install_root}"
  mv "${extracted}" "${install_root}"
  rm -rf "${staging}"
  write_tool_provenance "${install_root}" "CMake" "${EPOCH_CMAKE_VERSION}" \
    "${EPOCH_CMAKE_LINUX_X64_URL}" "${EPOCH_CMAKE_LINUX_X64_SHA256}"
  printf '%s\n' "${install_root}/bin/cmake"
}

bootstrap_ninja() {
  local cache_root
  local install_root
  local archive
  local license

  cache_root="$(tool_cache_root)"
  install_root="${cache_root}/ninja-${EPOCH_NINJA_VERSION}"
  if [[ -x "${install_root}/ninja" && -f "${install_root}/COPYING" ]]; then
    printf '%s\n' "${install_root}/ninja"
    return 0
  fi

  archive="$(download_verified \
    "${EPOCH_NINJA_LINUX_X64_URL}" \
    "${cache_root}/downloads/ninja-${EPOCH_NINJA_VERSION}-linux.zip" \
    "${EPOCH_NINJA_LINUX_X64_SHA256}" \
    "${EPOCH_NINJA_LINUX_X64_SIZE}" 38 1 "Managed Ninja ${EPOCH_NINJA_VERSION}")" || return $?
  license="$(download_verified \
    "${EPOCH_NINJA_LICENSE_URL}" \
    "${cache_root}/downloads/ninja-${EPOCH_NINJA_VERSION}-COPYING" \
    "${EPOCH_NINJA_LICENSE_SHA256}" \
    "${EPOCH_NINJA_LICENSE_SIZE}" 39 0 "Ninja ${EPOCH_NINJA_VERSION} license")" || return $?
  rm -rf "${install_root}"
  mkdir -p "${install_root}"
  if command -v unzip >/dev/null 2>&1; then
    unzip -q "${archive}" -d "${install_root}"
  elif command -v python3 >/dev/null 2>&1; then
    python3 -m zipfile -e "${archive}" "${install_root}"
  else
    echo "unzip or python3 is required to extract the managed Ninja archive." >&2
    return 1
  fi
  chmod 755 "${install_root}/ninja"
  cp "${license}" "${install_root}/COPYING"
  write_tool_provenance "${install_root}" "Ninja" "${EPOCH_NINJA_VERSION}" \
    "${EPOCH_NINJA_LINUX_X64_URL}" "${EPOCH_NINJA_LINUX_X64_SHA256}"
  printf '%s\n' "${install_root}/ninja"
}

bootstrap_llvm_toolchain() {
  local cache_root
  local install_root
  local archive
  local staging
  local clang_path
  local extracted

  cache_root="$(tool_cache_root)"
  install_root="${cache_root}/llvm-${EPOCH_LLVM_VERSION}"
  if [[ -x "${install_root}/bin/clang" \
    && -x "${install_root}/bin/clang++" \
    && -x "${install_root}/bin/clang-scan-deps" ]]; then
    printf '%s\n' "${install_root}"
    return 0
  fi

  archive="$(download_verified \
    "${EPOCH_LLVM_LINUX_X64_URL}" \
    "${cache_root}/downloads/LLVM-${EPOCH_LLVM_VERSION}-Linux-X64.tar.xz" \
    "${EPOCH_LLVM_LINUX_X64_SHA256}" \
    "${EPOCH_LLVM_LINUX_X64_SIZE}" 39 17 "Managed LLVM ${EPOCH_LLVM_VERSION}")" || return $?
  staging="${cache_root}/staging/llvm-${EPOCH_LLVM_VERSION}"
  rm -rf "${staging}"
  mkdir -p "${staging}"
  echo "[build.sh] Extracting LLVM ${EPOCH_LLVM_VERSION} into the executable-local tool cache." >&2
  tar -xJf "${archive}" -C "${staging}"
  clang_path="$(find "${staging}" -mindepth 2 -maxdepth 3 -path '*/bin/clang' -print -quit)"
  if [[ -z "${clang_path}" ]]; then
    echo "The verified LLVM archive did not contain bin/clang." >&2
    return 1
  fi
  extracted="$(dirname "$(dirname "${clang_path}")")"
  if [[ ! -f "${extracted}/LICENSE.TXT" && -f "${extracted}/include/llvm/Support/LICENSE.TXT" ]]; then
    cp "${extracted}/include/llvm/Support/LICENSE.TXT" "${extracted}/LICENSE.TXT"
  fi
  if [[ ! -f "${extracted}/LICENSE.TXT" ]]; then
    echo "The verified LLVM archive is missing LICENSE.TXT; refusing the managed toolchain." >&2
    return 1
  fi

  rm -rf "${install_root}"
  mv "${extracted}" "${install_root}"
  rm -rf "${staging}"
  write_tool_provenance "${install_root}" "LLVM/Clang" "${EPOCH_LLVM_VERSION}" \
    "${EPOCH_LLVM_LINUX_X64_URL}" "${EPOCH_LLVM_LINUX_X64_SHA256}"
  printf '%s\n' "${install_root}"
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

  if [[ ${BOOTSTRAP_CURRENT_TOOLCHAIN} -ne 0 ]]; then
    bootstrap_cmake
    return $?
  fi

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

    if version_at_least "${candidate_version}" "${EPOCH_NINJA_VERSION}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  if [[ ${BOOTSTRAP_CURRENT_TOOLCHAIN} -ne 0 ]]; then
    bootstrap_ninja
    return $?
  fi

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

resolve_first_executable_path() {
  local candidate
  for candidate in "$@"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
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

resolve_clang_scan_deps() {
  local compiler=$1
  local compiler_major
  local compiler_path
  local compiler_dir
  local llvm_bindir
  local package_path
  local configured="${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS:-}"

  if [[ -n "${configured}" ]]; then
    if [[ -x "${configured}" ]]; then
      printf '%s\n' "${configured}"
      return 0
    fi

    if command -v "${configured}" >/dev/null 2>&1; then
      command -v "${configured}"
      return 0
    fi
  fi

  compiler_major="$(compiler_version "${compiler}" | cut -d. -f1)"
  if [[ -z "${compiler_major}" ]]; then
    return 1
  fi

  compiler_path="${compiler}"
  if command -v "${compiler}" >/dev/null 2>&1; then
    compiler_path="$(command -v "${compiler}")"
  fi
  if command -v readlink >/dev/null 2>&1; then
    compiler_path="$(readlink -f "${compiler_path}" 2>/dev/null || printf '%s' "${compiler_path}")"
  fi
  compiler_dir="$(cd "$(dirname "${compiler_path}")" && pwd)"

  if resolve_first_executable_path \
    "${compiler_dir}/clang-scan-deps-${compiler_major}" \
    "${compiler_dir}/clang-scan-deps" \
    "/usr/lib/llvm-${compiler_major}/bin/clang-scan-deps-${compiler_major}" \
    "/usr/lib/llvm-${compiler_major}/bin/clang-scan-deps" \
    "/usr/local/lib/llvm-${compiler_major}/bin/clang-scan-deps-${compiler_major}" \
    "/usr/local/lib/llvm-${compiler_major}/bin/clang-scan-deps" \
    "/opt/llvm-${compiler_major}/bin/clang-scan-deps-${compiler_major}" \
    "/opt/llvm-${compiler_major}/bin/clang-scan-deps" \
    "/opt/llvm/bin/clang-scan-deps"; then
    return 0
  fi

  if [[ -n "${TOOL_CACHE_ROOT_OVERRIDE}" ]]; then
    if package_path="$(find "${TOOL_CACHE_ROOT_OVERRIDE}" -type f -path "*/bin/clang-scan-deps" -perm -u+x -print -quit 2>/dev/null)" \
      && [[ -n "${package_path}" ]]; then
      printf '%s\n' "${package_path}"
      return 0
    fi
  fi

  if command -v "llvm-config-${compiler_major}" >/dev/null 2>&1; then
    llvm_bindir="$("llvm-config-${compiler_major}" --bindir 2>/dev/null || true)"
    if resolve_first_executable_path \
      "${llvm_bindir}/clang-scan-deps-${compiler_major}" \
      "${llvm_bindir}/clang-scan-deps"; then
      return 0
    fi
  fi

  if command -v dpkg-query >/dev/null 2>&1; then
    package_path="$(dpkg-query -L "clang-tools-${compiler_major}" 2>/dev/null \
      | awk '/\/clang-scan-deps(-[0-9]+)?$/ { print; exit }')"
    if resolve_first_executable_path "${package_path}"; then
      return 0
    fi
  fi

  resolve_first_program "clang-scan-deps-${compiler_major}" clang-scan-deps
}

select_clang_toolchain() {
  local candidate_c
  local candidate_cxx
  local candidate_scanner
  local managed_root

  candidate_c="$(resolve_first_program "clang-${EPOCH_LLVM_MAJOR}" clang || true)"
  candidate_cxx="$(resolve_first_program "clang++-${EPOCH_LLVM_MAJOR}" clang++ || true)"

  if [[ -n "${candidate_c}" && -n "${candidate_cxx}" ]] \
    && [[ "$(compiler_version "${candidate_c}" | cut -d. -f1)" == "$(compiler_version "${candidate_cxx}" | cut -d. -f1)" ]] \
    && version_at_least "$(compiler_version "${candidate_cxx}")" "${EPOCH_LLVM_VERSION}"; then
    candidate_scanner="$(resolve_clang_scan_deps "${candidate_cxx}" || true)"
    if [[ -n "${candidate_scanner}" ]]; then
      COMPILER_C="${candidate_c}"
      COMPILER_CXX="${candidate_cxx}"
      CLANG_SCAN_DEPS="${candidate_scanner}"
      return 0
    fi
  fi

  if [[ ${BOOTSTRAP_CURRENT_TOOLCHAIN} -ne 0 ]]; then
    managed_root="$(bootstrap_llvm_toolchain || true)"
    if [[ -n "${managed_root}" \
      && -x "${managed_root}/bin/clang" \
      && -x "${managed_root}/bin/clang++" \
      && -x "${managed_root}/bin/clang-scan-deps" ]]; then
      COMPILER_C="${managed_root}/bin/clang"
      COMPILER_CXX="${managed_root}/bin/clang++"
      CLANG_SCAN_DEPS="${managed_root}/bin/clang-scan-deps"
      return 0
    fi
  fi

  return 1
}

read_vcpkg_manifest_baseline() {
  local manifest="${SCRIPT_DIR}/vcpkg.json"

  if [[ ! -f "${manifest}" ]]; then
    return 0
  fi

  sed -n 's/.*"builtin-baseline"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "${manifest}" | head -n1
}

ensure_vcpkg_baseline_available() {
  local baseline=$1

  if [[ -z "${baseline}" ]]; then
    return 0
  fi

  if [[ -e "${VCPKG_ROOT}/.git" ]] \
    && git -C "${VCPKG_ROOT}" cat-file -e "${baseline}^{commit}" >/dev/null 2>&1 \
    && git -C "${VCPKG_ROOT}" merge-base --is-ancestor "${baseline}" HEAD >/dev/null 2>&1 \
    && [[ -f "${VCPKG_ROOT}/versions/baseline.json" ]]; then
    return 0
  fi

  echo "[build.sh] vcpkg registry at '${VCPKG_ROOT}' is older than manifest baseline '${baseline}'." >&2
  return 1
}

prepare_managed_vcpkg_baseline() {
  local baseline=$1
  local cache_root
  local short_baseline
  local install_root
  local staging_root
  local bootstrap_script

  if [[ -z "${baseline}" || ${BOOTSTRAP_CURRENT_TOOLCHAIN} -eq 0 ]]; then
    return 1
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "git is required to prepare the managed vcpkg baseline." >&2
    return 1
  fi

  cache_root="$(tool_cache_root)"
  short_baseline="${baseline:0:12}"
  install_root="${cache_root}/vcpkg-${short_baseline}"
  staging_root="${cache_root}/staging/vcpkg-${short_baseline}-$$"

  if [[ -x "${install_root}/vcpkg" \
    && -f "${install_root}/scripts/buildsystems/vcpkg.cmake" \
    && -f "${install_root}/versions/baseline.json" \
    && -d "${install_root}/.git" \
    && "$(git -C "${install_root}" rev-parse HEAD 2>/dev/null || true)" == "${baseline}" ]]; then
    printf '%s\n' "${install_root}"
    return 0
  fi

  mkdir -p "${cache_root}/staging"
  rm -rf "${staging_root}"
  echo "[build.sh] Preparing managed vcpkg baseline ${baseline} in the updater tool cache." >&2
  run_cancellable git init -q "${staging_root}" || return $?
  run_cancellable git -C "${staging_root}" remote add origin https://github.com/microsoft/vcpkg.git || return $?
  run_cancellable git -C "${staging_root}" fetch --depth 1 origin "${baseline}" || return $?
  run_cancellable git -C "${staging_root}" checkout --detach -q FETCH_HEAD || return $?

  bootstrap_script="${staging_root}/bootstrap-vcpkg.sh"
  if [[ ! -x "${bootstrap_script}" ]]; then
    echo "Managed vcpkg baseline did not contain bootstrap-vcpkg.sh." >&2
    rm -rf "${staging_root}"
    return 1
  fi
  if [[ ! -f "${staging_root}/LICENSE.txt" && ! -f "${staging_root}/LICENSE" ]]; then
    echo "Managed vcpkg baseline is missing its license; refusing the managed tool." >&2
    rm -rf "${staging_root}"
    return 1
  fi

  (cd "${staging_root}" && run_cancellable ./bootstrap-vcpkg.sh -disableMetrics) || return $?
  if [[ ! -x "${staging_root}/vcpkg" ]]; then
    echo "Managed vcpkg bootstrap completed without producing the vcpkg executable." >&2
    rm -rf "${staging_root}"
    return 1
  fi

  rm -rf "${install_root}"
  mv "${staging_root}" "${install_root}"
  {
    printf 'Component: vcpkg\n'
    printf 'Release: %s\n' "${EPOCH_VCPKG_RELEASE}"
    printf 'Git commit: %s\n' "${baseline}"
    printf 'Source: https://github.com/microsoft/vcpkg.git\n'
    printf 'Prepared by: Engine/build.sh\n'
  } > "${install_root}/EPOCH_TOOL_PROVENANCE.txt"
  printf '%s\n' "${install_root}"
}

prepare_vcpkg_policy_overlays() {
  local root=$1
  local baseline=$2
  local overlay_root
  local port
  local source_port
  local overlay_port
  local portfile
  local staged_any=0

  overlay_root="$(tool_cache_root)/vcpkg-overlays-${baseline:0:12}"
  rm -rf "${overlay_root}"
  mkdir -p "${overlay_root}"

  for port in freetype glad glfw3 libogg libvorbis raylib sdl3 sfml shaderc spirv-tools zlib; do
    source_port="${root}/ports/${port}"
    [[ -d "${source_port}" ]] || continue
    overlay_port="${overlay_root}/${port}"
    cp -R "${source_port}" "${overlay_port}"
    portfile="${overlay_port}/portfile.cmake"
    [[ -f "${portfile}" ]] || continue

    if ! grep -q 'CMAKE_POLICY_VERSION_MINIMUM=3.5' "${portfile}"; then
      awk '
        BEGIN { inserted = 0 }
        {
          print
          if (!inserted && $0 ~ /^[[:space:]]*OPTIONS[[:space:]]*$/) {
            print "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
            inserted = 1
          } else if (!inserted && $0 ~ /vcpkg_cmake_configure\(/) {
            print "    OPTIONS"
            print "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
            inserted = 1
          }
        }
      ' "${portfile}" > "${portfile}.tmp"
      mv "${portfile}.tmp" "${portfile}"
    fi
    staged_any=1
  done

  if [[ ${staged_any} -eq 0 ]]; then
    rm -rf "${overlay_root}"
    return 1
  fi

  printf '%s\n' "${overlay_root}"
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
    --vcpkg-root)
      if [[ $# -lt 2 ]]; then
        echo "--vcpkg-root requires a vcpkg checkout path." >&2
        exit 1
      fi
      VCPKG_ROOT_OVERRIDE=$2
      shift 2
      ;;
    --vcpkg-overlay-ports)
      if [[ $# -lt 2 ]]; then
        echo "--vcpkg-overlay-ports requires an overlay-port path list." >&2
        exit 1
      fi
      VCPKG_OVERLAY_PORTS_OVERRIDE=$2
      shift 2
      ;;
    --bootstrap-current-toolchain)
      BOOTSTRAP_CURRENT_TOOLCHAIN=1
      shift
      ;;
    --tool-cache-root)
      if [[ $# -lt 2 ]]; then
        echo "--tool-cache-root requires a writable cache path." >&2
        exit 1
      fi
      TOOL_CACHE_ROOT_OVERRIDE=$2
      shift 2
      ;;
    --check-toolchain)
      CHECK_TOOLCHAIN_ONLY=1
      shift
      ;;
    --cancel-file)
      if [[ $# -lt 2 ]]; then
        echo "--cancel-file requires a marker path." >&2
        exit 1
      fi
      CANCEL_FILE=$2
      shift 2
      ;;
    --help|-h)
      echo "Usage: $0 [--no-vcpkg] [--updater-shell] [--bootstrap-current-toolchain] [--tool-cache-root <path>] [--cancel-file <path>] [--check-toolchain] [--vcpkg-root <path>] [--vcpkg-overlay-ports <paths>] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
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
  echo "Usage: $0 [--no-vcpkg] [--updater-shell] [--bootstrap-current-toolchain] [--tool-cache-root <path>] [--cancel-file <path>] [--check-toolchain] [--vcpkg-root <path>] [--vcpkg-overlay-ports <paths>] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
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
    if ! COMPILER_C="$(resolve_first_program "gcc-${EPOCH_GCC_MAJOR}" gcc)"; then
      echo "Unable to locate a GCC compiler." >&2
      exit 1
    fi

    if ! COMPILER_CXX="$(resolve_first_program "g++-${EPOCH_GCC_MAJOR}" g++)"; then
      echo "Unable to locate a G++ compiler." >&2
      exit 1
    fi

    if ! version_at_least "$(compiler_version "${COMPILER_CXX}")" "${EPOCH_GCC_VERSION}"; then
      echo "GCC ${EPOCH_GCC_VERSION}+ is required by the current Epoch toolchain lock. Install current GCC or use Clang ${EPOCH_LLVM_VERSION}." >&2
      exit 1
    fi

    COMPILER_NAME="GCC"
    ;;
  clang)
    if ! select_clang_toolchain; then
      echo "Unable to locate the current LLVM ${EPOCH_LLVM_VERSION} toolchain with clang, clang++, and matching clang-scan-deps." >&2
      echo "Install Clang ${EPOCH_LLVM_MAJOR} and clang-tools-${EPOCH_LLVM_MAJOR}, or use --bootstrap-current-toolchain with a writable tool cache." >&2
      exit 1
    fi

    echo "[build.sh] Clang toolchain: ${COMPILER_CXX}; module scanner: ${CLANG_SCAN_DEPS}." >&2
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

BUILD_ROOT="${EPOCH_BUILD_ROOT:-${SCRIPT_DIR}/Bin}"
BUILD_DIR="${BUILD_ROOT%/}/${COMPILER_NAME}-${BUILD_TYPE}${BUILD_VARIANT_SUFFIX}${BUILD_VERSION_SUFFIX}"
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
  -DCMAKE_CXX_STANDARD=23
  -DCMAKE_CXX_STANDARD_REQUIRED=ON
  -DCMAKE_CXX_EXTENSIONS=OFF
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON
  -DCMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP=ON
  -DEPOCH_VERSION_OVERRIDE_MAJOR=
  -DEPOCH_VERSION_OVERRIDE_MINOR=
  -DEPOCH_VERSION_OVERRIDE_REVISION=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR=
  -DEPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION=
)

# Updater/bootstrap builds can change compiler and vcpkg roots between runs.
# A stale CMake cache may otherwise trigger a second configure that silently
# drops the selected toolchain and resolves unrelated system packages.
if [[ ${BOOTSTRAP_CURRENT_TOOLCHAIN} -ne 0 ]]; then
  cmake_args=(--fresh "${cmake_args[@]}")
fi

if [[ "$(uname -s)" == "Linux" ]]; then
  cmake_args+=(
    -DVCPKG_TARGET_TRIPLET=x64-linux-epoch
    -DVCPKG_OVERLAY_TRIPLETS="${SCRIPT_DIR}/cmake/triplets"
  )
fi

if [[ "$COMPILER_CHOICE" == "clang" ]]; then
  cmake_args+=(-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS="$CLANG_SCAN_DEPS")
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
    echo "Ninja ${EPOCH_NINJA_VERSION}+ is required when using the Ninja generator with C++ modules." >&2
    echo "Install a newer Ninja in WSL, add it to PATH, or set EPOCH_NINJA to its full path." >&2
    exit 1
  fi

  cmake_args+=(-DCMAKE_MAKE_PROGRAM="$NINJA_BIN")
fi

# Keep vcpkg's port builds on the same verified tools as the engine build.
# Current vcpkg otherwise downloads its own older CMake even when a newer
# project CMake and Ninja were selected above.
tool_path_prefix="$(dirname "$CMAKE_BIN"):$(dirname "$COMPILER_CXX")"
if [[ "${GENERATOR_NAME}" == "Ninja" ]]; then
  tool_path_prefix="${tool_path_prefix}:$(dirname "$NINJA_BIN")"
fi
export PATH="${tool_path_prefix}:${PATH}"
export VCPKG_FORCE_SYSTEM_BINARIES=1
export CC="${COMPILER_C}"
export CXX="${COMPILER_CXX}"

if [[ $USE_VCPKG -ne 0 ]]; then
  detect_vcpkg_root() {
    if [[ -n "${VCPKG_ROOT_OVERRIDE}" && -f "${VCPKG_ROOT_OVERRIDE}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${VCPKG_ROOT_OVERRIDE}"
      return 0
    fi

    if [[ -n "${EPOCH_VCPKG_ROOT:-}" && -f "${EPOCH_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${EPOCH_VCPKG_ROOT}"
      return 0
    fi

    if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${VCPKG_ROOT}"
      return 0
    fi

    local candidate

    candidate="${SCRIPT_DIR}/../vcpkg"
    if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi

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
    if [[ -n "${VCPKG_ROOT_OVERRIDE}" ]]; then
      echo "The explicit --vcpkg-root path is not a usable vcpkg checkout: ${VCPKG_ROOT_OVERRIDE}" >&2
    fi
    echo "Set VCPKG_ROOT or EPOCH_VCPKG_ROOT to the root of your vcpkg checkout, install vcpkg and ensure it is on your PATH, or rerun with --no-vcpkg to rely on system packages." >&2
    exit 1
  fi

  manifest_baseline="$(read_vcpkg_manifest_baseline)"
  if ! ensure_vcpkg_baseline_available "${manifest_baseline}"; then
    if [[ ${BOOTSTRAP_CURRENT_TOOLCHAIN} -eq 0 ]]; then
      echo "Update the selected vcpkg checkout so its working registry contains the manifest baseline, or use --bootstrap-current-toolchain." >&2
      exit 1
    fi

    if ! VCPKG_ROOT="$(prepare_managed_vcpkg_baseline "${manifest_baseline}")"; then
      echo "Failed to prepare a managed vcpkg registry for manifest baseline '${manifest_baseline}'." >&2
      exit 1
    fi
    echo "[build.sh] Using managed vcpkg registry: ${VCPKG_ROOT}" >&2

    if managed_overlays="$(prepare_vcpkg_policy_overlays "${VCPKG_ROOT}" "${manifest_baseline}" || true)" \
      && [[ -n "${managed_overlays}" ]]; then
      VCPKG_OVERLAY_PORTS_OVERRIDE="${managed_overlays}"
      echo "[build.sh] Rebuilt updater policy overlays from the managed vcpkg registry." >&2
    else
      VCPKG_OVERLAY_PORTS_OVERRIDE=""
      echo "[build.sh] Managed vcpkg registry did not require policy overlays." >&2
    fi
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
  if [[ -n "${VCPKG_OVERLAY_PORTS_OVERRIDE}" ]]; then
    cmake_args+=(-DVCPKG_OVERLAY_PORTS="$VCPKG_OVERLAY_PORTS_OVERRIDE")
  fi
else
  echo "[build.sh] Proceeding without vcpkg integration; system-installed dependencies will be used." >&2
fi

if [[ ${CHECK_TOOLCHAIN_ONLY} -ne 0 ]]; then
  if [[ "${GENERATOR_NAME}" == "Ninja" ]]; then
    echo "[build.sh] Toolchain check passed: ${COMPILER_NAME} ${BUILD_TYPE}; CMake ${CMAKE_BIN}; Ninja ${NINJA_BIN}." >&2
  else
    echo "[build.sh] Toolchain check passed: ${COMPILER_NAME} ${BUILD_TYPE}; CMake ${CMAKE_BIN}; generator ${GENERATOR_NAME}." >&2
  fi
  exit 0
fi

cmake_args+=("${EXTRA_CMAKE_ARGS[@]}")

echo "[EPOCH_PROGRESS] 58% Linux CMake configure started." >&2
run_cancellable "$CMAKE_BIN" "${cmake_args[@]}"
echo "[EPOCH_PROGRESS] 62% Linux CMake configure completed." >&2

echo "[EPOCH_PROGRESS] 64% Linux full-engine build started." >&2
run_cancellable "$CMAKE_BIN" --build "$BUILD_DIR" --verbose
echo "[EPOCH_PROGRESS] 86% Linux full-engine build completed." >&2

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
