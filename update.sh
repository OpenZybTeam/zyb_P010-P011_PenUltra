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
TMPD="${Z03ULTRA_DEST:-/home/user/z03plus}/tmp"
mkdir -p "$TMPD" 2>/dev/null || TMPD=/tmp
TMPF=$(mktemp "$TMPD/z03ultra-update.XXXXXX" 2>/dev/null) || TMPF="$TMPD/z03ultra-update.$$"
trap 'rm -f "$TMPF"' EXIT INT TERM
curl -fsSLk --connect-timeout 20 --max-time 120 "$BASE" -o "$TMPF" \
    || die "下载安装脚本失败，可改用 --direct/--mirror 重试"
[ -s "$TMPF" ] || die "安装脚本内容为空"
log "安装脚本已获取（$(wc -c < "$TMPF" | tr -d ' ') 字节）"

sh "$TMPF" "$@"
