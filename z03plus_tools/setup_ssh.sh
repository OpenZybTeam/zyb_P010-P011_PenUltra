#!/bin/sh
# Run as root on the Z03 (for example: adb shell /home/user/z03plus/setup_ssh.sh).
# The Z03 root filesystem is read-only, so this intentionally uses the existing
# non-privileged `user` account and a writable, self-contained sshd config.
set -eu

USER_NAME=user
SSH_PORT="${SSH_PORT:-22}"
BASE=/home/user/z03plus/ssh
STATE_DIR=/home/user/z03plus
CONF="$BASE/sshd_config"
START="$BASE/start-sshd.sh"
[ "$(id -u)" = 0 ] || { echo "Run this script as root." >&2; exit 1; }
case "$SSH_PORT" in ''|*[!0-9]*|0|*[!0-9]*) exit 1;; esac

mkdir -p "$BASE" /var/run/sshd
chmod 700 "$BASE"
if [ ! -f "$BASE/ssh_host_ed25519_key" ]; then
    ssh-keygen -q -t ed25519 -N '' -f "$BASE/ssh_host_ed25519_key"
fi
cat > "$CONF" <<EOF
Port $SSH_PORT
ListenAddress 0.0.0.0
Protocol 2
HostKey $BASE/ssh_host_ed25519_key
PidFile $BASE/sshd.pid
AuthorizedKeysFile /home/$USER_NAME/.ssh/authorized_keys
PubkeyAuthentication yes
PasswordAuthentication yes
ChallengeResponseAuthentication yes
PermitEmptyPasswords yes
UsePAM no
PermitRootLogin yes
StrictModes no
AllowUsers $USER_NAME root
X11Forwarding no
AllowTcpForwarding no
Subsystem sftp /usr/lib/openssh/sftp-server
EOF
chmod 600 "$CONF"

# 免密登录：清空 user 与 root 口令，允许空密码认证。rootfs 默认只读，临时改为可写。
mount -o remount,rw / 2>/dev/null || true
passwd -d "$USER_NAME" >/dev/null 2>&1 || true
passwd -d root >/dev/null 2>&1 || true
mount -o remount,ro / 2>/dev/null || true
cat > "$START" <<'EOF'
#!/bin/sh
set -eu
BASE=/home/user/z03plus/ssh
CONF="$BASE/sshd_config"
PIDFILE="$BASE/sshd.pid"
if [ -s "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then exit 0; fi
rm -f "$PIDFILE"
mkdir -p /var/run/sshd
chmod 755 /var/run/sshd
SSHD=/usr/sbin/sshd
[ -x "$SSHD" ] || SSHD=$(command -v sshd)
"$SSHD" -t -f "$CONF"
exec "$SSHD" -f "$CONF"
EOF
chmod 700 "$START"
"$START"
"$STATE_DIR/update_ssh_status.sh"
printf '%s\n' "SSH enabled. Add a public key to /home/user/.ssh/authorized_keys."
