#include <QtGlobal>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "window.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(argv[0]);
    QApplication::setApplicationVersion("0.2");

    QCommandLineParser parser;
    parser.setApplicationDescription("ESP8266 Volume Control QT GUI (client)");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        "hostname",
        QApplication::translate("main", "Hostname to connect to. If specified, connect automatically."));
    parser.addPositionalArgument(
        "port",
        QApplication::translate("main", "Port to connect to. (default = 1182 (UDP), 1128 (TCP)"));

    // TODO: rewrite to use QCommandLineParser::addOptions instead
    QCommandLineOption useUdpOpt(
        QStringList({"u", "udp"}),
        QApplication::translate("main", "Connect using UDP protocol"));
    parser.addOption(useUdpOpt);
    QCommandLineOption useTcpOpt(
        QStringList({"t", "tcp"}),
        QApplication::translate("main", "Connect using TCP protocol (default)"));
    parser.addOption(useTcpOpt);
    QCommandLineOption updateIntervalOpt(
        QStringList({"f", "update-interval"}),
        QApplication::translate("main", "How often to ping server for status updates (UDP protocol only)"),
        "ms", "2000");
    parser.addOption(updateIntervalOpt);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    bool useUdp = parser.isSet(useUdpOpt);
    bool useTcp = parser.isSet(useTcpOpt);
    bool updateIntervalOk = false;
    unsigned updateInterval = parser.value(updateIntervalOpt).toUInt(&updateIntervalOk);

    if (!updateIntervalOk)
        qFatal("Update interval must be a positive integer.");

    Protocol *protocol = NULL;
    if (useTcp)
        protocol = new TcpProtocol();
    else if (useUdp)
        protocol = new UdpProtocol(updateInterval);
    else
        protocol = new TcpProtocol();

    bool portOk = true;
    Window *window;
    window = (args.length() == 0) ? new Window(protocol)                                           :
             (args.length() == 1) ? new Window(protocol, args.at(0))                               :
             (args.length() == 2) ? new Window(protocol, args.at(0), args.at(1).toUShort(&portOk)) : NULL;

    // TODO: stricter requirements here (check range) + toUShort feels ungood
    // when we actually are dealing with a quint16
    if (!portOk)
        qFatal("Port must be a positive integer.");

    if (window == NULL)
        qFatal("Too many positional arguments.");

    window->show();

    return app.exec();
}
