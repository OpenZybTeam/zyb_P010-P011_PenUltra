// Z03Plus QRC overlay with on-device verification of the final resolved file.
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QSettings>
#include <QUrl>
#include <QTextCodec>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
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
        if (!pidFile.open(QIODevice::ReadOnly))
            return false;
        bool ok = false;
        const qint64 pid = pidFile.readAll().trimmed().toLongLong(&ok);
        return ok && pid > 0 && ::kill(static_cast<pid_t>(pid), 0) == 0;
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
class Z03PlusFileService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantMap openDirectory(const QString &requested, int order, bool reversed) const
    {
        const QString path = requested.isEmpty() ? QStringLiteral("/mnt/data") : requested;
        QVariantMap result;
        if (!allowed(path)) {
            result.insert(QStringLiteral("ok"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("不允许访问该目录"));
            return result;
        }
        QDir dir(path);
        if (!dir.exists()) {
            result.insert(QStringLiteral("ok"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("目录不存在或无法读取"));
            return result;
        }
        QVariantList entries;
        QDir::SortFlags sort = QDir::DirsFirst | QDir::IgnoreCase;
        if (order == 1) sort |= QDir::Time;
        else if (order == 2) sort |= QDir::Size;
        else sort |= QDir::Name;
        if (reversed) sort |= QDir::Reversed;
        const QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, sort);
        for (const QFileInfo &info : list) {
            QVariantMap entry;
            const QString suffix = info.suffix().toLower();
            entry.insert(QStringLiteral("name"), info.fileName());
            entry.insert(QStringLiteral("path"), info.absoluteFilePath());
            entry.insert(QStringLiteral("isDir"), info.isDir());
            entry.insert(QStringLiteral("text"), !info.isDir() &&
                         (suffix == QStringLiteral("txt") || suffix == QStringLiteral("md") || suffix == QStringLiteral("lrc") || suffix == QStringLiteral("log")));
            entry.insert(QStringLiteral("size"), sizeString(info.size()));
            entry.insert(QStringLiteral("modified"), info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
            entries.append(entry);
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("path"), dir.absolutePath());
        result.insert(QStringLiteral("entries"), entries);
        return result;
    }

    Q_INVOKABLE bool rename(const QString &path, const QString &newName) const
    {
        if (!allowed(path) || newName.isEmpty() || newName.contains(QLatin1Char('/')) || newName == QStringLiteral(".") || newName == QStringLiteral("..")) return false;
        QFileInfo info(path);
        return QDir(info.absolutePath()).rename(info.fileName(), newName);
    }
    Q_INVOKABLE bool remove(const QString &path) const
    {
        if (!allowed(path)) return false;
        QFileInfo info(path);
        if (info.isDir()) return QDir(path).removeRecursively();
        return QFile::remove(path);
    }
    Q_INVOKABLE QString parentPath(const QString &path) const { QDir dir(path); return dir.absoluteFilePath(QStringLiteral("..")); }
    Q_INVOKABLE bool canGoUp(const QString &path) const { return !path.isEmpty() && QDir::cleanPath(path) != QStringLiteral("/mnt/data") && QDir::cleanPath(path) != QStringLiteral("/home/user"); }

    static bool allowed(const QString &path)
    {
        const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        return clean == QStringLiteral("/mnt/data") || clean.startsWith(QStringLiteral("/mnt/data/")) ||
               clean == QStringLiteral("/home/user") || clean.startsWith(QStringLiteral("/home/user/"));
    }
    static QString sizeString(qint64 bytes)
    {
        if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
        if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    }
};

class Z03PlusTextFileService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariantMap readText(const QString &path) const
    {
        QVariantMap result;
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (!Z03PlusFileService::allowed(path) || (suffix != QStringLiteral("txt") && suffix != QStringLiteral("md") && suffix != QStringLiteral("lrc") && suffix != QStringLiteral("log"))) {
            result.insert(QStringLiteral("ok"), false); result.insert(QStringLiteral("error"), QStringLiteral("不允许打开该文件")); return result;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.insert(QStringLiteral("ok"), false); result.insert(QStringLiteral("error"), QStringLiteral("打开文件失败")); return result;
        }
        const qint64 maxBytes = 2 * 1024 * 1024;
        const QByteArray bytes = file.read(maxBytes + 1);
        if (bytes.size() > maxBytes) {
            result.insert(QStringLiteral("ok"), false); result.insert(QStringLiteral("error"), QStringLiteral("文件超过 2 MB，无法打开")); return result;
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("content"), QString::fromUtf8(bytes));
        return result;
    }
    Q_INVOKABLE QString fileName(const QString &path) const { return QFileInfo(path).fileName(); }
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
            "MIRROR=https://mirrors.tuna.tsinghua.edu.cn\n"
            "NIX_VER=2.9.2\n"
            "TARBALL=nix-$NIX_VER-armv7l-linux.tar.xz\n"
            "WORK=/home/user/z03plus\n"
            "echo '[nix-progress] 3 检查环境'\n"
            "echo '[nix] checking existing installation'\n"
            "if command -v nix-env >/dev/null 2>&1 || [ -x /nix/var/nix/profiles/default/bin/nix-env ]; then\n"
            "  echo '[nix] already installed'\n"
            "  exit 0\n"
            "fi\n"
            "if [ ! -d /nix ]; then\n"
            "  mkdir -p /nix 2>/dev/null || true\n"
            "fi\n"
            "echo '[nix-progress] 8 检查 /nix 目录'\n"
            "if [ ! -w /nix ]; then\n"
            "  echo '[nix] /nix is missing or not writable; root filesystem looks read-only'\n"
            "  echo '[nix] Nix needs a writable /nix; free space and make the rootfs writable first'\n"
            "  exit 3\n"
            "fi\n"
            "mkdir -p \"$WORK\" || exit 3\n"
            "for tool in tar xz sha256sum; do\n"
            "  command -v $tool >/dev/null 2>&1 || { echo \"[nix] missing required tool: $tool\"; exit 4; }\n"
            "done\n"
            "cd \"$WORK\" || exit 3\n"
            "echo '[nix] downloading Nix '$NIX_VER' from TUNA mirror'\n"
            "echo '[nix-progress] 15 下载安装包（约 15 MB）'\n"
            "if command -v curl >/dev/null 2>&1; then\n"
            "  curl -fL --connect-timeout 15 -o \"$TARBALL\" \"$MIRROR/nix/nix-$NIX_VER/$TARBALL\" &\n"
            "else\n"
            "  wget -O \"$TARBALL\" \"$MIRROR/nix/nix-$NIX_VER/$TARBALL\" &\n"
            "fi\n"
            "FETCH=$!\n"
            "while kill -0 $FETCH 2>/dev/null; do\n"
            "  SIZE=$(wc -c < \"$TARBALL\" 2>/dev/null || echo 0)\n"
            "  PCT=$((15 + SIZE * 30 / 15547952))\n"
            "  [ $PCT -gt 45 ] && PCT=45\n"
            "  echo \"[nix-progress] $PCT 下载中 $((SIZE / 1024)) KB / 15183 KB\"\n"
            "  sleep 2\n"
            "done\n"
            "wait $FETCH || { echo '[nix] download failed'; exit 4; }\n"
            "echo '[nix-progress] 46 校验安装包'\n"
            "echo '983299e3d1b6fdadc487624d94e66a4e7b268c4acc77707739ef1bb78783a4a4  '$TARBALL > \"$TARBALL.sha256\"\n"
            "sha256sum -c \"$TARBALL.sha256\" >/dev/null 2>&1 || { echo '[nix] checksum mismatch'; exit 4; }\n"
            "mkdir -p \"$WORK/nix-unpack\"\n"
            "echo '[nix-progress] 50 解压安装包'\n"
            "echo '[nix] unpacking'\n"
            "tar -xJf \"$TARBALL\" -C \"$WORK/nix-unpack\" || {\n"
            "  xz -dc \"$TARBALL\" | tar -xf - -C \"$WORK/nix-unpack\" || { echo '[nix] unpack failed'; exit 4; }\n"
            "}\n"
            "INSTALLER=\"$WORK/nix-unpack/nix-$NIX_VER-armv7l-linux/install\"\n"
            "[ -f \"$INSTALLER\" ] || { echo '[nix] installer not found'; exit 4; }\n"
            "echo '[nix-progress] 60 安装到 /nix'\n"
            "echo '[nix] running single-user installer (no channel)'\n"
            "sh \"$INSTALLER\" --no-daemon --no-channel-add </dev/null || { echo '[nix] installer failed'; exit 5; }\n"
            ". /home/user/.nix-profile/etc/profile.d/nix.sh 2>/dev/null || true\n"
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
            "echo '[nix] adding nixos-22.05 channel from TUNA mirror'\n"
            "nix-channel --remove nixpkgs >/dev/null 2>&1 || true\n"
            "nix-channel --add \"$MIRROR/nix-channels/nixos-22.05\" nixpkgs || {\n"
            "  echo '[nix] channel add failed'\n"
            "  exit 7\n"
            "}\n"
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

static void registerZ03PlusQmlTypes()
{
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
}
Q_COREAPP_STARTUP_FUNCTION(registerZ03PlusQmlTypes)

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
    if (isZ03Resources && !gOverlayRegistered)
        gOverlayRegistered = QResource::registerResource(QStringLiteral("/home/user/z03plus/z03plus.rcc"));

    const int result = original ? original(version, tree, names, data) : 0;
    if (isZ03Resources)
        logResolvedResource();
    return result;
}

#include "z03plus_overlay.moc"