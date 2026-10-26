#!/bin/sh
# Z03Ultra one-shot installer / updater.
# Downloads the latest GitHub release, verifies every artifact, installs it to
# /home/user/z03plus and adds the persistent bind-mount hook to zybz03qt-init.
set -u

REPO="${Z03ULTRA_REPO:-OpenZybTeam/zyb_P010-P011_PenUltra}"
DEST="${Z03ULTRA_DEST:-/home/user/z03plus}"
MIRROR=${Z03ULTRA_MIRROR:-0}
for arg in "$@"; do
    case "$arg" in
        --mirror) MIRROR=1 ;;
        --direct) MIRROR=0 ;;
    esac
done
MIRROR_PREFIX="https://git.yylx.win/"
PACKAGE=z03ultra.zip
FILES="libZ03Plus.so z03plus.rcc Z03Plus Z03PlusCef Z03CefLaunch z03cef_host libZ03Cef.so libZ03GbmShim.so libZ03CrashRep.so setup_ssh.sh update_ssh_status.sh"
INIT=/etc/init.d/zybz03qt-init
HOOK_LINE='Z=/home/user/z03plus/Z03Plus; C=/home/user/z03plus/Z03PlusCef; W=/home/user/z03plus/Z03CefLaunch; F=/home/user/z03plus/cef/libcef.so; O=/home/user/z03plus/.cef-ok; L=/home/user/z03plus/libZ03Plus.so; B=$Z; [ -f "$C" ] && [ -f "$F" ] && [ -f "$O" ] && B=$C; [ -f "$W" ] && [ "$B" = "$C" ] && B=$W; [ -f "$B" ] && [ -f "$L" ] && umount -l /usr/bin/Z03 2>/dev/null; [ -f "$B" ] && [ -f "$L" ] && mount --bind "$B" /usr/bin/Z03'

log() { echo "[z03ultra] $*"; }
url() {
    if [ "$MIRROR" = 1 ]; then
        u=$1
        case "$u" in
            https://*) u=${u#https://} ;;
            http://*) u=${u#http://} ;;
        esac
        printf '%s%s' "$MIRROR_PREFIX" "$u"
    else
        printf '%s' "$1"
    fi
}
die() { echo "[z03ultra] $*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || die "请用 root 执行"
command -v curl >/dev/null 2>&1 || die "缺少 curl"
command -v sha256sum >/dev/null 2>&1 || die "缺少 sha256sum"
[ -d "$DEST" ] || mkdir -p "$DEST" || die "无法创建 $DEST"

mkdir -p "$DEST/tmp" || die "无法创建 $DEST/tmp"
TMP="$DEST/tmp/z03ultra-install.$$"
mkdir -p "$TMP" || die "无法创建临时目录"
trap 'rm -rf "$TMP"' EXIT INT TERM

download() {
    curl -fsSL --connect-timeout 15 --max-time "$3" -o "$2" "$1" && return 0
    curl -fsSLk --connect-timeout 15 --max-time "$3" -o "$2" "$1" && return 0
    return 1
}

report_progress() {
    sz=0
    if [ -f "$1" ]; then
        sz=$(wc -c < "$1" 2>/dev/null | tr -d ' ')
    fi
    [ -n "$sz" ] || sz=0
    if [ -n "$2" ] && [ "$2" -gt 0 ] 2>/dev/null; then
        awk -v s="$sz" -v t="$2" 'BEGIN { printf "\r[z03ultra]   %.1f / %.1f MB  %.0f%%", s/1048576, t/1048576, s*100/t }'
    else
        awk -v s="$sz" 'BEGIN { printf "\r[z03ultra]   %.1f MB", s/1048576 }'
    fi
    [ "${3:-}" = 1 ] && printf '\n'
}

asset_total() {
    curl -fsSLk --connect-timeout 5 --max-time 10 -r 0-0 -D - -o /dev/null "$1" 2>/dev/null \
        | tr -d '\r' \
        | awk 'tolower($1) == "content-range:" { split($3, a, "/"); print a[2] }'
}

# 逐个下载：curl 静默传输，脚本每 3 秒自查文件大小并输出进度。
fetch_asset() {
    name=$1
    src=$2
    dst=$3
    printf '[z03ultra] 下载 %s\n' "$name"
    total=$(asset_total "$src")
    case "$total" in
        ''|*[!0-9]*) total= ;;
    esac
    n=0
    max=5
    [ -n "${4:-}" ] && max=1
    while [ "$n" -lt "$max" ]; do
        n=$((n + 1))
        curl -fsSLk -C - -o "$dst" "$src" 2>/dev/null &
        pid=$!
        while kill -0 "$pid" 2>/dev/null; do
            sleep 3
            kill -0 "$pid" 2>/dev/null || break
            report_progress "$dst" "${total:-}"
        done
        wait "$pid"
        rc=$?
        report_progress "$dst" "${total:-}" 1
        [ "$rc" = 0 ] && return 0
        if [ "$rc" = 33 ] || [ "$rc" = 22 ]; then
            : > "$dst"
        fi
        [ "$n" -lt "$max" ] && printf '[z03ultra] %s 未完成，续传中（第 %s 次）\n' "$name" "$n"
        sleep 2
    done
    if [ -n "${4:-}" ]; then
        printf '[z03ultra] %s 镜像不可用，改直连重试\n' "$name"
        : > "$dst"
        fetch_asset "$name" "$4" "$dst" && return 0
    fi
    return 1
}

fetch_fresh() {
    : > "$3"
    fetch_asset "$1" "$2" "$3"
}

verify_asset() {
    want=$(grep " $1\$" "$TMP/SHA256SUMS" | awk '{print $1}' | head -n 1)
    [ -n "$want" ] || return 2
    got=$(sha256sum "$TMP/$1" | awk '{print $1}')
    [ "$want" = "$got" ]
}

unzip_cmd() {
    if command -v unzip >/dev/null 2>&1; then
        unzip -o -q "$1" -d "$2"
    elif command -v busybox >/dev/null 2>&1 && busybox --list 2>/dev/null | grep -qx unzip; then
        busybox unzip -o -q "$1" -d "$2"
    else
        return 127
    fi
}

TAG=
if [ -n "${Z03ULTRA_BASE:-}" ]; then
    BASE=$Z03ULTRA_BASE
else
    log "查询最新 release: $REPO（$( [ "$MIRROR" = 1 ] && echo 镜像 || echo 直连 )）"
    if download "https://api.github.com/repos/$REPO/releases/latest" "$TMP/release.json" 30; then
        BASE=$(grep -o '"browser_download_url": *"[^"]*SHA256SUMS"' "$TMP/release.json" \
            | head -n 1 \
            | sed 's/.*"\(http[^"]*\)".*/\1/' \
            | sed 's#/SHA256SUMS$##')
    fi
    if [ -z "${BASE:-}" ]; then
        log "API 不可用，改用 releases/latest 跳转"
        TAG=$(curl -fsSI --connect-timeout 10 "https://github.com/$REPO/releases/latest" 2>/dev/null \
            | tr -d '\r' | awk 'tolower($1) == "location:" { print $2 }' | sed 's#.*/tag/##')
        [ -n "$TAG" ] || TAG=$(curl -fsSIk --connect-timeout 10 "https://github.com/$REPO/releases/latest" 2>/dev/null \
            | tr -d '\r' | awk 'tolower($1) == "location:" { print $2 }' | sed 's#.*/tag/##')
        [ -n "$TAG" ] && BASE="https://github.com/$REPO/releases/download/$TAG"
    fi
    [ -n "${BASE:-}" ] || die "无法获取最新 release，可设置 Z03ULTRA_BASE 指向下载目录"
fi
log "下载源: $(url "$BASE")"

can_unzip() {
    command -v unzip >/dev/null 2>&1 && return 0
    command -v busybox >/dev/null 2>&1 && busybox --list 2>/dev/null | grep -qx unzip
}

fetch_asset SHA256SUMS "$(url "$BASE/SHA256SUMS")" "$TMP/SHA256SUMS" "$BASE/SHA256SUMS" || die "下载 SHA256SUMS 失败"

if can_unzip; then
    log "下载资产: $PACKAGE"
    fetch_asset "$PACKAGE" "$(url "$BASE/$PACKAGE")" "$TMP/$PACKAGE" "$BASE/$PACKAGE" || die "下载 $PACKAGE 失败"
    verify_asset "$PACKAGE" || {
        verify_asset "$PACKAGE"
        [ "$?" = 2 ] && die "SHA256SUMS 中没有 $PACKAGE"
        log "$PACKAGE 校验失败，重新下载"
        fetch_fresh "$PACKAGE" "$(url "$BASE/$PACKAGE")" "$TMP/$PACKAGE" "$BASE/$PACKAGE" || die "下载 $PACKAGE 失败"
        verify_asset "$PACKAGE" || die "$PACKAGE 校验失败"
    }
    log "已校验 $PACKAGE"
    SRC="$TMP/pkg"
    mkdir -p "$SRC" || die "无法创建解压目录"
    unzip_cmd "$TMP/$PACKAGE" "$SRC" || die "解压 $PACKAGE 失败"
else
    log "设备没有 unzip，改用单文件下载"
    for f in $FILES; do
        fetch_asset "$f" "$(url "$BASE/$f")" "$TMP/$f" "$BASE/$f" || die "下载 $f 失败"
        verify_asset "$f" || {
            verify_asset "$f"
            [ "$?" = 2 ] && die "SHA256SUMS 中没有 $f"
            log "$f 校验失败，重新下载"
            fetch_fresh "$f" "$(url "$BASE/$f")" "$TMP/$f" || die "下载 $f 失败"
            verify_asset "$f" || die "$f 校验失败"
        }
        log "已校验 $f"
    done
    SRC="$TMP"
fi

for f in $FILES; do
    [ -f "$SRC/$f" ] || die "缺少 $f"
done

grep -q 'libZ03Plus.so' "$SRC/Z03Plus" || die "Z03Plus 未包含 DT_NEEDED 依赖"
grep -q 'libcef.so' "$SRC/Z03PlusCef" || die "Z03PlusCef 未包含 libcef 依赖"
cmp -s "$SRC/Z03Plus" "$SRC/Z03PlusCef" && die "Z03PlusCef 与 Z03Plus 相同"

mkdir -p "$DEST/backup"
for f in $FILES; do
    [ -f "$DEST/$f" ] && cp -f "$DEST/$f" "$DEST/backup/$f"
done

for f in $FILES; do
    cp -f "$SRC/$f" "$DEST/$f" || die "写入 $DEST/$f 失败"
done
chmod 755 "$DEST/libZ03Plus.so" "$DEST/Z03Plus" "$DEST/Z03PlusCef" "$DEST/Z03CefLaunch" "$DEST/z03cef_host" "$DEST/libZ03Cef.so" "$DEST/libZ03GbmShim.so" "$DEST/libZ03CrashRep.so" "$DEST/setup_ssh.sh" "$DEST/update_ssh_status.sh"
chmod 644 "$DEST/z03plus.rcc"
if [ -d "$DEST/cef" ]; then
    for f in libZ03Cef.so libZ03GbmShim.so z03cef_host; do
        [ -f "$DEST/$f" ] && cp -f "$DEST/$f" "$DEST/cef/$f" && chmod 755 "$DEST/cef/$f"
    done
fi
log "文件已安装到 $DEST"

if [ -f "$DEST/ssh/sshd_config" ] && [ -f "$DEST/setup_ssh.sh" ]; then
    log "重新应用 SSH 配置"
    if [ -s "$DEST/ssh/sshd.pid" ]; then
        kill "$(cat "$DEST/ssh/sshd.pid")" 2>/dev/null || true
        rm -f "$DEST/ssh/sshd.pid"
    fi
    sh "$DEST/setup_ssh.sh" >/dev/null 2>&1 || log "SSH 配置更新失败，可在 SSH 页手动重启"
fi

mount -o remount,rw / 2>/dev/null || die "rootfs 无法改为可写"
if ! grep -qF "$HOOK_LINE" "$INIT" 2>/dev/null; then
    [ -f "$INIT" ] || die "找不到 $INIT"
    if [ ! -f "$DEST/zybz03qt-init.orig" ]; then
        cp -a "$INIT" "$DEST/zybz03qt-init.orig" || die "备份 $INIT 失败"
    fi
    awk -v hook="$HOOK_LINE" '
        /z03ultra-boot\.sh/ { next }
        /^Z=\/home\/user\/z03plus\/Z03Plus; / { next }
        /^\/usr\/bin\/zyb_z03_launch\.sh &/ && !done { print hook; done = 1 }
        { print }
    ' "$DEST/zybz03qt-init.orig" > "$INIT.z03ultra.new" || die "生成启动钩子失败"
    chmod 755 "$INIT.z03ultra.new"
    mv -f "$INIT.z03ultra.new" "$INIT" || die "替换 $INIT 失败"
    log "已写入持久化启动钩子"
else
    log "启动钩子已是最新"
fi
sync
mount -o remount,ro / 2>/dev/null || true

BOOT_BIN="$DEST/Z03Plus"
[ -f "$DEST/Z03PlusCef" ] && [ -f "$DEST/cef/libcef.so" ] && [ -f "$DEST/.cef-ok" ] && BOOT_BIN="$DEST/Z03PlusCef"
[ "$BOOT_BIN" = "$DEST/Z03PlusCef" ] && [ -f "$DEST/Z03CefLaunch" ] && BOOT_BIN="$DEST/Z03CefLaunch"
umount -l /usr/bin/Z03 2>/dev/null
[ -f "$BOOT_BIN" ] && [ -f "$DEST/libZ03Plus.so" ] && mount --bind "$BOOT_BIN" /usr/bin/Z03 2>/dev/null
kill $(pidof Z03) 2>/dev/null
kill $(pidof Z03Plus) 2>/dev/null
GUARD=$(ps w | grep '[z]yb_z03_launch.sh' | awk '{print $1}' | head -n 1)
[ -n "$GUARD" ] && kill -CONT "$GUARD" 2>/dev/null
sleep 15

PID=$(pidof Z03)
[ -n "$PID" ] || PID=$(pidof Z03Plus)
if [ -z "$PID" ]; then
    log "守护循环未拉起应用，手动启动副本"
    cd /home || exit 1
    if command -v setsid >/dev/null 2>&1; then D=setsid; else D=nohup; fi
    HOME=/home XDG_RUNTIME_DIR=/run/user/0 QT_LOGGING_RULES="qt.virtualkeyboard*=true" \
        nohup $D "$BOOT_BIN" -platform wayland \
        >"$DEST/z03plus.out" 2>&1 </dev/null &
    sleep 12
    PID=$(pidof Z03)
    [ -n "$PID" ] || PID=$(pidof Z03Plus)
fi

[ -n "$PID" ] || die "应用未启动，查看 $DEST/z03plus.out"
if grep -q 'libZ03Plus.so' "/proc/$PID/maps" 2>/dev/null; then
    log "注入成功 pid=$PID"
    log "打开 设置 -> Z03Ultra 扩展"
else
    die "注入未生效，查看 $DEST/boot-inject.log 与 /proc/$PID/maps"
fi
