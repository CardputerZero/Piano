#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PACKAGE_NAME="${PACKAGE_NAME:-m5cardputerzero-piano}"
PACKAGE_SUFFIX="${PACKAGE_SUFFIX:-m5stack1}"
DEB_ARCH="arm64"
MAINTAINER="${MAINTAINER:-m5stack <m5stack@m5stack.com>}"
PARALLEL="${PARALLEL:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/package-arm64}"
STAGE_DIR="${STAGE_DIR:-${ROOT_DIR}/build/deb-root}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
DEPS_DIR="${PIANO_DEPS_DIR:-${ROOT_DIR}/dependencies}"
BIN_NAME="M5CardputerZero-Piano"
CMAKE_BIN="${CMAKE:-cmake}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
READELF_BIN="${READELF:-readelf}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

read_cmake_cache_value() {
    local name="$1"
    local cache_file="${BUILD_DIR}/CMakeCache.txt"
    local line=""

    if [[ ! -f "${cache_file}" ]]; then
        echo "CMake cache not found: ${cache_file}" >&2
        return 1
    fi

    line="$(grep -E "^${name}(:[^=]*)?=" "${cache_file}" | tail -n 1 || true)"
    if [[ -z "${line}" ]]; then
        echo "CMake cache value not found: ${name}" >&2
        return 1
    fi

    printf "%s\n" "${line#*=}"
}

for dependency in lvgl spdlog smooth_ui_toolkit; do
    if [[ ! -f "${DEPS_DIR}/${dependency}/CMakeLists.txt" ]]; then
        echo "Missing dependency: ${DEPS_DIR}/${dependency}" >&2
        echo "Run ./bootstrap.sh first or set PIANO_DEPS_DIR to a prepared dependency directory." >&2
        exit 1
    fi
done
if [[ ! -f "${DEPS_DIR}/miniaudio/miniaudio.h" ]]; then
    echo "Missing dependency: ${DEPS_DIR}/miniaudio" >&2
    echo "Run ./bootstrap.sh first or set PIANO_DEPS_DIR to a prepared dependency directory." >&2
    exit 1
fi
if [[ ! -f "${DEPS_DIR}/TinySoundFont/tsf.h" || ! -f "${DEPS_DIR}/TinySoundFont/LICENSE" ]]; then
    echo "Missing dependency: ${DEPS_DIR}/TinySoundFont" >&2
    echo "Run ./bootstrap.sh first or set PIANO_DEPS_DIR to a prepared dependency directory." >&2
    exit 1
fi

SOUNDFONT_FILE="${ROOT_DIR}/assets/soundfonts/piano.sf2"
SOUNDFONT_INSTALL_PATH="/usr/share/Piano/soundfonts/piano.sf2"
SOUNDFONT_SHA256="addb732569676727b02777a8df1f5d49eca0a19d4f306cca31624fdbfa0e1909"
for path in "${SOUNDFONT_FILE}" "${ROOT_DIR}/assets/soundfonts/SOURCES.txt" "${ROOT_DIR}/THIRD_PARTY_NOTICES.md"; do
    if [[ ! -f "${path}" ]]; then
        echo "Required file not found: ${path}" >&2
        exit 1
    fi
done
require_command sha256sum
actual_soundfont_sha256="$(sha256sum "${SOUNDFONT_FILE}" | awk '{print $1}')"
if [[ "${actual_soundfont_sha256}" != "${SOUNDFONT_SHA256}" ]]; then
    echo "SoundFont checksum mismatch: expected ${SOUNDFONT_SHA256}, got ${actual_soundfont_sha256}" >&2
    exit 1
fi

CMAKE_CONFIGURE_ARGS=(
    -S "${ROOT_DIR}"
    -B "${BUILD_DIR}"
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    -DPIANO_USE_SDL=OFF
    -DPIANO_USE_PULSEAUDIO=ON
    -DPIANO_BUILD_TESTS=OFF
    -DPIANO_DEFAULT_SOUNDFONT_PATH="${SOUNDFONT_INSTALL_PATH}"
    -DPIANO_BIN_NAME="${BIN_NAME}"
    -DPIANO_DEPS_DIR="${DEPS_DIR}"
    -DPIANO_OUTPUT_DIR="${BUILD_DIR}/dist"
)

host_arch="$(uname -m)"
if [[ "${host_arch}" != "aarch64" && "${host_arch}" != "arm64" ]]; then
    for compiler in aarch64-linux-gnu-gcc aarch64-linux-gnu-g++; do
        require_command "${compiler}"
    done
    READELF_BIN="${READELF:-aarch64-linux-gnu-readelf}"
    CMAKE_CONFIGURE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${ROOT_DIR}/cmake/aarch64-linux-gnu.cmake")
fi

for command in "${CMAKE_BIN}" "${READELF_BIN}" dpkg-deb; do
    require_command "${command}"
done

"${CMAKE_BIN}" "${CMAKE_CONFIGURE_ARGS[@]}"
if [[ "$(read_cmake_cache_value PIANO_USE_SDL)" != "OFF" ]]; then
    echo "Invalid package build: PIANO_USE_SDL must be OFF." >&2
    exit 1
fi
if [[ "$(read_cmake_cache_value PIANO_USE_PULSEAUDIO)" != "ON" ]]; then
    echo "Invalid package build: PIANO_USE_PULSEAUDIO must be ON." >&2
    exit 1
fi
if [[ "$(read_cmake_cache_value PIANO_BUILD_TESTS)" != "OFF" ]]; then
    echo "Invalid package build: PIANO_BUILD_TESTS must be OFF." >&2
    exit 1
fi
if [[ "$(read_cmake_cache_value PIANO_DEFAULT_SOUNDFONT_PATH)" != "${SOUNDFONT_INSTALL_PATH}" ]]; then
    echo "Invalid package build: unexpected SoundFont install path." >&2
    exit 1
fi

PACKAGE_VERSION="$(read_cmake_cache_value CMAKE_PROJECT_VERSION)"
"${CMAKE_BIN}" --build "${BUILD_DIR}" -j"${PARALLEL}"

EXECUTABLE="${BUILD_DIR}/dist/${BIN_NAME}"
DESKTOP_TEMPLATE="${SCRIPT_DIR}/piano.desktop.in"
ICON_FILE="${SCRIPT_DIR}/images/piano.png"
for path in "${EXECUTABLE}" "${DESKTOP_TEMPLATE}" "${ICON_FILE}"; do
    if [[ ! -f "${path}" ]]; then
        echo "Required file not found: ${path}" >&2
        exit 1
    fi
done

machine="$(${READELF_BIN} -h "${EXECUTABLE}" | awk -F: '/Machine:/ { sub(/^[[:space:]]+/, "", $2); print $2; exit }')"
if [[ "${machine}" != "AArch64" ]]; then
    echo "Invalid package executable architecture: expected AArch64, got ${machine:-unknown}." >&2
    exit 1
fi

dynamic_section="$(${READELF_BIN} -d "${EXECUTABLE}")"
for forbidden_library in libSDL libGL.so libGLES libEGL libdrm libgbm libglfw libX11 libwayland; do
    if [[ "${dynamic_section}" == *"${forbidden_library}"* ]]; then
        echo "Invalid device executable: unexpected ${forbidden_library} dependency." >&2
        exit 1
    fi
done

rm -rf "${STAGE_DIR}"
mkdir -p \
    "${STAGE_DIR}/DEBIAN" \
    "${STAGE_DIR}/usr/share/APPLaunch/bin" \
    "${STAGE_DIR}/usr/share/APPLaunch/applications" \
    "${STAGE_DIR}/usr/share/APPLaunch/share/images" \
    "${STAGE_DIR}/usr/share/Piano/soundfonts" \
    "${STAGE_DIR}/usr/share/doc/${PACKAGE_NAME}" \
    "${DIST_DIR}"

install -m 755 "${EXECUTABLE}" "${DIST_DIR}/${BIN_NAME}"
install -m 755 "${EXECUTABLE}" "${STAGE_DIR}/usr/share/APPLaunch/bin/${BIN_NAME}"
install -m 644 "${DESKTOP_TEMPLATE}" "${STAGE_DIR}/usr/share/APPLaunch/applications/piano.desktop"
install -m 644 "${ICON_FILE}" "${STAGE_DIR}/usr/share/APPLaunch/share/images/piano.png"
install -m 644 "${SOUNDFONT_FILE}" "${STAGE_DIR}${SOUNDFONT_INSTALL_PATH}"
install -m 644 "${ROOT_DIR}/assets/soundfonts/SOURCES.txt" \
    "${STAGE_DIR}/usr/share/doc/${PACKAGE_NAME}/SOUNDFONT-SOURCE"
install -m 644 "${ROOT_DIR}/THIRD_PARTY_NOTICES.md" \
    "${STAGE_DIR}/usr/share/doc/${PACKAGE_NAME}/THIRD_PARTY-NOTICES.md"
install -m 644 "${DEPS_DIR}/TinySoundFont/LICENSE" \
    "${STAGE_DIR}/usr/share/doc/${PACKAGE_NAME}/TinySoundFont-LICENSE"
install -m 644 "${SOUNDFONT_FILE}" "${DIST_DIR}/piano.sf2"

INSTALLED_SIZE="$(du -sk "${STAGE_DIR}/usr" | awk '{print $1}')"
cat >"${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${PACKAGE_VERSION}
Section: sound
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: ${MAINTAINER}
Depends: libc6, libstdc++6, libgcc-s1, libpulse0
Installed-Size: ${INSTALLED_SIZE}
Description: Piano application for M5CardputerZero APPLaunch
 Interactive sampled piano with a low-latency polyphonic synthesizer.
EOF

DEB_PATH="${DIST_DIR}/${PACKAGE_NAME}_${PACKAGE_VERSION}_${PACKAGE_SUFFIX}_${DEB_ARCH}.deb"
dpkg-deb --build --root-owner-group "${STAGE_DIR}" "${DEB_PATH}"
if [[ "$(dpkg-deb -f "${DEB_PATH}" Architecture)" != "${DEB_ARCH}" ]]; then
    echo "Generated package has an invalid architecture field." >&2
    exit 1
fi

echo "Generated cp0 executable: ${DIST_DIR}/${BIN_NAME}"
echo "Generated Debian package: ${DEB_PATH}"
