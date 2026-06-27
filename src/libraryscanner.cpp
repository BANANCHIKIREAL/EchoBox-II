#include "libraryscanner.h"
#include <QDirIterator>
#include <QFileInfo>

static const QStringList kMediaExts = {
    "*.mp3","*.flac","*.wav","*.ogg","*.m4a","*.aac","*.opus",
    "*.wma","*.mp4","*.mkv","*.avi","*.mov","*.webm","*.wmv"
};

LibraryScanner::LibraryScanner(const QString &rootPath, QObject *parent)
    : QObject(parent), m_root(rootPath)
{}

void LibraryScanner::run()
{
    QDirIterator it(m_root, kMediaExts, QDir::Files, QDirIterator::Subdirectories);

    QList<QUrl> batch;
    int scanned = 0;
    int found   = 0;
    constexpr int BATCH = 50;

    while (it.hasNext() && !m_cancelled) {
        batch.append(QUrl::fromLocalFile(it.next()));
        ++scanned;
        ++found;

        if (batch.size() >= BATCH) {
            emit batchReady(batch);
            batch.clear();
            emit progress(found, scanned);
        }
    }

    if (!batch.isEmpty() && !m_cancelled)
        emit batchReady(batch);

    emit progress(found, scanned);
    emit finished(found);
}
