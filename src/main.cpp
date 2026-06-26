#include <QApplication>
#include "mainwindow.h"
#include "logo.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EchoBox II");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("EchoBox");
    app.setWindowIcon(QIcon(createLogo(256)));

    MainWindow w;
    w.show();

    return app.exec();
}
