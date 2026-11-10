#!/usr/bin/env bash
# build-appimage_lite.sh
# Builds a lightweight AppImage containing only the ProtonVPN Qt App.
# The ProtonVPN CLI must be installed separately on the host system.
# Analogous to the Flatpak build, but without the sandbox.
#
# Usage:
#   ./build-appimage_lite.sh          # build only
#   ./build-appimage_lite.sh --run    # build, then launch the resulting AppImage
#
# Requirements:
#   cmake, ninja-build, python3, python3-venv, wget, binutils (ar), gcc
#   (Qt itself is fetched via aqtinstall, as described below, not from the system.)
#
# Builds are native: the AppImage targets the CPU architecture of the machine
# running this script (x86_64 or aarch64). CI uses a separate ARM runner for
# the aarch64 build.
#
# Output:
#   dist/ProtonVPN-Qt-<version>-lite-<arch>.AppImage

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/.appimage-build"
APPDIR="${BUILD_ROOT}/AppDir-lite"
TOOLS_DIR="${BUILD_ROOT}/tools"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
APP_ID="io.github.wheat32.ProtonVPNQt"

# -- Read version --------------------------------------------------------------
VERSION=$(python3 -c "import json; print(json.load(open('${SCRIPT_DIR}/src/version.json'))['app_version'])")

# -- Color helpers -------------------------------------------------------------
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[build-appimage-lite]${NC} $*"; }
warn()  { echo -e "${YELLOW}[build-appimage-lite]${NC} $*"; }
die()   { echo -e "${RED}[build-appimage-lite] ERROR:${NC} $*" >&2; exit 1; }

# -- Target architecture -------------------------------------------------------
# Each upstream names the same CPU differently, so map the host architecture to
# every naming scheme this script needs in one place.
case "$(uname -m)" in
    x86_64)
        APPIMAGE_ARCH="x86_64"         # linuxdeploy / appimagetool / runtime
        QT_HOST="linux"                # aqt host for prebuilt Qt binaries
        QT_ARCH_PATTERN='gcc_64$'      # "linux_gcc_64" (Qt >= 6.8) or "gcc_64"
        QT_DIR_NAME="gcc_64"           # aqt's on-disk install directory
        MARCH="x86-64"                 # generic baseline: no AVX2 requirement
        MULTIARCH="x86_64-linux-gnu"   # Debian/Ubuntu library path triplet
        ;;
    aarch64)
        APPIMAGE_ARCH="aarch64"
        QT_HOST="linux_arm64"
        QT_ARCH_PATTERN='gcc_arm64$'   # "linux_gcc_arm64"
        QT_DIR_NAME="gcc_arm64"
        MARCH="armv8-a"                # base ARMv8 (e.g. Raspberry Pi 4)
        MULTIARCH="aarch64-linux-gnu"
        ;;
    *)
        die "Unsupported architecture: $(uname -m) (supported: x86_64, aarch64)"
        ;;
esac
# appimagetool reads ARCH to choose which AppImage runtime to embed.
export ARCH="${APPIMAGE_ARCH}"
info "Target architecture: ${APPIMAGE_ARCH}"

OUTPUT="${OUTPUT_DIR}/ProtonVPN-Qt-${VERSION}-lite-${APPIMAGE_ARCH}.AppImage"

# -- Sanity checks -------------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "cmake is not installed"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed"
command -v wget   >/dev/null 2>&1 || die "wget is not installed"
command -v ar     >/dev/null 2>&1 || die "ar not found; install binutils"
command -v gcc    >/dev/null 2>&1 || die "gcc is not installed (needed for stub library)"
[[ -f "${SCRIPT_DIR}/src/CMakeLists.txt" ]] || die "Run this script from the repository root."

# -- Install Qt via aqt ---------------------------------------------------------
# aqt fetches Qt from upstream instead of the distro's qt6-base package,
# avoiding distros (e.g. Ubuntu 26.04) that compile against a raised CPU
# baseline (AVX2). QT_SPEC="6" always resolves to the newest 6.x release,
# bounded so a future Qt7 can't get pulled in silently.
QT_SPEC="6"
AQT_VENV="${BUILD_ROOT}/aqt-venv"
python3 -m venv "${AQT_VENV}"
"${AQT_VENV}/bin/pip" install --quiet aqtinstall

info "Resolving latest Qt ${QT_SPEC}.x release via aqt..."
QT_VERSION=$("${AQT_VENV}/bin/aqt" list-qt "${QT_HOST}" desktop --spec "${QT_SPEC}" --latest-version)
[[ -n "${QT_VERSION}" ]] || die "Could not resolve latest Qt ${QT_SPEC}.x version via aqt"
info "Latest available: Qt ${QT_VERSION}"

# The x86_64 arch identifier used to query/install changed from "gcc_64"
# (Qt <=6.5) to "linux_gcc_64" (Qt >=6.8); resolve it instead of hardcoding
# either. The on-disk install directory keeps the short name (gcc_64 or
# gcc_arm64) regardless of which identifier was used to install it.
QT_ARCH=$("${AQT_VENV}/bin/aqt" list-qt "${QT_HOST}" desktop --arch "${QT_VERSION}" | tr ' ' '\n' | grep -m1 "${QT_ARCH_PATTERN}")
[[ -n "${QT_ARCH}" ]] || die "Could not resolve Qt ${QT_HOST} desktop arch for ${QT_VERSION}"

QT_CACHE="${BUILD_ROOT}/qt"
QT_DIR="${QT_CACHE}/${QT_VERSION}/${QT_DIR_NAME}"

# qtsvg and qtwayland (the platform plugin) ship in the base install as of at
# least Qt 6.10+, so no -m addon modules are needed; requesting them by name errors.
if [[ ! -x "${QT_DIR}/bin/qmake6" ]]; then
    info "Installing Qt ${QT_VERSION} (${QT_ARCH}) via aqt..."
    "${AQT_VENV}/bin/aqt" install-qt "${QT_HOST}" desktop "${QT_VERSION}" "${QT_ARCH}" \
        -O "${QT_CACHE}"
fi

QMAKE="${QT_DIR}/bin/qmake6"
[[ -x "${QMAKE}" ]] || die "aqt-installed qmake6 not found at ${QMAKE}"
export QMAKE
export PATH="${QT_DIR}/bin:${PATH}"

# -- Build the Qt app ----------------------------------------------------------
# -march pins our binary to the generic baseline for the target CPU, regardless of the CI runner's compiler default.
info "Building ProtonVPN Qt App (Release)..."
cmake -S "${SCRIPT_DIR}/src" -B "${BUILD_ROOT}/native-lite" \
      -DCMAKE_BUILD_TYPE=Release -G Ninja \
      -DCMAKE_PREFIX_PATH="${QT_DIR}" \
      -DCMAKE_C_FLAGS="-march=${MARCH}" \
      -DCMAKE_CXX_FLAGS="-march=${MARCH}"
cmake --build "${BUILD_ROOT}/native-lite" --parallel

info "Installing into AppDir..."
rm -rf "${APPDIR}"
cmake --install "${BUILD_ROOT}/native-lite" --prefix "${APPDIR}/usr"

# -- Desktop integration -------------------------------------------------------
DESKTOP_DST="${APPDIR}/usr/share/applications/${APP_ID}.desktop"
ICON_DIR="${APPDIR}/usr/share/icons/hicolor/scalable/apps"

mkdir -p "$(dirname "${DESKTOP_DST}")" "${ICON_DIR}"

cp "${SCRIPT_DIR}/proton-vpn-qt-app.desktop" "${DESKTOP_DST}"
sed -i "s|^Exec=.*|Exec=proton_vpn_qt|"  "${DESKTOP_DST}"
sed -i "s|^Icon=.*|Icon=${APP_ID}|"      "${DESKTOP_DST}"

cp "${SCRIPT_DIR}/proton-vpn-sign.svg" "${ICON_DIR}/${APP_ID}.svg"
cp "${DESKTOP_DST}"            "${APPDIR}/${APP_ID}.desktop"
cp "${ICON_DIR}/${APP_ID}.svg" "${APPDIR}/${APP_ID}.svg"
# appimagetool creates .DirIcon (the AppImage's file thumbnail) from a PNG named
# after the desktop file's Icon= in the top level of the AppDir, and will
# refuse to build without one. The PNG is a 256x256 render of the SVG; after
# changing the SVG, regenerate it with:
#   rsvg-convert -w 256 -h 256 proton-vpn-sign.svg -o appimage/io.github.wheat32.ProtonVPNQt.png
cp "${SCRIPT_DIR}/appimage/${APP_ID}.png" "${APPDIR}/${APP_ID}.png"

# -- Custom AppRun -------------------------------------------------------------
info "Installing AppRun..."
cp "${SCRIPT_DIR}/appimage/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# -- Download linuxdeploy tools (shared cache with standalone build) -----------
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage"
APPIMAGETOOL="${TOOLS_DIR}/appimagetool-${APPIMAGE_ARCH}.AppImage"

mkdir -p "${TOOLS_DIR}"

declare -A TOOL_URLS=(
    ["${LINUXDEPLOY}"]="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
    ["${LINUXDEPLOY_QT}"]="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage"
    ["${APPIMAGETOOL}"]="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage"
)

for dest in "${!TOOL_URLS[@]}"; do
    if [[ ! -f "${dest}" ]]; then
        info "Downloading $(basename "${dest}")..."
        wget -q --show-progress -O "${dest}" "${TOOL_URLS[${dest}]}"
        chmod +x "${dest}"
    fi
done

# -- Bundle Qt shared libraries ------------------------------------------------
# QMAKE was already set above (aqt-installed Qt); linuxdeploy-plugin-qt queries
# it to locate Qt's own libs/plugins to bundle.
info "Bundling Qt libraries..."

FAKE_LIBS="${BUILD_ROOT}/fake-libs"
mkdir -p "${FAKE_LIBS}"
gcc -shared -Wl,-soname,libjxrglue.so.0 -x c /dev/null \
    -o "${FAKE_LIBS}/libjxrglue.so.0" 2>/dev/null || true

export PATH="${TOOLS_DIR}:${PATH}"

# linuxdeploy resolves proton_vpn_qt's shared library dependencies the same
# way ldd does (via LD_LIBRARY_PATH/system paths), not via QMAKE. Since aqt's
# Qt lives outside any system library path, it must be added explicitly or
# linuxdeploy can't find libQt6*.so at all.
LD_LIBRARY_PATH="${QT_DIR}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
NO_STRIP=1 APPIMAGE_EXTRACT_AND_RUN=1 "${LINUXDEPLOY}" \
    --appdir       "${APPDIR}" \
    --executable   "${APPDIR}/usr/bin/proton_vpn_qt" \
    --desktop-file "${DESKTOP_DST}" \
    --icon-file    "${ICON_DIR}/${APP_ID}.svg"

LINUXDEPLOY_QT_EXTRACTED="${BUILD_ROOT}/linuxdeploy-plugin-qt-extracted"
if [[ ! -d "${LINUXDEPLOY_QT_EXTRACTED}" ]]; then
    info "Extracting linuxdeploy-plugin-qt..."
    (cd "${BUILD_ROOT}" && APPIMAGE_EXTRACT_AND_RUN=1 "${LINUXDEPLOY_QT}" \
        --appimage-extract 2>/dev/null)
    mv "${BUILD_ROOT}/squashfs-root" "${LINUXDEPLOY_QT_EXTRACTED}"
fi

info "Deploying Qt plugins..."
LD_LIBRARY_PATH="${QT_DIR}/lib:${FAKE_LIBS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
NO_STRIP=1 \
    "${LINUXDEPLOY_QT_EXTRACTED}/usr/bin/linuxdeploy-plugin-qt" \
    --appdir "${APPDIR}"

rm -f "${APPDIR}/usr/plugins/imageformats/kimg_jxr.so"
rm -f "${APPDIR}/usr/lib/libjxrglue.so.0"

for _wayland_dir in \
    "${QT_DIR}/plugins/platforms" \
    "/usr/lib/${MULTIARCH}/qt6/plugins/platforms" \
    "/usr/lib/qt6/plugins/platforms" \
    "/usr/lib64/qt6/plugins/platforms"; do
    if [[ -f "${_wayland_dir}/libqwayland.so" ]]; then
        cp "${_wayland_dir}/libqwayland.so" "${APPDIR}/usr/plugins/platforms/"
        info "Bundled Wayland platform plugin (${_wayland_dir})"

        # Libraries that must come from the host system at runtime are skipped,
        # matching linuxdeploy's own excludelist (AppImage's excludelist).
        _host_libs=(
            # Core glibc: bundling libc.so.6 ties the AppImage to the CI
            # runner's glibc build (e.g. its compiled CPU baseline), breaking
            # it on hosts with an older/different CPU.
            'ld-linux(-x86-64)?\.so\.2' 'ld-linux-aarch64\.so\.1' 'libc\.so\.6'
            'libm\.so\.6' 'libpthread\.so\.0' 'libdl\.so\.2' 'librt\.so\.1'
            'libresolv\.so\.2' 'libnsl\.so\.1' 'libutil\.so\.1' 'libcrypt\.so\.1'
            'libnss_.*\.so.*'
            # Graphics and display: these have to match the host's GPU driver
            # and display server. Bundled copies are known to break Mesa and
            # proprietary drivers with "undefined symbol" errors.
            'libGL\.so\.1' 'libEGL\.so\.1' 'libGLX\.so\.0' 'libGLdispatch\.so\.0'
            'libX11\.so\.6' 'libxcb\.so\.1' 'libwayland-client\.so\.0'
            # Fonts: a bundled fontconfig older than the host cannot parse the
            # host's /etc/fonts config, so it prints a wall of warnings and
            # drops the generic families (sans-serif, monospace, emoji), which
            # changes font fallback. expat and zlib are their dependencies and
            # come from the host too, so the host's fontconfig and freetype
            # never run against older bundled copies.
            'libfontconfig\.so\.1' 'libfreetype\.so\.6' 'libexpat\.so\.1' 'libz\.so\.1'
        )
        _host_excludelist="^($(IFS='|'; echo "${_host_libs[*]}"))$"
        while IFS= read -r _dep; do
            _dep_name=$(basename "${_dep}")
            if [[ "${_dep_name}" =~ ${_host_excludelist} ]]; then
                continue
            fi
            if [[ -f "${_dep}" && ! -f "${APPDIR}/usr/lib/${_dep_name}" ]]; then
                cp "${_dep}" "${APPDIR}/usr/lib/"
                info "  Bundled Wayland dep: ${_dep_name}"
            fi
        done < <(ldd "${_wayland_dir}/libqwayland.so" 2>/dev/null \
                 | awk '/=>/ {print $3}' \
                 | grep -v "not found")

        _qt_plugins_dir="$(dirname "${_wayland_dir}")"
        for _sub in wayland-shell-integration wayland-graphics-integration-client \
                    wayland-decoration-client; do
            if [[ -d "${_qt_plugins_dir}/${_sub}" ]]; then
                mkdir -p "${APPDIR}/usr/plugins/${_sub}"
                cp "${_qt_plugins_dir}/${_sub}"/*.so \
                   "${APPDIR}/usr/plugins/${_sub}/" 2>/dev/null || true
                info "  Bundled Qt Wayland sub-plugin dir: ${_sub}"
            fi
        done
        break
    fi
done

cp "${SCRIPT_DIR}/appimage/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# -- Create AppImage -----------------------------------------------------------
mkdir -p "${OUTPUT_DIR}"
info "Creating AppImage → ${OUTPUT}..."
APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGETOOL}" "${APPDIR}" "${OUTPUT}"

info "Done: ${OUTPUT}"

# -- Optional launch -----------------------------------------------------------
if [[ "${1:-}" == "--run" ]]; then
    info "Launching ${OUTPUT}..."
    chmod +x "${OUTPUT}"
    "${OUTPUT}"
fi
