#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VAMergeSplit"));
    QCoreApplication::setApplicationName(QStringLiteral("VAMergeSplit"));

    QIcon appIcon;
    for (int size : {16, 32, 48, 64, 128, 256})
        appIcon.addFile(QStringLiteral(":/icons/app-%1.png").arg(size));
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.show();
    return app.exec();
}
