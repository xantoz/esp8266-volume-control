#include <QApplication>

#include "window.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Window *window;

    QStringList args = QCoreApplication::arguments();
    window = (args.length() <= 1) ? new Window()                                 :
             (args.length() == 2) ? new Window(args.at(1))                       :
             (args.length() == 3) ? new Window(args.at(1), args.at(2).toShort()) : NULL;

    if (window == NULL)
    {
        // TODO: Be more helpful
        qDebug() << "ERROR: Bad cmdline";
        return 1;
    }

    window->show();

    return app.exec();
}
