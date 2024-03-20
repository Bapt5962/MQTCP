#include "mqtcpserver.h"

MqTcpServer::MqTcpServer(bool v, QObject *parent) : QObject(parent)
{
    sockets = new QList<MqTcpSocket*>();
    server = new QTcpServer(this);
    verbose = v;
}

MqTcpServer::~MqTcpServer()
{
    qDeleteAll(*sockets);
    delete server;
    delete sockets;
}

bool MqTcpServer::start(quint16 port)
{
    if(!server->listen(QHostAddress::Any, port))
    {
        //if(verbose)
        emit debugLine("Error : " + server->errorString());
        return false;
    }
    else
    {
        //if(verbose)
        emit debugLine("Server launched on port " + QString::number(server->serverPort()));
        connect(server, SIGNAL(newConnection()), this, SLOT(connected()));
        return true;
    }
}

void MqTcpServer::sendMessage(const QString &author, const QString &topic, const QByteArray &message)
{
    for(int s(0); s < sockets->size(); s++)
    {
        if(sockets->at(s)->getClientName() != author)
            sockets->at(s)->write(author, topic, message);
    }
}

int MqTcpServer::sendPending()
{
    int sended(0);
    for(int s(0); s < sockets->size(); s++)
    {
        sended += sockets->at(s)->sendPending();
    }
    return sended;
}

void MqTcpServer::connected()
{
    sockets->append(new MqTcpSocket(server->nextPendingConnection()));

    connect(sockets->last(), SIGNAL(messageReceived(QString, QByteArray)), this, SLOT(messageReceived(QString, QByteArray)));
    connect(sockets->last(), SIGNAL(disconnectSocket()), this, SLOT(disconnected()));
    connect(sockets->last(), SIGNAL(checkForTransfer()), this, SLOT(checkForTransfer()));
    if(verbose)
        connect(sockets->last(), SIGNAL(debugLine(QString)), this, SIGNAL(debugLine(QString)));
    connect(sockets->last(), SIGNAL(wakeUp()), this, SIGNAL(wakeUp()));

    //if(verbose)
    emit debugLine("New connection : " + sockets->last()->getSocket()->peerAddress().toString());

    emit newConnection(sockets->last());

    emit wakeUp();
}

void MqTcpServer::messageReceived(QString topic, QByteArray message)
{
    MqTcpSocket *socket = qobject_cast<MqTcpSocket *>(sender());
    if(verbose)
        emit debugLine("Got message from " + socket->getClientName() + ": " + topic + " : " + QString(message));
    sendMessage(socket->getClientName(), topic, message);
}

void MqTcpServer::disconnected()
{
    MqTcpSocket *socket = qobject_cast<MqTcpSocket *>(sender());
    //if(verbose)
    //{
    if(socket->getMessageQueue() == nullptr)
        emit debugLine("Client reconnected : " + socket->getSocket()->peerAddress().toString());
    else
        emit debugLine("Client disconnected : " + socket->getSocket()->peerAddress().toString());
    //}
}

void MqTcpServer::checkForTransfer()
{
    MqTcpSocket *socket = qobject_cast<MqTcpSocket *>(sender());
    //if(verbose)
    emit debugLine("Received name from : " + socket->getSocket()->peerAddress().toString() + " : " + socket->getClientName());
    for(int s(0); s < sockets->size(); s++)
    {
        if(sockets->at(s) == socket)
            continue;
        if(sockets->at(s)->getMacAddress() == socket->getMacAddress() && sockets->at(s)->getClientName() == socket->getClientName())
        {
            delete socket->getMessageQueue();
            socket->setMessageQueue(sockets->at(s)->getMessageQueue());
            sockets->at(s)->setMessageQueue(nullptr);
            sockets->at(s)->deleteLater();
            sockets->removeAt(s);
            //if(verbose)
            emit debugLine("Message queue transfered for : " + socket->getSocket()->peerAddress().toString());
            return;
        }
    }

    emit wakeUp();
}

QList<MqTcpSocket *>* MqTcpServer::getSockets() const
{
    return sockets;
}
