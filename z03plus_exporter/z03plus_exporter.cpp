// Z03Plus stage-3: export the *runtime-visible* Qt resources after Z03 has
// registered its embedded QRC.  This does not modify a resource or UI.
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QString>

extern "C" void *dlsym(void *handle, const char *symbol) noexcept;
#define RTLD_NEXT ((void *)-1l)

typedef unsigned char uchar;
typedef int (*RegisterResource)(int, const uchar *, const uchar *, const uchar *);

static void exportResources()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    const QString root = QStringLiteral("/home/user/z03plus/qrc");
    QDir().mkpath(root);

    QDirIterator it(QStringLiteral(":"), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QFileInfo info(source);
        const QString relative = source.mid(2); // remove ':/'
        const QString destination = root + QLatin1Char('/') + relative;
        if (info.isDir()) {
            QDir().mkpath(destination);
            continue;
        }
        QDir().mkpath(QFileInfo(destination).path());
        QFile::remove(destination);
        QFile::copy(source, destination);
    }

    QFile marker(root + QStringLiteral("/.z03plus-export-complete"));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write("Z03Plus resource export completed\n");
    }
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

    const int result = original ? original(version, tree, names, data) : 0;
    // Z03's own QRC is position-independent only in its data, but this table
    // itself is a fixed image address in the non-PIE executable.
    if (tree == reinterpret_cast<const uchar *>(0x001ab51c))
        exportResources();
    return result;
}
