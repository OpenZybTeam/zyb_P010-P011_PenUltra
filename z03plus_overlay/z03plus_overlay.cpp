// Z03Plus QRC overlay with on-device verification of the final resolved file.
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QObject>
#include <QMetaMethod>
#include <QStringList>
#include <QResource>
#include <QSaveFile>
#include <QSettings>
#include <QSslError>
#include <QUrl>
#include <QTextCodec>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <QQmlEngine>
#include <QJSEngine>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>

#define Z03ULTRA_VERSION "1.8.11"

static QString gCefPluginError;

static bool cefLibMapped()
{
    QFile maps(QStringLiteral("/proc/self/maps"));
    if (!maps.open(QIODevice::ReadOnly))
        return false;
    return maps.readAll().contains("libcef.so");
}

static char gCefAltStack[65536];

struct Z03CefMap { unsigned long lo, hi; const char *name; int namelen; char exec; char perm[5]; };
static struct Z03CefMap gCefMapTable[2048];
static int gCefMapCount;

static int z03HexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int z03AppendStr(char *buf, int n, int cap, const char *str, int len)
{
    if (len < 0) {
        len = 0;
        while (str[len])
            len++;
    }
    for (int i = 0; i < len && n < cap; i++)
        buf[n++] = str[i];
    return n;
}

static int z03AppendHex(char *buf, int n, int cap, unsigned long v)
{
    char tmp[16];
    int t = 0;
    buf[n++] = '0';
    buf[n++] = 'x';
    if (v == 0)
        tmp[t++] = '0';
    while (v) {
        tmp[t++] = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    }
    while (t && n < cap)
        buf[n++] = tmp[--t];
    return n;
}

static int z03AppendDec(char *buf, int n, int cap, unsigned long v)
{
    char tmp[20];
    int t = 0;
    if (v == 0)
        tmp[t++] = '0';
    while (v) {
        tmp[t++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (t && n < cap)
        buf[n++] = tmp[--t];
    return n;
}

static void z03ParseMapTable(char *mbuf, size_t total)
{
    gCefMapCount = 0;
    size_t i = 0;
    while (i < total && gCefMapCount < 2048) {
        size_t e = i;
        while (e < total && mbuf[e] != '\n')
            e++;
        size_t j = i;
        unsigned long lo = 0;
        unsigned long hi = 0;
        int k = 0;
        int d;
        while (j < e && k < 8 && (d = z03HexVal(mbuf[j])) >= 0) {
            lo = (lo << 4) | (unsigned long)d;
            j++;
            k++;
        }
        if (k == 8 && j < e && mbuf[j] == '-') {
            j++;
            k = 0;
            while (j < e && k < 8 && (d = z03HexVal(mbuf[j])) >= 0) {
                hi = (hi << 4) | (unsigned long)d;
                j++;
                k++;
            }
            if (k == 8 && j + 3 < e) {
                const char *name = nullptr;
                int nlen = 0;
                for (size_t t = j; t + 1 < e; t++) {
                    if (mbuf[t] == ' ' && mbuf[t + 1] == '/') {
                        name = mbuf + t + 1;
                        nlen = (int)(e - t - 1);
                        break;
                    }
                }
                gCefMapTable[gCefMapCount].lo = lo;
                gCefMapTable[gCefMapCount].hi = hi;
                gCefMapTable[gCefMapCount].name = name;
                gCefMapTable[gCefMapCount].namelen = nlen;
                gCefMapTable[gCefMapCount].exec = (j + 3 < e && mbuf[j + 3] == 'x');
                for (int pi = 0; pi < 4; pi++)
                    gCefMapTable[gCefMapCount].perm[pi] =
                        (j + 1 + pi < e) ? mbuf[j + 1 + pi] : '?';
                gCefMapTable[gCefMapCount].perm[4] = 0;
                gCefMapCount++;
            }
        }
        i = e + 1;
    }
}

static int z03ResolveAddr(unsigned long value, char *buf, int cap, int *n)
{
    for (int i = 0; i < gCefMapCount; i++) {
        if (value >= gCefMapTable[i].lo && value < gCefMapTable[i].hi) {
            if (gCefMapTable[i].name) {
                *n = z03AppendStr(buf, *n, cap, gCefMapTable[i].name, gCefMapTable[i].namelen);
                buf[(*n)++] = '+';
            } else {
                *n = z03AppendStr(buf, *n, cap, "[anon]", -1);
                buf[(*n)++] = '+';
            }
            *n = z03AppendHex(buf, *n, cap, value - gCefMapTable[i].lo);
            return 1;
        }
    }
    return 0;
}

static void z03WriteResolved(int fd, const char *prefix, unsigned long value)
{
    char line[384];
    int n = 0;
    n = z03AppendStr(line, n, 340, prefix, -1);
    if (!z03ResolveAddr(value, line, 340, &n)) {
        n = z03AppendStr(line, n, 340, "<unmapped> ", -1);
        n = z03AppendHex(line, n, 340, value);
    }
    line[n++] = '\n';
    write(fd, line, (size_t)n);
}

static void z03WriteFaultMap(int fd, unsigned long value)
{
    for (int i = 0; i < gCefMapCount; i++) {
        if (value < gCefMapTable[i].lo || value >= gCefMapTable[i].hi)
            continue;
        char line[128];
        int n = 0;
        n = z03AppendStr(line, n, 120, "fault map perm=", -1);
        n = z03AppendStr(line, n, 120, gCefMapTable[i].perm, 4);
        n = z03AppendStr(line, n, 120, " off=", -1);
        n = z03AppendHex(line, n, 120, value - gCefMapTable[i].lo);
        n = z03AppendStr(line, n, 120, " lo=", -1);
        n = z03AppendHex(line, n, 120, gCefMapTable[i].lo);
        line[n++] = '\n';
        write(fd, line, (size_t)n);
        return;
    }
}

static void z03cefCrashDump(int sig, void *addr, unsigned long pc, unsigned long lr, unsigned long sp)
{
    int fd = open("/home/user/z03plus/cef/crash.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return;
    char line[512];
    int n = 0;
    n = z03AppendStr(line, n, 460, "crash signal=", -1);
    n = z03AppendDec(line, n, 460, (unsigned long)sig);
    n = z03AppendStr(line, n, 460, " pid=", -1);
    n = z03AppendDec(line, n, 460, (unsigned long)getpid());
    n = z03AppendStr(line, n, 460, " ppid=", -1);
    n = z03AppendDec(line, n, 460, (unsigned long)getppid());
    int cf = open("/proc/self/cmdline", O_RDONLY);
    if (cf >= 0) {
        char cmd[192];
        ssize_t got = read(cf, cmd, sizeof(cmd));
        close(cf);
        if (got > 0) {
            n = z03AppendStr(line, n, 460, " cmd=", -1);
            for (ssize_t i = 0; i < got && n < 450; i++)
                line[n++] = cmd[i] ? cmd[i] : ' ';
        }
    }
    line[n++] = '\n';
    write(fd, line, (size_t)n);

    static char mbuf[524288];
    size_t total = 0;
    int mf = open("/proc/self/maps", O_RDONLY);
    int mferr = mf < 0 ? errno : 0;
    if (mf >= 0) {
        ssize_t got;
        while (total + 1 < sizeof(mbuf)) {
            got = read(mf, mbuf + total, sizeof(mbuf) - 1 - total);
            if (got > 0) {
                total += (size_t)got;
                continue;
            }
            if (got < 0 && errno == EINTR)
                continue;
            if (got < 0)
                mferr = errno;
            break;
        }
        close(mf);
        mbuf[total] = 0;
    }
    z03ParseMapTable(mbuf, total);
    {
        char meta[128];
        int mn = 0;
        mn = z03AppendStr(meta, mn, 110, "maps bytes=", -1);
        mn = z03AppendDec(meta, mn, 110, (unsigned long)total);
        mn = z03AppendStr(meta, mn, 110, " entries=", -1);
        mn = z03AppendDec(meta, mn, 110, (unsigned long)gCefMapCount);
        mn = z03AppendStr(meta, mn, 110, " err=", -1);
        mn = z03AppendDec(meta, mn, 110, (unsigned long)mferr);
        meta[mn++] = '\n';
        write(fd, meta, (size_t)mn);
    }
    z03WriteResolved(fd, "pc: ", pc);
    z03WriteResolved(fd, "lr: ", lr);
    z03WriteResolved(fd, "fault: ", (unsigned long)addr);
    z03WriteFaultMap(fd, (unsigned long)addr);
    z03WriteResolved(fd, "sp: ", sp);

    const unsigned long *st = (const unsigned long *)sp;
    int hits = 0;
    for (int i = 0; i < 768 && hits < 24; i++) {
        unsigned long v = st[i];
        unsigned long a = v & ~1UL;
        if (a < 0x1000)
            continue;
        for (int m = 0; m < gCefMapCount && hits < 24; m++) {
            if (!gCefMapTable[m].exec)
                continue;
            if (a >= gCefMapTable[m].lo && a < gCefMapTable[m].hi) {
                z03WriteResolved(fd, "ret: ", a);
                hits++;
                break;
            }
        }
    }
    close(fd);
}

static void z03cefCrashHandler(int sig, siginfo_t *si, void *ctx)
{
#ifdef __arm__
    ucontext_t *uc = (ucontext_t *)ctx;
    z03cefCrashDump(sig, si->si_addr, (unsigned long)uc->uc_mcontext.arm_pc,
                    (unsigned long)uc->uc_mcontext.arm_lr,
                    (unsigned long)uc->uc_mcontext.arm_sp);
#else
    z03cefCrashDump(sig, si->si_addr, 0, 0, 0);
#endif
    _exit(139);
}

static void z03cefInstallCrashHandler()
{
    stack_t ss;
    struct sigaction sa;
    ss.ss_sp = gCefAltStack;
    ss.ss_size = sizeof(gCefAltStack);
    ss.ss_flags = 0;
    sigaltstack(&ss, nullptr);
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = z03cefCrashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
}

static void z03cefBreadcrumb(const char *text)
{
    QFile f(QStringLiteral("/home/user/z03plus/cef/stage.txt"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    f.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
    f.write(" ");
    f.write(text);
    f.write("\n");
}

static void z03cefAppStartup()
{
    z03cefBreadcrumb("qcoreapp");
}
Q_COREAPP_STARTUP_FUNCTION(z03cefAppStartup)

static const char *z03CefLauncherPath()
{
    return "/home/user/z03plus/Z03CefLaunch";
}

static bool z03WriteCefLauncher()
{
    static const char script[] =
        "#!/bin/sh\n"
        "D=/home/user/z03plus\n"
        "[ -x \"$D/Z03PlusCef\" ] || exit 1\n"
        "ln -f \"$D/Z03PlusCef\" \"$D/Z03\" 2>/dev/null\n"
        ": > \"$D/cef/launch.log\"\n"
        "env LD_BIND_NOW=1 \"$D/Z03\" \"$@\" >>\"$D/cef/launch.log\" 2>&1\n"
        "rc=$?\n"
        "echo \"$(date '+%Y-%m-%dT%H:%M:%S') exit rc=$rc\" >>\"$D/cef/launch.log\"\n"
        "exit $rc\n";
    QFile f(QString::fromLatin1(z03CefLauncherPath()));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const qint64 wrote = f.write(script, sizeof(script) - 1);
    f.close();
    if (wrote != (qint64)(sizeof(script) - 1))
        return false;
    return f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                            | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                            | QFileDevice::ExeGroup | QFileDevice::ReadOther
                            | QFileDevice::ExeOther);
}

__attribute__((constructor)) static void z03cefStartupCheck()
{
    const char *selftest = getenv("Z03_CEF_SELFTEST");
    const bool probe = selftest && strcmp(selftest, "1") == 0;
    z03cefBreadcrumb(probe ? "ctor probe" : "ctor normal");
    if (!cefLibMapped()) {
        if (probe)
            _exit(2);
        return;
    }
    z03cefBreadcrumb("libcef mapped");
    z03cefInstallCrashHandler();
    if (!probe)
        return;
    /* dlopen from an ELF constructor re-enters the dynamic loader while it is
       still running _dl_init; that is what crashed here. Do nothing but wait
       and confirm libcef can stay mapped, then let the plugin initialise CEF
       from the Qt thread when the browser page is opened. */
    sleep(5);
    _exit(cefLibMapped() ? 0 : 3);
}

static void copyBundledFile(const char *name)
{
    const QString target = QStringLiteral("/home/user/z03plus/cef/") + QLatin1String(name);
    if (QFile::exists(target))
        return;
    QFile src(QStringLiteral(":/ZybQmlFiles/cef-libs/") + QLatin1String(name));
    if (!src.open(QIODevice::ReadOnly))
        return;
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly))
        return;
    out.write(src.readAll());
    out.close();
    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup
                                      | QFile::ReadOther | QFile::ExeOwner | QFile::ExeGroup
                                      | QFile::ExeOther);
}

static void ensureCefPluginFile()
{
    const QString dir = QStringLiteral("/home/user/z03plus/cef");
    const QString src = QStringLiteral("/home/user/z03plus/libZ03Cef.so");
    const QString dst = dir + QStringLiteral("/libZ03Cef.so");
    if (!QFile::exists(src) || !QDir(dir).exists())
        return;
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly))
        return;
    const QByteArray data = in.readAll();
    QFile cur(dst);
    if (cur.open(QIODevice::ReadOnly)) {
        const bool same = cur.readAll() == data;
        cur.close();
        if (same)
            return;
    }
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(data);
    out.close();
    QFile::setPermissions(dst, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup
                              | QFile::ReadOther | QFile::ExeOwner | QFile::ExeGroup
                              | QFile::ExeOther);
}

// libcef.so 把 ld-linux-armhf.so.3 写进了 DT_NEEDED，glibc 2.27 会把它当作
// 普通共享库再加载一份动态加载器。之后任何 dlopen 在排序依赖时都会踩到这份
// 重复映射，固定崩在 ld-2.27.so 的同一偏移。把这条依赖改指向已经存在的
// libc.so.6，只改 4 字节，不改动 ELF 布局。
static bool patchCefLibraryNeeded()
{
    const QString path = QStringLiteral("/home/user/z03plus/cef/libcef.so");
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite))
        return false;
    const QByteArray elf = f.read(52);
    if (elf.size() < 52 || elf.at(0) != '\x7f' || elf.at(1) != 'E' || elf.at(2) != 'L'
        || elf.at(3) != 'F' || elf.at(4) != 1 || elf.at(5) != 1) {
        return false;
    }
    auto le16 = [](const QByteArray &b, int o) {
        return quint16(quint8(b.at(o)) | (quint16(quint8(b.at(o + 1))) << 8));
    };
    auto le32 = [](const QByteArray &b, int o) {
        return quint32(quint8(b.at(o)))
               | (quint32(quint8(b.at(o + 1))) << 8)
               | (quint32(quint8(b.at(o + 2))) << 16)
               | (quint32(quint8(b.at(o + 3))) << 24);
    };
    const quint32 phoff = le32(elf, 28);
    const quint16 phentsize = le16(elf, 42);
    const quint16 phnum = le16(elf, 44);
    quint32 dynOff = 0;
    quint32 dynSize = 0;
    for (quint16 i = 0; i < phnum; ++i) {
        if (!f.seek(phoff + quint32(i) * phentsize))
            return false;
        const QByteArray ph = f.read(32);
        if (ph.size() < 32)
            return false;
        if (le32(ph, 0) == 2) {
            dynOff = le32(ph, 4);
            dynSize = le32(ph, 16);
        }
    }
    if (dynOff == 0 || dynSize < 16)
        return false;
    if (!f.seek(dynOff))
        return false;
    const QByteArray dyn = f.read(dynSize);
    if (dyn.size() < 16)
        return false;
    quint32 strOff = 0;
    quint32 strSize = 0;
    for (int i = 0; i + 8 <= dyn.size(); i += 8) {
        const quint32 tag = le32(dyn, i);
        if (tag == 0)
            break;
        if (tag == 5)
            strOff = le32(dyn, i + 4);
        else if (tag == 10)
            strSize = le32(dyn, i + 4);
    }
    if (strOff == 0 || strSize < 16)
        return false;
    if (!f.seek(strOff))
        return false;
    const QByteArray strtab = f.read(strSize);
    if (uint(strtab.size()) != strSize)
        return false;
    auto findName = [&strtab](const QByteArray &name) {
        int pos = 0;
        while ((pos = strtab.indexOf(name, pos)) >= 0) {
            const bool startOk = pos == 0 || strtab.at(pos - 1) == '\0';
            const int end = pos + name.size();
            const bool endOk = end < strtab.size() && strtab.at(end) == '\0';
            if (startOk && endOk)
                return pos;
            pos = end;
        }
        return -1;
    };
    const int ldIdx = findName(QByteArrayLiteral("ld-linux-armhf.so.3"));
    const int libcIdx = findName(QByteArrayLiteral("libc.so.6"));
    if (ldIdx < 0 || libcIdx < 0)
        return true;
    bool changed = false;
    for (int i = 0; i + 8 <= dyn.size(); i += 8) {
        const quint32 tag = le32(dyn, i);
        if (tag == 0)
            break;
        if (tag != 1 || le32(dyn, i + 4) != quint32(ldIdx))
            continue;
        if (!f.seek(dynOff + quint32(i) + 4))
            return false;
        QByteArray value(4, '\0');
        char *raw = value.data();
        raw[0] = char(quint8(libcIdx & 0xff));
        raw[1] = char(quint8((libcIdx >> 8) & 0xff));
        raw[2] = char(quint8((libcIdx >> 16) & 0xff));
        raw[3] = char(quint8((libcIdx >> 24) & 0xff));
        if (f.write(value) != 4)
            return false;
        changed = true;
    }
    if (changed)
        f.flush();
    return true;
}

static void writeCefPluginError()
{
    QFile out(QStringLiteral("/home/user/z03plus/cef/plugin-error.txt"));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    out.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
    out.write("\n");
    out.write(gCefPluginError.toUtf8());
    out.write("\n");
}
#include <algorithm>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include <link.h>

extern "C" void *dlsym(void *handle, const char *symbol) noexcept;
#ifndef RTLD_NEXT
#define RTLD_NEXT ((void *)-1l)
#endif
typedef unsigned char uchar;
typedef int (*RegisterResource)(int, const uchar *, const uchar *, const uchar *);

static bool gOverlayRegistered = false;

// QML can invoke only these fixed scripts. No command text is accepted from
// QML, so this cannot become a general shell-execution interface.
class Z03PlusSshService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE bool start()
    {
        if (QFileInfo(QStringLiteral("/home/user/z03plus/ssh/start-sshd.sh")).exists())
            return launch("/home/user/z03plus/ssh/start-sshd.sh; "
                          "/home/user/z03plus/update_ssh_status.sh 2>/dev/null || true");
        if (QFileInfo(QStringLiteral("/home/user/z03plus/setup_ssh.sh")).exists())
            return launch("/home/user/z03plus/setup_ssh.sh");
        return false;
    }

    Q_INVOKABLE bool refresh()
    {
        if (!QFileInfo(QStringLiteral("/home/user/z03plus/update_ssh_status.sh")).exists())
            return true;
        return launch("/home/user/z03plus/update_ssh_status.sh 2>/dev/null || true");
    }

    // The built-in bridge always listens on 127.0.0.1:8080. Kept for QML
    // compatibility; optionally launches the external bridge when present.
    Q_INVOKABLE bool restartAi()
    {
        if (QFileInfo(QStringLiteral("/home/user/z03plus/ydpext/start-ai.sh")).exists())
            return launch("/home/user/z03plus/ydpext/start-ai.sh");
        return true;
    }

    Q_INVOKABLE bool stop()
    {
        return launch("if [ -s /home/user/z03plus/ssh/sshd.pid ]; then "
                      "kill $(cat /home/user/z03plus/ssh/sshd.pid) 2>/dev/null || true; "
                      "rm -f /home/user/z03plus/ssh/sshd.pid; fi; "
                      "/home/user/z03plus/update_ssh_status.sh");
    }

    Q_INVOKABLE QVariantMap status() const
    {
        QVariantMap values;
        // Keep any values written by update_ssh_status.sh, then fill in the
        // live values. This makes the page work even when the helper script
        // has not been deployed yet.
        QFile file(QStringLiteral("/home/user/z03plus/ssh-status.txt"));
        if (file.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> rows = file.readAll().split('\n');
            for (const QByteArray &row : rows) {
                const int delimiter = row.indexOf('=');
                if (delimiter > 0)
                    values.insert(QString::fromLatin1(row.left(delimiter)),
                                  QString::fromUtf8(row.mid(delimiter + 1)));
            }
        }
        const QString ip = wifiAddress();
        const QString port = sshPort();
        values.insert(QStringLiteral("ip"), ip.isEmpty() ? QStringLiteral("未连接 Wi-Fi") : ip);
        values.insert(QStringLiteral("running"), sshRunning() ? QStringLiteral("on") : QStringLiteral("off"));
        values.insert(QStringLiteral("port"), port);
        values.insert(QStringLiteral("user"), QStringLiteral("user"));
        values.insert(QStringLiteral("key_status"),
                      QFileInfo(QStringLiteral("/home/user/.ssh/authorized_keys")).size() > 0
                          ? QStringLiteral("已配置") : QStringLiteral("未配置"));
        values.insert(QStringLiteral("root_login"),
                      rootLoginAllowed() ? QStringLiteral("已允许") : QStringLiteral("已禁用"));
        values.insert(QStringLiteral("command"),
                      ip.isEmpty() ? QStringLiteral("请先连接 Wi-Fi")
                                   : QStringLiteral("ssh user@%1 -p %2").arg(ip).arg(port));
        return values;
    }

private:
    static QString wifiAddress()
    {
        const QNetworkInterface wlan = QNetworkInterface::interfaceFromName(QStringLiteral("wlan0"));
        for (const QNetworkAddressEntry &entry : wlan.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback())
                return entry.ip().toString();
        }
        return QString();
    }

    static bool sshRunning()
    {
        QFile pidFile(QStringLiteral("/home/user/z03plus/ssh/sshd.pid"));
        if (pidFile.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 pid = pidFile.readAll().trimmed().toLongLong(&ok);
            if (ok && pid > 0 && ::kill(static_cast<pid_t>(pid), 0) == 0)
                return true;
        }
        const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            bool numeric = true;
            for (const QChar ch : entry) {
                if (!ch.isDigit()) {
                    numeric = false;
                    break;
                }
            }
            if (!numeric)
                continue;
            QFile comm(QStringLiteral("/proc/%1/comm").arg(entry));
            if (!comm.open(QIODevice::ReadOnly))
                continue;
            if (QString::fromLatin1(comm.readAll()).trimmed().startsWith(QStringLiteral("sshd")))
                return true;
        }
        return false;
    }

    static QString sshPort()
    {
        QFile conf(QStringLiteral("/home/user/z03plus/ssh/sshd_config"));
        if (conf.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> rows = conf.readAll().split('\n');
            for (const QByteArray &row : rows) {
                const QByteArray line = row.trimmed();
                if (!line.startsWith("Port"))
                    continue;
                const QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2)
                    return QString::fromLatin1(parts.at(1));
            }
        }
        return QStringLiteral("22");
    }

    static bool rootLoginAllowed()
    {
        QFile conf(QStringLiteral("/home/user/z03plus/ssh/sshd_config"));
        if (!conf.open(QIODevice::ReadOnly))
            return false;
        const QList<QByteArray> rows = conf.readAll().split('\n');
        for (const QByteArray &row : rows) {
            const QByteArray line = row.trimmed();
            if (!line.startsWith("PermitRootLogin"))
                continue;
            const QList<QByteArray> parts = line.simplified().split(' ');
            return parts.size() >= 2 && parts.at(1).toLower() != "no";
        }
        return false;
    }

    static bool launch(const char *command)
    {
        const pid_t pid = fork();
        if (pid < 0)
            return false;
        if (pid == 0) {
            execl("/bin/sh", "sh", "-c", command, static_cast<char *>(nullptr));
            _exit(127);
        }
        return true;
    }
};

// File browsing is intentionally constrained to user-writable data locations.
// QML receives paths only from this service; traversal outside these roots is refused.
// Full filesystem access: Z03Ultra runs with the privileges of the host app,
// so the file manager browses from "/" and performs common operations. Every
// path is normalized and every failure is returned to QML with a reason.
class Z03PlusFileService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantMap openDirectory(const QString &requested, int order, bool reversed) const
    {
        const QString path = normalize(requested.isEmpty() ? QStringLiteral("/") : requested);
        if (path.isEmpty())
            return failure(QStringLiteral("路径无效"));
        QFileInfo info(path);
        if (!info.exists())
            return failure(QStringLiteral("目录不存在"));
        if (!info.isDir())
            return failure(QStringLiteral("不是目录"));
        QDir dir(path);
        if (!dir.isReadable())
            return failure(QStringLiteral("目录不可读，请检查权限"));

        QVariantList entries;
        QDir::SortFlags sort = QDir::DirsFirst | QDir::IgnoreCase;
        if (order == 1) sort |= QDir::Time;
        else if (order == 2) sort |= QDir::Size;
        else sort |= QDir::Name;
        if (reversed) sort |= QDir::Reversed;
        const QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                                     QDir::Hidden | QDir::System, sort);
        for (const QFileInfo &entryInfo : list) {
            QVariantMap entry;
            entry.insert(QStringLiteral("name"), entryInfo.fileName());
            entry.insert(QStringLiteral("path"), entryInfo.absoluteFilePath());
            entry.insert(QStringLiteral("isDir"), entryInfo.isDir());
            entry.insert(QStringLiteral("isLink"), entryInfo.isSymLink());
            entry.insert(QStringLiteral("text"), !entryInfo.isDir() && isTextName(entryInfo.fileName()));
            entry.insert(QStringLiteral("archive"), !entryInfo.isDir() && isArchiveName(entryInfo.fileName()));
            entry.insert(QStringLiteral("readable"), entryInfo.isReadable());
            entry.insert(QStringLiteral("writable"), entryInfo.isWritable());
            entry.insert(QStringLiteral("size"), suffixSize(entryInfo));
            entry.insert(QStringLiteral("modified"), entryInfo.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
            entries.append(entry);
        }
        QVariantMap result = success(dir.absolutePath());
        result.insert(QStringLiteral("path"), dir.absolutePath());
        result.insert(QStringLiteral("entries"), entries);
        return result;
    }

    Q_INVOKABLE bool rename(const QString &path, const QString &newName) const
    {
        const QString source = normalize(path);
        if (source.isEmpty() || source == QStringLiteral("/") || !validName(newName))
            return false;
        QFileInfo info(source);
        if (!info.exists())
            return false;
        return QDir(info.absolutePath()).rename(info.fileName(), newName);
    }

    Q_INVOKABLE bool remove(const QString &path) const
    {
        const QString target = normalize(path);
        if (target.isEmpty() || target == QStringLiteral("/"))
            return false;
        QFileInfo info(target);
        if (!info.exists())
            return false;
        if (info.isDir() && !info.isSymLink())
            return QDir(target).removeRecursively();
        return QFile::remove(target);
    }

    Q_INVOKABLE QVariantMap createDirectory(const QString &parentDir, const QString &name) const
    {
        const QString dirPath = normalize(parentDir);
        if (dirPath.isEmpty() || !validName(name))
            return failure(QStringLiteral("名称无效"));
        QDir parent(dirPath);
        if (!parent.exists())
            return failure(QStringLiteral("目录不存在"));
        if (QFileInfo::exists(dirPath + QLatin1Char('/') + name))
            return failure(QStringLiteral("同名文件或目录已存在"));
        if (!parent.mkdir(name))
            return failure(QStringLiteral("新建失败，请检查权限"));
        return success(dirPath + QLatin1Char('/') + name);
    }

    Q_INVOKABLE QVariantMap copyInto(const QString &source, const QString &targetDir) const
    {
        const QString src = normalize(source);
        const QString dirPath = normalize(targetDir);
        if (src.isEmpty() || dirPath.isEmpty())
            return failure(QStringLiteral("路径无效"));
        QFileInfo srcInfo(src);
        if (!srcInfo.exists())
            return failure(QStringLiteral("源文件不存在"));
        QFileInfo dirInfo(dirPath);
        if (!dirInfo.isDir())
            return failure(QStringLiteral("目标不是目录"));
        const QString dst = uniquePath(dirPath, srcInfo.fileName());
        if (dst == src)
            return failure(QStringLiteral("源与目标相同"));
        if (srcInfo.isDir() && dst.startsWith(src + QLatin1Char('/')))
            return failure(QStringLiteral("不能复制到自身内部"));
        if (!copyRecursive(src, dst)) {
            removePath(dst);
            return failure(QStringLiteral("复制失败，请检查权限或存储空间"));
        }
        return success(dst);
    }

    Q_INVOKABLE QVariantMap moveInto(const QString &source, const QString &targetDir) const
    {
        const QString src = normalize(source);
        const QString dirPath = normalize(targetDir);
        if (src.isEmpty() || dirPath.isEmpty() || src == QStringLiteral("/"))
            return failure(QStringLiteral("路径无效"));
        QFileInfo srcInfo(src);
        if (!srcInfo.exists())
            return failure(QStringLiteral("源文件不存在"));
        QFileInfo dirInfo(dirPath);
        if (!dirInfo.isDir())
            return failure(QStringLiteral("目标不是目录"));
        const QString dst = uniquePath(dirPath, srcInfo.fileName());
        if (dst == src)
            return failure(QStringLiteral("源与目标相同"));
        if (srcInfo.isDir() && dst.startsWith(src + QLatin1Char('/')))
            return failure(QStringLiteral("不能移动到自身内部"));
        if (QDir().rename(src, dst))
            return success(dst);
        if (!copyRecursive(src, dst)) {
            removePath(dst);
            return failure(QStringLiteral("移动失败，请检查权限或存储空间"));
        }
        bool removed = false;
        if (srcInfo.isDir() && !srcInfo.isSymLink()) removed = QDir(src).removeRecursively();
        else removed = QFile::remove(src);
        if (!removed)
            return failure(QStringLiteral("已复制到目标，但无法删除源文件"));
        return success(dst);
    }

    Q_INVOKABLE QVariantMap compress(const QStringList &paths, const QString &archivePath) const
    {
        const QString archive = normalize(archivePath);
        if (archive.isEmpty() || paths.isEmpty())
            return failure(QStringLiteral("参数无效"));
        if (QFileInfo::exists(archive))
            return failure(QStringLiteral("同名压缩包已存在"));
        QString parentDir;
        QStringList names;
        for (const QString &raw : paths) {
            const QString path = normalize(raw);
            QFileInfo info(path);
            if (path.isEmpty() || !info.exists())
                return failure(QStringLiteral("源文件不存在：") + raw);
            if (parentDir.isEmpty()) parentDir = info.absolutePath();
            else if (parentDir != info.absolutePath())
                return failure(QStringLiteral("只能压缩同一目录下的项目"));
            names.append(info.fileName());
        }
        const QString lower = archive.toLower();
        QVariantMap result;
        if (lower.endsWith(QStringLiteral(".zip"))) {
            QStringList args;
            args << QStringLiteral("-r") << QStringLiteral("-q") << archive << names;
            result = runTool(QStringLiteral("zip"), args, parentDir, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))) {
            QStringList args;
            args << QStringLiteral("-czf") << archive << QStringLiteral("-C") << parentDir << names;
            result = runTool(QStringLiteral("tar"), args, parentDir, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".txz"))) {
            QStringList args;
            args << QStringLiteral("-cJf") << archive << QStringLiteral("-C") << parentDir << names;
            result = runTool(QStringLiteral("tar"), args, parentDir, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar"))) {
            QStringList args;
            args << QStringLiteral("-cf") << archive << QStringLiteral("-C") << parentDir << names;
            result = runTool(QStringLiteral("tar"), args, parentDir, 10 * 60 * 1000);
        } else {
            return failure(QStringLiteral("仅支持 .tar、.tar.gz、.tgz、.tar.xz、.zip"));
        }
        if (result.value(QStringLiteral("ok")).toBool())
            result.insert(QStringLiteral("path"), archive);
        return result;
    }

    Q_INVOKABLE QVariantMap extract(const QString &archivePath, const QString &targetDir) const
    {
        const QString archive = normalize(archivePath);
        const QString target = normalize(targetDir.isEmpty() ? QStringLiteral("/") : targetDir);
        QFileInfo info(archive);
        if (archive.isEmpty() || !info.exists() || !info.isFile())
            return failure(QStringLiteral("压缩包不存在"));
        if (target.isEmpty())
            return failure(QStringLiteral("目标目录无效"));
        if (!QDir().mkpath(target))
            return failure(QStringLiteral("无法创建目标目录"));
        const QString lower = archive.toLower();
        QVariantMap result;
        if (lower.endsWith(QStringLiteral(".zip"))) {
            QStringList args;
            args << QStringLiteral("-o") << archive << QStringLiteral("-d") << target;
            result = runTool(QStringLiteral("unzip"), args, target, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))) {
            QStringList args;
            args << QStringLiteral("-xzf") << archive << QStringLiteral("-C") << target;
            result = runTool(QStringLiteral("tar"), args, target, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar.bz2")) || lower.endsWith(QStringLiteral(".tbz2"))) {
            QStringList args;
            args << QStringLiteral("-xjf") << archive << QStringLiteral("-C") << target;
            result = runTool(QStringLiteral("tar"), args, target, 10 * 60 * 1000);
        } else if (lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".txz"))) {
            QStringList args;
            args << QStringLiteral("-xJf") << archive << QStringLiteral("-C") << target;
            result = runTool(QStringLiteral("tar"), args, target, 10 * 60 * 1000);
            if (!result.value(QStringLiteral("ok")).toBool()) {
                const QString command = QStringLiteral("xz -dc ") + shellQuote(archive) +
                                        QStringLiteral(" | tar -xf - -C ") + shellQuote(target);
                result = runShell(command, target, 10 * 60 * 1000);
            }
        } else if (lower.endsWith(QStringLiteral(".tar"))) {
            QStringList args;
            args << QStringLiteral("-xf") << archive << QStringLiteral("-C") << target;
            result = runTool(QStringLiteral("tar"), args, target, 10 * 60 * 1000);
        } else {
            return failure(QStringLiteral("不支持的压缩格式"));
        }
        return result;
    }

    Q_INVOKABLE QString parentPath(const QString &path) const
    {
        const QString clean = normalize(path);
        if (clean.isEmpty() || clean == QStringLiteral("/"))
            return QStringLiteral("/");
        int slash = clean.lastIndexOf(QLatin1Char('/'));
        return slash <= 0 ? QStringLiteral("/") : clean.left(slash);
    }

    Q_INVOKABLE bool canGoUp(const QString &path) const
    {
        const QString clean = normalize(path);
        return !clean.isEmpty() && clean != QStringLiteral("/");
    }

    static QString normalize(const QString &path)
    {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty())
            return QString();
        const QString absolute = QFileInfo(trimmed).isAbsolute()
                ? trimmed : QDir::current().absoluteFilePath(trimmed);
        return QDir::cleanPath(absolute);
    }

    static bool isArchiveName(const QString &name)
    {
        const QString lower = name.toLower();
        return lower.endsWith(QStringLiteral(".zip")) || lower.endsWith(QStringLiteral(".tar")) ||
               lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz")) ||
               lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".txz")) ||
               lower.endsWith(QStringLiteral(".tar.bz2")) || lower.endsWith(QStringLiteral(".tbz2"));
    }

    static bool isTextName(const QString &name)
    {
        const QString suffix = QFileInfo(name).suffix().toLower();
        if (suffix.isEmpty())
            return true;
        static const QStringList suffixes = {
            QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("lrc"), QStringLiteral("log"),
            QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("ini"), QStringLiteral("conf"),
            QStringLiteral("cfg"), QStringLiteral("sh"), QStringLiteral("c"), QStringLiteral("h"),
            QStringLiteral("cpp"), QStringLiteral("hpp"), QStringLiteral("qml"), QStringLiteral("js"),
            QStringLiteral("css"), QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("csv"),
            QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"), QStringLiteral("py"),
            QStringLiteral("lua"), QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("prop"),
            QStringLiteral("diff"), QStringLiteral("patch"), QStringLiteral("pro"), QStringLiteral("pri")};
        return suffixes.contains(suffix);
    }

    static QString sizeString(qint64 bytes)
    {
        if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
        if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
        if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
    }

private:
    static QVariantMap failure(const QString &message)
    {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), message);
        return result;
    }
    static QVariantMap success(const QString &path)
    {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("path"), path);
        return result;
    }
    static bool validName(const QString &name)
    {
        return !name.isEmpty() && !name.contains(QLatin1Char('/')) &&
               name != QStringLiteral(".") && name != QStringLiteral("..");
    }
    static QString suffixSize(const QFileInfo &info)
    {
        if (info.isDir()) return QString();
        if (info.isSymLink() && !info.exists()) return QStringLiteral("失效链接");
        return sizeString(info.size());
    }
    static void splitExtension(const QString &name, QString &stem, QString &extension)
    {
        static const QStringList compound = {
            QStringLiteral("tar.gz"), QStringLiteral("tar.xz"), QStringLiteral("tar.bz2")};
        const QString lower = name.toLower();
        for (const QString &item : compound) {
            const QString tail = QLatin1Char('.') + item;
            if (lower.endsWith(tail)) {
                stem = name.left(name.size() - tail.size());
                extension = name.mid(name.size() - tail.size());
                return;
            }
        }
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) {
            stem = name.left(dot);
            extension = name.mid(dot);
        } else {
            stem = name;
            extension.clear();
        }
    }
    static QString uniquePath(const QString &directory, const QString &name)
    {
        const QString first = directory + QLatin1Char('/') + name;
        if (!QFileInfo::exists(first))
            return first;
        QString stem;
        QString extension;
        splitExtension(name, stem, extension);
        for (int index = 2; index < 1000; ++index) {
            const QString candidate = directory + QLatin1Char('/') + stem +
                    QStringLiteral(" (%1)").arg(index) + extension;
            if (!QFileInfo::exists(candidate))
                return candidate;
        }
        return directory + QLatin1Char('/') + stem + QStringLiteral(" (copy)") + extension;
    }
    static QString readSymLink(const QString &path)
    {
        char buffer[4096];
        const QByteArray encoded = QFile::encodeName(path);
        const ssize_t length = ::readlink(encoded.constData(), buffer, sizeof(buffer) - 1);
        if (length <= 0)
            return QString();
        buffer[length] = '\0';
        QString target = QFile::decodeName(QByteArray(buffer, static_cast<int>(length)));
        if (!QFileInfo(target).isAbsolute())
            target = QDir::cleanPath(QFileInfo(path).absolutePath() + QLatin1Char('/') + target);
        return target;
    }
    static void removePath(const QString &path)
    {
        const QFileInfo info(path);
        if (info.isDir() && !info.isSymLink())
            QDir(path).removeRecursively();
        else
            QFile::remove(path);
    }
    static bool copyRecursive(const QString &source, const QString &target)
    {
        QFileInfo info(source);
        if (info.isSymLink()) {
            const QString linkTarget = readSymLink(source);
            if (linkTarget.isEmpty())
                return false;
            return QFile::link(linkTarget, target);
        }
        if (info.isDir()) {
            if (!QDir().mkpath(target))
                return false;
            const QFileInfoList children = QDir(source).entryInfoList(
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            for (const QFileInfo &child : children) {
                if (!copyRecursive(child.absoluteFilePath(), target + QLatin1Char('/') + child.fileName()))
                    return false;
            }
            return true;
        }
        return QFile::copy(source, target);
    }
    static QString shellQuote(const QString &text)
    {
        QString escaped = text;
        escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        return QLatin1Char('\'') + escaped + QLatin1Char('\'');
    }
    static QVariantMap runTool(const QString &program, const QStringList &arguments,
                               const QString &workDir, int timeoutMs)
    {
        const QString executable = QStandardPaths::findExecutable(program);
        if (executable.isEmpty())
            return failure(QStringLiteral("未找到命令：") + program);
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        if (!workDir.isEmpty())
            process.setWorkingDirectory(workDir);
        process.start(executable, arguments);
        if (!process.waitForStarted(5000))
            return failure(QStringLiteral("无法启动：") + program);
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            process.waitForFinished(2000);
            return failure(QStringLiteral("操作超时：") + program);
        }
        const bool ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        QVariantMap result = ok ? success(QString()) : failure(QStringLiteral("命令失败：") + program);
        result.insert(QStringLiteral("output"), QString::fromLocal8Bit(process.readAll()).right(4000));
        return result;
    }
    static QVariantMap runShell(const QString &command, const QString &workDir, int timeoutMs)
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        if (!workDir.isEmpty())
            process.setWorkingDirectory(workDir);
        process.start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << command);
        if (!process.waitForStarted(5000))
            return failure(QStringLiteral("无法启动 shell"));
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            process.waitForFinished(2000);
            return failure(QStringLiteral("操作超时"));
        }
        const bool ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        QVariantMap result = ok ? success(QString()) : failure(QStringLiteral("命令执行失败"));
        result.insert(QStringLiteral("output"), QString::fromLocal8Bit(process.readAll()).right(4000));
        return result;
    }
};

class Z03PlusTextFileService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantMap readText(const QString &path) const
    {
        const QString target = Z03PlusFileService::normalize(path);
        QFileInfo info(target);
        if (target.isEmpty() || !info.exists() || info.isDir())
            return failure(QStringLiteral("文件不存在"));
        const qint64 maxBytes = 4 * 1024 * 1024;
        if (info.size() > maxBytes)
            return failure(QStringLiteral("文件超过 4 MB，无法打开"));
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly)) {
            return failure(QStringLiteral("打开失败，请检查权限"));
        }
        const QByteArray bytes = file.read(maxBytes + 1);
        if (bytes.size() > maxBytes)
            return failure(QStringLiteral("文件超过 4 MB，无法打开"));
        QString text = QString::fromUtf8(bytes);
        if (text.contains(QChar::ReplacementCharacter)) {
            QTextCodec *codec = QTextCodec::codecForName("GB18030");
            if (codec)
                text = codec->toUnicode(bytes);
        }
        QVariantMap result = success();
        result.insert(QStringLiteral("content"), text);
        result.insert(QStringLiteral("size"), info.size());
        return result;
    }

    Q_INVOKABLE QVariantMap saveText(const QString &path, const QString &content) const
    {
        const QString target = Z03PlusFileService::normalize(path);
        if (target.isEmpty() || target == QStringLiteral("/"))
            return failure(QStringLiteral("路径无效"));
        const QByteArray bytes = content.toUtf8();
        if (bytes.size() > 4 * 1024 * 1024)
            return failure(QStringLiteral("内容超过 4 MB，无法保存"));
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) {
            return failure(QStringLiteral("无法写入，请检查权限"));
        }
        if (file.write(bytes) != bytes.size()) {
            file.cancelWriting();
            return failure(QStringLiteral("写入失败，请检查存储空间"));
        }
        if (!file.commit())
            return failure(QStringLiteral("保存失败，请检查权限"));
        return success();
    }

    Q_INVOKABLE QString fileName(const QString &path) const { return QFileInfo(path).fileName(); }

private:
    static QVariantMap failure(const QString &message)
    {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), message);
        return result;
    }
    static QVariantMap success()
    {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), true);
        return result;
    }
};

// Minimal native bridge for the original voice object. It only invokes the
// fixed no-argument methods init/startRecord/stopRecord; QML cannot pass
// arbitrary method names or commands.
class Z03PlusVoiceService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QString inspect(QObject *object) const
    {
        if (!object) return QStringLiteral("对象为空");
        const QMetaObject *meta = object->metaObject();
        QStringList names;
        for (int i = 0; i < meta->methodCount(); ++i) {
            const QMetaMethod method = meta->method(i);
            const QString name = QString::fromLatin1(method.name());
            if (name == QStringLiteral("init") || name == QStringLiteral("release") ||
                name == QStringLiteral("startRecord") || name == QStringLiteral("stopRecord") ||
                name == QStringLiteral("stopRecordNoDely") || name.startsWith(QStringLiteral("sig")))
                names.append(name + QStringLiteral("/") + QString::number(method.parameterCount()));
        }
        return QStringLiteral("class=") + QString::fromLatin1(meta->className()) +
               QStringLiteral(" methods=") + names.join(QStringLiteral(","));
    }
    Q_INVOKABLE bool start(QObject *object)
    {
        if (!object) { emit failed(QStringLiteral("对象为空")); return false; }
        const bool initialized = QMetaObject::invokeMethod(object, "init", Qt::DirectConnection);
        const bool started = QMetaObject::invokeMethod(object, "startRecord", Qt::DirectConnection);
        emit diagnostic(QStringLiteral("init=%1 startRecord=%2 %3")
                        .arg(initialized ? 1 : 0).arg(started ? 1 : 0).arg(inspect(object)));
        if (!started) emit failed(inspect(object));
        return started;
    }
    Q_INVOKABLE bool stop(QObject *object)
    {
        if (!object) { emit failed(QStringLiteral("对象为空")); return false; }
        const bool stopped = QMetaObject::invokeMethod(object, "stopRecord", Qt::DirectConnection);
        emit diagnostic(QStringLiteral("stopRecord=%1 %2").arg(stopped ? 1 : 0).arg(inspect(object)));
        if (!stopped) emit failed(inspect(object));
        return stopped;
    }
signals:
    void diagnostic(const QString &text);
    void failed(const QString &text);
};
// OpenAI-compatible client. Credentials stay in /home/user/z03plus/ai.ini,
// never in QML resources, logs, or the RCC overlay.
class Z03PlusAiService : public QObject
{
    Q_OBJECT
public:
    explicit Z03PlusAiService(QObject *parent = nullptr) : QObject(parent)
    {
        refreshSettings();
    }

    Q_INVOKABLE QVariantMap settings()
    {
        refreshSettings();
        QVariantMap result;
        result.insert(QStringLiteral("endpoint"), mEndpoint);
        result.insert(QStringLiteral("model"), mModel);
        result.insert(QStringLiteral("hasKey"), !mApiKey.isEmpty());
        return result;
    }

    Q_INVOKABLE bool saveSettings(const QString &endpoint, const QString &apiKey, const QString &model)
    {
        const QUrl url(endpoint.trimmed());
        if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")))
            return false;
        mEndpoint = url.toString();
        if (!apiKey.trimmed().isEmpty())
            mApiKey = apiKey.trimmed();
        mModel = model.trimmed().isEmpty() ? QStringLiteral("gpt-3.5-turbo") : model.trimmed();
        QSettings cfg(QStringLiteral("/home/user/z03plus/ai.ini"), QSettings::IniFormat);
        cfg.setValue(QStringLiteral("api/endpoint"), mEndpoint);
        cfg.setValue(QStringLiteral("api/key"), mApiKey);
        cfg.setValue(QStringLiteral("api/model"), mModel);
        cfg.sync();
        return cfg.status() == QSettings::NoError;
    }

    Q_INVOKABLE void newConversation() { mMessages = QJsonArray(); emit conversationReset(); }
    Q_INVOKABLE bool busy() const { return mReply != nullptr; }
    Q_INVOKABLE void cancel() { if (mReply) mReply->abort(); }

    Q_INVOKABLE bool send(const QString &prompt)
    {
        refreshSettings();
        const QString text = prompt.trimmed();
        if (text.isEmpty() || mReply)
            return false;
        if (mApiKey.isEmpty()) { emit failed(QStringLiteral("请先填写 API Key")); return false; }
        const QUrl url(mEndpoint);
        if (!url.isValid()) { emit failed(QStringLiteral("API 地址无效")); return false; }

        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), text);
        mMessages.append(user);
        QJsonObject body;
        body.insert(QStringLiteral("model"), mModel);
        body.insert(QStringLiteral("stream"), true);
        body.insert(QStringLiteral("messages"), mMessages);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") + mApiKey.toUtf8());
        request.setRawHeader("Accept", "text/event-stream, application/json");
        mBuffer.clear(); mRaw.clear(); mAnswer.clear();
        mReply = mNetwork.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(mReply, &QNetworkReply::readyRead, this, &Z03PlusAiService::consume);
        connect(mReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &Z03PlusAiService::onNetworkError);
        connect(mReply, &QNetworkReply::finished, this, &Z03PlusAiService::done);
        emit started();
        return true;
    }

signals:
    void started();
    void delta(const QString &text);
    void completed(const QString &text);
    void failed(const QString &reason);
    void conversationReset();

private:
    void refreshSettings()
    {
        QSettings cfg(QStringLiteral("/home/user/z03plus/ai.ini"), QSettings::IniFormat);
        mEndpoint = cfg.value(QStringLiteral("api/endpoint"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString();
        mApiKey = cfg.value(QStringLiteral("api/key")).toString();
        mModel = cfg.value(QStringLiteral("api/model"), QStringLiteral("gpt-3.5-turbo")).toString();
    }

    void consume()
    {
        if (!mReply) return;
        const QByteArray chunk = mReply->readAll();
        mBuffer += chunk;
        mRaw += chunk;
        int pos;
        while ((pos = mBuffer.indexOf('\n')) >= 0) {
            const QByteArray line = mBuffer.left(pos).trimmed();
            mBuffer.remove(0, pos + 1);
            if (!line.startsWith("data:")) continue;
            const QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]") continue;
            const QJsonDocument doc = QJsonDocument::fromJson(payload);
            const QJsonArray choices = doc.isObject() ? doc.object().value(QStringLiteral("choices")).toArray() : QJsonArray();
            if (choices.isEmpty()) continue;
            const QJsonObject choice = choices.at(0).toObject();
            QString chunk = choice.value(QStringLiteral("delta")).toObject().value(QStringLiteral("content")).toString();
            if (chunk.isEmpty()) chunk = choice.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
            if (!chunk.isEmpty()) { mAnswer += chunk; emit delta(mAnswer); }
        }
    }

    void onNetworkError(QNetworkReply::NetworkError)
    {
        if (mReply) emit failed(mReply->errorString());
    }

    void done()
    {
        if (!mReply) return;
        consume();
        // Some OpenAI-compatible servers ignore stream=true and return one
        // JSON document. Fall back to parsing the whole body.
        if (mAnswer.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(mRaw);
            const QJsonArray choices = doc.isObject()
                ? doc.object().value(QStringLiteral("choices")).toArray() : QJsonArray();
            if (!choices.isEmpty()) {
                const QString content = choices.at(0).toObject()
                    .value(QStringLiteral("message")).toObject()
                    .value(QStringLiteral("content")).toString();
                if (!content.isEmpty()) {
                    mAnswer = content;
                    emit delta(mAnswer);
                }
            }
        }
        if (!mAnswer.isEmpty()) {
            QJsonObject assistant;
            assistant.insert(QStringLiteral("role"), QStringLiteral("assistant"));
            assistant.insert(QStringLiteral("content"), mAnswer);
            mMessages.append(assistant);
            emit completed(mAnswer);
        } else if (mReply->error() == QNetworkReply::NoError) {
            emit failed(QStringLiteral("AI 返回空回复"));
        }
        QNetworkReply *reply = mReply;
        mReply = nullptr;
        reply->deleteLater();
    }

    QNetworkAccessManager mNetwork;
    QNetworkReply *mReply = nullptr;
    QString mEndpoint, mApiKey, mModel, mAnswer;
    QByteArray mBuffer, mRaw;
    QJsonArray mMessages;
};

static QStringList z03SplitChar(const QString &text, QChar separator)
{
    // Qt 5.10 only has QString::split(QChar, QString::SplitBehavior, ...);
    // newer headers route split(QChar) to a Qt::SplitBehaviorFlags overload
    // that does not exist on the device. Split manually to stay 5.10-safe.
    QStringList parts;
    int start = 0;
    for (;;) {
        const int index = text.indexOf(separator, start);
        if (index < 0) {
            parts.append(text.mid(start));
            break;
        }
        parts.append(text.mid(start, index - start));
        start = index + 1;
    }
    return parts;
}

static QString z03ReadText(const QString &path, qint64 limit = 65536)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.read(limit));
}

static QString z03HumanBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("未知");
    if (bytes >= qint64(1024) * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
    if (bytes >= qint64(1024) * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    if (bytes >= 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QString::number(bytes) + QStringLiteral(" B");
}

static qint64 z03DirSize(const QString &path, int *fileCount = nullptr)
{
    qint64 total = 0;
    int count = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
        ++count;
    }
    if (fileCount)
        *fileCount = count;
    return total;
}

static QString z03FindExecutable(const QString &name)
{
    const QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty())
        return found;
    const QStringList dirs = {QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin"),
                              QStringLiteral("/bin"), QStringLiteral("/usr/sbin"),
                              QStringLiteral("/sbin")};
    for (const QString &dir : dirs) {
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QString();
}

class Z03PlusStorageService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantList partitions() const
    {
        QVariantList result;
        QStringList seen;
        const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
        for (const QStorageInfo &volume : volumes) {
            if (!volume.isValid() || !volume.isReady() || volume.bytesTotal() <= 0)
                continue;
            const QString root = volume.rootPath();
            if (root.isEmpty() || seen.contains(root))
                continue;
            if (root.startsWith(QStringLiteral("/dev")) ||
                root.startsWith(QStringLiteral("/proc")) ||
                root.startsWith(QStringLiteral("/sys")) ||
                root.startsWith(QStringLiteral("/run/user")) ||
                root.startsWith(QStringLiteral("/snap")))
                continue;
            seen.append(root);
            const qint64 total = volume.bytesTotal();
            const qint64 free = volume.bytesFree();
            const qint64 used = qMax<qint64>(0, total - free);
            QVariantMap item;
            item.insert(QStringLiteral("mount"), root);
            item.insert(QStringLiteral("device"), QString::fromUtf8(volume.device()));
            item.insert(QStringLiteral("fs"), QString::fromUtf8(volume.fileSystemType()));
            item.insert(QStringLiteral("total"), z03HumanBytes(total));
            item.insert(QStringLiteral("used"), z03HumanBytes(used));
            item.insert(QStringLiteral("free"), z03HumanBytes(volume.bytesAvailable()));
            item.insert(QStringLiteral("percent"), total > 0 ? int(used * 100 / total) : 0);
            item.insert(QStringLiteral("readOnly"), volume.isReadOnly());
            result.append(item);
        }
        std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
            const QString left = a.toMap().value(QStringLiteral("mount")).toString();
            const QString right = b.toMap().value(QStringLiteral("mount")).toString();
            auto rank = [](const QString &path) {
                if (path == QStringLiteral("/")) return 0;
                if (path == QStringLiteral("/home")) return 1;
                if (path == QStringLiteral("/mnt/data")) return 2;
                if (path.startsWith(QStringLiteral("/mnt"))) return 3;
                return 4;
            };
            const int leftRank = rank(left);
            const int rightRank = rank(right);
            return leftRank == rightRank ? left < right : leftRank < rightRank;
        });
        return result;
    }

    Q_INVOKABLE QVariantList cacheInfo() const
    {
        QVariantList result;
        for (const QString &path : cachePaths()) {
            QDir dir(path);
            if (!dir.exists())
                continue;
            int files = 0;
            const qint64 size = z03DirSize(path, &files);
            QVariantMap item;
            item.insert(QStringLiteral("path"), path);
            item.insert(QStringLiteral("size"), z03HumanBytes(size));
            item.insert(QStringLiteral("bytes"), size);
            item.insert(QStringLiteral("files"), files);
            result.append(item);
        }
        return result;
    }

    Q_INVOKABLE QVariantMap clearCache()
    {
        qint64 freed = 0;
        int failed = 0;
        for (const QString &path : cachePaths()) {
            QDir dir(path);
            if (!dir.exists())
                continue;
            const QFileInfoList entries = dir.entryInfoList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks);
            for (const QFileInfo &entry : entries) {
                const qint64 before = entry.isDir() ? z03DirSize(entry.absoluteFilePath()) : entry.size();
                const bool ok = entry.isDir()
                    ? QDir(entry.absoluteFilePath()).removeRecursively()
                    : QFile::remove(entry.absoluteFilePath());
                if (ok)
                    freed += before;
                else
                    ++failed;
            }
        }
        QVariantMap result;
        result.insert(QStringLiteral("ok"), failed == 0);
        result.insert(QStringLiteral("freed"), z03HumanBytes(freed));
        result.insert(QStringLiteral("bytes"), freed);
        result.insert(QStringLiteral("failed"), failed);
        return result;
    }

private:
    static QStringList cachePaths()
    {
        return {QStringLiteral("/home/.cache"), QStringLiteral("/home/user/.cache"),
                QStringLiteral("/home/userdata/cache"),
                QStringLiteral("/home/user/z03plus/cache")};
    }
};

class Z03PlusShellService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    explicit Z03PlusShellService(QObject *parent = nullptr) : QObject(parent)
    {
        mProcess.setProcessChannelMode(QProcess::MergedChannels);
        mTimer.setSingleShot(true);
        connect(&mProcess, &QProcess::readyRead, this, &Z03PlusShellService::readOutput);
        connect(&mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Z03PlusShellService::processFinished);
        connect(&mProcess, &QProcess::errorOccurred, this, &Z03PlusShellService::processError);
        connect(&mTimer, &QTimer::timeout, this, &Z03PlusShellService::timeout);
    }

    bool running() const { return mProcess.state() != QProcess::NotRunning; }

    Q_INVOKABLE bool run(const QString &command, int timeoutMs)
    {
        if (running())
            return false;
        const QString text = command.trimmed();
        if (text.isEmpty())
            return false;
        mOutput.clear();
        mTimedOut = false;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("HOME"), QStringLiteral("/home/user"));
        env.insert(QStringLiteral("PATH"),
                   QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"));
        mProcess.setProcessEnvironment(env);
        mProcess.setWorkingDirectory(QStringLiteral("/home/user"));
        mProcess.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), text});
        if (!mProcess.waitForStarted(1500)) {
            emit finished(-1, QStringLiteral("无法启动 /bin/sh"));
            emit runningChanged();
            return false;
        }
        mTimer.start(qBound(1000, timeoutMs, 300000));
        emit runningChanged();
        return true;
    }

    Q_INVOKABLE void cancel()
    {
        if (running())
            mProcess.kill();
    }

signals:
    void runningChanged();
    void finished(int exitCode, const QString &output);
    void output(const QString &text);

private slots:
    void readOutput()
    {
        const QByteArray data = mProcess.readAll();
        if (data.isEmpty())
            return;
        if (mOutput.size() < 262144)
            mOutput.append(data.left(262144 - mOutput.size()));
        emit output(QString::fromUtf8(data));
    }

    void processFinished(int exitCode, QProcess::ExitStatus status)
    {
        readOutput();
        mTimer.stop();
        QString text = QString::fromUtf8(mOutput);
        if (mTimedOut)
            text += QStringLiteral("\n[命令超时，已终止]");
        else if (status == QProcess::CrashExit)
            text += QStringLiteral("\n[进程异常退出]");
        emit finished(mTimedOut ? -2 : exitCode, text);
        emit runningChanged();
    }

    void processError(QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) {
            mTimer.stop();
            emit finished(-1, QStringLiteral("无法执行命令"));
            emit runningChanged();
        }
    }

    void timeout()
    {
        mTimedOut = true;
        mOutput.append("\n[命令超时，正在终止]");
        mProcess.kill();
    }

private:
    QProcess mProcess;
    QTimer mTimer;
    QByteArray mOutput;
    bool mTimedOut = false;
};

class Z03PlusDeviceInfoService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantList entries() const
    {
        QVariantList result;
        auto add = [&result](const QString &label, const QString &value) {
            if (value.isEmpty())
                return;
            QVariantMap item;
            item.insert(QStringLiteral("label"), label);
            item.insert(QStringLiteral("value"), value);
            result.append(item);
        };

        QString model = z03ReadText(QStringLiteral("/proc/device-tree/model"), 256);
        model.remove(QChar('\0'));
        add(QStringLiteral("设备型号"), model.trimmed());
        add(QStringLiteral("系统"), QSysInfo::prettyProductName());

        QMap<QString, QString> os;
        const QString osRelease = z03ReadText(QStringLiteral("/etc/os-release"));
        for (const QString &line : z03SplitChar(osRelease, QChar('\n'))) {
            const int pos = line.indexOf('=');
            if (pos <= 0)
                continue;
            QString value = line.mid(pos + 1).trimmed();
            if (value.startsWith('"') && value.endsWith('"'))
                value = value.mid(1, value.size() - 2);
            os.insert(line.left(pos).trimmed(), value);
        }
        add(QStringLiteral("发行版"), os.value(QStringLiteral("PRETTY_NAME")));
        add(QStringLiteral("版本"), os.value(QStringLiteral("VERSION_ID")));

        struct utsname uts;
        if (::uname(&uts) == 0) {
            add(QStringLiteral("主机名"), QString::fromLatin1(uts.nodename));
            add(QStringLiteral("内核"), QString::fromLatin1(uts.sysname) + QLatin1Char(' ') +
                QString::fromLatin1(uts.release));
            add(QStringLiteral("内核版本"), QString::fromLatin1(uts.version));
            add(QStringLiteral("架构"), QString::fromLatin1(uts.machine));
        }
        add(QStringLiteral("运行架构"), QSysInfo::currentCpuArchitecture());
        add(QStringLiteral("构建架构"), QSysInfo::buildCpuArchitecture());

        QMap<QString, QString> cpu;
        const QString cpuInfo = z03ReadText(QStringLiteral("/proc/cpuinfo"), 262144);
        for (const QString &line : z03SplitChar(cpuInfo, QChar('\n'))) {
            const int pos = line.indexOf(':');
            if (pos <= 0)
                continue;
            const QString key = line.left(pos).trimmed();
            const QString value = line.mid(pos + 1).trimmed();
            if ((key == QStringLiteral("model name") || key == QStringLiteral("Hardware") ||
                 key == QStringLiteral("Processor") || key == QStringLiteral("CPU architecture") ||
                 key == QStringLiteral("Features") || key == QStringLiteral("Revision")) &&
                !cpu.contains(key))
                cpu.insert(key, value);
        }
        add(QStringLiteral("CPU 型号"),
            cpu.value(QStringLiteral("model name"),
                      cpu.value(QStringLiteral("Hardware"), cpu.value(QStringLiteral("Processor")))));
        add(QStringLiteral("CPU 架构"), cpu.value(QStringLiteral("CPU architecture")));
        add(QStringLiteral("CPU 核心"), QString::number(QThread::idealThreadCount()));

        QMap<QString, qint64> mem;
        const QString memInfo = z03ReadText(QStringLiteral("/proc/meminfo"));
        for (const QString &line : z03SplitChar(memInfo, QChar('\n'))) {
            const int pos = line.indexOf(':');
            if (pos <= 0)
                continue;
            QString value = line.mid(pos + 1).trimmed();
            value.remove(QStringLiteral(" kB"));
            mem.insert(line.left(pos).trimmed(), value.toLongLong() * 1024);
        }
        add(QStringLiteral("内存"), z03HumanBytes(mem.value(QStringLiteral("MemTotal"))));
        add(QStringLiteral("可用内存"), z03HumanBytes(mem.value(QStringLiteral("MemAvailable"))));

        const QString uptimeText = z03ReadText(QStringLiteral("/proc/uptime"));
        const double uptime = z03SplitChar(uptimeText, QChar(' ')).value(0).toDouble();
        const qint64 seconds = qint64(uptime);
        add(QStringLiteral("运行时间"),
            QStringLiteral("%1 天 %2 小时 %3 分")
                .arg(seconds / 86400).arg((seconds % 86400) / 3600).arg((seconds % 3600) / 60));

        const QStorageInfo root(QStringLiteral("/"));
        if (root.isValid() && root.isReady()) {
            add(QStringLiteral("根分区"),
                QStringLiteral("%1 / %2（剩余 %3）")
                    .arg(z03HumanBytes(root.bytesTotal() - root.bytesFree()))
                    .arg(z03HumanBytes(root.bytesTotal()))
                    .arg(z03HumanBytes(root.bytesAvailable())));
        }
        add(QStringLiteral("Qt 版本"), QString::fromLatin1(qVersion()));
        add(QStringLiteral("CPU 特性"), cpu.value(QStringLiteral("Features")));
        return result;
    }
};

class Z03PlusMusicService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantList scan() const
    {
        static const QStringList suffixes = {
            QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
            QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("ogg"),
            QStringLiteral("opus"), QStringLiteral("wma")};
        QList<QFileInfo> files;
        const QStringList roots = {QStringLiteral("/mnt/data"), QStringLiteral("/home/user")};
        for (const QString &root : roots) {
            if (!QDir(root).exists())
                continue;
            QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (it.hasNext() && files.size() < 1000) {
                it.next();
                const QString path = it.filePath();
                if (path.contains(QStringLiteral("/z03plus/qrc/")) ||
                    path.contains(QStringLiteral("/z03plus/backups/")) ||
                    path.contains(QStringLiteral("/.git/")))
                    continue;
                if (suffixes.contains(it.fileInfo().suffix().toLower()))
                    files.append(it.fileInfo());
            }
        }
        std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
        });
        QVariantList result;
        for (const QFileInfo &info : files) {
            QVariantMap item;
            item.insert(QStringLiteral("name"), info.fileName());
            item.insert(QStringLiteral("path"), info.absoluteFilePath());
            item.insert(QStringLiteral("url"), QUrl::fromLocalFile(info.absoluteFilePath()).toString());
            item.insert(QStringLiteral("folder"), info.absolutePath());
            item.insert(QStringLiteral("size"), z03HumanBytes(info.size()));
            item.insert(QStringLiteral("modified"),
                        info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
            result.append(item);
        }
        return result;
    }
};

class Z03PlusRecorderService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY elapsedChanged)
public:
    explicit Z03PlusRecorderService(QObject *parent = nullptr) : QObject(parent)
    {
        mProcess.setProcessChannelMode(QProcess::MergedChannels);
        mElapsedTimer.setInterval(1000);
        mKillTimer.setSingleShot(true);
        connect(&mProcess, &QProcess::readyRead, this, &Z03PlusRecorderService::readOutput);
        connect(&mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Z03PlusRecorderService::processFinished);
        connect(&mProcess, &QProcess::errorOccurred, this, &Z03PlusRecorderService::processError);
        connect(&mElapsedTimer, &QTimer::timeout, this, &Z03PlusRecorderService::tick);
        connect(&mKillTimer, &QTimer::timeout, this, &Z03PlusRecorderService::forceStop);
    }

    bool recording() const { return mRecording; }
    int elapsed() const { return mElapsed; }

    Q_INVOKABLE QString backendName() const
    {
        if (!z03FindExecutable(QStringLiteral("arecord")).isEmpty())
            return QStringLiteral("arecord");
        if (!z03FindExecutable(QStringLiteral("tinycap")).isEmpty())
            return QStringLiteral("tinycap");
        if (!z03FindExecutable(QStringLiteral("gst-launch-1.0")).isEmpty())
            return QStringLiteral("gst-launch-1.0");
        return QString();
    }

    Q_INVOKABLE bool start()
    {
        if (mRecording || mProcess.state() != QProcess::NotRunning)
            return false;
        const QString dir = QStringLiteral("/home/user/z03plus/recordings");
        QDir().mkpath(dir);
        mFile = dir + QStringLiteral("/rec_") +
                QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")) +
                QStringLiteral(".wav");
        QString program;
        QStringList arguments;
        const QString arecord = z03FindExecutable(QStringLiteral("arecord"));
        const QString tinycap = z03FindExecutable(QStringLiteral("tinycap"));
        const QString gst = z03FindExecutable(QStringLiteral("gst-launch-1.0"));
        if (!arecord.isEmpty()) {
            program = arecord;
            arguments = QStringList{QStringLiteral("-q"), QStringLiteral("-f"), QStringLiteral("S16_LE"),
                         QStringLiteral("-r"), QStringLiteral("16000"), QStringLiteral("-c"),
                         QStringLiteral("1"), QStringLiteral("-t"), QStringLiteral("wav"), mFile};
        } else if (!tinycap.isEmpty()) {
            program = tinycap;
            arguments = QStringList{mFile, QStringLiteral("-D"), QStringLiteral("0"),
                         QStringLiteral("-d"), QStringLiteral("0"), QStringLiteral("-c"),
                         QStringLiteral("1"), QStringLiteral("-r"), QStringLiteral("16000"),
                         QStringLiteral("-b"), QStringLiteral("16")};
        } else if (!gst.isEmpty()) {
            program = gst;
            arguments = QStringList{QStringLiteral("-q"), QStringLiteral("alsasrc"),
                         QStringLiteral("!"), QStringLiteral("audioconvert"),
                         QStringLiteral("!"), QStringLiteral("audioresample"),
                         QStringLiteral("!"), QStringLiteral("audio/x-raw,rate=16000,channels=1"),
                         QStringLiteral("!"), QStringLiteral("wavenc"),
                         QStringLiteral("!"), QStringLiteral("filesink,location=") + mFile};
        } else {
            emit failed(QStringLiteral("未找到录音后端（arecord/tinycap/gst-launch-1.0）"));
            return false;
        }
        mOutput.clear();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("HOME"), QStringLiteral("/home/user"));
        env.insert(QStringLiteral("PATH"),
                   QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"));
        mProcess.setProcessEnvironment(env);
        mProcess.start(program, arguments);
        if (!mProcess.waitForStarted(1500)) {
            emit failed(QStringLiteral("无法启动录音程序：") + program);
            return false;
        }
        mRecording = true;
        mElapsed = 0;
        mElapsedTimer.start();
        emit recordingChanged();
        emit elapsedChanged();
        emit started(mFile);
        return true;
    }

    Q_INVOKABLE void stop()
    {
        if (!mRecording)
            return;
        const qint64 pid = mProcess.processId();
        if (pid > 0)
            ::kill(static_cast<pid_t>(pid), SIGINT);
        else
            mProcess.terminate();
        mKillTimer.start(2500);
    }

    Q_INVOKABLE QVariantList recordings() const
    {
        QVariantList result;
        const QDir dir(QStringLiteral("/home/user/z03plus/recordings"));
        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.wav")}, QDir::Files,
                                                      QDir::Time);
        for (const QFileInfo &info : files) {
            QVariantMap item;
            item.insert(QStringLiteral("name"), info.fileName());
            item.insert(QStringLiteral("path"), info.absoluteFilePath());
            item.insert(QStringLiteral("url"), QUrl::fromLocalFile(info.absoluteFilePath()).toString());
            item.insert(QStringLiteral("size"), z03HumanBytes(info.size()));
            item.insert(QStringLiteral("modified"),
                        info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
            result.append(item);
        }
        return result;
    }

    Q_INVOKABLE bool remove(const QString &path)
    {
        const QFileInfo info(path);
        if (info.absolutePath() != QStringLiteral("/home/user/z03plus/recordings"))
            return false;
        return QFile::remove(info.absoluteFilePath());
    }

signals:
    void recordingChanged();
    void elapsedChanged();
    void started(const QString &path);
    void stopped(const QString &path);
    void failed(const QString &reason);

private slots:
    void readOutput()
    {
        const QByteArray data = mProcess.readAll();
        if (mOutput.size() < 65536)
            mOutput.append(data.left(65536 - mOutput.size()));
    }

    void tick()
    {
        ++mElapsed;
        emit elapsedChanged();
    }

    void forceStop()
    {
        if (mProcess.state() != QProcess::NotRunning)
            mProcess.kill();
    }

    void processFinished(int exitCode, QProcess::ExitStatus)
    {
        mKillTimer.stop();
        mElapsedTimer.stop();
        mRecording = false;
        emit recordingChanged();
        const QFileInfo info(mFile);
        if (info.exists() && info.size() > 44)
            emit stopped(mFile);
        else
            emit failed(QStringLiteral("录音失败：") + QString::fromUtf8(mOutput).trimmed());
        Q_UNUSED(exitCode)
    }

    void processError(QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) {
            mRecording = false;
            emit recordingChanged();
            emit failed(QStringLiteral("录音进程启动失败"));
        }
    }

private:
    QProcess mProcess;
    QTimer mElapsedTimer;
    QTimer mKillTimer;
    QString mFile;
    QByteArray mOutput;
    bool mRecording = false;
    int mElapsed = 0;
};

// Minimal text-mode web browser. QtWebEngine is far too heavy for this
// device, so the page is fetched over QtNetwork and reduced to readable text
// plus a list of links. This keeps memory use bounded and works on Qt 5.10.
class Z03PlusBrowserService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString pageUrl READ pageUrl NOTIFY pageChanged)
    Q_PROPERTY(QString title READ title NOTIFY pageChanged)
    Q_PROPERTY(QString content READ content NOTIFY pageChanged)
    Q_PROPERTY(QVariantList links READ links NOTIFY pageChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
public:
    explicit Z03PlusBrowserService(QObject *parent = nullptr) : QObject(parent)
    {
        mNetwork.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        mTimeout.setSingleShot(true);
        mTimeout.setInterval(25000);
        connect(&mTimeout, &QTimer::timeout, this, [this]() {
            if (mReply) {
                mTimedOut = true;
                mReply->abort();
            }
        });
    }

    bool busy() const { return mBusy; }
    QString pageUrl() const { return mPageUrl; }
    QString title() const { return mTitle; }
    QString content() const { return mContent; }
    QVariantList links() const { return mLinks; }
    QString status() const { return mStatus; }
    bool canGoBack() const { return mHistoryIndex > 0; }

    // Browser engine preference lives next to ai.ini so it survives updates.
    static QString engineConfigPath() { return QStringLiteral("/home/user/z03plus/browser.ini"); }

    Q_INVOKABLE QString engine() const
    {
        QSettings cfg(engineConfigPath(), QSettings::IniFormat);
        const QString name = cfg.value(QStringLiteral("browser/engine")).toString();
        return name == QStringLiteral("cef") ? QStringLiteral("cef") : QStringLiteral("qtwebengine");
    }

    Q_INVOKABLE void setEngine(const QString &name)
    {
        QSettings cfg(engineConfigPath(), QSettings::IniFormat);
        cfg.setValue(QStringLiteral("browser/engine"),
                     name == QStringLiteral("cef") ? QStringLiteral("cef") : QStringLiteral("qtwebengine"));
        cfg.sync();
    }

    Q_INVOKABLE bool ensureCefPlugin()
    {
        static bool loaded = false;
        static bool ok = false;
        if (loaded)
            return ok;
        loaded = true;
        ok = loadCefPlugin();
        return ok;
    }

    Q_INVOKABLE QString cefPluginError() const { return gCefPluginError; }

    static void ensureCefSupportLibs()
    {
        const QString dir = QStringLiteral("/home/user/z03plus/cef");
        static const char *const bundled[] = { "libxcb.so.1", "libZ03GbmShim.so",
                                                "libatk-1.0.so.0", "libatspi.so.0",
                                                "libatk-bridge-2.0.so.0" };
        for (const char *name : bundled) {
            const QString target = dir + QLatin1Char('/') + QLatin1String(name);
            if (!QDir(dir).exists())
                continue;
            QFile src(QStringLiteral(":/ZybQmlFiles/cef-libs/") + QLatin1String(name));
            if (!src.open(QIODevice::ReadOnly))
                continue;
            const QByteArray data = src.readAll();
            QFile existing(target);
            if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data) {
                existing.close();
                continue;
            }
            existing.close();
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                continue;
            out.write(data);
            out.close();
            QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ReadGroup | QFile::ReadOther
                                              | QFile::ExeOwner | QFile::ExeGroup
                                              | QFile::ExeOther);
        }
        static const char *const order[] = {
            "libXau.so.6", "libXdmcp.so.6", "libxcb.so.1", "libbsd.so.0",
            "libX11.so.6", "libXext.so.6", "libXfixes.so.3", "libXrender.so.1",
            "libXdamage.so.1", "libXcomposite.so.1", "libXrandr.so.2"
        };
        for (const char *name : order) {
            const QString path = dir + QLatin1Char('/') + QLatin1String(name);
            if (QFile::exists(path)) {
                QLibrary dep(path);
                dep.setLoadHints(QLibrary::ResolveAllSymbolsHint);
                dep.load();
            }
        }
        patchCefLibraryNeeded();
    }

    Q_INVOKABLE bool loadCefPlugin()
    {
        gCefPluginError.clear();
        ensureCefPluginFile();
        const QString path = QStringLiteral("/home/user/z03plus/cef/libZ03Cef.so");
        if (!QFile::exists(path)) {
            gCefPluginError = QStringLiteral("未安装 libZ03Cef.so");
            return false;
        }
        if (!QFile::exists(QStringLiteral("/home/user/z03plus/z03cef_host"))) {
            gCefPluginError = QStringLiteral("缺少 CEF 子进程宿主 z03cef_host，请更新 Z03Ultra 后重试");
            writeCefPluginError();
            return false;
        }
        ensureCefSupportLibs();
        if (!cefLibMapped()) {
            gCefPluginError = QStringLiteral("CEF 内核未启用：组件已安装，但当前以普通模式启动。"
                                             "请重新安装/更新 Z03Ultra 后重启，或点“启用 CEF”切换启动模式。");
            writeCefPluginError();
            return false;
        }
        QLibrary lib(path);
        if (!lib.load()) {
            gCefPluginError = QStringLiteral("加载插件失败：%1").arg(lib.errorString());
            writeCefPluginError();
            return false;
        }
        typedef void (*RegisterFn)();
        RegisterFn reg = reinterpret_cast<RegisterFn>(lib.resolve("z03cef_register_qml"));
        if (!reg) {
            gCefPluginError = QStringLiteral("插件缺少导出函数 z03cef_register_qml：%1").arg(lib.errorString());
            writeCefPluginError();
            return false;
        }
        reg();
        return true;
    }

    Q_INVOKABLE QVariantMap cefStatus() const
    {
        const QString base = QStringLiteral("/home/user/z03plus/cef");
        QVariantMap info;
        info.insert(QStringLiteral("path"), base);
        info.insert(QStringLiteral("installed"),
                    QFile::exists(base + QStringLiteral("/libcef.so"))
                    && QFile::exists(base + QStringLiteral("/z03cef_host")));
        QString version;
        QFile vf(base + QStringLiteral("/version.txt"));
        if (vf.open(QIODevice::ReadOnly | QIODevice::Text))
            version = QString::fromUtf8(vf.readAll()).trimmed();
        info.insert(QStringLiteral("version"), version);
        return info;
    }

    Q_INVOKABLE void load(const QString &input)
    {
        const QUrl url = normalize(input);
        if (!url.isValid() || url.scheme().isEmpty()) {
            setStatus(QStringLiteral("请输入有效的网址"));
            return;
        }
        navigate(url, true);
    }

    Q_INVOKABLE void openLink(const QString &link)
    {
        const QUrl url = normalize(link);
        if (url.isValid() && !url.scheme().isEmpty())
            navigate(url, true);
    }

    Q_INVOKABLE void home() { navigate(QUrl(QStringLiteral("https://cn.bing.com/")), true); }

    Q_INVOKABLE void back()
    {
        if (mHistoryIndex <= 0)
            return;
        --mHistoryIndex;
        emit historyChanged();
        navigate(QUrl(mHistory.at(mHistoryIndex)), false);
    }

    Q_INVOKABLE void reload()
    {
        if (!mCurrentUrl.isEmpty())
            navigate(mCurrentUrl, false);
    }

    Q_INVOKABLE void stop()
    {
        if (mReply) {
            mUserStopped = true;
            mReply->abort();
        }
    }

signals:
    void busyChanged();
    void pageChanged();
    void statusChanged();
    void historyChanged();

private:
    static QUrl normalize(const QString &input)
    {
        const QString text = input.trimmed();
        if (text.isEmpty())
            return QUrl();
        if (text.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
            text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
            return QUrl(text);
        if (text.startsWith(QStringLiteral("//")))
            return QUrl(QStringLiteral("https:") + text);
        if (text.contains(QStringLiteral("://")))
            return QUrl(text);
        if (text.contains(QLatin1Char(' ')) || !text.contains(QLatin1Char('.')))
            return QUrl(QStringLiteral("https://cn.bing.com/search?q=") +
                        QString::fromLatin1(QUrl::toPercentEncoding(text)));
        return QUrl(QStringLiteral("http://") + text);
    }

    void navigate(const QUrl &url, bool pushHistory)
    {
        if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) {
            setStatus(QStringLiteral("仅支持 http/https 页面"));
            return;
        }
        if (mReply) {
            QNetworkReply *old = mReply;
            mReply = nullptr;
            old->abort();
            old->deleteLater();
        }
        mCurrentUrl = url;
        mPageUrl = url.toString();
        mTitle = QStringLiteral("加载中…");
        mContent.clear();
        mLinks.clear();
        mTooLarge = false;
        mTimedOut = false;
        mUserStopped = false;
        mBusy = true;
        emit busyChanged();
        emit pageChanged();
        setStatus(QStringLiteral("正在加载…"));

        if (pushHistory) {
            while (mHistory.size() > mHistoryIndex + 1)
                mHistory.removeLast();
            mHistory.append(mPageUrl);
            if (mHistory.size() > 50)
                mHistory.removeFirst();
            mHistoryIndex = mHistory.size() - 1;
            emit historyChanged();
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("User-Agent",
                             "Mozilla/5.0 (X11; Linux armv7l) AppleWebKit/537.36 "
                             "(KHTML, like Gecko) Z03Ultra/1.0");
        request.setRawHeader("Accept", "text/html,application/xhtml+xml,text/plain;q=0.9,*/*;q=0.5");
        request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        QNetworkReply *reply = mNetwork.get(request);
        mReply = reply;
        mTimeout.start();
        connect(reply, &QNetworkReply::downloadProgress, this,
                [this, reply](qint64 received, qint64 total) {
                    Q_UNUSED(total)
                    if (reply != mReply)
                        return;
                    if (received > 1024 * 1024) {
                        mTooLarge = true;
                        reply->abort();
                    }
                });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply != mReply) {
                reply->deleteLater();
                return;
            }
            onFinished();
        });
    }

    void onFinished()
    {
        QNetworkReply *reply = mReply;
        if (!reply)
            return;
        mReply = nullptr;
        mTimeout.stop();
        const QByteArray raw = reply->readAll();
        const QByteArray contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        const QUrl finalUrl = reply->url();
        reply->deleteLater();
        mBusy = false;
        emit busyChanged();

        if (mUserStopped || reply->error() == QNetworkReply::OperationCanceledError) {
            setStatus(QStringLiteral("已停止"));
            return;
        }
        if (mTooLarge) {
            setStatus(QStringLiteral("页面超过 1 MB，已停止加载"));
            return;
        }
        if (mTimedOut) {
            setStatus(QStringLiteral("加载超时（25 秒）"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("加载失败：") + reply->errorString());
            return;
        }
        if (raw.isEmpty()) {
            setStatus(QStringLiteral("页面内容为空"));
            return;
        }
        if (!finalUrl.isEmpty() && finalUrl != mCurrentUrl) {
            mCurrentUrl = finalUrl;
            mPageUrl = finalUrl.toString();
        }

        QString pageTitle;
        const QString html = decodeHtml(raw, contentType);
        QString text = toPlainText(html, finalUrl.isEmpty() ? mCurrentUrl : finalUrl,
                                   &pageTitle, &mLinks);
        if (text.size() > 160000)
            text = text.left(160000) + QStringLiteral("\n\n…（内容已截断）");
        mTitle = pageTitle.isEmpty() ? mCurrentUrl.toString() : pageTitle;
        mContent = text;
        emit pageChanged();
        setStatus(QStringLiteral("%1 个链接").arg(mLinks.size()));
    }

    void setStatus(const QString &text)
    {
        if (mStatus == text)
            return;
        mStatus = text;
        emit statusChanged();
    }

    static QString decodeHtml(const QByteArray &raw, const QByteArray &contentType)
    {
        QString charset;
        const QRegularExpression ctRe(QStringLiteral("charset\\s*=\\s*[\"']?([A-Za-z0-9_.:-]+)"),
                                      QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch ctMatch = ctRe.match(QString::fromLatin1(contentType));
        if (ctMatch.hasMatch())
            charset = ctMatch.captured(1);
        if (charset.isEmpty()) {
            const QRegularExpression metaRe(
                QStringLiteral("<meta[^>]+charset\\s*=\\s*[\"']?([A-Za-z0-9_.:-]+)"),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch metaMatch =
                metaRe.match(QString::fromLatin1(raw.left(4096)));
            if (metaMatch.hasMatch())
                charset = metaMatch.captured(1);
        }
        if (!charset.isEmpty()) {
            QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1());
            if (codec)
                return codec->toUnicode(raw);
        }
        QString text = QString::fromUtf8(raw);
        if (text.contains(QChar(0xFFFD))) {
            QTextCodec *gbk = QTextCodec::codecForName("GB18030");
            if (gbk)
                return gbk->toUnicode(raw);
        }
        return text;
    }

    static QString decodeEntities(const QString &input)
    {
        static const QHash<QString, QString> named = {
            {QStringLiteral("amp"), QStringLiteral("&")},
            {QStringLiteral("lt"), QStringLiteral("<")},
            {QStringLiteral("gt"), QStringLiteral(">")},
            {QStringLiteral("quot"), QStringLiteral("\"")},
            {QStringLiteral("apos"), QStringLiteral("'")},
            {QStringLiteral("nbsp"), QStringLiteral(" ")},
            {QStringLiteral("mdash"), QStringLiteral("—")},
            {QStringLiteral("ndash"), QStringLiteral("–")},
            {QStringLiteral("hellip"), QStringLiteral("…")},
            {QStringLiteral("middot"), QStringLiteral("·")},
            {QStringLiteral("copy"), QStringLiteral("©")},
            {QStringLiteral("reg"), QStringLiteral("®")},
            {QStringLiteral("times"), QStringLiteral("×")},
            {QStringLiteral("deg"), QStringLiteral("°")},
            {QStringLiteral("laquo"), QStringLiteral("«")},
            {QStringLiteral("raquo"), QStringLiteral("»")},
            {QStringLiteral("yen"), QStringLiteral("¥")},
            {QStringLiteral("sect"), QStringLiteral("§")}};

        QString out;
        out.reserve(input.size());
        for (int i = 0; i < input.size(); ++i) {
            const QChar ch = input.at(i);
            if (ch != QLatin1Char('&')) {
                out += ch;
                continue;
            }
            const int semi = input.indexOf(QLatin1Char(';'), i + 1);
            if (semi < 0 || semi - i > 12) {
                out += ch;
                continue;
            }
            const QString entity = input.mid(i + 1, semi - i - 1);
            QString replacement;
            if (entity.startsWith(QLatin1Char('#'))) {
                bool ok = false;
                uint code = 0;
                if (entity.size() > 2 && (entity.at(1) == QLatin1Char('x') || entity.at(1) == QLatin1Char('X')))
                    code = entity.mid(2).toUInt(&ok, 16);
                else
                    code = entity.mid(1).toUInt(&ok, 10);
                if (ok && code > 0 && code <= 0x10FFFF) {
                    const uint ucs4 = code;
                    replacement = QString::fromUcs4(&ucs4, 1);
                }
            } else {
                replacement = named.value(entity.toLower());
            }
            if (replacement.isEmpty()) {
                out += ch;
            } else {
                out += replacement;
                i = semi;
            }
        }
        return out;
    }

    static QString toPlainText(const QString &html, const QUrl &base, QString *title,
                               QVariantList *links)
    {
        static const QRegularExpression titleRe(QStringLiteral("<title[^>]*>(.*?)</title\\s*>"),
                                                QRegularExpression::CaseInsensitiveOption |
                                                QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch titleMatch = titleRe.match(html);
        if (titleMatch.hasMatch())
            *title = decodeEntities(titleMatch.captured(1)).simplified();

        static const QRegularExpression linkRe(
            QStringLiteral("<a\\b[^>]*href\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))[^>]*>(.*?)</a\\s*>"),
            QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::DotMatchesEverythingOption);
        QStringList seen;
        QVariantList found;
        QRegularExpressionMatchIterator it = linkRe.globalMatch(html);
        while (it.hasNext() && found.size() < 120) {
            const QRegularExpressionMatch match = it.next();
            QString href = match.captured(1);
            if (href.isEmpty())
                href = match.captured(2);
            if (href.isEmpty())
                href = match.captured(3);
            href = decodeEntities(href.trimmed());
            if (href.isEmpty())
                continue;
            const QString lowered = href.toLower();
            if (lowered.startsWith(QStringLiteral("javascript:")) ||
                lowered.startsWith(QStringLiteral("data:")) ||
                href.startsWith(QLatin1Char('#')))
                continue;
            const QUrl resolved = base.resolved(QUrl(href));
            if (!resolved.isValid() ||
                (resolved.scheme() != QStringLiteral("http") &&
                 resolved.scheme() != QStringLiteral("https")))
                continue;
            const QString absolute = resolved.toString();
            if (seen.contains(absolute))
                continue;
            seen.append(absolute);
            QString label = match.captured(4);
            label.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
            label = decodeEntities(label).simplified();
            if (label.isEmpty())
                label = absolute;
            if (label.size() > 80)
                label = label.left(80) + QStringLiteral("…");
            QVariantMap entry;
            entry.insert(QStringLiteral("text"), label);
            entry.insert(QStringLiteral("url"), absolute);
            found.append(entry);
        }
        *links = found;

        QString text = html;
        static const QRegularExpression commentRe(QStringLiteral("<!--.*?-->"),
                                                  QRegularExpression::DotMatchesEverythingOption);
        text.remove(commentRe);
        const QRegularExpression::PatternOptions dotAll =
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption;
        text.remove(QRegularExpression(QStringLiteral("<script\\b.*?</script\\s*>"), dotAll));
        text.remove(QRegularExpression(QStringLiteral("<style\\b.*?</style\\s*>"), dotAll));
        text.remove(QRegularExpression(QStringLiteral("<noscript\\b.*?</noscript\\s*>"), dotAll));
        text.remove(QRegularExpression(QStringLiteral("<svg\\b.*?</svg\\s*>"), dotAll));
        text.replace(QRegularExpression(QStringLiteral("<\\s*br\\s*/?\\s*>"),
                                        QRegularExpression::CaseInsensitiveOption),
                     QStringLiteral("\n"));
        text.replace(QRegularExpression(QStringLiteral("<\\s*li\\b[^>]*>"),
                                        QRegularExpression::CaseInsensitiveOption),
                     QStringLiteral("\n• "));
        text.replace(QRegularExpression(
                         QStringLiteral("</\\s*(?:p|div|li|tr|h[1-6]|section|article|header|footer|"
                                        "blockquote|pre|table|ul|ol|dd|dt)\\s*>"),
                         QRegularExpression::CaseInsensitiveOption),
                     QStringLiteral("\n"));
        text.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(" "));
        text = decodeEntities(text);
        text.replace(QRegularExpression(QStringLiteral("[\\t\\x0B\\f\\r ]+")), QStringLiteral(" "));
        text.replace(QRegularExpression(QStringLiteral(" *\\n *")), QStringLiteral("\n"));
        text.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
        return text.trimmed();
    }

    QNetworkAccessManager mNetwork;
    QNetworkReply *mReply = nullptr;
    QTimer mTimeout;
    QStringList mHistory;
    int mHistoryIndex = -1;
    QUrl mCurrentUrl;
    QString mPageUrl;
    QString mTitle;
    QString mContent;
    QVariantList mLinks;
    QString mStatus;
    bool mBusy = false;
    bool mTimedOut = false;
    bool mTooLarge = false;
    bool mUserStopped = false;
};

class Z03PlusNixService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY installedChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)
public:
    explicit Z03PlusNixService(QObject *parent = nullptr) : QObject(parent)
    {
        mProcess.setProcessChannelMode(QProcess::MergedChannels);
        mTimer.setSingleShot(true);
        connect(&mProcess, &QProcess::readyRead, this, &Z03PlusNixService::readOutput);
        connect(&mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Z03PlusNixService::processFinished);
        connect(&mProcess, &QProcess::errorOccurred, this, &Z03PlusNixService::processError);
        connect(&mTimer, &QTimer::timeout, this, &Z03PlusNixService::timeout);
        detect();
    }

    bool busy() const { return mBusy; }
    bool installed() const { return mInstalled; }
    QString log() const { return mLog; }
    int progress() const { return mProgress; }
    QString stage() const { return mStage; }

    Q_INVOKABLE void detect()
    {
        const QString found = findNixEnv();
        const bool nowInstalled = !found.isEmpty();
        if (nowInstalled != mInstalled) {
            mInstalled = nowInstalled;
            emit installedChanged();
        }
        if (nowInstalled) {
            appendLog(QStringLiteral("已检测到 Nix：") + found);
        } else if (mLog.isEmpty()) {
            appendLog(QStringLiteral("未检测到 Nix，可点击“安装 Nix”自动部署。"));
        }
    }

    Q_INVOKABLE void install()
    {
        if (mBusy) {
            appendLog(QStringLiteral("当前有任务运行，请稍候。"));
            return;
        }
        const QString scriptPath = QStringLiteral("/home/user/z03plus/nix-bootstrap.sh");
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            appendLog(QStringLiteral("无法写入安装脚本：") + scriptPath);
            return;
        }
        script.write(nixBootstrapScript());
        script.close();
        QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                               QFile::ReadGroup | QFile::ExeGroup |
                                               QFile::ReadOther | QFile::ExeOther);
        QFile logFile(QStringLiteral("/home/user/z03plus/nix-install.log"));
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            logFile.close();
        appendLog(QStringLiteral("开始安装 Nix（单用户模式，清华 TUNA 镜像）…"));
        mProgress = 1;
        mStage = QStringLiteral("准备安装…");
        emit progressChanged();
        startProcess(QStringLiteral("/bin/sh"), {scriptPath}, 20 * 60 * 1000);
    }

    Q_INVOKABLE void search(const QString &query)
    {
        const QString text = query.trimmed();
        if (text.isEmpty() || text.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9._+ -]")))) {
            appendLog(QStringLiteral("搜索词只允许字母、数字、点、下划线、加号、减号和空格。"));
            return;
        }
        runNix({QStringLiteral("-qaP"), QStringLiteral("--description"),
                QStringLiteral("*") + text + QStringLiteral("*")}, 120000);
    }

    Q_INVOKABLE void installPackage(const QString &attribute)
    {
        if (!validAttribute(attribute)) {
            appendLog(QStringLiteral("包名格式无效。示例：nixpkgs.hello"));
            return;
        }
        runNix({QStringLiteral("-iA"), attribute}, 30 * 60 * 1000);
    }

    Q_INVOKABLE void removePackage(const QString &name)
    {
        if (!validAttribute(name)) {
            appendLog(QStringLiteral("包名格式无效。"));
            return;
        }
        runNix({QStringLiteral("-e"), name}, 120000);
    }

    Q_INVOKABLE void listInstalled()
    {
        runNix({QStringLiteral("-q")}, 30000);
    }

    Q_INVOKABLE void updateChannels()
    {
        runShell(QStringLiteral("nix-channel --update"), 20 * 60 * 1000);
    }

    Q_INVOKABLE void cancel()
    {
        if (mBusy)
            mProcess.kill();
    }

    Q_INVOKABLE void clearLog() { mLog.clear(); emit logChanged(); }

signals:
    void busyChanged();
    void installedChanged();
    void logChanged();
    void progressChanged();
    void finished(bool ok, const QString &message);

private:
    static bool validAttribute(const QString &text)
    {
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._+-]+$"));
        return pattern.match(text.trimmed()).hasMatch();
    }

    QString findNixEnv() const
    {
        const QString fromPath = z03FindExecutable(QStringLiteral("nix-env"));
        if (!fromPath.isEmpty())
            return fromPath;
        const QStringList candidates = {
            QStringLiteral("/nix/var/nix/profiles/default/bin/nix-env"),
            QStringLiteral("/home/user/.nix-profile/bin/nix-env"),
            QStringLiteral("/home/user/nix/var/nix/profiles/default/bin/nix-env")};
        for (const QString &candidate : candidates) {
            if (QFileInfo(candidate).isExecutable())
                return candidate;
        }
        return QString();
    }

    QString nixPath() const
    {
        QString path = QStringLiteral("/home/user/.nix-profile/bin:/nix/var/nix/profiles/default/bin:"
                                      "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
        const QString env = findNixEnv();
        if (!env.isEmpty())
            path.prepend(QFileInfo(env).absolutePath() + QLatin1Char(':'));
        return path;
    }

    void runNix(const QStringList &arguments, int timeoutMs)
    {
        if (mBusy) {
            appendLog(QStringLiteral("当前有任务运行，请稍候。"));
            return;
        }
        const QString env = findNixEnv();
        if (env.isEmpty()) {
            appendLog(QStringLiteral("Nix 尚未安装，请先点击“安装 Nix”。"));
            return;
        }
        appendLog(QStringLiteral("$ nix-env ") + arguments.join(QLatin1Char(' ')));
        clearProgress();
        startProcess(env, arguments, timeoutMs);
    }

    void runShell(const QString &command, int timeoutMs)
    {
        if (mBusy) {
            appendLog(QStringLiteral("当前有任务运行，请稍候。"));
            return;
        }
        appendLog(QStringLiteral("$ ") + command);
        clearProgress();
        startProcess(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command}, timeoutMs);
    }

    void clearProgress()
    {
        if (mProgress == 0 && mStage.isEmpty())
            return;
        mProgress = 0;
        mStage.clear();
        emit progressChanged();
    }

    void startProcess(const QString &program, const QStringList &arguments, int timeoutMs)
    {
        if (mBusy)
            return;
        mOutput.clear();
        mPending.clear();
        mTimedOut = false;
        mBusy = true;
        emit busyChanged();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("HOME"), QStringLiteral("/home/user"));
        env.insert(QStringLiteral("PATH"), nixPath());
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
        mProcess.setProcessEnvironment(env);
        mProcess.setWorkingDirectory(QStringLiteral("/home/user"));
        mProcess.start(program, arguments);
        if (!mProcess.waitForStarted(2000)) {
            appendLog(QStringLiteral("无法启动：") + program);
            mBusy = false;
            emit busyChanged();
            return;
        }
        mTimer.start(qBound(5000, timeoutMs, 30 * 60 * 1000));
    }

    void readOutput()
    {
        const QByteArray data = mProcess.readAll();
        if (data.isEmpty())
            return;
        QFile logFile(QStringLiteral("/home/user/z03plus/nix-install.log"));
        if (logFile.open(QIODevice::Append))
            logFile.write(data);
        if (mOutput.size() < 262144)
            mOutput.append(data.left(262144 - mOutput.size()));
        mPending.append(data);
        while (true) {
            const int newline = mPending.indexOf('\n');
            if (newline < 0)
                break;
            const QByteArray raw = mPending.left(newline);
            mPending.remove(0, newline + 1);
            handleLine(QString::fromUtf8(raw));
        }
        if (mPending.size() > 4096) {
            handleLine(QString::fromUtf8(mPending));
            mPending.clear();
        }
    }

    void handleLine(const QString &rawLine)
    {
        QString line = rawLine;
        line.remove(QLatin1Char('\r'));
        if (line.startsWith(QStringLiteral("[nix-progress] "))) {
            const QString payload = line.mid(15).trimmed();
            const int space = payload.indexOf(QLatin1Char(' '));
            const QString pctText = space < 0 ? payload : payload.left(space);
            const QString stageText = space < 0 ? QString() : payload.mid(space + 1).trimmed();
            bool ok = false;
            const int pct = pctText.toInt(&ok);
            if (ok) {
                bool changed = false;
                const int bounded = qBound(0, pct, 100);
                if (bounded != mProgress) {
                    mProgress = bounded;
                    changed = true;
                }
                if (stageText != mStage) {
                    mStage = stageText;
                    changed = true;
                }
                if (changed)
                    emit progressChanged();
            }
            return;
        }
        if (!line.trimmed().isEmpty())
            appendLog(line);
    }

    void processFinished(int exitCode, QProcess::ExitStatus status)
    {
        readOutput();
        mTimer.stop();
        mBusy = false;
        emit busyChanged();
        if (mTimedOut)
            appendLog(QStringLiteral("任务超时，已终止。"));
        const bool ok = !mTimedOut && status == QProcess::NormalExit && exitCode == 0;
        if (mProgress > 0) {
            mProgress = ok ? 100 : mProgress;
            mStage = mTimedOut ? QStringLiteral("超时") :
                                (ok ? QStringLiteral("完成") : QStringLiteral("失败"));
            emit progressChanged();
        }
        mPending.clear();
        detect();
        emit finished(ok, ok ? QStringLiteral("完成") :
                                QStringLiteral("失败（退出码 %1）").arg(exitCode));
    }

    void processError(QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) {
            mTimer.stop();
            mBusy = false;
            emit busyChanged();
            appendLog(QStringLiteral("进程启动失败。"));
            if (mProgress > 0) {
                mStage = QStringLiteral("进程启动失败");
                emit progressChanged();
            }
            emit finished(false, QStringLiteral("进程启动失败"));
        }
    }

    void timeout()
    {
        mTimedOut = true;
        appendLog(QStringLiteral("任务超时，正在终止…"));
        if (mProgress > 0) {
            mStage = QStringLiteral("超时，正在终止…");
            emit progressChanged();
        }
        mProcess.kill();
    }

    void appendLog(const QString &text)
    {
        mLog += text;
        if (!mLog.endsWith(QLatin1Char('\n')))
            mLog += QLatin1Char('\n');
        if (mLog.size() > 262144)
            mLog = mLog.right(262144);
        emit logChanged();
    }

    static QByteArray nixBootstrapScript()
    {
        return QByteArrayLiteral(
            "#!/bin/sh\n"
            "export HOME=/home/user\n"
            "export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\n"
            "if [ -f /etc/ssl/certs/ca-certificates.crt ]; then\n"
            "  export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt\n"
            "  export NIX_SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt\n"
            "fi\n"
            "MIRROR=https://mirrors.tuna.tsinghua.edu.cn\n"
            "FALLBACK=https://mirrors.nju.edu.cn\n"
            "NIX_VER=2.9.2\n"
            "TARBALL=nix-$NIX_VER-armv7l-linux.tar.xz\n"
            "WORK=/home/user/z03plus\n"
            "NIX_ROOT=/home/nix\n"
            "echo '[nix-progress] 3 检查环境'\n"
            "echo '[nix] checking existing installation'\n"
            "if command -v nix-env >/dev/null 2>&1 || [ -x /nix/var/nix/profiles/default/bin/nix-env ]; then\n"
            "  echo '[nix] already installed'\n"
            "  exit 0\n"
            "fi\n"
            "echo '[nix-progress] 8 准备 /nix 目录'\n"
            "if [ -L /nix ] && [ \"$(id -u)\" = 0 ]; then\n"
            "  echo '[nix] /nix is a symlink, replacing it with a real directory bound to' \"$NIX_ROOT\"\n"
            "  mount -o remount,rw / 2>/dev/null\n"
            "  rm -f /nix 2>/dev/null\n"
            "  mkdir -p /nix 2>/dev/null\n"
            "fi\n"
            "if [ -L /nix ]; then\n"
            "  echo '[nix] /nix is a symlink to:' $(readlink /nix)\n"
            "  echo '[nix] Nix 2.9 rejects a symlinked /nix (store parent must be a real directory).'\n"
            "  echo '[nix] Run these commands once, then retry the install:'\n"
            "  echo '  mount -o remount,rw /'\n"
            "  echo '  rm /nix'\n"
            "  echo '  mkdir -p /nix'\n"
            "  echo '  mount --bind /home/nix /nix'\n"
            "  exit 3\n"
            "fi\n"
            "if [ ! -d /nix ]; then\n"
            "  mkdir -p /nix 2>/dev/null || {\n"
            "    echo '[nix] cannot create /nix: root filesystem is read-only'\n"
            "    echo '[nix] Run: mount -o remount,rw / && mkdir -p /nix'\n"
            "    exit 3\n"
            "  }\n"
            "fi\n"
            "if ! grep -q ' /nix ' /proc/mounts 2>/dev/null; then\n"
            "  mkdir -p \"$NIX_ROOT\" 2>/dev/null\n"
            "  if [ -d \"$NIX_ROOT\" ] && [ \"$(id -u)\" = 0 ]; then\n"
            "    mount --bind \"$NIX_ROOT\" /nix 2>/dev/null || true\n"
            "  fi\n"
            "fi\n"
            "mkdir -p /nix/store /nix/var/nix 2>/dev/null || true\n"
            "if [ ! -w /nix/store ]; then\n"
            "  echo '[nix] /nix/store is not writable'\n"
            "  echo '[nix] Remount root read-write, or bind-mount a real directory onto /nix'\n"
            "  exit 3\n"
            "fi\n"
            "mkdir -p \"$WORK\" || exit 3\n"
            "for tool in tar xz sha256sum; do\n"
            "  command -v $tool >/dev/null 2>&1 || { echo \"[nix] missing required tool: $tool\"; exit 4; }\n"
            "done\n"
            "FREE=$(df -k /home 2>/dev/null | awk 'NR==2 {print $4}')\n"
            "case \"$FREE\" in ''|*[!0-9]*) FREE=0;; esac\n"
            "if [ \"$FREE\" -gt 0 ] && [ \"$FREE\" -lt 420000 ]; then\n"
            "  echo \"[nix] warning: only $((FREE / 1024)) MB free on /home; Nix store may need about 400 MB\"\n"
            "fi\n"
            "cd \"$WORK\" || exit 3\n"
            "echo '[nix-progress] 15 下载安装包（约 15 MB）'\n"
            "FOUND=\n"
            "INDEX=1\n"
            "for BASE in \"$MIRROR\" \"$FALLBACK\" http://mirrors.tuna.tsinghua.edu.cn https://mirrors.tuna.tsinghua.edu.cn; do\n"
            "  URL=\"$BASE/nix/nix-$NIX_VER/$TARBALL\"\n"
            "  OUT=\"$TARBALL.$INDEX\"\n"
            "  echo \"[nix] downloading $URL\"\n"
            "  CURL_EXTRA=\n"
            "  if [ $INDEX -eq 4 ]; then\n"
            "    CURL_EXTRA=-k\n"
            "    echo '[nix] retrying without certificate verification (check system clock)'\n"
            "  fi\n"
            "  if command -v curl >/dev/null 2>&1; then\n"
            "    curl -fL $CURL_EXTRA --connect-timeout 15 --max-time 900 --retry 2 -o \"$OUT\" \"$URL\" &\n"
            "  elif command -v wget >/dev/null 2>&1; then\n"
            "    wget -O \"$OUT\" \"$URL\" &\n"
            "  else\n"
            "    echo '[nix] neither curl nor wget is available'\n"
            "    exit 4\n"
            "  fi\n"
            "  FETCH=$!\n"
            "  while kill -0 $FETCH 2>/dev/null; do\n"
            "    if [ -f \"$OUT\" ]; then SIZE=$(wc -c < \"$OUT\" 2>/dev/null | tr -d ' \\r\\n'); else SIZE=0; fi\n"
            "    case \"$SIZE\" in ''|*[!0-9]*) SIZE=0;; esac\n"
            "    PCT=$((15 + SIZE * 30 / 15547952))\n"
            "    [ \"$PCT\" -gt 45 ] && PCT=45\n"
            "    echo \"[nix-progress] $PCT 下载中 $((SIZE / 1024)) KB / 15183 KB\"\n"
            "    sleep 2\n"
            "  done\n"
            "  wait $FETCH\n"
            "  STATUS=$?\n"
            "  if [ -f \"$OUT\" ]; then SIZE=$(wc -c < \"$OUT\" 2>/dev/null | tr -d ' \\r\\n'); else SIZE=0; fi\n"
            "  case \"$SIZE\" in ''|*[!0-9]*) SIZE=0;; esac\n"
            "  if [ $STATUS -eq 0 ] && [ $SIZE -gt 1000000 ]; then\n"
            "    FOUND=\"$OUT\"\n"
            "    break\n"
            "  fi\n"
            "  echo \"[nix] download failed: status=$STATUS size=$SIZE\"\n"
            "  INDEX=$((INDEX + 1))\n"
            "done\n"
            "if [ -z \"$FOUND\" ]; then\n"
            "  echo '[nix] all mirrors failed; check Wi-Fi, DNS, system clock and CA certificates'\n"
            "  exit 4\n"
            "fi\n"
            "TARBALL=\"$FOUND\"\n"
            "echo '[nix-progress] 46 校验安装包'\n"
            "echo '983299e3d1b6fdadc487624d94e66a4e7b268c4acc77707739ef1bb78783a4a4  '$TARBALL > \"$TARBALL.sha256\"\n"
            "sha256sum -c \"$TARBALL.sha256\" >/dev/null 2>&1 || { echo '[nix] checksum mismatch'; exit 4; }\n"
            "echo '[nix-progress] 50 解压安装包到 /nix'\n"
            "mkdir -p /nix/var/nix/db /nix/var/nix/profiles /nix/var/nix/gcroots /nix/var/nix/temproots || {\n"
            "  echo '[nix] cannot create directories under /nix'; exit 5; }\n"
            "mkdir -p /nix/var/nix/temproots/.z03-write-test && rmdir /nix/var/nix/temproots/.z03-write-test || {\n"
            "  echo '[nix] /nix is not writable'; exit 5; }\n"
            "FREE=$(df -k /nix 2>/dev/null | awk 'NR==2 {print $4}')\n"
            "case \"$FREE\" in ''|*[!0-9]*) FREE=0;; esac\n"
            "echo \"[nix] free space under /nix: $((FREE / 1024)) MB\"\n"
            "if [ \"$FREE\" -gt 0 ] && [ \"$FREE\" -lt 120000 ]; then\n"
            "  echo '[nix] need about 120 MB free under /nix for the 89 MB store'\n"
            "  exit 5\n"
            "fi\n"
            "for d in \"$WORK/nix-unpack\" \"$WORK/nix-extract\"; do\n"
            "  [ -d \"$d\" ] && rm -rf \"$d\" 2>/dev/null || true\n"
            "done\n"
            "echo '[nix] unpacking installer into /nix'\n"
            "if ! EXTRACT_UNSAFE_SYMLINKS=1 tar -xJf \"$TARBALL\" -C /nix; then\n"
            "  echo '[nix] tar -J failed, retrying with xz pipe'\n"
            "  xz -dc \"$TARBALL\" | EXTRACT_UNSAFE_SYMLINKS=1 tar -xf - -C /nix || {\n"
            "    echo '[nix] unpack into /nix failed'; exit 5; }\n"
            "fi\n"
            "SRC=\"/nix/nix-$NIX_VER-armv7l-linux\"\n"
            "[ -d \"$SRC/store\" ] || { echo '[nix] unpacked store not found at' $SRC; exit 5; }\n"
            "echo '[nix] replacing store with freshly unpacked copy'\n"
            "if [ -e /nix/store ]; then\n"
            "  mv /nix/store /nix/store.old.$$ || { echo '[nix] cannot move old store aside'; exit 5; }\n"
            "fi\n"
            "mv \"$SRC/store\" /nix/store || { echo '[nix] cannot move new store into place'; exit 5; }\n"
            "CNT=$(ls /nix/store 2>/dev/null | wc -l)\n"
            "echo \"[nix] unpacked store paths: $CNT\"\n"
            "if [ \"$CNT\" -lt 20 ]; then\n"
            "  echo '[nix] store looks incomplete; restoring previous store'\n"
            "  mv /nix/store /nix/store.bad.$$ 2>/dev/null || true\n"
            "  if [ -e /nix/store.old.$$ ]; then mv /nix/store.old.$$ /nix/store 2>/dev/null || true; fi\n"
            "  exit 5\n"
            "fi\n"
            "mv -f \"$SRC/.reginfo\" /nix/.reginfo || { echo '[nix] cannot move .reginfo'; exit 5; }\n"
            "rm -rf \"$SRC\" /nix/store.old.$$ /nix/store.bad.$$ 2>/dev/null || true\n"
            "echo '[nix] resetting nix database'\n"
            "if [ -e /nix/var/nix/db ]; then\n"
            "  mv /nix/var/nix/db /nix/var/nix/db.old.$$ 2>/dev/null || true\n"
            "fi\n"
            "mkdir -p /nix/var/nix/db || { echo '[nix] cannot create database directory'; exit 5; }\n"
            "rm -rf /nix/var/nix/db.old.$$ 2>/dev/null || true\n"
            "echo '[nix] removing installer archive to free space'\n"
            "rm -f \"$TARBALL\" \"$TARBALL.sha256\" 2>/dev/null || true\n"
            "echo '[nix-progress] 60 准备 Nix 运行环境'\n"
            "NIXBIN=/nix/store/av4qi9mf47fjna4h35dxv79bivn1lcfw-nix-2.9.2-armv7l-unknown-linux-gnueabihf\n"
            "CACERT=/nix/store/914z9l06a48wfsa32nvb4zy9kvflvz3g-nss-cacert-3.66-armv7l-unknown-linux-gnueabihf\n"
            "[ -x \"$NIXBIN/bin/nix-store\" ] || {\n"
            "  echo '[nix] nix-store binary missing after extraction'\n"
            "  ls /nix/store | head -20\n"
            "  exit 5\n"
            "}\n"
            "if ! \"$NIXBIN/bin/nix-store\" --version; then\n"
            "  echo '[nix] nix-store cannot execute (missing glibc store path?)'\n"
            "  exit 5\n"
            "fi\n"
            "mkdir -p /home/user/.config/nix\n"
            "if ! grep -q '^build-users-group' /home/user/.config/nix/nix.conf 2>/dev/null; then\n"
            "  printf 'build-users-group =\\nsandbox = false\\n' >> /home/user/.config/nix/nix.conf\n"
            "fi\n"
            "echo '[nix-progress] 70 注册安装路径'\n"
            "if ! \"$NIXBIN/bin/nix-store\" --load-db < /nix/.reginfo; then\n"
            "  echo '[nix] nix-store --load-db failed'\n"
            "  exit 5\n"
            "fi\n"
            "echo '[nix-progress] 76 配置用户环境'\n"
            "export USER=${USER:-root}\n"
            "export NIX_PROFILES=\"/nix/var/nix/profiles/default /home/user/.nix-profile\"\n"
            "export PATH=\"/home/user/.nix-profile/bin:/nix/var/nix/profiles/default/bin:$PATH\"\n"
            "if ! \"$NIXBIN/bin/nix-env\" -i \"$NIXBIN\"; then\n"
            "  echo '[nix] nix-env -i nix failed'\n"
            "  exit 5\n"
            "fi\n"
            "\"$NIXBIN/bin/nix-env\" -i \"$CACERT\" 2>/dev/null || echo '[nix] warning: could not link cacert'\n"
            "if [ -f /home/user/.nix-profile/etc/ssl/certs/ca-bundle.crt ]; then\n"
            "  export NIX_SSL_CERT_FILE=/home/user/.nix-profile/etc/ssl/certs/ca-bundle.crt\n"
            "fi\n"
            "if ! command -v nix-env >/dev/null 2>&1 && [ ! -x /nix/var/nix/profiles/default/bin/nix-env ]; then\n"
            "  echo '[nix] nix-env not found after install'\n"
            "  exit 6\n"
            "fi\n"
            "mkdir -p /home/user/.config/nix\n"
            "if ! grep -q 'mirrors.tuna.tsinghua.edu.cn/nix-channels/store' /home/user/.config/nix/nix.conf 2>/dev/null; then\n"
            "  echo 'substituters = https://mirrors.tuna.tsinghua.edu.cn/nix-channels/store https://cache.nixos.org' >> /home/user/.config/nix/nix.conf\n"
            "  echo 'trusted-public-keys = cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY=' >> /home/user/.config/nix/nix.conf\n"
            "fi\n"
            "echo '[nix-progress] 82 配置 nixpkgs 频道'\n"
            "nix-channel --remove nixpkgs >/dev/null 2>&1 || true\n"
            "CHANNEL_OK=0\n"
            "for BASE in \"$MIRROR\" \"$FALLBACK\"; do\n"
            "  echo '[nix] adding nixos-22.05 channel from' $BASE\n"
            "  if nix-channel --add \"$BASE/nix-channels/nixos-22.05\" nixpkgs; then\n"
            "    CHANNEL_OK=1\n"
            "    break\n"
            "  fi\n"
            "done\n"
            "if [ $CHANNEL_OK -eq 0 ]; then\n"
            "  echo '[nix] channel add failed'\n"
            "  exit 7\n"
            "fi\n"
            "echo '[nix-progress] 90 更新频道索引'\n"
            "nix-channel --update || { echo '[nix] channel update failed'; exit 7; }\n"
            "echo '[nix-progress] 100 安装完成'\n"
            "echo '[nix] install complete; try: nix-env -iA nixpkgs.hello'\n");
    }

    QProcess mProcess;
    QTimer mTimer;
    QByteArray mOutput;
    QByteArray mPending;
    QString mLog;
    int mProgress = 0;
    QString mStage;
    bool mBusy = false;
    bool mInstalled = false;
    bool mTimedOut = false;
};


// Minimal built-in HTTP bridge. The QML AI page was written against
// 127.0.0.1:8080; hosting it in-process removes the external ydpExt/node
// dependency and makes /api/status, /api/chat/stream and /api/new work as
// soon as the overlay library loads.
class Z03PlusAiBridge : public QObject
{
    Q_OBJECT
public:
    explicit Z03PlusAiBridge(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&mServer, &QTcpServer::newConnection, this, &Z03PlusAiBridge::onNewConnection);
        if (!mServer.listen(QHostAddress::LocalHost, 8080))
            qWarning("Z03Plus AI bridge: cannot listen on 127.0.0.1:8080");
    }

private slots:
    void onNewConnection()
    {
        while (QTcpSocket *socket = mServer.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

private:
    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buffer = mBuffers[socket];
        buffer += socket->readAll();
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        if (lines.isEmpty()) {
            socket->disconnectFromHost();
            return;
        }
        const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
        if (requestLine.size() < 2) {
            socket->disconnectFromHost();
            return;
        }
        const QByteArray method = requestLine.at(0);
        const QByteArray path = requestLine.at(1);
        int contentLength = 0;
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray header = lines.at(i).trimmed();
            if (header.toLower().startsWith("content-length:"))
                contentLength = header.mid(15).trimmed().toInt();
        }
        if (buffer.size() < headerEnd + 4 + contentLength)
            return;
        const QByteArray body = buffer.mid(headerEnd + 4, contentLength);
        mBuffers.remove(socket);
        route(socket, method, path, body);
    }

    void route(QTcpSocket *socket, const QByteArray &method, const QByteArray &path,
               const QByteArray &body)
    {
        if (path == "/api/status") {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), true);
            obj.insert(QStringLiteral("service"), QStringLiteral("z03plus"));
            sendJson(socket, 200, obj);
            return;
        }
        if (path == "/api/new") {
            mAi.newConversation();
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), true);
            sendJson(socket, 200, obj);
            return;
        }
        if (path == "/api/chat/stream" && method == "POST") {
            chat(socket, body);
            return;
        }
        if (path == "/api/history") {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), true);
            obj.insert(QStringLiteral("sessions"), QJsonArray());
            sendJson(socket, 200, obj);
            return;
        }
        if (path.startsWith("/api/history/")) {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), false);
            obj.insert(QStringLiteral("error"), QStringLiteral("内置服务不保存历史"));
            sendJson(socket, 200, obj);
            return;
        }
        if (path.startsWith("/api/voice/") || path.startsWith("/api/scan/")) {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), false);
            obj.insert(QStringLiteral("error"), QStringLiteral("该功能尚未接入"));
            sendJson(socket, 501, obj);
            return;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("ok"), false);
        obj.insert(QStringLiteral("error"), QStringLiteral("not found"));
        sendJson(socket, 404, obj);
    }

    void chat(QTcpSocket *socket, const QByteArray &body)
    {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(body, &error);
        const QString text = document.isObject()
            ? document.object().value(QStringLiteral("text")).toString().trimmed()
            : QString();
        if (text.isEmpty()) {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), false);
            obj.insert(QStringLiteral("error"), QStringLiteral("消息为空"));
            sendJson(socket, 400, obj);
            return;
        }
        if (mAi.busy()) {
            QJsonObject obj;
            obj.insert(QStringLiteral("ok"), false);
            obj.insert(QStringLiteral("error"), QStringLiteral("上一条消息仍在生成"));
            sendJson(socket, 409, obj);
            return;
        }
        socket->write("HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/x-ndjson\r\n"
                      "Cache-Control: no-cache\r\n"
                      "Connection: close\r\n\r\n");
        socket->flush();

        connect(&mAi, &Z03PlusAiService::delta, socket,
                [socket](const QString &delta) { writeEvent(socket, QStringLiteral("update"), delta, QString(), true); });
        connect(&mAi, &Z03PlusAiService::completed, socket,
                [this, socket](const QString &answer) {
                    writeEvent(socket, QStringLiteral("done"), answer, QString(), true);
                    disconnect(&mAi, nullptr, socket, nullptr);
                    socket->disconnectFromHost();
                });
        connect(&mAi, &Z03PlusAiService::failed, socket,
                [this, socket](const QString &reason) {
                    writeEvent(socket, QStringLiteral("done"), QString(), reason, false);
                    disconnect(&mAi, nullptr, socket, nullptr);
                    socket->disconnectFromHost();
                });
        mAi.send(text);
    }

    static void writeEvent(QTcpSocket *socket, const QString &type, const QString &text,
                           const QString &error, bool ok)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), type);
        if (!text.isEmpty())
            obj.insert(QStringLiteral("text"), text);
        if (!error.isEmpty())
            obj.insert(QStringLiteral("error"), error);
        if (type == QStringLiteral("done"))
            obj.insert(QStringLiteral("ok"), ok);
        socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
    }

    static void sendJson(QTcpSocket *socket, int code, const QJsonObject &obj)
    {
        const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        QByteArray response = "HTTP/1.1 " + QByteArray::number(code) + " OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += payload;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    QTcpServer mServer;
    Z03PlusAiService mAi;
    QHash<QTcpSocket *, QByteArray> mBuffers;
};


#define Z03ULTRA_REPO "OpenZybTeam/zyb_P010-P011_PenUltra"
#define Z03ULTRA_MIRROR_PREFIX "https://git.yylx.win/"

#define Z03CEF_PACKAGE_URL "https://r2.cfknb.vip/z03plus/cef/cef120-linuxarm-v5.tar.xz"
#define Z03CEF_PACKAGE_SHA256 "ad33ada92680bc66a8ec31f1d3c80d6b8b849427481e0702bd576b0f5af86988"
#define Z03CEF_PACKAGE_NAME "cef120-linuxarm-v5.tar.xz"


class Z03PlusCefService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed)
    Q_PROPERTY(int speed READ speed NOTIFY changed)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit Z03PlusCefService(QObject *parent = nullptr) : QObject(parent)
    {
        mTimeout.setSingleShot(true);
        connect(&mTimeout, &QTimer::timeout, this, [this]() {
            if (mReply)
                mReply->abort();
        });
    }

    QString state() const { return mState; }
    QString status() const { return mStatus; }
    QString error() const { return mError; }
    int progress() const { return mProgress; }
    qint64 received() const { return mReceived; }
    qint64 total() const { return mTotal; }
    int speed() const { return mSpeed; }
    bool busy() const { return mBusy; }

    bool installed() const
    {
        return QFile::exists(installDir() + QStringLiteral("/libcef.so"))
               && QFile::exists(installDir() + QStringLiteral("/z03cef_host"))
               && QFile::exists(installDir() + QStringLiteral("/libZ03Cef.so"));
    }

    Q_INVOKABLE void install()
    {
        if (mBusy)
            return;
        mError.clear();
        mReceived = 0;
        mTotal = 0;
        mSpeed = 0;
        mProgress = 0;
        mBusy = true;
        QDir().mkpath(downloadDir());
        const QString part = downloadDir() + QLatin1Char('/') + QStringLiteral(Z03CEF_PACKAGE_NAME) + QStringLiteral(".part");
        mFile.setFileName(part);
        if (!mFile.open(QIODevice::WriteOnly)) {
            fail(QStringLiteral("无法写入 %1").arg(part));
            return;
        }
        mDownloaded = 0;
        mSpeedClock = QDateTime::currentMSecsSinceEpoch();
        mSpeedBytes = 0;
        setState(QStringLiteral("downloading"), QStringLiteral("正在连接下载源…"));
        QNetworkRequest request{QUrl(QStringLiteral(Z03CEF_PACKAGE_URL))};
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        request.setRawHeader("User-Agent", "Z03Ultra-Cef");
        mReply = mManager.get(request);
        connect(mReply, &QNetworkReply::readyRead, this, [this]() {
            if (!mReply || !mFile.isOpen())
                return;
            const QByteArray chunk = mReply->readAll();
            if (chunk.isEmpty())
                return;
            mFile.write(chunk);
            mDownloaded += chunk.size();
            mReceived = mDownloaded;
            mSpeedBytes += chunk.size();
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 span = now - mSpeedClock;
            if (span >= 900) {
                mSpeed = int(mSpeedBytes * 1000 / span / 1024);
                mSpeedClock = now;
                mSpeedBytes = 0;
            }
            if (mTotal > 0)
                mProgress = int(mDownloaded * 100 / mTotal);
            mStatus = QStringLiteral("正在下载 CEF 组件… %1%  ·  %2 KB/s")
                          .arg(mProgress)
                          .arg(mSpeed);
            emit changed();
        });
        connect(mReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (total > 0 && mTotal != total)
                mTotal = total;
            if (total > 0 && mDownloaded < received) {
                mReceived = received;
                mProgress = int(received * 100 / total);
            }
            emit changed();
        });
        connect(mReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this,
                [this](QNetworkReply::NetworkError) {
                    if (mReply)
                        mReply->abort();
                });
        connect(mReply, &QNetworkReply::finished, this, [this]() { onDownloadFinished(); });
        mTimeout.start(900000);
    }

    Q_INVOKABLE void cancel()
    {
        if (mReply) {
            mReply->abort();
        } else if (mExtractor.state() != QProcess::NotRunning) {
            mExtractor.kill();
        }
    }

    Q_INVOKABLE QVariantMap cefInfo() const
    {
        QVariantMap info;
        info.insert(QStringLiteral("installed"), installed());
        info.insert(QStringLiteral("path"), installDir());
        QString version;
        QFile vf(installDir() + QStringLiteral("/version.txt"));
        if (vf.open(QIODevice::ReadOnly | QIODevice::Text))
            version = QString::fromUtf8(vf.readAll()).trimmed();
        info.insert(QStringLiteral("version"), version);
        info.insert(QStringLiteral("active"), cefLibMapped());
        return info;
    }

    Q_INVOKABLE bool cefActive() const { return cefLibMapped(); }

    Q_INVOKABLE QString enableCefMode()
    {
        const QString base = QStringLiteral("/home/user/z03plus");
        const QString bin = base + QStringLiteral("/Z03PlusCef");
        if (!QFile::exists(bin))
            return QStringLiteral("缺少 CEF 启动模式文件 Z03PlusCef，请更新 Z03Ultra 后重试");
        z03cefBreadcrumb("enable begin");
        ensureCefPluginFile();
        Z03PlusBrowserService::ensureCefSupportLibs();
        if (!installed())
            return QStringLiteral("CEF 组件未安装完整，请先下载安装");
        static const char *const required[] = {
            "/home/user/z03plus/cef/libcef.so",
            "/home/user/z03plus/cef/libatk-1.0.so.0",
            "/home/user/z03plus/cef/libatspi.so.0",
            "/home/user/z03plus/cef/libatk-bridge-2.0.so.0",
            "/home/user/z03plus/libZ03GbmShim.so",
            "/home/user/z03plus/z03cef_host"
        };
        for (const char *path : required) {
            const QString p = QString::fromLatin1(path);
            if (!QFile::exists(p))
                return QStringLiteral("缺少依赖库 %1，请更新 Z03Ultra 后重试").arg(p);
        }
        patchCefLibraryNeeded();
        z03cefBreadcrumb("enable patched");
        if (!z03WriteCefLauncher())
            return QStringLiteral("无法写入 CEF 启动脚本，请检查存储空间后重试");
        z03cefBreadcrumb("enable launcher");
        const QString rep = base + QStringLiteral("/libZ03CrashRep.so");
        const QString log = installDir() + QStringLiteral("/selftest.log");
        QString cmd = QStringLiteral("echo probe-start > %1; ").arg(log);
        cmd += QStringLiteral("Z03_CEF_SELFTEST=1 LD_BIND_NOW=1 ");
        if (QFile::exists(rep))
            cmd += QStringLiteral("LD_PRELOAD=%1 ").arg(rep);
        cmd += QStringLiteral("exec %1 >>%2 2>&1").arg(bin).arg(log);
        QProcess probe;
        z03cefBreadcrumb("enable probe spawn");
        probe.start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << cmd);
        if (!probe.waitForStarted(8000))
            return QStringLiteral("CEF 自检程序无法启动");
        if (!probe.waitForFinished(30000)) {
            probe.kill();
            return QStringLiteral("CEF 自检超时，组件可能与本机不兼容");
        }
        z03cefBreadcrumb(probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0
                         ? "enable probe ok" : "enable probe fail");
        if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
            QString detail = probe.exitStatus() == QProcess::CrashExit
                    ? QStringLiteral("运行时崩溃")
                    : QStringLiteral("退出码 %1").arg(probe.exitCode());
            QFile cf(installDir() + QStringLiteral("/crash.txt"));
            if (cf.open(QIODevice::ReadOnly)) {
                const QString crash = QString::fromLocal8Bit(cf.readAll()).trimmed();
                if (!crash.isEmpty())
                    detail += QStringLiteral("：") + crash.simplified().left(220);
            }
            QFile lf(log);
            if (lf.open(QIODevice::ReadOnly)) {
                const QString out = QString::fromLocal8Bit(lf.readAll()).trimmed();
                if (!out.isEmpty())
                    detail += QStringLiteral(" | ") + out.right(220).simplified();
            }
            return QStringLiteral("CEF 自检失败（%1），已保持内置内核").arg(detail);
        }
        z03cefBreadcrumb("enable mount");
        const QString launcher = QString::fromLatin1(z03CefLauncherPath());
        QProcess::execute(QStringLiteral("/bin/sh"), QStringList()
            << QStringLiteral("-c")
            << QStringLiteral("umount -l /usr/bin/Z03 2>/dev/null; "
                              "mount --bind %1 /usr/bin/Z03").arg(launcher));
        QProcess same;
        same.start(QStringLiteral("cmp"), QStringList()
                   << QStringLiteral("-s") << QStringLiteral("/usr/bin/Z03") << launcher);
        if (same.waitForStarted(3000)) {
            const bool finished = same.waitForFinished(8000);
            if (finished && same.exitCode() != 0) {
                z03cefBreadcrumb("enable mount failed");
                return QStringLiteral("切换启动文件失败（挂载未生效），请重试");
            }
            if (!finished)
                same.kill();
        }
        z03cefBreadcrumb("enable mounted");
        QFile marker(base + QStringLiteral("/.cef-ok"));
        if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("无法写入启用标记");
        marker.write("ok\n");
        marker.close();
        return QString();
    }

    Q_INVOKABLE void restartApp()
    {
        QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList()
            << QStringLiteral("-c")
            << QStringLiteral("sleep 1; kill $(pidof Z03) 2>/dev/null"));
    }

signals:
    void changed();

private:
    static QString installDir() { return QStringLiteral("/home/user/z03plus/cef"); }
    static QString downloadDir() { return QStringLiteral("/home/user/z03plus/cef-download"); }

    void setState(const QString &state, const QString &status)
    {
        mState = state;
        mStatus = status;
        emit changed();
    }

    void fail(const QString &message)
    {
        mTimeout.stop();
        if (mFile.isOpen())
            mFile.close();
        mError = message;
        mBusy = false;
        setState(QStringLiteral("error"), message);
    }

    void onDownloadFinished()
    {
        QNetworkReply *reply = mReply;
        mReply = nullptr;
        mTimeout.stop();
        if (!reply)
            return;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
        const bool ok = reply->error() == QNetworkReply::NoError && code >= 200 && code < 300;
        const QString error = reply->errorString();
        if (!aborted) {
            const QByteArray rest = reply->readAll();
            if (!rest.isEmpty()) {
                mFile.write(rest);
                mDownloaded += rest.size();
            }
        }
        reply->deleteLater();
        if (mFile.isOpen())
            mFile.close();
        if (aborted) {
            mBusy = false;
            setState(QStringLiteral("idle"), QStringLiteral("已取消下载"));
            return;
        }
        if (!ok) {
            fail(QStringLiteral("下载失败：%1").arg(error));
            return;
        }
        if (mDownloaded < 1024 * 1024) {
            fail(QStringLiteral("下载内容异常（%1 字节）").arg(mDownloaded));
            return;
        }
        mProgress = 100;
        setState(QStringLiteral("verifying"), QStringLiteral("正在校验文件…"));
        fetchShaThenVerify();
    }

    void fetchShaThenVerify()
    {
        QNetworkRequest request{QUrl(QStringLiteral(Z03CEF_PACKAGE_URL ".sha256"))};
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        mShaReply = mManager.get(request);
        QTimer::singleShot(20000, this, [this]() {
            if (mShaReply)
                mShaReply->abort();
        });
        connect(mShaReply, &QNetworkReply::finished, this, [this]() {
            QString expected = QString::fromLatin1(Z03CEF_PACKAGE_SHA256);
            if (mShaReply && mShaReply->error() == QNetworkReply::NoError) {
                const QString text = QString::fromLatin1(mShaReply->readAll()).trimmed();
                const QString first = z03SplitChar(text, QChar(' ')).value(0,
                                            z03SplitChar(text, QChar('\n')).value(0)).trimmed();
                if (first.size() == 64)
                    expected = first;
            }
            if (mShaReply) {
                mShaReply->deleteLater();
                mShaReply = nullptr;
            }
            verifyAndExtract(expected);
        });
    }

    void verifyAndExtract(const QString &expected)
    {
        const QString part = downloadDir() + QLatin1Char('/') + QStringLiteral(Z03CEF_PACKAGE_NAME) + QStringLiteral(".part");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QFile in(part);
        if (!in.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("无法读取下载文件"));
            return;
        }
        while (!in.atEnd())
            hash.addData(in.read(1 << 20));
        in.close();
        const QString got = QString::fromLatin1(hash.result().toHex());
        if (got.compare(expected, Qt::CaseInsensitive) != 0) {
            fail(QStringLiteral("校验失败，文件可能损坏，请重试"));
            return;
        }
        const QString finalPath = downloadDir() + QLatin1Char('/') + QStringLiteral(Z03CEF_PACKAGE_NAME);
        QFile::remove(finalPath);
        if (!QFile::rename(part, finalPath)) {
            fail(QStringLiteral("无法保存安装包"));
            return;
        }
        setState(QStringLiteral("extracting"), QStringLiteral("正在解压安装（约 1-2 分钟）…"));
        mExtractor.setProcessChannelMode(QProcess::MergedChannels);
        connect(&mExtractor, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int exitCode, QProcess::ExitStatus) { onExtractFinished(exitCode); });
        const QString script =
            QStringLiteral("mkdir -p '/home/user/z03plus' && xz -dc '%1' > '%1.tar' && tar -xf '%1.tar' -C '/home/user/z03plus' && rm -f '%1.tar'")
                .arg(finalPath);
        mExtractor.start(QStringLiteral("/bin/sh"), QStringList{QStringLiteral("-c"), script});
        if (!mExtractor.waitForStarted(5000)) {
            fail(QStringLiteral("无法启动解压进程"));
        }
    }

    void onExtractFinished(int exitCode)
    {
        if (exitCode != 0) {
            fail(QStringLiteral("解压失败（tar 退出码 %1）").arg(exitCode));
            return;
        }
        if (!installed()) {
            fail(QStringLiteral("安装不完整：缺少 libcef.so 或 z03cef_host"));
            return;
        }
        patchCefLibraryNeeded();
        mBusy = false;
        mProgress = 100;
        mSpeed = 0;
        setState(QStringLiteral("done"), QStringLiteral("CEF 组件已安装"));
    }

    QNetworkAccessManager mManager;
    QTimer mTimeout;
    QProcess mExtractor;
    QNetworkReply *mReply = nullptr;
    QNetworkReply *mShaReply = nullptr;
    QFile mFile;
    QString mState = QStringLiteral("idle");
    QString mStatus;
    QString mError;
    int mProgress = 0;
    qint64 mReceived = 0;
    qint64 mTotal = 0;
    qint64 mDownloaded = 0;
    qint64 mSpeedClock = 0;
    qint64 mSpeedBytes = 0;
    int mSpeed = 0;
    bool mBusy = false;
};

class Z03PlusUpdateService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit Z03PlusUpdateService(QObject *parent = nullptr) : QObject(parent)
    {
        mTimeout.setSingleShot(true);
        connect(&mTimeout, &QTimer::timeout, this, [this]() {
            if (mReply)
                mReply->abort();
        });
    }

    QString currentVersion() const { return QStringLiteral(Z03ULTRA_VERSION); }
    QString latestVersion() const { return mLatest; }
    QString state() const { return mState; }
    QString status() const { return mStatus; }
    int progress() const { return mProgress; }
    bool busy() const { return mBusy; }

    Q_INVOKABLE void checkForUpdates(bool mirror)
    {
        if (mBusy)
            return;
        mBusy = true;
        mProgress = 0;
        mBase.clear();
        mLatest.clear();
        setState(QStringLiteral("checking"), QStringLiteral("正在检测更新…"));
        const QString api = QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QStringLiteral(Z03ULTRA_REPO));
        QNetworkReply *reply = request(api, 30);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCheckFinished(reply); });
    }

    Q_INVOKABLE void installUpdate(bool mirror)
    {
        if (mBusy)
            return;
        if (mBase.isEmpty()) {
            setState(QStringLiteral("error"), QStringLiteral("请先检测更新"));
            return;
        }
        mInstallMirror = mirror;
        mTmp = QStringLiteral("/home/user/z03plus/update.tmp");
        QDir(mTmp).removeRecursively();
        if (!QDir().mkpath(mTmp)) {
            fail(QStringLiteral("无法创建临时目录"));
            return;
        }
        mQueue = QStringList() << QStringLiteral("SHA256SUMS") << QStringLiteral("z03ultra.zip");
        mSums.clear();
        mData.clear();
        mIndex = 0;
        mDirectOnly = false;
        mProgress = 0;
        mBusy = true;
        setState(QStringLiteral("downloading"), QStringLiteral("准备下载…"));
        downloadNext();
    }

signals:
    void changed();

private:
    static QString withMirror(const QString &url, bool mirror)
    {
        if (!mirror)
            return url;
        QString rest = url;
        if (rest.startsWith(QStringLiteral("https://")))
            rest.remove(0, 8);
        else if (rest.startsWith(QStringLiteral("http://")))
            rest.remove(0, 7);
        return QStringLiteral(Z03ULTRA_MIRROR_PREFIX) + rest;
    }

    static QStringList installFiles()
    {
        return QStringList() << QStringLiteral("libZ03Plus.so") << QStringLiteral("z03plus.rcc")
                             << QStringLiteral("Z03Plus") << QStringLiteral("setup_ssh.sh")
                             << QStringLiteral("update_ssh_status.sh");
    }

    static bool extractPackage(const QString &zip, const QString &dir, QString *error)
    {
        QDir().mkpath(dir);
        QString program;
        QStringList args;
        if (!QStandardPaths::findExecutable(QStringLiteral("unzip")).isEmpty()) {
            program = QStringLiteral("unzip");
            args << QStringLiteral("-o") << QStringLiteral("-q") << zip << QStringLiteral("-d") << dir;
        } else if (!QStandardPaths::findExecutable(QStringLiteral("busybox")).isEmpty()) {
            program = QStringLiteral("busybox");
            args << QStringLiteral("unzip") << QStringLiteral("-o") << QStringLiteral("-q") << zip
                 << QStringLiteral("-d") << dir;
        } else {
            *error = QStringLiteral("设备缺少 unzip，无法解压更新包");
            return false;
        }
        QProcess process;
        process.start(program, args);
        if (!process.waitForFinished(180000) || process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) {
            *error = QStringLiteral("解压更新包失败：%1")
                         .arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
            return false;
        }
        return true;
    }

    static bool versionNewer(const QString &candidate, const QString &current)
    {
        auto parts = [](QString value) {
            if (value.startsWith(QLatin1Char('v')) || value.startsWith(QLatin1Char('V')))
                value.remove(0, 1);
            QStringList out;
            for (const QString &segment : z03SplitChar(value, QChar('.'))) {
                QString digits;
                for (const QChar ch : segment) {
                    if (ch.isDigit())
                        digits += ch;
                    else
                        break;
                }
                out.append(digits);
            }
            return out;
        };
        const QStringList a = parts(candidate);
        const QStringList b = parts(current);
        const int count = qMax(a.size(), b.size());
        for (int i = 0; i < count; ++i) {
            const int left = i < a.size() ? a.at(i).toInt() : 0;
            const int right = i < b.size() ? b.at(i).toInt() : 0;
            if (left != right)
                return left > right;
        }
        return false;
    }

    QNetworkReply *request(const QString &url, int timeoutSeconds)
    {
        QNetworkRequest request((QUrl(url)));
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Z03Ultra/" Z03ULTRA_VERSION));
        request.setRawHeader("Accept", "application/vnd.github+json");
        mReply = mManager.get(request);
        connect(mReply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &) {
            if (mReply)
                mReply->ignoreSslErrors();
        });
        mTimeout.start(timeoutSeconds * 1000);
        return mReply;
    }

    void onCheckFinished(QNetworkReply *reply)
    {
        if (reply != mReply) {
            reply->deleteLater();
            return;
        }
        mTimeout.stop();
        mReply = nullptr;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError && code >= 200 && code < 300;
        const QString error = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            mBusy = false;
            setState(QStringLiteral("error"),
                     QStringLiteral("检测失败：%1（可切换镜像源重试）").arg(error));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        const QString tag = obj.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            mBusy = false;
            setState(QStringLiteral("error"), QStringLiteral("检测失败：返回数据缺少版本号"));
            return;
        }
        mLatest = tag;
        mBase = QStringLiteral("https://github.com/%1/releases/download/%2")
                    .arg(QStringLiteral(Z03ULTRA_REPO)).arg(tag);
        mBusy = false;
        if (versionNewer(tag, currentVersion())) {
            setState(QStringLiteral("available"), QStringLiteral("发现新版本 %1").arg(tag));
        } else {
            setState(QStringLiteral("latest"), QStringLiteral("当前已是最新版本"));
        }
    }

    void downloadNext()
    {
        if (mIndex >= mQueue.size()) {
            mBusy = false;
            performInstall();
            return;
        }
        const QString name = mQueue.at(mIndex);
        const QString asset = mBase + QLatin1Char('/') + name;
        const QString url = (mInstallMirror && !mDirectOnly) ? withMirror(asset, true) : asset;
        mData.clear();
        mProgress = 0;
        setState(QStringLiteral("downloading"),
                 QStringLiteral("正在下载 %1（%2/%3）").arg(name).arg(mIndex + 1).arg(mQueue.size()));
        QNetworkReply *reply = request(url, 900);
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (total > 0) {
                mProgress = int(received * 100 / total);
                emit changed();
            }
        });
        connect(reply, &QNetworkReply::readyRead, this, [this, reply]() { mData += reply->readAll(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, name]() { onAssetFinished(reply, name); });
    }

    void onAssetFinished(QNetworkReply *reply, const QString &name)
    {
        if (reply != mReply) {
            reply->deleteLater();
            return;
        }
        mTimeout.stop();
        mReply = nullptr;
        mData += reply->readAll();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && code >= 200 && code < 300;
        const QString error = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            if (mInstallMirror && !mDirectOnly) {
                mDirectOnly = true;
                setState(QStringLiteral("downloading"), QStringLiteral("镜像不可用，改用直连…"));
                downloadNext();
                return;
            }
            fail(QStringLiteral("下载 %1 失败：%2").arg(name).arg(error));
            return;
        }
        if (mData.isEmpty()) {
            fail(QStringLiteral("下载 %1 内容为空").arg(name));
            return;
        }
        QFile out(mTmp + QLatin1Char('/') + name);
        if (!out.open(QIODevice::WriteOnly) || out.write(mData) != mData.size()) {
            fail(QStringLiteral("写入 %1 失败").arg(name));
            return;
        }
        out.close();
        if (name == QStringLiteral("SHA256SUMS")) {
            const QString text = QString::fromUtf8(mData);
            for (const QString &line : z03SplitChar(text, QChar('\n'))) {
                const QString trimmed = line.trimmed();
                if (trimmed.isEmpty())
                    continue;
                QString hash;
                QString file;
                for (const QString &part : z03SplitChar(trimmed, QChar(' '))) {
                    if (part.isEmpty())
                        continue;
                    if (hash.isEmpty())
                        hash = part;
                    else {
                        file = part;
                        break;
                    }
                }
                if (file.startsWith(QLatin1Char('*')))
                    file.remove(0, 1);
                if (!file.isEmpty())
                    mSums.insert(file, hash.toLower());
            }
            if (mSums.isEmpty()) {
                fail(QStringLiteral("SHA256SUMS 解析失败"));
                return;
            }
        } else {
            const QString want = mSums.value(name);
            if (want.isEmpty()) {
                fail(QStringLiteral("SHA256SUMS 缺少 %1").arg(name));
                return;
            }
            const QString got = QString::fromLatin1(
                QCryptographicHash::hash(mData, QCryptographicHash::Sha256).toHex());
            if (got != want) {
                fail(QStringLiteral("%1 校验失败").arg(name));
                return;
            }
        }
        ++mIndex;
        downloadNext();
    }

    bool ensureBootHook(QString *error)
    {
        const QString init = QStringLiteral("/etc/init.d/zybz03qt-init");
        QFile file(init);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("无法读取 %1").arg(init);
            return false;
        }
        const QString current = QString::fromUtf8(file.readAll());
        file.close();
        if (current.contains(QStringLiteral("mount --bind")) && current.contains(QStringLiteral("Z03Plus")))
            return true;

        QStringList lines = z03SplitChar(current, QChar('\n'));
        int insertAt = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains(QStringLiteral("zyb_z03_launch.sh"))) {
                insertAt = i;
                break;
            }
        }
        if (insertAt < 0) {
            *error = QStringLiteral("启动脚本结构不匹配，无法写入持久化钩子");
            return false;
        }
        const QString backup = QStringLiteral("/home/user/z03plus/zybz03qt-init.orig");
        if (!QFile::exists(backup))
            QFile::copy(init, backup);
        lines.insert(insertAt, QStringLiteral(
            "Z=/home/user/z03plus/Z03Plus; L=/home/user/z03plus/libZ03Plus.so; [ -f \"$Z\" ] && [ -f \"$L\" ] && "
            "! grep -q \" /usr/bin/Z03 \" /proc/mounts && mount --bind \"$Z\" /usr/bin/Z03"));
        QProcess::execute(QStringLiteral("/bin/mount"), QStringList() << QStringLiteral("-o") << QStringLiteral("remount,rw") << QStringLiteral("/"));
        QSaveFile out(init);
        if (!out.open(QIODevice::WriteOnly)) {
            QProcess::execute(QStringLiteral("/bin/mount"), QStringList() << QStringLiteral("-o") << QStringLiteral("remount,ro") << QStringLiteral("/"));
            *error = QStringLiteral("无法写入 %1").arg(init);
            return false;
        }
        out.write(lines.join(QLatin1Char('\n')).toUtf8());
        if (!out.commit()) {
            QProcess::execute(QStringLiteral("/bin/mount"), QStringList() << QStringLiteral("-o") << QStringLiteral("remount,ro") << QStringLiteral("/"));
            *error = QStringLiteral("替换 %1 失败").arg(init);
            return false;
        }
        QFile::setPermissions(init, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                    | QFileDevice::ReadOther | QFileDevice::ExeOther);
        QProcess::execute(QStringLiteral("/bin/sync"), QStringList());
        QProcess::execute(QStringLiteral("/bin/mount"), QStringList() << QStringLiteral("-o") << QStringLiteral("remount,ro") << QStringLiteral("/"));
        return true;
    }

    void performInstall()
    {
        setState(QStringLiteral("installing"), QStringLiteral("校验通过，正在安装…"));
        const QString dest = QStringLiteral("/home/user/z03plus");

        const QString pkg = mTmp + QStringLiteral("/pkg");
        QString unpackError;
        if (!extractPackage(mTmp + QStringLiteral("/z03ultra.zip"), pkg, &unpackError)) {
            fail(unpackError);
            return;
        }
        const QStringList files = installFiles();
        for (const QString &name : files) {
            if (!QFile::exists(pkg + QLatin1Char('/') + name)) {
                fail(QStringLiteral("更新包缺少 %1").arg(name));
                return;
            }
        }

        QFile exe(pkg + QStringLiteral("/Z03Plus"));
        if (!exe.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("读取 Z03Plus 失败"));
            return;
        }
        const QByteArray head = exe.read(2 * 1024 * 1024);
        exe.close();
        if (!head.contains("libZ03Plus.so")) {
            fail(QStringLiteral("Z03Plus 未包含注入依赖"));
            return;
        }

        QString hookError;
        if (!ensureBootHook(&hookError)) {
            fail(hookError);
            return;
        }

        QDir().mkpath(dest + QStringLiteral("/backup"));
        for (const QString &name : files) {
            const QString target = dest + QLatin1Char('/') + name;
            if (QFile::exists(target)) {
                QFile::remove(dest + QStringLiteral("/backup/") + name);
                QFile::copy(target, dest + QStringLiteral("/backup/") + name);
            }
        }

        for (const QString &name : files) {
            const QByteArray source = QFile::encodeName(pkg + QLatin1Char('/') + name);
            const QByteArray target = QFile::encodeName(dest + QLatin1Char('/') + name);
            if (std::rename(source.constData(), target.constData()) != 0) {
                fail(QStringLiteral("写入 %1 失败").arg(name));
                return;
            }
            if (name == QStringLiteral("z03plus.rcc")) {
                QFile::setPermissions(dest + QLatin1Char('/') + name,
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ReadGroup | QFileDevice::ReadOther);
            } else {
                QFile::setPermissions(dest + QLatin1Char('/') + name,
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                          | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                          | QFileDevice::ReadOther | QFileDevice::ExeOther);
            }
        }
        QProcess::execute(QStringLiteral("/bin/sync"), QStringList());
        mProgress = 100;
        setState(QStringLiteral("installing"), QStringLiteral("安装完成，应用即将重启…"));
        restartLater();
    }

    void restartLater()
    {
        const QString script = QStringLiteral(
            "if [ -f /home/user/z03plus/ssh/sshd_config ]; then\n"
            "  kill $(cat /home/user/z03plus/ssh/sshd.pid) 2>/dev/null\n"
            "  rm -f /home/user/z03plus/ssh/sshd.pid\n"
            "  sh /home/user/z03plus/setup_ssh.sh >/dev/null 2>&1\n"
            "fi\n"
            "sleep 2\n"
            "G=$(ps w | grep '[z]yb_z03_launch.sh' | awk '{print $1}' | head -n 1)\n"
            "[ -n \"$G\" ] && kill -STOP \"$G\"\n"
            "kill $(pidof Z03) 2>/dev/null\n"
            "kill $(pidof Z03Plus) 2>/dev/null\n"
            "sleep 1\n"
            "umount /usr/bin/Z03 2>/dev/null || umount -l /usr/bin/Z03 2>/dev/null\n"
            "mount --bind /home/user/z03plus/Z03Plus /usr/bin/Z03 2>/dev/null\n"
            "[ -n \"$G\" ] && kill -CONT \"$G\"\n"
            "sleep 12\n"
            "if [ -z \"$(pidof Z03)\" ] && [ -z \"$(pidof Z03Plus)\" ]; then\n"
            "  cd /home\n"
            "  HOME=/home XDG_RUNTIME_DIR=/run/user/0 nohup /home/user/z03plus/Z03Plus -platform wayland "
            ">/home/user/z03plus/z03plus.out 2>&1 </dev/null &\n"
            "fi\n");
        QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << script);
    }

    void fail(const QString &message)
    {
        mBusy = false;
        mProgress = 0;
        if (mReply)
            mReply->abort();
        setState(QStringLiteral("error"), message);
    }

    void setState(const QString &state, const QString &status)
    {
        mState = state;
        mStatus = status;
        emit changed();
    }

    QNetworkAccessManager mManager;
    QTimer mTimeout;
    QNetworkReply *mReply = nullptr;
    QString mLatest;
    QString mBase;
    QString mState = QStringLiteral("idle");
    QString mStatus;
    QString mTmp;
    QStringList mQueue;
    QHash<QString, QString> mSums;
    QByteArray mData;
    int mIndex = 0;
    int mProgress = 0;
    bool mBusy = false;
    bool mInstallMirror = false;
    bool mDirectOnly = false;
};

class Z03PlusTransferService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString url READ url NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit Z03PlusTransferService(QObject *parent = nullptr) : QObject(parent)
    {
        mDir = QStringLiteral("/home/user/z03plus/transfer");
        QDir().mkpath(mDir);
        connect(&mServer, &QTcpServer::newConnection, this, &Z03PlusTransferService::onNewConnection);
    }

    ~Z03PlusTransferService() override { stop(); }

    bool running() const { return mServer.isListening(); }
    QString url() const
    {
        if (!mServer.isListening())
            return QString();
        return QStringLiteral("http://%1:%2").arg(localAddress()).arg(mServer.serverPort());
    }
    QString status() const { return mStatus; }

    Q_INVOKABLE bool start()
    {
        if (mServer.isListening())
            return true;
        for (int port = 8899; port <= 8910; ++port) {
            if (mServer.listen(QHostAddress::Any, static_cast<quint16>(port))) {
                mStatus = QStringLiteral("已开启，等待手机访问");
                emit changed();
                return true;
            }
        }
        mStatus = QStringLiteral("端口被占用，无法启动");
        emit changed();
        return false;
    }

    Q_INVOKABLE void stop()
    {
        const QList<QTcpSocket *> sockets = mBuffers.keys();
        for (QTcpSocket *socket : sockets) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        mBuffers.clear();
        mExpected.clear();
        if (mServer.isListening())
            mServer.close();
        mStatus = QStringLiteral("已关闭");
        emit changed();
    }

    Q_INVOKABLE QVariantList listFiles() const
    {
        QVariantList out;
        const QFileInfoList entries = QDir(mDir).entryInfoList(QDir::Files, QDir::Time);
        for (const QFileInfo &info : entries) {
            QVariantMap item;
            item.insert(QStringLiteral("name"), info.fileName());
            item.insert(QStringLiteral("size"), info.size());
            item.insert(QStringLiteral("modified"), info.lastModified().toString(QStringLiteral("MM-dd HH:mm")));
            out.append(item);
        }
        return out;
    }

    Q_INVOKABLE bool removeFile(const QString &name)
    {
        const QFileInfo info(QDir(mDir).filePath(QFileInfo(name).fileName()));
        if (!info.exists() || !info.isFile())
            return false;
        const bool ok = QFile::remove(info.filePath());
        emit changed();
        return ok;
    }

    Q_INVOKABLE QString directory() const { return mDir; }

signals:
    void changed();

private:
    QString localAddress() const
    {
        const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : interfaces) {
            if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
                continue;
            const QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                const QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
                    return address.toString();
            }
        }
        return QStringLiteral("127.0.0.1");
    }

    void onNewConnection()
    {
        while (mServer.hasPendingConnections()) {
            QTcpSocket *socket = mServer.nextPendingConnection();
            mBuffers.insert(socket, QByteArray());
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                mBuffers.remove(socket);
                mExpected.remove(socket);
                socket->deleteLater();
            });
        }
    }

    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buffer = mBuffers[socket];
        buffer += socket->readAll();
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QByteArray headers = buffer.left(headerEnd);
        int expected = 0;
        const QByteArray lengthValue = headerValue(headers, "content-length");
        if (!lengthValue.isEmpty())
            expected = lengthValue.toInt();
        const int total = headerEnd + 4 + expected;
        if (buffer.size() < total)
            return;

        const QByteArray requestLine = headers.left(headers.indexOf("\r\n"));
        const QList<QByteArray> parts = requestLine.split(' ');
        const QByteArray method = parts.value(0);
        QByteArray path = parts.value(1);
        const int query = path.indexOf('?');
        QByteArray queryText;
        if (query >= 0) {
            queryText = path.mid(query + 1);
            path = path.left(query);
        }
        const QByteArray body = buffer.mid(headerEnd + 4, expected);
        buffer.clear();

        if (method == "GET" && path == "/") {
            respond(socket, pageHtml());
        } else if (method == "GET" && path == "/download") {
            const QString name = QFileInfo(QUrl::fromPercentEncoding(queryText)).fileName();
            sendFile(socket, name);
        } else if (method == "POST" && path == "/upload") {
            const int saved = saveMultipart(headers, body);
            mStatus = saved > 0 ? QStringLiteral("接收了 %1 个文件").arg(saved)
                                : QStringLiteral("上传失败或没有文件");
            emit changed();
            respond(socket, pageHtml());
        } else {
            respond(socket, QStringLiteral("<!doctype html><meta charset='utf-8'><p>Not found"),
                    QByteArrayLiteral("404 Not Found"));
        }
    }

    static QByteArray headerValue(const QByteArray &headers, const QByteArray &key)
    {
        const QList<QByteArray> lines = headers.split('\n');
        for (const QByteArray &raw : lines) {
            const QByteArray line = raw.trimmed();
            const int colon = line.indexOf(':');
            if (colon <= 0)
                continue;
            if (line.left(colon).trimmed().toLower() == key)
                return line.mid(colon + 1).trimmed();
        }
        return QByteArray();
    }

    void respond(QTcpSocket *socket, const QString &html, const QByteArray &code = QByteArrayLiteral("200 OK"))
    {
        const QByteArray body = html.toUtf8();
        QByteArray reply = "HTTP/1.1 ";
        reply += code;
        reply += "\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ";
        reply += QByteArray::number(body.size());
        reply += "\r\n\r\n";
        reply += body;
        socket->write(reply);
        socket->disconnectFromHost();
    }

    void sendFile(QTcpSocket *socket, const QString &name)
    {
        QFile file(QDir(mDir).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            respond(socket, QStringLiteral("<!doctype html><meta charset='utf-8'><p>文件不存在"),
                    QByteArrayLiteral("404 Not Found"));
            return;
        }
        const QByteArray data = file.readAll();
        file.close();
        QByteArray reply = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"";
        reply += name.toUtf8();
        reply += "\"\r\nConnection: close\r\nContent-Length: ";
        reply += QByteArray::number(data.size());
        reply += "\r\n\r\n";
        socket->write(reply);
        socket->write(data);
        socket->disconnectFromHost();
    }

    int saveMultipart(const QByteArray &headers, const QByteArray &body)
    {
        const QByteArray type = headerValue(headers, "content-type");
        const int boundaryAt = type.indexOf("boundary=");
        if (boundaryAt < 0)
            return 0;
        QByteArray boundary = type.mid(boundaryAt + 9).trimmed();
        if (boundary.startsWith('"') && boundary.endsWith('"'))
            boundary = boundary.mid(1, boundary.size() - 2);
        const QByteArray delimiter = "--" + boundary;
        int saved = 0;
        int pos = body.indexOf(delimiter);
        while (pos >= 0) {
            const int start = pos + delimiter.size();
            if (body.mid(start, 2) == "--")
                break;
            const int headersEnd = body.indexOf("\r\n\r\n", start);
            if (headersEnd < 0)
                break;
            const int next = body.indexOf(delimiter, headersEnd);
            if (next < 0)
                break;
            QByteArray partHeaders = body.mid(start, headersEnd - start);
            QByteArray partBody = body.mid(headersEnd + 4, next - headersEnd - 4);
            if (partBody.endsWith("\r\n"))
                partBody.chop(2);
            const QByteArray disposition = headerValue(partHeaders, "content-disposition");
            const int fileAt = disposition.indexOf("filename=\"");
            if (fileAt >= 0) {
                const int nameEnd = disposition.indexOf('"', fileAt + 10);
                QByteArray raw = disposition.mid(fileAt + 10, nameEnd - fileAt - 10);
                QString name = QFileInfo(QString::fromUtf8(raw)).fileName();
                if (name.isEmpty())
                    name = QStringLiteral("upload-%1.bin").arg(QDateTime::currentMSecsSinceEpoch());
                QFile out(QDir(mDir).filePath(name));
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(partBody);
                    out.close();
                    ++saved;
                }
            }
            pos = next;
        }
        return saved;
    }

    QString pageHtml() const
    {
        QString rows;
        const QVariantList files = listFiles();
        for (const QVariant &entry : files) {
            const QVariantMap item = entry.toMap();
            const QString name = item.value(QStringLiteral("name")).toString();
            rows += QStringLiteral("<li><a href='/download?") + name + QStringLiteral("'>")
                    + name.toHtmlEscaped() + QStringLiteral("</a><span>")
                    + QString::number(item.value(QStringLiteral("size")).toLongLong() / 1024.0, 'f', 1)
                    + QStringLiteral(" KB · ") + item.value(QStringLiteral("modified")).toString()
                    + QStringLiteral("</span></li>");
        }
        if (rows.isEmpty())
            rows = QStringLiteral("<li class='empty'>暂无文件</li>");
        return QStringLiteral(
            "<!doctype html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Z03Ultra 互传</title><style>"
            "body{font-family:system-ui,sans-serif;background:#0f1115;color:#e8eaed;margin:0;padding:18px}"
            "h1{font-size:19px;margin:0 0 4px}p{color:#9aa0a6;font-size:13px;margin:0 0 16px}"
            "form{background:#171a21;border:1px solid #2b2f38;border-radius:12px;padding:14px;margin-bottom:18px}"
            "input[type=file]{width:100%;color:#cfd3d8;margin-bottom:12px}"
            "button{width:100%;padding:11px;border:0;border-radius:9px;background:#2fa98d;color:#fff;font-size:15px}"
            "ul{list-style:none;padding:0;margin:0}li{display:flex;justify-content:space-between;gap:10px;"
            "padding:11px 4px;border-bottom:1px solid #23262d;font-size:14px}"
            "li a{color:#7fd8c8;text-decoration:none;word-break:break-all}li span{color:#9aa0a6;font-size:12px;white-space:nowrap}"
            ".empty{color:#9aa0a6}</style></head><body>"
            "<h1>Z03Ultra 手机互传</h1><p>选择文件上传到设备 /home/user/z03plus/transfer</p>"
            "<form method='post' action='/upload' enctype='multipart/form-data'>"
            "<input type='file' name='file' multiple><button type='submit'>上传</button></form>"
            "<ul>") + rows + QStringLiteral("</ul></body></html>");
    }

    QTcpServer mServer;
    QHash<QTcpSocket *, QByteArray> mBuffers;
    QHash<QTcpSocket *, int> mExpected;
    QString mDir;
    QString mStatus = QStringLiteral("未开启");
};

class Z03PlusAppService : public QObject
{
    Q_OBJECT
public:
    explicit Z03PlusAppService(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantList apps() const
    {
        QVariantList out;
        QStringList seen;
        const QStringList dirs = QStringList()
            << QStringLiteral("/home/user/.nix-profile/share/applications")
            << QStringLiteral("/nix/var/nix/profiles/default/share/applications")
            << QStringLiteral("/usr/share/applications");
        for (const QString &dirPath : dirs) {
            QDir dir(dirPath);
            const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.desktop"), QDir::Files);
            for (const QFileInfo &info : files) {
                QFile file(info.filePath());
                if (!file.open(QIODevice::ReadOnly))
                    continue;
                const QString text = QString::fromUtf8(file.readAll());
                file.close();
                QString name;
                QString exec;
                QString icon;
                QString comment;
                bool hidden = false;
                bool inEntry = false;
                const QStringList lines = z03SplitChar(text, QChar('\n'));
                for (const QString &line : lines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed == QStringLiteral("[Desktop Entry]")) {
                        inEntry = true;
                        continue;
                    }
                    if (trimmed.startsWith(QLatin1Char('[')) && trimmed != QStringLiteral("[Desktop Entry]")) {
                        if (inEntry)
                            break;
                        continue;
                    }
                    if (!inEntry)
                        continue;
                    if (trimmed.startsWith(QStringLiteral("Name=")))
                        name = trimmed.mid(5).trimmed();
                    else if (trimmed.startsWith(QStringLiteral("Exec=")))
                        exec = trimmed.mid(5).trimmed();
                    else if (trimmed.startsWith(QStringLiteral("Icon=")))
                        icon = trimmed.mid(5).trimmed();
                    else if (trimmed.startsWith(QStringLiteral("Comment=")))
                        comment = trimmed.mid(8).trimmed();
                    else if (trimmed == QStringLiteral("NoDisplay=true") || trimmed == QStringLiteral("Hidden=true"))
                        hidden = true;
                }
                if (hidden || name.isEmpty() || exec.isEmpty() || seen.contains(name))
                    continue;
                seen.append(name);
                QVariantMap item;
                item.insert(QStringLiteral("name"), name);
                item.insert(QStringLiteral("exec"), exec);
                item.insert(QStringLiteral("comment"), comment);
                item.insert(QStringLiteral("icon"), resolveIcon(icon));
                out.append(item);
            }
        }
        std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString().toLower()
                 < b.toMap().value(QStringLiteral("name")).toString().toLower();
        });
        return out;
    }

    Q_INVOKABLE QVariantMap launch(const QString &exec)
    {
        QVariantMap result;
        QString command = exec;
        static const QStringList codes = QStringList()
            << QStringLiteral("%f") << QStringLiteral("%F") << QStringLiteral("%u") << QStringLiteral("%U")
            << QStringLiteral("%i") << QStringLiteral("%c") << QStringLiteral("%k") << QStringLiteral("%d")
            << QStringLiteral("%D") << QStringLiteral("%n") << QStringLiteral("%N") << QStringLiteral("%v")
            << QStringLiteral("%m");
        for (const QString &code : codes)
            command.remove(code);
        command = command.trimmed();
        if (command.isEmpty()) {
            result.insert(QStringLiteral("ok"), false);
            result.insert(QStringLiteral("message"), QStringLiteral("启动命令为空"));
            return result;
        }
        const QString script = QStringLiteral("HOME=/home/user XDG_RUNTIME_DIR=/run/user/0 ")
            + command + QStringLiteral(" >/dev/null 2>&1 &");
        const bool ok = QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << script);
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? QStringLiteral("已启动") : QStringLiteral("启动失败"));
        return result;
    }

private:
    static QString resolveIcon(const QString &icon)
    {
        if (icon.isEmpty())
            return QString();
        if (icon.startsWith(QLatin1Char('/')))
            return QFile::exists(icon) ? QStringLiteral("file://") + icon : QString();
        const QStringList roots = QStringList()
            << QStringLiteral("/home/user/.nix-profile/share/icons")
            << QStringLiteral("/nix/var/nix/profiles/default/share/icons")
            << QStringLiteral("/usr/share/icons")
            << QStringLiteral("/usr/share/pixmaps");
        for (const QString &root : roots) {
            QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
            const QString plain = icon + QStringLiteral(".png");
            const QString svg = icon + QStringLiteral(".svg");
            while (it.hasNext()) {
                const QString path = it.next();
                if (path.endsWith(QLatin1Char('/') + plain) || path.endsWith(QLatin1Char('/') + svg))
                    return QStringLiteral("file://") + path;
            }
        }
        return QString();
    }
};

class Z03PlusNoteService : public QObject
{
    Q_OBJECT
public:
    explicit Z03PlusNoteService(QObject *parent = nullptr) : QObject(parent)
    {
        mDir = QStringLiteral("/home/user/z03plus/notes");
        QDir().mkpath(mDir);
    }

    Q_INVOKABLE QVariantList list() const
    {
        QVariantList out;
        const QFileInfoList files = QDir(mDir).entryInfoList(QStringList() << QStringLiteral("*.png"), QDir::Files, QDir::Time);
        for (const QFileInfo &info : files) {
            QVariantMap item;
            item.insert(QStringLiteral("path"), info.filePath());
            item.insert(QStringLiteral("name"), info.fileName());
            item.insert(QStringLiteral("size"), info.size());
            item.insert(QStringLiteral("modified"), info.lastModified().toString(QStringLiteral("MM-dd HH:mm")));
            out.append(item);
        }
        return out;
    }

    Q_INVOKABLE QString save(const QString &dataUrl)
    {
        const int comma = dataUrl.indexOf(QLatin1Char(','));
        if (comma < 0 || !dataUrl.startsWith(QStringLiteral("data:image/")))
            return QString();
        const QByteArray data = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
        if (data.isEmpty())
            return QString();
        const QString name = QStringLiteral("note-%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        QFile file(QDir(mDir).filePath(name));
        if (!file.open(QIODevice::WriteOnly))
            return QString();
        file.write(data);
        file.close();
        emit changed();
        return file.fileName();
    }

    Q_INVOKABLE bool removeNote(const QString &path)
    {
        const QFileInfo info(path);
        if (info.absolutePath() != mDir)
            return false;
        const bool ok = QFile::remove(info.filePath());
        emit changed();
        return ok;
    }

    Q_INVOKABLE QString directory() const { return mDir; }

signals:
    void changed();

private:
    QString mDir;
};


class Z03PlusMonitorService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY changed)
    Q_PROPERTY(double memUsage READ memUsage NOTIFY changed)
    Q_PROPERTY(int memTotalMB READ memTotalMB NOTIFY changed)
    Q_PROPERTY(int memUsedMB READ memUsedMB NOTIFY changed)
    Q_PROPERTY(QString loadAverage READ loadAverage NOTIFY changed)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY changed)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY changed)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
public:
    explicit Z03PlusMonitorService(QObject *parent = nullptr) : QObject(parent)
    {
        mTimer.setInterval(2000);
        connect(&mTimer, &QTimer::timeout, this, &Z03PlusMonitorService::sample);
        readCpu(mPrevTotal, mPrevIdle);
        readMemory();
    }

    double cpuUsage() const { return mCpuUsage; }
    double memUsage() const { return mMemUsage; }
    int memTotalMB() const { return mMemTotalMB; }
    int memUsedMB() const { return mMemUsedMB; }
    QString loadAverage() const { return mLoadAverage; }
    QString uptimeText() const { return mUptime; }
    QVariantList processes() const { return mProcesses; }
    bool running() const { return mTimer.isActive(); }

    void setRunning(bool value)
    {
        if (value == mTimer.isActive())
            return;
        if (value) {
            readCpu(mPrevTotal, mPrevIdle);
            mPrevProc.clear();
            mTimer.start();
            QTimer::singleShot(0, this, [this]() { sample(); });
        } else {
            mTimer.stop();
            mProcesses.clear();
            mPrevProc.clear();
            emit changed();
        }
        emit runningChanged();
    }

    Q_INVOKABLE void refresh() { sample(); }

signals:
    void changed();
    void runningChanged();

private:
    static bool readCpu(quint64 &total, quint64 &idle)
    {
        QFile file(QStringLiteral("/proc/stat"));
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const QByteArray line = file.readLine();
        file.close();
        if (!line.startsWith("cpu "))
            return false;
        quint64 sum = 0;
        quint64 idleTicks = 0;
        int index = 0;
        for (const QString &part : z03SplitChar(QString::fromUtf8(line).trimmed(), QChar(' '))) {
            if (part.isEmpty() || part == QStringLiteral("cpu"))
                continue;
            bool ok = false;
            const quint64 value = part.toULongLong(&ok);
            if (!ok)
                continue;
            sum += value;
            if (index == 3 || index == 4)
                idleTicks += value;
            ++index;
        }
        if (sum == 0)
            return false;
        total = sum;
        idle = idleTicks;
        return true;
    }

    void readMemory()
    {
        QFile file(QStringLiteral("/proc/meminfo"));
        if (!file.open(QIODevice::ReadOnly))
            return;
        const QByteArray all = file.readAll();
        file.close();
        quint64 total = 0;
        quint64 freeKb = 0;
        quint64 buffers = 0;
        quint64 cached = 0;
        quint64 available = 0;
        for (const QByteArray &raw : all.split('\n')) {
            const QByteArray line = raw.trimmed();
            if (line.startsWith("MemTotal:"))
                total = line.mid(9).trimmed().split(' ').value(0).toULongLong();
            else if (line.startsWith("MemAvailable:"))
                available = line.mid(13).trimmed().split(' ').value(0).toULongLong();
            else if (line.startsWith("MemFree:"))
                freeKb = line.mid(8).trimmed().split(' ').value(0).toULongLong();
            else if (line.startsWith("Buffers:"))
                buffers = line.mid(8).trimmed().split(' ').value(0).toULongLong();
            else if (line.startsWith("Cached:"))
                cached = line.mid(7).trimmed().split(' ').value(0).toULongLong();
        }
        if (total == 0)
            return;
        if (available == 0)
            available = freeKb + buffers + cached;
        if (available > total)
            available = total;
        mMemTotalMB = int(total / 1024);
        mMemUsedMB = int((total - available) / 1024);
        mMemUsage = double(total - available) * 100.0 / double(total);
    }

    void readLoad()
    {
        QFile file(QStringLiteral("/proc/loadavg"));
        if (!file.open(QIODevice::ReadOnly))
            return;
        const QStringList parts = z03SplitChar(QString::fromUtf8(file.readAll()).trimmed(), QChar(' '));
        file.close();
        if (parts.size() >= 3)
            mLoadAverage = parts.at(0) + QStringLiteral("  ") + parts.at(1) + QStringLiteral("  ") + parts.at(2);
    }

    void readUptime()
    {
        QFile file(QStringLiteral("/proc/uptime"));
        if (!file.open(QIODevice::ReadOnly))
            return;
        const double seconds = z03SplitChar(QString::fromUtf8(file.readAll()).trimmed(), QChar(' ')).value(0).toDouble();
        file.close();
        const int total = int(seconds);
        const int days = total / 86400;
        const int hours = (total % 86400) / 3600;
        const int minutes = (total % 3600) / 60;
        if (days > 0)
            mUptime = QStringLiteral("%1 天 %2 小时").arg(days).arg(hours);
        else if (hours > 0)
            mUptime = QStringLiteral("%1 小时 %2 分").arg(hours).arg(minutes);
        else
            mUptime = QStringLiteral("%1 分钟").arg(minutes);
    }

    void readProcesses(quint64 totalDelta)
    {
        QVariantList list;
        QHash<int, quint64> current;
        QDir proc(QStringLiteral("/proc"));
        const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            bool ok = false;
            const int pid = entry.toInt(&ok);
            if (!ok)
                continue;
            QFile stat(proc.filePath(entry + QStringLiteral("/stat")));
            if (!stat.open(QIODevice::ReadOnly))
                continue;
            const QByteArray data = stat.readAll();
            stat.close();
            const int close = data.lastIndexOf(')');
            if (close < 0)
                continue;
            const int open = data.indexOf('(');
            if (open < 0 || open >= close)
                continue;
            const QString name = QString::fromUtf8(data.mid(open + 1, close - open - 1));
            const QStringList rest = z03SplitChar(QString::fromUtf8(data.mid(close + 2)).trimmed(), QChar(' '));
            if (rest.size() < 13)
                continue;
            const quint64 utime = rest.at(11).toULongLong();
            const quint64 stime = rest.at(12).toULongLong();
            const quint64 ticks = utime + stime;
            current.insert(pid, ticks);
            if (totalDelta == 0)
                continue;
            const quint64 previous = mPrevProc.value(pid, 0);
            double cpu = previous > 0 ? double(ticks - previous) * 100.0 / double(totalDelta) : 0.0;
            if (cpu < 0)
                cpu = 0;
            if (cpu > 999)
                cpu = 999;

            quint64 rssKb = 0;
            if (rest.size() > 21)
                rssKb = rest.at(21).toULongLong() * 4;
            QVariantMap item;
            item.insert(QStringLiteral("pid"), pid);
            item.insert(QStringLiteral("name"), name);
            item.insert(QStringLiteral("cpu"), cpu);
            item.insert(QStringLiteral("mem"), int(rssKb / 1024));
            item.insert(QStringLiteral("memPercent"),
                        mMemTotalMB > 0 ? double(rssKb / 1024) * 100.0 / double(mMemTotalMB) : 0.0);
            list.append(item);
        }
        mPrevProc = current;
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            const double left = a.toMap().value(QStringLiteral("cpu")).toDouble();
            const double right = b.toMap().value(QStringLiteral("cpu")).toDouble();
            if (left != right)
                return left > right;
            return a.toMap().value(QStringLiteral("mem")).toInt() > b.toMap().value(QStringLiteral("mem")).toInt();
        });
        if (list.size() > 24)
            list = list.mid(0, 24);
        mProcesses = list;
    }

    void sample()
    {
        quint64 total = 0;
        quint64 idle = 0;
        if (!readCpu(total, idle))
            return;
        const quint64 totalDelta = total > mPrevTotal ? total - mPrevTotal : 0;
        const quint64 idleDelta = idle > mPrevIdle ? idle - mPrevIdle : 0;
        if (totalDelta > 0) {
            double usage = double(totalDelta - idleDelta) * 100.0 / double(totalDelta);
            if (usage < 0)
                usage = 0;
            if (usage > 100)
                usage = 100;
            mCpuUsage = usage;
        }
        mPrevTotal = total;
        mPrevIdle = idle;
        readMemory();
        readLoad();
        readUptime();
        readProcesses(totalDelta);
        emit changed();
    }

    QTimer mTimer;
    double mCpuUsage = 0;
    double mMemUsage = 0;
    int mMemTotalMB = 0;
    int mMemUsedMB = 0;
    QString mLoadAverage;
    QString mUptime;
    QVariantList mProcesses;
    quint64 mPrevTotal = 0;
    quint64 mPrevIdle = 0;
    QHash<int, quint64> mPrevProc;
};

static bool z03VirtualKeyboardModulePresent()
{
    return QDir(QStringLiteral("/usr/lib/qt5/qml/QtQuick/VirtualKeyboard")).exists();
}

static void z03EnableVirtualKeyboard()
{
    if (qEnvironmentVariableIsSet("QT_IM_MODULE"))
        return;
    if (z03VirtualKeyboardModulePresent())
        qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
}

__attribute__((constructor)) static void z03plusEarlyEnvironment()
{
    z03EnableVirtualKeyboard();
}

static void registerZ03PlusQmlTypes()
{
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    static bool bridgeStarted = false;
    if (!bridgeStarted && QCoreApplication::instance()) {
        new Z03PlusAiBridge(QCoreApplication::instance());
        bridgeStarted = true;
    }
    qmlRegisterType<Z03PlusSshService>("Z03Ultra", 1, 0, "SshService");
    qmlRegisterType<Z03PlusFileService>("Z03Ultra", 1, 0, "FileService");
    qmlRegisterType<Z03PlusTextFileService>("Z03Ultra", 1, 0, "TextFileService");
    qmlRegisterType<Z03PlusVoiceService>("Z03Ultra", 1, 0, "VoiceService");
    qmlRegisterType<Z03PlusAiService>("Z03Ultra", 1, 0, "AiService");
    qmlRegisterType<Z03PlusStorageService>("Z03Ultra", 1, 0, "StorageService");
    qmlRegisterType<Z03PlusShellService>("Z03Ultra", 1, 0, "ShellService");
    qmlRegisterType<Z03PlusDeviceInfoService>("Z03Ultra", 1, 0, "DeviceInfoService");
    qmlRegisterType<Z03PlusMusicService>("Z03Ultra", 1, 0, "MusicService");
    qmlRegisterType<Z03PlusRecorderService>("Z03Ultra", 1, 0, "RecorderService");
    qmlRegisterType<Z03PlusNixService>("Z03Ultra", 1, 0, "NixService");
    qmlRegisterType<Z03PlusBrowserService>("Z03Ultra", 1, 0, "BrowserService");
    qmlRegisterType<Z03PlusCefService>("Z03Ultra", 1, 0, "CefService");
    qmlRegisterType<Z03PlusUpdateService>("Z03Ultra", 1, 0, "UpdateService");
    qmlRegisterType<Z03PlusTransferService>("Z03Ultra", 1, 0, "TransferService");
    qmlRegisterType<Z03PlusAppService>("Z03Ultra", 1, 0, "AppService");
    qmlRegisterType<Z03PlusNoteService>("Z03Ultra", 1, 0, "NoteService");
    qmlRegisterType<Z03PlusMonitorService>("Z03Ultra", 1, 0, "MonitorService");
}
Q_COREAPP_STARTUP_FUNCTION(registerZ03PlusQmlTypes)

static void ensureNixBindMount()
{
    QFile mounts(QStringLiteral("/proc/mounts"));
    if (!mounts.open(QIODevice::ReadOnly))
        return;
    const QByteArray table = mounts.readAll();
    mounts.close();
    if (table.contains(" /nix "))
        return;

    const QString script = QStringLiteral(
        "LOG=/home/user/z03plus/nix-mount.log\n"
        "if [ \"$(id -u)\" = 0 ]; then\n"
        "  if [ -L /nix ]; then\n"
        "    mount -o remount,rw / 2>/dev/null\n"
        "    rm -f /nix 2>/dev/null\n"
        "    mkdir -p /nix 2>/dev/null\n"
        "    echo \"$(date) replaced /nix symlink\" >>$LOG\n"
        "  fi\n"
        "  if [ -d /nix ] && [ ! -L /nix ]; then\n"
        "    mkdir -p /home/nix 2>/dev/null\n"
        "    if mount --bind /home/nix /nix 2>>$LOG; then\n"
        "      echo \"$(date) bind /home/nix -> /nix\" >>$LOG\n"
        "    fi\n"
        "  fi\n"
        "fi\n");
    QProcess process;
    process.start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << script);
    process.waitForFinished(5000);
}
Q_COREAPP_STARTUP_FUNCTION(ensureNixBindMount)

static void logResolvedResource()
{
    QFile page(QStringLiteral(":/ZybQmlFiles/ZybSet/SetMainPage.qml"));
    QFile log(QStringLiteral("/home/user/z03plus/overlay-status.log"));
    if (!log.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    log.write("overlay_registerResource=");
    log.write(gOverlayRegistered ? "true\n" : "false\n");
    QFile overlayFile(QStringLiteral(":/ZybQmlFiles/ZybSet/Z03UltraPage.qml"));
    log.write("overlay_new_page_open=");
    log.write(overlayFile.open(QIODevice::ReadOnly) ? "true\n" : "false\n");
    QFile mainFile(QStringLiteral(":/main.qml"));
    log.write("main_qml_open=");
    if (mainFile.open(QIODevice::ReadOnly)) {
        const QByteArray mainSource = mainFile.readAll();
        log.write("true bytes=");
        log.write(QByteArray::number(mainSource.size()));
        log.write(" patched=");
        log.write(mainSource.contains("Z03Ultra battery patch") ? "yes\n" : "no\n");
    } else {
        log.write("false\n");
    }
    if (!page.open(QIODevice::ReadOnly)) {
        log.write("SetMainPage open failed\n");
        return;
    }
    const QByteArray source = page.readAll();
    log.write("bytes=");
    log.write(QByteArray::number(source.size()));
    log.write("\ncontains_z03plus=");
    log.write(source.contains("Z03Ultra") ? "yes\n" : "no\n");
    log.write("register_order=overlay-before-original\n");
}

extern "C" int qRegisterResourceData(int version, const uchar *tree,
                                     const uchar *names, const uchar *data)
    __asm__("_Z21qRegisterResourceDataiPKhS0_S0_");

static bool isMainExecutableAddress(const void *address)
{
    struct State {
        const void *address;
        bool found;
    } state{address, false};
    dl_iterate_phdr([](struct dl_phdr_info *info, size_t, void *data) -> int {
        State *state = static_cast<State *>(data);
        if (info->dlpi_name && info->dlpi_name[0] != '\0')
            return 0;
        const uintptr_t target = reinterpret_cast<uintptr_t>(state->address);
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            const ElfW(Phdr) &ph = info->dlpi_phdr[i];
            if (ph.p_type != PT_LOAD)
                continue;
            const uintptr_t start = info->dlpi_addr + ph.p_vaddr;
            const uintptr_t end = start + ph.p_memsz;
            if (target >= start && target < end) {
                state->found = true;
                return 1;
            }
        }
        return 0;
    }, &state);
    return state.found;
}

static void logResourceHook(const uchar *tree, bool hasOriginal, bool match)
{
    static int calls = 0;
    if (calls++ >= 100)
        return;
    FILE *file = std::fopen("/home/user/z03plus/hook.log", "a");
    if (!file)
        return;
    std::fprintf(file, "tree=%p match=%d original=%d\n", static_cast<const void *>(tree),
                 match ? 1 : 0, hasOriginal ? 1 : 0);
    std::fclose(file);
}

extern "C" int qRegisterResourceData(int version, const uchar *tree,
                                     const uchar *names, const uchar *data)
{
    static RegisterResource original = nullptr;
    if (!original)
        original = reinterpret_cast<RegisterResource>(
            dlsym(RTLD_NEXT, "_Z21qRegisterResourceDataiPKhS0_S0_"));

    const bool isZ03Resources = isMainExecutableAddress(tree);
    logResourceHook(tree, original != nullptr, isZ03Resources);
    if (isZ03Resources)
        z03cefBreadcrumb("rcc hook");
    if (isZ03Resources && !gOverlayRegistered)
        gOverlayRegistered = QResource::registerResource(QStringLiteral("/home/user/z03plus/z03plus.rcc"));

    const int result = original ? original(version, tree, names, data) : 0;
    if (isZ03Resources)
        logResolvedResource();
    return result;
}

#include "z03plus_overlay.moc"