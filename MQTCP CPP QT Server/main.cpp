#include <QCoreApplication>
#include "consolehandler.h"

ConsoleHandler* handler;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    handler = new ConsoleHandler(true);

    return a.exec();
}
