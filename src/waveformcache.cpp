#include "waveformcache.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

namespace {
constexpr quint32 kMagic = 0x45425746u;
constexpr quint16 kVersion = 1;
constexpr int kMemoryEntries = 12;
constexpr int kDiskEntries = 2048;
constexpr int kMaxPeaks = 2048;
}

QString WaveformCache::directoryPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/waveforms";
}

QString WaveformCache::keyFor(const QUrl &url, qint64 duration) const {
    QByteArray identity = url.toEncoded(QUrl::FullyEncoded);
    if (url.isLocalFile()) {
        const QFileInfo info(url.toLocalFile());
        const QString path = info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath() : info.canonicalFilePath();
        identity += "\npath=";
        identity += path.toUtf8();
        identity += "\nsize=";
        identity += QByteArray::number(info.size());
        identity += "\nmodified=";
        identity += QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    }
    identity += "\nduration=";
    identity += QByteArray::number((duration + 500) / 1000);
    return QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

void WaveformCache::remember(const QString &key, const QVector<float> &peaks) {
    if (key.isEmpty() || peaks.isEmpty()) return;
    m_memoryOrder.removeAll(key);
    m_memoryOrder.append(key);
    m_memory.insert(key, peaks);
    while (m_memoryOrder.size() > kMemoryEntries)
        m_memory.remove(m_memoryOrder.takeFirst());
}

bool WaveformCache::load(const QUrl &url, qint64 duration, QVector<float> *peaks) {
    if (!peaks || !url.isValid() || duration <= 0) return false;
    const QString key = keyFor(url, duration);
    const auto memory = m_memory.constFind(key);
    if (memory != m_memory.constEnd()) {
        *peaks = memory.value();
        m_memoryOrder.removeAll(key);
        m_memoryOrder.append(key);
        return true;
    }

    const QString path = directoryPath() + "/" + key + ".wfc";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    quint32 magic = 0;
    quint16 version = 0;
    qint64 cachedDuration = 0;
    quint32 count = 0;
    stream >> magic >> version >> cachedDuration >> count;
    if (magic != kMagic || version != kVersion || count == 0 || count > kMaxPeaks ||
        qAbs(cachedDuration - duration) > 1500) {
        file.close();
        QFile::remove(path);
        return false;
    }

    QVector<float> loaded;
    loaded.reserve(int(count));
    for (quint32 i = 0; i < count; ++i) {
        float value = 0.0f;
        stream >> value;
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f) {
            file.close();
            QFile::remove(path);
            return false;
        }
        loaded.append(value);
    }
    if (stream.status() != QDataStream::Ok) {
        file.close();
        QFile::remove(path);
        return false;
    }

    *peaks = loaded;
    remember(key, loaded);
    return true;
}

void WaveformCache::save(const QUrl &url, qint64 duration,
                         const QVector<float> &peaks) {
    if (!url.isValid() || duration <= 0 || peaks.isEmpty() || peaks.size() > kMaxPeaks) return;
    for (float value : peaks)
        if (!std::isfinite(value)) return;

    const QString key = keyFor(url, duration);
    remember(key, peaks);

    const QString dirPath = directoryPath();
    if (!QDir().mkpath(dirPath)) return;
    QSaveFile file(dirPath + "/" + key + ".wfc");
    if (!file.open(QIODevice::WriteOnly)) return;

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    stream << kMagic << kVersion << duration << quint32(peaks.size());
    for (float value : peaks) stream << qBound(0.0f, value, 1.0f);
    if (stream.status() != QDataStream::Ok || !file.commit()) return;

    const QFileInfoList files = QDir(dirPath).entryInfoList(
        QStringList() << "*.wfc", QDir::Files, QDir::Time);
    for (int i = kDiskEntries; i < files.size(); ++i)
        QFile::remove(files[i].absoluteFilePath());
}

void WaveformCache::rememberPartial(const QUrl &url, qint64 duration,
                                    const QVector<float> &peaks) {
    if (!url.isValid() || duration <= 0 || peaks.isEmpty()) return;
    remember(keyFor(url, duration), peaks);
}

bool WaveformCache::remove(const QUrl &url, qint64 duration) {
    if (!url.isValid() || duration <= 0) return false;
    const QString key = keyFor(url, duration);
    m_memory.remove(key);
    m_memoryOrder.removeAll(key);
    const QString path = directoryPath() + "/" + key + ".wfc";
    return !QFile::exists(path) || QFile::remove(path);
}

void WaveformCache::clearMemory() {
    m_memory.clear();
    m_memoryOrder.clear();
}
