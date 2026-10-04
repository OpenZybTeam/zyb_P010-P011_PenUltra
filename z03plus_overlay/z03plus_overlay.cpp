// Z03Plus QRC overlay with on-device verification of the final resolved file.
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
#include <unistd.h>

extern "C" void *dlsym(void *handle, const char *symbol);
#define RTLD_NEXT ((void *)-1l)
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
        return launch("/home/user/z03plus/ssh/start-sshd.sh; "
                      "/home/user/z03plus/update_ssh_status.sh");
    }

    Q_INVOKABLE bool refresh()
    {
        return launch("/home/user/z03plus/update_ssh_status.sh");
    }

    // Pulls up the ydpExt host + HTTP bridge (idempotent). QML cannot pass
    // arbitrary command text; only this fixed script is ever executed.
    Q_INVOKABLE bool restartAi()
    {
        return launch("/home/user/z03plus/ydpext/start-ai.sh");
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
        QFile file(QStringLiteral("/home/user/z03plus/ssh-status.txt"));
        if (!file.open(QIODevice::ReadOnly))
            return values;
        const QList<QByteArray> rows = file.readAll().split('\n');
        for (const QByteArray &row : rows) {
            const int delimiter = row.indexOf('=');
            if (delimiter > 0)
                values.insert(QString::fromLatin1(row.left(delimiter)),
                              QString::fromUtf8(row.mid(delimiter + 1)));
        }
        return values;
    }

private:
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
        QSettings cfg(QStringLiteral("/home/user/z03plus/ai.ini"), QSettings::IniFormat);
        mEndpoint = cfg.value(QStringLiteral("api/endpoint"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString();
        mApiKey = cfg.value(QStringLiteral("api/key")).toString();
        mModel = cfg.value(QStringLiteral("api/model"), QStringLiteral("gpt-3.5-turbo")).toString();
    }

    Q_INVOKABLE QVariantMap settings() const
    {
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
        mBuffer.clear(); mAnswer.clear();
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
    void consume()
    {
        if (!mReply) return;
        mBuffer += mReply->readAll();
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
        if (!mAnswer.isEmpty()) {
            QJsonObject assistant;
            assistant.insert(QStringLiteral("role"), QStringLiteral("assistant"));
            assistant.insert(QStringLiteral("content"), mAnswer);
            mMessages.append(assistant);
            emit completed(mAnswer);
        }
        QNetworkReply *reply = mReply;
        mReply = nullptr;
        reply->deleteLater();
    }

    QNetworkAccessManager mNetwork;
    QNetworkReply *mReply = nullptr;
    QString mEndpoint, mApiKey, mModel, mAnswer;
    QByteArray mBuffer;
    QJsonArray mMessages;
};

static void registerZ03PlusQmlTypes()
{
    qmlRegisterType<Z03PlusSshService>("Z03Plus", 1, 0, "SshService");
    qmlRegisterType<Z03PlusFileService>("Z03Plus", 1, 0, "FileService");
    qmlRegisterType<Z03PlusTextFileService>("Z03Plus", 1, 0, "TextFileService");
    qmlRegisterType<Z03PlusVoiceService>("Z03Plus", 1, 0, "VoiceService");
    qmlRegisterType<Z03PlusAiService>("Z03Plus", 1, 0, "AiService");
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
    QFile overlayFile(QStringLiteral(":/ZybQmlFiles/ZybSet/Z03PlusPage.qml"));
    log.write("overlay_new_page_open=");
    log.write(overlayFile.open(QIODevice::ReadOnly) ? "true\n" : "false\n");
    if (!page.open(QIODevice::ReadOnly)) {
        log.write("SetMainPage open failed\n");
        return;
    }
    const QByteArray source = page.readAll();
    log.write("bytes=");
    log.write(QByteArray::number(source.size()));
    log.write("\ncontains_z03plus=");
    log.write(source.contains("Z03Plus") ? "yes\n" : "no\n");
    log.write("register_order=overlay-before-original\n");
}

extern "C" int qRegisterResourceData(int version, const uchar *tree,
                                     const uchar *names, const uchar *data)
    __asm__("_Z21qRegisterResourceDataiPKhS0_S0_");

extern "C" int qRegisterResourceData(int version, const uchar *tree,
                                     const uchar *names, const uchar *data)
{
    static RegisterResource original = nullptr;
    if (!original)
        original = reinterpret_cast<RegisterResource>(
            dlsym(RTLD_NEXT, "_Z21qRegisterResourceDataiPKhS0_S0_"));

    const bool isZ03Resources = tree == reinterpret_cast<const uchar *>(0x001ab51c);
    if (isZ03Resources && !gOverlayRegistered)
        gOverlayRegistered = QResource::registerResource(QStringLiteral("/home/user/z03plus/z03plus.rcc"));

    const int result = original ? original(version, tree, names, data) : 0;
    if (isZ03Resources)
        logResolvedResource();
    return result;
}

#include "z03plus_overlay.moc"