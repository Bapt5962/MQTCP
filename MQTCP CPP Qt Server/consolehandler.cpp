#include "consolehandler.h"

ConsoleHandler::ConsoleHandler(bool verbose, quint16 port, QObject *parent) : QObject(parent)
{
    server = new MqTcpServer(verbose);
    connect(server, SIGNAL(debugLine(QString)), this, SLOT(debugLine(QString)));
    connect(server, SIGNAL(wakeUp()), this, SLOT(loop()));
    server->start(port);
}

ConsoleHandler::~ConsoleHandler()
{
    delete server;
}

void ConsoleHandler::debugLine(QString debug)
{
    qInfo() << debug;
}

void ConsoleHandler::loop()
{
    if(server->sendPending() > 0)
        QTimer::singleShot(1, this, SLOT(loop()));
}
