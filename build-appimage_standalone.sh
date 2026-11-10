#!/usr/bin/env bash
# build-appimage_standalone.sh
# Builds a self-contained AppImage bundling the ProtonVPN Qt App and CLI.
#
# Uses python-build-standalone to embed a portable Python interpreter matching
# the CI system's Python version, ensuring native extensions (e.g. gi._gi) are
# ABI-compatible.  Runs correctly on Arch, Ubuntu, Fedora, or any other distro.
#
# The CLI version bundled is read from src/version.json (cli_version_tested_max).
# Its packages are downloaded from the ProtonVPN public apt repository; PyPI
# supplies the remaining Python dependencies in a venv.
#
# Build-order rationale:
#   1. ProtonVPN .deb packages + gi module are placed in AppDir first so that
#      linuxdeploy can scan their ELF deps and bundle libgirepository, GLib, etc.
#   2. python-build-standalone is copied AFTER linuxdeploy.  Its own libssl.so.3
#      lives under python/lib/ and is found by the Python binary via RPATH.
#      If it were present during linuxdeploy, linuxdeploy might copy a different
#      libssl version to usr/lib/, conflicting with the Qt SSL backend.
#
# Usage:
#   ./build-appimage.sh          # build only
#   ./build-appimage.sh --run    # build, then launch the resulting AppImage
#
# Requirements (install with your package manager before running):
#   cmake, ninja-build, python3, python3-venv, python3-gi, python3-gi-cairo,
#   wget, binutils (ar), gcc
#   (Qt itself is fetched via aqtinstall, as described below, not from the system.)
#
# linuxdeploy and appimagetool are downloaded automatically on first run
# and cached in .appimage-build/tools/.
#
# Builds are native: the AppImage targets the CPU architecture of the machine
# running this script (x86_64 or aarch64). CI uses a separate ARM runner for
# the aarch64 build.
#
# Output:
#   dist/ProtonVPN-Qt-<version>-standalone-<arch>.AppImage

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/.appimage-build"
APPDIR="${BUILD_ROOT}/AppDir"
TOOLS_DIR="${BUILD_ROOT}/tools"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
APP_ID="io.github.wheat32.ProtonVPNQt"

# -- Read version --------------------------------------------------------------
VERSION=$(python3 -c "import json; print(json.load(open('${SCRIPT_DIR}/src/version.json'))['app_version'])")

# -- Color helpers -------------------------------------------------------------
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[build-appimage]${NC} $*"; }
warn()  { echo -e "${YELLOW}[build-appimage]${NC} $*"; }
die()   { echo -e "${RED}[build-appimage] ERROR:${NC} $*" >&2; exit 1; }

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
        DEB_ARCH="amd64"               # Proton apt repo architecture section
        MULTIARCH="x86_64-linux-gnu"   # Debian/Ubuntu library path triplet
        PBS_ARCH_PREFIXES="'x86_64-unknown', 'x86_64_v1-unknown'"
        ;;
    aarch64)
        APPIMAGE_ARCH="aarch64"
        QT_HOST="linux_arm64"
        QT_ARCH_PATTERN='gcc_arm64$'   # "linux_gcc_arm64"
        QT_DIR_NAME="gcc_arm64"
        MARCH="armv8-a"                # base ARMv8 (e.g. Raspberry Pi 4)
        DEB_ARCH="arm64"
        MULTIARCH="aarch64-linux-gnu"
        PBS_ARCH_PREFIXES="'aarch64-unknown',"
        ;;
    *)
        die "Unsupported architecture: $(uname -m) (supported: x86_64, aarch64)"
        ;;
esac
# appimagetool reads ARCH to choose which AppImage runtime to embed.
export ARCH="${APPIMAGE_ARCH}"
info "Target architecture: ${APPIMAGE_ARCH}"

# APPIMAGE_VARIANT_SUFFIX distinguishes the ubuntu-24.04-pinned "compat" build
# (see release.yml) from the regular build in the release artifact filename.
OUTPUT="${OUTPUT_DIR}/ProtonVPN-Qt-${VERSION}-standalone${APPIMAGE_VARIANT_SUFFIX:+-${APPIMAGE_VARIANT_SUFFIX}}-${APPIMAGE_ARCH}.AppImage"

# -- Sanity checks -------------------------------------------------------------
command -v cmake   >/dev/null 2>&1 || die "cmake is not installed"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed"
command -v wget    >/dev/null 2>&1 || die "wget is not installed"
command -v ar      >/dev/null 2>&1 || die "ar not found; install binutils"
command -v gcc     >/dev/null 2>&1 || die "gcc is not installed (needed for stub library)"
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
cmake -S "${SCRIPT_DIR}/src" -B "${BUILD_ROOT}/native" \
      -DCMAKE_BUILD_TYPE=Release -G Ninja \
      -DCMAKE_PREFIX_PATH="${QT_DIR}" \
      -DCMAKE_C_FLAGS="-march=${MARCH}" \
      -DCMAKE_CXX_FLAGS="-march=${MARCH}"
cmake --build "${BUILD_ROOT}/native" --parallel

info "Installing into AppDir..."
rm -rf "${APPDIR}"
cmake --install "${BUILD_ROOT}/native" --prefix "${APPDIR}/usr"

# -- Bundle ProtonVPN CLI packages ---------------------------------------------
# Download from the ProtonVPN public apt repository and extract (no dpkg needed).
# The system still needs proton-vpn-daemon + NetworkManager for VPN connections.

CLI_VERSION=$(python3 -c "
import json
print(json.load(open('${SCRIPT_DIR}/src/version.json'))['cli_version_tested_max'])
")
info "Bundling ProtonVPN CLI v${CLI_VERSION}..."

CLI_DIR="${APPDIR}/usr/share/protonvpn"
DEB_CACHE="${BUILD_ROOT}/protonvpn-debs"
DEB_EXTRACT="${BUILD_ROOT}/protonvpn-extracted"
mkdir -p "${CLI_DIR}" "${DEB_CACHE}" "${DEB_EXTRACT}"

DEB_URLS="${BUILD_ROOT}/protonvpn-deb-urls.txt"
info "Resolving package URLs from ProtonVPN apt repo..."
python3 "${SCRIPT_DIR}/appimage/fetch-cli-debs.py" "${CLI_VERSION}" "${DEB_ARCH}" > "${DEB_URLS}"

while IFS= read -r url; do
    dest="${DEB_CACHE}/$(basename "${url}")"
    if [[ ! -f "${dest}" ]]; then
        info "Downloading $(basename "${url}")..."
        wget -q --show-progress -O "${dest}" "${url}"
    fi
done < "${DEB_URLS}"

extract_deb() {
    local deb="$1" dest="$2"
    local work
    work=$(mktemp -d)
    pushd "${work}" > /dev/null
    ar x "${deb}"
    for data_tar in data.tar.*; do
        tar -xf "${data_tar}" -C "${dest}"
    done
    popd > /dev/null
    rm -rf "${work}"
}

# Extract in two passes: api-core last so its session helpers take precedence.
while IFS= read -r url; do
    name=$(basename "${url}")
    if [[ "${name}" != python3-proton-vpn-api-core* ]]; then
        extract_deb "${DEB_CACHE}/${name}" "${DEB_EXTRACT}"
    fi
done < "${DEB_URLS}"
while IFS= read -r url; do
    name=$(basename "${url}")
    if [[ "${name}" == python3-proton-vpn-api-core* ]]; then
        extract_deb "${DEB_CACHE}/${name}" "${DEB_EXTRACT}"
    fi
done < "${DEB_URLS}"

if [[ -d "${DEB_EXTRACT}/usr/lib/python3/dist-packages" ]]; then
    mkdir -p "${CLI_DIR}/dist-packages"
    cp -r "${DEB_EXTRACT}/usr/lib/python3/dist-packages/." "${CLI_DIR}/dist-packages/"
fi

# -- Bundle PyGObject (gi) from system Python ----------------------------------
# The ProtonVPN CLI imports 'gi' at startup for its NetworkManager backend.
# gi cannot be pip-installed portably; we copy it from the system's Python 3
# package.  It must be in AppDir NOW so that linuxdeploy picks up its native
# dependency: libgirepository.
# The system Python is asked where each package lives because distros install
# them in different places: /usr/lib/python3/dist-packages on Debian/Ubuntu,
# /usr/lib/python3.X/site-packages on Arch and Fedora.
# gi is required: without it every CLI command fails with "No module named
# 'gi'", so a build that cannot find it must stop here.  cairo is optional;
# nothing in the ProtonVPN packages imports it.
system_py_package_dir() {
    python3 -c "import importlib.util, os; s = importlib.util.find_spec('$1'); print(os.path.dirname(s.origin) if s and s.origin else '')"
}
for gi_pkg in gi cairo; do
    src=$(system_py_package_dir "${gi_pkg}")
    if [[ -n "${src}" && -d "${src}" ]]; then
        info "Copying system Python package: ${gi_pkg} (${src})"
        cp -r "${src}" "${CLI_DIR}/dist-packages/"
    elif [[ "${gi_pkg}" == "gi" ]]; then
        die "System Python package not found: gi (install python3-gi on Debian/Ubuntu, python-gobject on Arch)"
    else
        warn "System Python package not found: ${gi_pkg} (install python3-gi-cairo on Debian/Ubuntu, python-cairo on Arch)"
    fi
done

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

# -- Download linuxdeploy tools (cached after first run) -----------------------
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

# -- Bundle Qt + gi shared libraries via linuxdeploy --------------------------
# python-build-standalone is NOT yet in AppDir.  That is deliberate: its own
# libssl.so.3 (newer than Ubuntu's) must not overwrite the libssl that Qt was
# compiled against.  linuxdeploy will bundle Ubuntu's libssl.so.3 here; the
# bundled Python later uses its own copy found via RPATH.
# QMAKE was already set above (aqt-installed Qt); linuxdeploy-plugin-qt queries
# it to locate Qt's own libs/plugins to bundle.
info "Bundling Qt and gi libraries..."

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

# linuxdeploy-plugin-qt skips the Wayland platform plugin on headless CI runners.
# EXTRA_PLATFORM_PLUGINS uses a naming convention that doesn't match the
# libqwayland.so filename, so we copy it manually instead along with any of its
# shared library dependencies (e.g. libQt6WaylandClient) not already bundled.
# The aqt-installed Qt's own plugin dir is checked first; the /usr/lib* paths
# remain as a fallback for local builds against a system-installed Qt.
for _wayland_dir in \
    "${QT_DIR}/plugins/platforms" \
    "/usr/lib/${MULTIARCH}/qt6/plugins/platforms" \
    "/usr/lib/qt6/plugins/platforms" \
    "/usr/lib64/qt6/plugins/platforms"; do
    if [[ -f "${_wayland_dir}/libqwayland.so" ]]; then
        cp "${_wayland_dir}/libqwayland.so" "${APPDIR}/usr/plugins/platforms/"
        info "Bundled Wayland platform plugin (${_wayland_dir})"

        # Copy shared library dependencies of libqwayland.so not already bundled.
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

        # Copy Wayland shell integration and graphics plugins.  These live in
        # sibling directories of platforms/ and are required for the Wayland
        # compositor to negotiate a display protocol (xdg-shell, etc.).
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

# -- Explicitly bundle Qt SSL libraries ----------------------------------------
# Qt loads libssl at runtime through its OpenSSL backend plugin
# (libqopensslbackend.so), not as a link-time ELF dependency, so linuxdeploy
# does not pick it up automatically.  We locate and copy both libraries here.
# python-build-standalone's Python finds its own libssl via DT_RPATH
# ($ORIGIN/../lib), which takes precedence over LD_LIBRARY_PATH, so there is
# no conflict between the two copies.
info "Bundling Qt SSL libraries..."
for lib in libssl.so.3 libcrypto.so.3; do
    src=$(ldconfig -p 2>/dev/null | awk -v lib="${lib}" '$1 == lib {print $NF; exit}' || true)
    if [[ -n "${src}" && -f "${src}" ]]; then
        cp -n "${src}" "${APPDIR}/usr/lib/"
        info "  Bundled: ${lib} (${src})"
    else
        warn "  ${lib} not found via ldconfig; Qt TLS will not work"
    fi
done

# -- Portable Python (python-build-standalone) ---------------------------------
# Copied AFTER linuxdeploy so its libssl.so.3 stays in python/lib/ and does not
# replace the Ubuntu libssl.so.3 that linuxdeploy placed in usr/lib/ for Qt.
# The Python binary finds its own libs via RPATH ($ORIGIN/../lib).
PYTHON_STANDALONE_DIR="${CLI_DIR}/python"
PYTHON_CACHE="${BUILD_ROOT}/python-standalone"

# Match the python-build-standalone version to the CI system's Python.
# python3-gi (and other native extensions) are compiled for the system Python;
# the bundled interpreter must be the same major.minor so extensions load correctly.
SYSTEM_PY_VER=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
info "System Python: ${SYSTEM_PY_VER}, fetching matching python-build-standalone"

if [[ ! -d "${PYTHON_CACHE}/python" ]]; then
    info "Resolving latest python-build-standalone release..."
    PBS_URL=$(python3 -c "
import urllib.request, json, sys
py_ver = f'{sys.version_info.major}.{sys.version_info.minor}'
req = urllib.request.Request(
    'https://api.github.com/repos/indygreg/python-build-standalone/releases/latest',
    headers={'User-Agent': 'build-appimage'})
with urllib.request.urlopen(req, timeout=30) as r:
    data = json.loads(r.read())
for asset in data['assets']:
    n = asset['name']
    if (f'cpython-{py_ver}' in n and 'linux-gnu-install_only' in n and
            any(p in n for p in (${PBS_ARCH_PREFIXES})) and
            n.endswith('.tar.gz')):
        print(asset['browser_download_url'])
        break
")
    [[ -n "${PBS_URL}" ]] || die "Could not find python-build-standalone ${SYSTEM_PY_VER} asset"
    info "Downloading portable Python: $(basename "${PBS_URL}")..."
    mkdir -p "${PYTHON_CACHE}"
    wget -q --show-progress -O "${PYTHON_CACHE}/python.tar.gz" "${PBS_URL}"
    tar -xzf "${PYTHON_CACHE}/python.tar.gz" -C "${PYTHON_CACHE}"
fi

info "Installing portable Python into AppDir..."
mkdir -p "${PYTHON_STANDALONE_DIR}"
cp -r "${PYTHON_CACHE}/python/." "${PYTHON_STANDALONE_DIR}/"
BUNDLED_PYTHON="${PYTHON_STANDALONE_DIR}/bin/python${SYSTEM_PY_VER}"
[[ -x "${BUNDLED_PYTHON}" ]] || die "Bundled Python not found at ${BUNDLED_PYTHON}"

PY_VER=$("${BUNDLED_PYTHON}" -c \
    "import sys; print(f'python{sys.version_info.major}.{sys.version_info.minor}')")
info "Bundled $("${BUNDLED_PYTHON}" --version)"

# -- Create venv with PyPI dependencies ----------------------------------------
VENV="${CLI_DIR}/venv"
"${BUNDLED_PYTHON}" -m venv "${VENV}"
"${VENV}/bin/pip" install --quiet --upgrade pip
"${VENV}/bin/pip" install --quiet \
    click dbus-fast tabulate packaging \
    aiohttp bcrypt python-gnupg pyOpenSSL requests "importlib-metadata" \
    keyring secretstorage cryptography distro fido2 Jinja2 PyNaCl sentry-sdk

VENV_PROTONVPN="${VENV}/bin/protonvpn"
if [[ -f "${VENV_PROTONVPN}" ]]; then
    mv "${VENV_PROTONVPN}" "${VENV_PROTONVPN}.orig"
fi

# -- Create protonvpn launcher -------------------------------------------------
# PYTHONHOME: bundled Python finds its own stdlib.
# PYTHONPATH: ProtonVPN .deb packages + PyPI venv site-packages + system
#             Python dist-packages (for gi, which was bundled from the system).
# The bundled Python uses its own RPATH-resolved OpenSSL; no LD_LIBRARY_PATH
# manipulation is needed.
# sys.argv[0] is set because the CLI names itself after it: under "python -c"
# it would otherwise be "-c", which then appears in the CLI's own messages
# ("Please sign in with '-c signin'").
cat > "${CLI_DIR}/protonvpn" << EOF
#!/bin/bash
PROTON_DIR="\${APPDIR}/usr/share/protonvpn"
export PYTHONHOME="\${PROTON_DIR}/python"
VENV_SITE="\${PROTON_DIR}/venv/lib/${PY_VER}/site-packages"
PROTON_PKG="\${PROTON_DIR}/dist-packages"
export PYTHONPATH="\${PROTON_PKG}:\${VENV_SITE}"
exec "\${PROTON_DIR}/python/bin/python${SYSTEM_PY_VER}" -c "import sys; sys.argv[0] = 'protonvpn'; from proton.vpn.cli import main; main()" "\$@"
EOF
chmod +x "${CLI_DIR}/protonvpn"

info "ProtonVPN CLI v${CLI_VERSION} bundled (${CLI_DIR})"

# linuxdeploy may have overwritten AppRun, so restore ours.
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
