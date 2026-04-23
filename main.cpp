#include <QApplication>

#include "window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("DJS-103");
    Window window;
    window.show();
    return app.exec();
}
