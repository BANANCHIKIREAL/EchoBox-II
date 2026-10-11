#include "windowsmediasession.h"

#include <QApplication>
#include <QDebug>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(QStringLiteral("EchoBox media controls smoke test"));
    window.show();

    WindowsMediaSession session(reinterpret_cast<void*>(window.winId()));
    if (!session.isAvailable()) {
        qCritical() << "Windows media controls were not initialized";
        return 1;
    }
    session.setTrack(QStringLiteral("Тестовая музыка"), QStringLiteral("EchoBox II"));
    session.setPlaybackState(WindowsMediaSession::PlaybackState::Playing);
    qInfo() << "Windows media controls initialized and music metadata sent";
    return 0;
}
