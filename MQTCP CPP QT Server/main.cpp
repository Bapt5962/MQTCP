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
