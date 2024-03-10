#ifndef CONSOLEHANDLER_H
#define CONSOLEHANDLER_H

#include <QObject>
#include <QTimer>
#include "MqTcp/mqtcpserver.h"

class ConsoleHandler : public QObject
{
    Q_OBJECT
public:
    explicit ConsoleHandler(bool verbose, quint16 port = 4283, QObject *parent = nullptr);
    ~ConsoleHandler();

public slots:
    void debugLine(QString debug);

    void loop();

private:
    MqTcpServer *server;

};

#endif // CONSOLEHANDLER_H
