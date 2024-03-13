/*
mqtcp.hpp

Version: 1.1.1
Protocol Version: 1

This file is part of MQTCP Arduino Client.

MQTCP Arduino Client is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MQTCP Arduino Client is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MQTCP Arduino Client. If not, see <http://www.gnu.org/licenses/>.

Author: Bapt59
*/

#pragma once
#ifndef MQTCP_HPP
#define MQTCP_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <cstring>

//Set to true if you want the library to check :
//- Size of publish packets
//
//Set to false to disable these securities and increase performance
#define MQTCP_SECURE_MODE false

#define MQTCP_PROTOCOL_VERSION 1
#define MQTCP_INITIAL_MESSAGE_SIZE 4/*Message Size*/+1/*Message ID*/+1/*Version number*/+4/*Message Size*/+1/*Message ID*/+6/*Mac address*/ //Client name is not counted
#define MQTCP_COMMON_TIMEOUT 30000

struct MqTcpMessage
{
    String author;
    String topic;
    uint8_t* message;
    uint32_t messageSize;
};

class MqTcpClient
{
public:
    enum SendPendingReturnedValue {
        NothingToSend, //No messages were to be sent
        WaitingAcknowledgement, //Can't send next message as the previous one wasn't acknowledged yet
        NotConnected, //Couldn't connect to the server
        FinishedSending, //Message was fully sent to the server
        PartiallySended //Message was partly sent to the server, next part will be sent upon next call of the function
    };

    //Convert all of message.message to a returned String
    static String dataToString(const MqTcpMessage& message);
    //Convert data from start index (included) to end index (excluded) to a returned String
    static String dataToString(uint8_t* data, uint32_t start, uint32_t end);
    //Convert all of message.message to the string str
    static void dataToString(const MqTcpMessage& message, String& str);
    //Convert data from start index (included) to end index (excluded) to the string str
    static void dataToString(uint8_t* data, uint32_t start, uint32_t end, String& str);
    
    MqTcpClient(String name);
    ~MqTcpClient();

    //Set server IP and port
    //Setting port to 0 (default value) disable the client
    //Also delete all messages received
    void connect(IPAddress ip, uint16_t port = 4283);

    //Prepare a packet to send with a topic and a message
    //Warning : putting '\0' in the topic is unauthorized and the server will kick you if you do that
    //Return true if the packet was added to the list, always return true if not on secure mode
    bool publish(String topic, String msg);
    //Prepare a packet to send with a topic and a message
    //Warning : putting '\0' in the topic is unauthorized and the server will kick you if you do that
    //Return true if the packet was added to the list, always return true if not on secure mode
    bool publish(String topic, uint8_t* msg, uint32_t msgSize);

    //Prepare a packet to subscribe to a topic to the server
    void subscribe(String topic);

    //Prepare a packet to unsubscribe to a topic to the server
    void unsubscribe(String topic);

    //Send last queued messages and return how the function terminated
    //If not connected to the server, it will first try to connect and return NotConnected if a failure ocurred
    //Tip : Call this method as often as possible to ensure that all messages are sent ASAP
    SendPendingReturnedValue sendPending();

    //Is MQTCPClient connected to the server?
    bool isConnected();

    //Is there an unread message?
    bool hasMessage();

    //Get the last unread message
    //That message will then permanently be removed from MQTCPClient and should be stored somewhere else if needed
    MqTcpMessage readMessage();

private:
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

    void renit();

    bool connect();
    void readWifi();
    // SendPendingReturnedValue sendOne();
    void sendAcknowledgement();

    //messageSize must not consider messageSize size
    void writeMessageSize(uint8_t* buffer, uint32_t messageSize);
    void subUnsub(String topic, MessageMQTCPClientType type);

    void readNullTerminatedString(char* buffer, uint32_t& index, uint32_t size, String& string);
    void readNonTerminatedString(char* buffer, uint32_t& index, uint32_t size, String& string);
    void readData(char* buffer, uint32_t& index, uint32_t size, uint8_t* bufferOut);

    WiFiClient client;
    String clientName;

    std::vector<uint8_t*> messagesQueued; //std::vector<uint8_t[]>
    IPAddress serverIp;
    uint16_t serverPort;

    uint8_t nextMessageSizeSplited[4];
    uint8_t nextMessageSizePart;
    uint32_t nextMessageSize; //0 = Don't know
    MqTcpMessage pendingMessage;
    bool hasPendingMessage;

    bool waitingForAcknowledgement;
    uint32_t waitingForAcknowledgementUntil;
    bool sendAcknowledgementSoon;

    uint32_t nextTcpWriteOffset;
};

#endif