/*
mqtcpserver.h

Version: 1.0.1
Protocol Version: 1

This file is part of MQTCP CPP Qt Server.

MQTCP CPP Qt Server is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MQTCP CPP Qt Server is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MQTCP CPP Qt Server. If not, see <http://www.gnu.org/licenses/>.

Author: Bapt59
*/

#ifndef MQTCPSERVER_H
#define MQTCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include "mqtcpsocket.h"

class MqTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit MqTcpServer(bool verbose, QObject *parent = nullptr);
    ~MqTcpServer();

    bool start(quint16 port = 4283);

    void sendMessage(const QString &author, const QString &topic, const QByteArray &message);
    int sendPending();

    QList<MqTcpSocket *>* getSockets() const;

public slots:
    void connected();
    void messageReceived(QString topic, QByteArray message);
    void disconnected();
    void checkForTransfer();

signals:
    void newConnection(MqTcpSocket* socket);

    void debugLine(QString);

    void wakeUp();

private:
    bool verbose;

    QTcpServer *server;
    QList<MqTcpSocket*>* sockets;

};

#endif // MQTCPSERVER_H
