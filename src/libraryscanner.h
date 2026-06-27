#pragma once
#include <QObject>
#include <QUrl>
#include <QList>
#include <atomic>

class LibraryScanner : public QObject {
    Q_OBJECT
public:
    explicit LibraryScanner(const QString &rootPath, QObject *parent = nullptr);
    void cancel() { m_cancelled = true; }

public slots:
    void run();

signals:
    void progress(int found, int scanned);
    void batchReady(QList<QUrl> batch);
    void finished(int total);

private:
    QString            m_root;
    std::atomic<bool>  m_cancelled{false};
};
