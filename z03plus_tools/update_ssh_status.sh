#!/bin/sh
# Refreshes the status consumed by the QML page. Safe to run without root.
set -eu
STATE_DIR=/home/user/z03plus
BASE="$STATE_DIR/ssh"
STATUS_FILE="$STATE_DIR/ssh-status.txt"
CONF="$BASE/sshd_config"
PORT=$(awk '/^[[:space:]]*Port[[:space:]]+[0-9]+/ { print $2; exit }' "$CONF" 2>/dev/null || true)
[ -n "$PORT" ] || PORT=22

IP=$(busybox ifconfig wlan0 2>/dev/null | awk '/inet addr:/ { sub(/^.*inet addr:/, ""); sub(/[[:space:]].*$/, ""); print; exit }')
[ -n "$IP" ] || IP="未连接 Wi-Fi"
if [ -s "$BASE/sshd.pid" ] && kill -0 "$(cat "$BASE/sshd.pid")" 2>/dev/null; then RUNNING=on; else RUNNING=off; fi
if [ -s /home/user/.ssh/authorized_keys ]; then KEY_STATUS="已配置"; else KEY_STATUS="未配置"; fi
if [ ! -f "$CONF" ]; then ROOT_STATUS="未配置"
elif grep -qs '^[[:space:]]*PermitRootLogin[[:space:]]*no' "$CONF"; then ROOT_STATUS="已禁用"
else ROOT_STATUS="已允许"; fi

umask 077
mkdir -p "$STATE_DIR"
cat > "$STATUS_FILE" <<EOF
running=$RUNNING
ip=$IP
port=$PORT
user=user
key_status=$KEY_STATUS
root_login=$ROOT_STATUS
command=ssh user@$IP -p $PORT
EOF
chmod 600 "$STATUS_FILE"
