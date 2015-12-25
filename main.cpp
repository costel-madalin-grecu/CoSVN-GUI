#include "Gui/MainWindow.h"
#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QIcon appIcon(":/Icons/TheCostel.png");
    a.setWindowIcon(appIcon);

    MainWindow w(a);
    w.showMaximized();

    return a.exec();
}
