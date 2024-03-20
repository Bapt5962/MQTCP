/*
main.cpp

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

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include "consolehandler.h"

static const QList<QString> configKeys = {
    "verbose",
    "port"
};

ConsoleHandler* handler;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    bool verbose = false;
    quint16 port = 4283;

    if(QFile::exists("config.txt"))
    {
        qInfo() << "Loading config.txt";

        QFile config("config.txt");
        if(config.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&config);
            int lineNumber = 0;
            while (!in.atEnd()) {
                QString line = in.readLine();
                QStringList pair = line.split(":");
                if(pair.size() == 2)
                {
                    switch (configKeys.indexOf(pair.first())) {
                    case 0: //Verbose
                        if(pair.last() == "true")
                            verbose = true;
                        else if(pair.last() == "false")
                            verbose = false;
                        else
                            qInfo() << "Couldn't set verbosity from file";
                        break;
                    case 1: //Port
                    {
                        bool isOk;
                        int number = pair.last().toInt(&isOk);
                        if(!isOk)
                            qInfo() << "Port value is not an integer";
                        else if(number < 1 || number > 65535)
                            qInfo() << "Port value is not within [1 ; 65535]";
                        else
                            port = number;
                    }
                        break;
                    }
                }
                else
                {
                    qInfo() << "Line " << lineNumber << " of config file is unreadable";
                }
                lineNumber++;
            }
        }
        else
        {
            qInfo() << "Could not open config.txt";
        }
    }
    else
    {
        qInfo() << "No config.txt file found";
    }

    handler = new ConsoleHandler(verbose, port);

    return a.exec();
}
