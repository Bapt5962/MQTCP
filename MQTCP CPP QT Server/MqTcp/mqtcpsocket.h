#ifndef MQTCPSOCKET_H
#define MQTCPSOCKET_H

#include <QObject>
#include <QTcpSocket>
#include <QDataStream>
#include <QHostAddress>

#define MQTCP_PROTOCOL_VERSION 0

struct MacAddress
{
    char d[6] = {0, 0, 0, 0, 0, 0};
};

class MqTcpSocket : public QObject
{
    Q_OBJECT
public:
    enum MessageMQTCPClientType {
        ClientPublish = 0,
        ClientSubscribe = 1,
        ClientLogin = 2,
        ClientUnsubscribe = 3,
        ClientVersion = 4,
    };
    enum MessageMQTCPServerType {
        ServerPublish = 0,
        ServerKick = 1,
    };
    enum KickReason {
        KickUnspecified = 0,
        KickStringSpecified = 1,
        KickVersionNonMatching = 2,
        KickVersionNotReceived = 3,
        KickLoginNotReceived = 4,
    };

    explicit MqTcpSocket(QTcpSocket* s, QObject *parent = nullptr);
    ~MqTcpSocket();

    QTcpSocket *getSocket() const;

    void write(const QString &author, const QString &topic, const QByteArray &message);
    void forceWrite(const QString &author, const QString &topic, const QByteArray &message);
    void packMessage(const QByteArray &message);

    int sendPending();

    MacAddress getMacAddress() const;

    QList<QByteArray> *getMessageQueue() const;
    void setMessageQueue(QList<QByteArray> *value);

    QString getClientName() const;

    void kickClient(KickReason reason, const QByteArray& infos = QByteArray());

public slots:
    void dataGet();
    void disconnected();
    //void dataSent(qint64 bytes);

signals:
    void messageReceived(QString topic, QByteArray message);
    void disconnectSocket();
    void checkForTransfer();

    void debugLine(QString);

    void wakeUp();

private:    
    enum AuthentificationState {
        NoInfo,
        NotAuthentified,
        Authentified
    };

    QTcpSocket* socket;

    QString clientName;

    quint8 messageLengthSplited[4];
    quint8 messageLengthPart;
    quint32 messageLength;

    //qint64 waitingToSendBytes;
    //qint64 lastWriteBytes;
    QList<QByteArray>* messageQueue;
    quint32 nextTcpWriteOffset;

    MacAddress macAddress;

    QList<QString> subbedTopic;

    bool dead;
    AuthentificationState authentified;
    bool waitingAcknowledgement;
    bool acknowledgementToSend;
    bool toKick;

    const static char acknowledgement[1];
};

bool operator==(const MacAddress& a, const MacAddress& b);

#endif // MQTCPSOCKET_H
