#!/usr/bin/env bash
#
# Build Z03Plus for ARMv7 hard-float glibc 2.27 (Z03 / P010 / P011).
#
# Usage:
#   ./build_armhf.sh [path-to-original-Z03]
#
# Environment:
#   CROSS=arm-linux-gnueabihf-      cross toolchain prefix
#   RCC=/usr/bin/rcc                Qt resource compiler (must support v2)
#   MOC=/path/to/moc                moc 5.10.x, only needed with REGEN_MOC=1
#   REGEN_MOC=1                     regenerate z03plus_overlay.moc
#   PKG_CONFIG_LIBDIR=...           armhf pkg-config path
#   LIBDL_DIR=...                   dir containing libdl.so.2 with dlsym@GLIBC_2.4
#
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD="$ROOT/build"
CROSS=${CROSS:-arm-linux-gnueabihf-}
CXX="${CROSS}g++"
READELF="${CROSS}readelf"
OBJDUMP="${CROSS}objdump"
RCC=${RCC:-rcc}
MOC=${MOC:-moc}
Z03_SOURCE=${1:-}
DEVICE_LIB=/home/user/z03plus/libZ03Plus.so

export PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR:-/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig}

die() { printf 'error: %b\n' "$*" >&2; exit 1; }

for tool in "$CXX" "$READELF" "$OBJDUMP" "$RCC"; do
    command -v "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done

mkdir -p "$BUILD"

if [[ "${REGEN_MOC:-0}" == "1" ]]; then
    command -v "$MOC" >/dev/null 2>&1 || die "missing moc: $MOC"
    MOC_OUT="$BUILD/z03plus_overlay.moc.new"
    "$MOC" "$ROOT/z03plus_overlay/z03plus_overlay.cpp" -o "$MOC_OUT"
    revision=$(awk '/\/\/ content:/{seen=1; next} seen && NF {gsub(/,/, ""); print $1; exit}' "$MOC_OUT")
    [[ "$revision" == "7" ]] || die "moc revision is '$revision', need 7 (Qt 5.10.x)"
    mv "$MOC_OUT" "$ROOT/z03plus_overlay/z03plus_overlay.moc"
fi

[[ -f "$ROOT/z03plus_overlay/z03plus_overlay.moc" ]] || die "z03plus_overlay.moc missing"

SHIM="$BUILD/dlsym_glibc24.h"
cat > "$SHIM" <<'EOF'
#ifdef __cplusplus
extern "C" {
#endif
void *z03_dlsym(void *handle, const char *symbol)
#ifdef __cplusplus
    noexcept
#endif
    ;
#ifdef __cplusplus
}
#endif
__asm__(".symver z03_dlsym,dlsym@GLIBC_2.4");
#define dlsym z03_dlsym
/* glibc 2.32+ exports this flag and newer libstdc++ headers consult it from
   inline COW-string helpers. Device glibc is 2.27, so provide a local copy
   fixed at 0 (multi-threaded) to keep atomics enabled and avoid the
   GLIBC_2.32 symbol dependency. */
#ifdef __cplusplus
extern "C" {
#endif
char __libc_single_threaded = 0;
#ifdef __cplusplus
}
#endif
EOF

PKG_CONFIG=${PKG_CONFIG:-pkg-config}
command -v "$PKG_CONFIG" >/dev/null 2>&1 || die "missing pkg-config: $PKG_CONFIG"
QT_CORE_FLAGS=$("$PKG_CONFIG" --cflags Qt5Core)
QT_CORE_LIBS=$("$PKG_CONFIG" --libs Qt5Core)
QT_OVERLAY_CFLAGS=$("$PKG_CONFIG" --cflags Qt5Core Qt5Qml Qt5Network)
QT_OVERLAY_LIBS=$("$PKG_CONFIG" --libs Qt5Core Qt5Qml Qt5Network)

LIBDL_DIR=${LIBDL_DIR:-}
if [[ -z "$LIBDL_DIR" ]]; then
    for candidate in /tmp/opencode/glibc228/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf; do
        if "$OBJDUMP" -T "$candidate/libdl.so.2" 2>/dev/null | grep -q 'GLIBC_2.4.*dlsym'; then
            LIBDL_DIR=$candidate
            break
        fi
    done
fi
[[ -n "$LIBDL_DIR" ]] || die "need libdl.so.2 exporting dlsym@GLIBC_2.4; set LIBDL_DIR (device glibc keeps dlsym in libdl.so.2)"
LIBDL_LINK=(-L"$LIBDL_DIR" -Wl,--no-as-needed -l:libdl.so.2)

COMMON=(-std=c++11 -O2 -fPIC -shared -include "$SHIM" -Wl,--no-undefined
        -static-libstdc++ -static-libgcc)

echo "[1/3] libZ03Plus.so"
"$CXX" "${COMMON[@]}" -Wl,-soname,libZ03Plus.so $QT_OVERLAY_CFLAGS \
    "$ROOT/z03plus_overlay/z03plus_overlay.cpp" \
    -o "$BUILD/libZ03Plus.so" "${LIBDL_LINK[@]}" $QT_OVERLAY_LIBS

echo "[2/3] libZ03PlusExporter.so"
"$CXX" "${COMMON[@]}" -Wl,-soname,libZ03PlusExporter.so $QT_CORE_FLAGS \
    "$ROOT/z03plus_exporter/z03plus_exporter.cpp" \
    -o "$BUILD/libZ03PlusExporter.so" "${LIBDL_LINK[@]}" $QT_CORE_LIBS

echo "[3/3] z03plus.rcc"
"$RCC" --binary --format-version 2 --no-compress \
    "$ROOT/z03plus_overlay/z03plus.qrc" -o "$BUILD/z03plus.rcc"

check_glibc() {
    local lib=$1 bad
    bad=$("$OBJDUMP" -T "$lib" | grep -E 'GLIBC_2\.(2[89]|[3-9][0-9])' || true)
    [[ -z "$bad" ]] || die "glibc newer than 2.27 in $lib:\n$bad"
}

for lib in "$BUILD/libZ03Plus.so" "$BUILD/libZ03PlusExporter.so"; do
    "$READELF" -h "$lib" | grep -q 'Machine:.*ARM' || die "not an ARM object: $lib"
    check_glibc "$lib"
    "$READELF" -d "$lib" | grep -q 'libdl.so.2' || die "missing DT_NEEDED libdl.so.2: $lib"
    "$READELF" -V "$lib" | grep -A2 'libdl.so.2' | grep -q 'GLIBC_2.4' \
        || die "dlsym is not bound to libdl.so.2@GLIBC_2.4: $lib"
done

rcc_magic=$(od -An -tx1 -N8 "$BUILD/z03plus.rcc" | tr -d ' \n')
[[ "$rcc_magic" == "7172657300000002" ]] || die "z03plus.rcc is not qres format v2"

if [[ -n "$Z03_SOURCE" ]]; then
    [[ -f "$Z03_SOURCE" ]] || die "Z03 binary not found: $Z03_SOURCE"
    command -v patchelf >/dev/null 2>&1 || die "missing patchelf"
    cp "$Z03_SOURCE" "$BUILD/Z03Plus"
    if ! patchelf --print-needed "$BUILD/Z03Plus" | grep -qxF "$DEVICE_LIB"; then
        patchelf --add-needed "$DEVICE_LIB" "$BUILD/Z03Plus"
    fi
    echo "[extra] prepared $BUILD/Z03Plus with NEEDED $DEVICE_LIB"
fi

echo
ls -l "$BUILD/libZ03Plus.so" "$BUILD/libZ03PlusExporter.so" "$BUILD/z03plus.rcc"
echo "ok"
