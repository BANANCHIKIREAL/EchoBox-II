#include <QApplication>
#include <QTimer>
#include "mainwindow.h"
#include "logo.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EchoBox II");
    app.setApplicationVersion(QStringLiteral(ECHOBOX_VERSION));
    app.setOrganizationName("EchoBox");
    app.setWindowIcon(QIcon(createLogo(256, ThemeManager::palette("mocha"))));

    MainWindow w;
    if (!w.startsMinimized()) w.show();
    const QStringList arguments = app.arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments[i];
        if (argument == "--micdeck") {
            QTimer::singleShot(0, &w, [&w] { w.showSoundpad(); });
        }
    }

    return app.exec();
}
