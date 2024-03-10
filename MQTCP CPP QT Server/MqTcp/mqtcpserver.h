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
