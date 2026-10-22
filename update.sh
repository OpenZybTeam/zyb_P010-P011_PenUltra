#!/bin/sh
# Z03Ultra one-shot updater. Fetches the latest install.sh in memory and runs
# it, so nothing is left on the device. Mirrors are supported with --mirror.
set -u

REPO="${Z03ULTRA_REPO:-OpenZybTeam/zyb_P010-P011_PenUltra}"
MIRROR=${Z03ULTRA_MIRROR:-0}
for arg in "$@"; do
    case "$arg" in
        --mirror) MIRROR=1 ;;
        --direct) MIRROR=0 ;;
    esac
done

BASE="https://github.com/$REPO/releases/latest/download/install.sh"
[ "$MIRROR" = 1 ] && BASE="https://v4.gh-proxy.org/$BASE"

log() { echo "[z03ultra] $*"; }
die() { echo "[z03ultra] $*" >&2; exit 1; }

command -v curl >/dev/null 2>&1 || die "缺少 curl"

log "更新源: $( [ "$MIRROR" = 1 ] && echo 镜像 || echo 直连 )"
TMPF=$(mktemp /tmp/z03ultra-update.XXXXXX) || die "无法创建临时文件"
trap 'rm -f "$TMPF"' EXIT INT TERM
wget -O "$TMPF" "$BASE" \
    || curl -fLk --http1.1 --progress-bar --connect-timeout 20 "$BASE" -o "$TMPF" \
    || die "下载安装脚本失败，可改用 --direct/--mirror 重试"
[ -s "$TMPF" ] || die "安装脚本内容为空"

sh "$TMPF" "$@"
