#include "mqtcpsocket.h"

const char MqTcpSocket::acknowledgement[1] =
{
    char(0xFF)
};

MqTcpSocket::MqTcpSocket(QTcpSocket* s, QObject *parent) : QObject(parent)
{
    socket = s;
    //socket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    nextTcpWriteOffset = 0;
    messageLengthPart = 0;
    messageLength = 0;
    messageQueue = new QList<QByteArray>();
    dead = false;
    authentified = NoInfo;
    waitingAcknowledgement = false;
    acknowledgementToSend = false;
    toKick = false;

    connect(socket, SIGNAL(readyRead()), this, SLOT(dataGet()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    //connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(dataSent(qint64)));
}

MqTcpSocket::~MqTcpSocket()
{
    delete socket;
    if(messageQueue != nullptr)
        delete messageQueue;
}

QTcpSocket *MqTcpSocket::getSocket() const
{
    return socket;
}

void MqTcpSocket::write(const QString &author, const QString &topic, const QByteArray &message)
{
    if(subbedTopic.contains(topic))
        forceWrite(author, topic, message);
}

void MqTcpSocket::forceWrite(const QString &author, const QString &topic, const QByteArray &message)
{
    if(toKick)
        return;
    QByteArray msg;
    msg.append(quint8(ServerPublish));
    msg.append(author);
    msg.append('\0');
    msg.append(topic);
    msg.append('\0');
    msg.append(message);
    //msg.append('\0');

    packMessage(msg);
}

void MqTcpSocket::packMessage(const QByteArray &message)
{
    if(toKick)
        return;

    messageQueue->append(QByteArray());
    QDataStream out(&messageQueue->last(), QIODevice::WriteOnly);
    out << quint32(message.size());
    messageQueue->last().append(message);

    sendPending();
}

int MqTcpSocket::sendPending()
{
    if(authentified != Authentified && (!toKick))
        return 0;
    if(dead)
        return 0;
    if(acknowledgementToSend)
    {
        if(socket->write(acknowledgement, 1) == 0)
            return 0;
        acknowledgementToSend = false;
    }
    if(waitingAcknowledgement)
        return 0;
    if(messageQueue->size() == 0)
        return 0;

    quint64 written = socket->write(messageQueue->first().data() + nextTcpWriteOffset, messageQueue->first().size() - nextTcpWriteOffset);
    if(written == messageQueue->first().size() - nextTcpWriteOffset)
    {
        if(toKick && messageQueue->size() == 1)
            socket->disconnectFromHost();
        waitingAcknowledgement = true;
        nextTcpWriteOffset = 0;
    }
    else
    {
        nextTcpWriteOffset += written;
    }
    return written;
}

void MqTcpSocket::dataGet()
{
    QDataStream in(socket);

    /*if (socket->bytesAvailable() >= (int)sizeof(quint8))
    {
        quint8 value;
        in >> value;
        qDebug() << QString::number(value) << " | " << char(value);
        dataGet();
    }

    return;*/

    if (messageLength == 0)
    {
        while (socket->bytesAvailable() > 0 && messageLengthPart < 4)
        {
            in >> messageLengthSplited[messageLengthPart];
            if(messageLengthSplited[0] == 0xFF) //Acknowledgement
            {
                if(waitingAcknowledgement)
                {
                    messageQueue->removeFirst();
                    waitingAcknowledgement = false;
                }
                emit wakeUp();
                return;
            }
            messageLengthPart++;
        }
        if(messageLengthPart == 4)
        {
            std::reverse_copy(messageLengthSplited, messageLengthSplited + 4, (quint8*)(&messageLength));
            messageLengthPart = 0;
        }
        else
        {
            return;
        }
        //qDebug() << "About to receive : " + QString::number(messageLength) + "o of data.";
    }
    if (socket->bytesAvailable() < messageLength)
    {
        return;
    }

    quint8 typeMsg;
    in >> typeMsg;

    int leftToRead = messageLength - 1;

    switch (authentified) {
    case NoInfo:
        if(typeMsg != ClientVersion)
            kickClient(KickVersionNotReceived);
        break;
    case NotAuthentified:
        if(typeMsg != ClientLogin)
            kickClient(KickLoginNotReceived);
        break;
    default:
        break;
    }


    switch (typeMsg) {
    case ClientPublish:
    {
        QString topic("");
        while (socket->bytesAvailable() > 0 && leftToRead > 0) {
            qint8 nextChar;
            in >> nextChar;
            leftToRead--;
            if(nextChar == '\0')
                break;
            topic.append(nextChar);
        }
        QByteArray msg("");
        while (socket->bytesAvailable() > 0 && leftToRead > 0) {
            qint8 nextChar;
            in >> nextChar;
            leftToRead--;
            msg.append(nextChar);
        }
        acknowledgementToSend = true;
        if(!(toKick || dead))
            emit messageReceived(topic, msg);
    }
        break;
    case ClientSubscribe:
    case ClientUnsubscribe:
    {
        QString topic("");
        while (socket->bytesAvailable() > 0 && leftToRead > 0) {
            qint8 nextChar;
            in >> nextChar;
            leftToRead--;
            topic.append(nextChar);
        }
        acknowledgementToSend = true;
        if(typeMsg == ClientSubscribe)
        {
            emit debugLine(clientName + " subbed to " + topic);
            if(!subbedTopic.contains(topic))
                subbedTopic.append(topic);
        }
        else //typeMsg == ClientUnsubscribe
        {
            emit debugLine(clientName + " unsubbed to " + topic);
            subbedTopic.removeOne(topic);
        }
    }
        break;
    case ClientLogin:
    {
        socket->read(macAddress.d, 6);
        leftToRead -= 6;
        /*for(int d(0); d < 6; d++)
        {
            qDebug() << macAddress.d[d];
        }*/
        clientName = "";
        while (socket->bytesAvailable() > 0 && leftToRead > 0) {
            qint8 nextChar;
            in >> nextChar;
            leftToRead--;
            clientName.append(nextChar);
        }
        authentified = Authentified;
        acknowledgementToSend = true;
        emit checkForTransfer();
    }
        break;
    case ClientVersion:
    {
        quint8 protocolVersion;
        in >> protocolVersion;
        if(MQTCP_PROTOCOL_VERSION != protocolVersion)
        {
            emit debugLine("Kicking " + socket->peerAddress().toString() + " for using protocol version " + QString::number(protocolVersion));
            kickClient(KickVersionNonMatching, QByteArray(1, MQTCP_PROTOCOL_VERSION));
            return;
        }
        authentified = NotAuthentified;
    }
        break;
    }

    messageLength = 0;

    emit wakeUp();

    dataGet();
}

void MqTcpSocket::disconnected()
{
    dead = true;
    emit disconnectSocket();
}

QString MqTcpSocket::getClientName() const
{
    return clientName;
}

void MqTcpSocket::kickClient(MqTcpSocket::KickReason reason, const QByteArray &infos)
{
    QByteArray kickPacket;
    kickPacket.append(ServerKick);
    kickPacket.append(reason);
    if(!infos.isEmpty())
        kickPacket.append(infos);
    packMessage(kickPacket);
    toKick = true;
}

QList<QByteArray> *MqTcpSocket::getMessageQueue() const
{
    return messageQueue;
}

void MqTcpSocket::setMessageQueue(QList<QByteArray> *value)
{
    messageQueue = value;
}

MacAddress MqTcpSocket::getMacAddress() const
{
    return macAddress;
}

bool operator==(const MacAddress &a, const MacAddress &b)
{
    for(int d(0); d < 6; d++)
    {
        if(a.d[d] != b.d[d])
            return false;
    }
    return true;
}
