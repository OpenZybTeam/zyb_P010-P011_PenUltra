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
MIRROR_PREFIX="https://gh-proxy.com/"
FILES="libZ03Plus.so z03plus.rcc Z03Plus setup_ssh.sh update_ssh_status.sh"
INIT=/etc/init.d/zybz03qt-init
HOOK_LINE='Z=/home/user/z03plus/Z03Plus; L=/home/user/z03plus/libZ03Plus.so; [ -f "$Z" ] && [ -f "$L" ] && ! grep -q " /usr/bin/Z03 " /proc/mounts && mount --bind "$Z" /usr/bin/Z03'

log() { echo "[z03ultra] $*"; }
url() {
    if [ "$MIRROR" = 1 ]; then
        printf '%s%s' "$MIRROR_PREFIX" "$1"
    else
        printf '%s' "$1"
    fi
}
die() { echo "[z03ultra] $*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || die "请用 root 执行"
command -v curl >/dev/null 2>&1 || die "缺少 curl"
command -v sha256sum >/dev/null 2>&1 || die "缺少 sha256sum"
[ -d "$DEST" ] || mkdir -p "$DEST" || die "无法创建 $DEST"

TMP=${TMPDIR:-/tmp}/z03ultra-install.$$
mkdir -p "$TMP" || die "无法创建临时目录"
trap 'rm -rf "$TMP"' EXIT INT TERM

download() {
    curl -fsSL --connect-timeout 15 --max-time "$3" -o "$2" "$1" && return 0
    curl -fsSLk --connect-timeout 15 --max-time "$3" -o "$2" "$1" && return 0
    return 1
}

# 逐个下载并显示进度条；单文件独立连接，避免镜像长连接复用导致的卡死。
fetch_asset() {
    name=$1
    src=$2
    dst=$3
    printf '[z03ultra] 下载 %s\n' "$name"
    curl -fL --progress-bar --connect-timeout 15 --max-time 300 "$src" -o "$dst" && return 0
    curl -fLk --progress-bar --connect-timeout 15 --max-time 300 "$src" -o "$dst" && return 0
    return 1
}

TAG=
if [ -n "${Z03ULTRA_BASE:-}" ]; then
    BASE=$Z03ULTRA_BASE
else
    log "查询最新 release: $REPO（$( [ "$MIRROR" = 1 ] && echo 镜像 || echo 直连 )）"
    if download "$(url "https://api.github.com/repos/$REPO/releases/latest")" "$TMP/release.json" 30; then
        BASE=$(grep -o '"browser_download_url": *"[^"]*SHA256SUMS"' "$TMP/release.json" \
            | head -n 1 \
            | sed 's/.*"\(http[^"]*\)".*/\1/' \
            | sed 's#/SHA256SUMS$##')
    fi
    if [ -z "${BASE:-}" ]; then
        log "API 不可用，改用 releases/latest 跳转"
        TAG=$(curl -fsSI --connect-timeout 10 "$(url "https://github.com/$REPO/releases/latest")" 2>/dev/null \
            | tr -d '\r' | awk 'tolower($1) == "location:" { print $2 }' | sed 's#.*/tag/##')
        [ -n "$TAG" ] || TAG=$(curl -fsSIk --connect-timeout 10 "$(url "https://github.com/$REPO/releases/latest")" 2>/dev/null \
            | tr -d '\r' | awk 'tolower($1) == "location:" { print $2 }' | sed 's#.*/tag/##')
        [ -n "$TAG" ] && BASE="https://github.com/$REPO/releases/download/$TAG"
    fi
    [ -n "${BASE:-}" ] || die "无法获取最新 release，可设置 Z03ULTRA_BASE 指向下载目录"
fi
log "下载源: $(url "$BASE")"

log "下载资产: $FILES"
fetch_asset SHA256SUMS "$(url "$BASE/SHA256SUMS")" "$TMP/SHA256SUMS" || die "下载 SHA256SUMS 失败"
for f in $FILES; do
    fetch_asset "$f" "$(url "$BASE/$f")" "$TMP/$f" || die "下载 $f 失败"
done

for f in $FILES; do
    want=$(grep " $f\$" "$TMP/SHA256SUMS" | awk '{print $1}' | head -n 1)
    got=$(sha256sum "$TMP/$f" | awk '{print $1}')
    [ -n "$want" ] || die "SHA256SUMS 中没有 $f"
    [ "$want" = "$got" ] || die "$f 校验失败"
    log "已校验 $f"
done

grep -q 'libZ03Plus.so' "$TMP/Z03Plus" || die "Z03Plus 未包含 DT_NEEDED 依赖"

mkdir -p "$DEST/backup"
for f in $FILES; do
    [ -f "$DEST/$f" ] && cp -f "$DEST/$f" "$DEST/backup/$f"
done

for f in $FILES; do
    cp -f "$TMP/$f" "$DEST/$f" || die "写入 $DEST/$f 失败"
done
chmod 755 "$DEST/libZ03Plus.so" "$DEST/Z03Plus" "$DEST/setup_ssh.sh" "$DEST/update_ssh_status.sh"
chmod 644 "$DEST/z03plus.rcc"
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
if ! grep -q 'mount --bind' "$INIT" 2>/dev/null; then
    [ -f "$INIT" ] || die "找不到 $INIT"
    if [ ! -f "$DEST/zybz03qt-init.orig" ]; then
        cp -a "$INIT" "$DEST/zybz03qt-init.orig" || die "备份 $INIT 失败"
    fi
    awk -v hook="$HOOK_LINE" '
        /z03ultra-boot\.sh/ { next }
        /^\/usr\/bin\/zyb_z03_launch\.sh &/ && !done { print hook; done = 1 }
        { print }
    ' "$DEST/zybz03qt-init.orig" > "$INIT.z03ultra.new" || die "生成启动钩子失败"
    chmod 755 "$INIT.z03ultra.new"
    mv -f "$INIT.z03ultra.new" "$INIT" || die "替换 $INIT 失败"
    log "已写入持久化启动钩子"
else
    log "启动钩子已存在，跳过"
fi
sync
mount -o remount,ro / 2>/dev/null || true

[ -f "$DEST/Z03Plus" ] && [ -f "$DEST/libZ03Plus.so" ] && ! grep -q " /usr/bin/Z03 " /proc/mounts && mount --bind "$DEST/Z03Plus" /usr/bin/Z03 2>/dev/null
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
    HOME=/home XDG_RUNTIME_DIR=/run/user/0 nohup $D "$DEST/Z03Plus" -platform wayland \
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
