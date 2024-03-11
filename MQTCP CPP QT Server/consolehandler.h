/*
consolehandler.h

Version: 1.0.0
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
