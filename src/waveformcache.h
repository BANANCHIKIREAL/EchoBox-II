#pragma once

#include <QHash>
#include <QStringList>
#include <QUrl>
#include <QVector>

// Persistent, validated waveform cache. MainWindow only asks this component
// to load/save peaks and does not know anything about the binary format,
// invalidation rules or memory/disk limits.
class WaveformCache {
public:
    bool load(const QUrl &url, qint64 duration, QVector<float> *peaks);
    void save(const QUrl &url, qint64 duration, const QVector<float> &peaks);

    static QString directoryPath();

private:
    QString keyFor(const QUrl &url, qint64 duration) const;
    void remember(const QString &key, const QVector<float> &peaks);

    QHash<QString, QVector<float>> m_memory;
    QStringList m_memoryOrder;
};
