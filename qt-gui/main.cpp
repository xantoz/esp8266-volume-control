#include <QApplication>

#include "window.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Window window("192.168.0.21", 1128);
    window.show();
    return app.exec();
}

