#include "mqtcp.hpp"

String MqTcpClient::dataToString(const MqTcpMessage& message)
{
    String temp;
    dataToString(message, temp);
    return temp;
}

String MqTcpClient::dataToString(uint8_t* data, uint32_t start, uint32_t end)
{
    String temp;
    dataToString(data, start, end, temp);
    return temp;
}

void MqTcpClient::dataToString(const MqTcpMessage& message, String& str)
{
    dataToString(message.message, 0, message.messageSize, str);
}

void MqTcpClient::dataToString(uint8_t* data, uint32_t start, uint32_t end, String& str)
{
    str = "";
    for(uint32_t i = start; i < end; i++)
    {
        str += (char)(data[i]);
    }
}

MqTcpClient::MqTcpClient(String name)
{
    serverPort = 0;
    clientName = name;
    pendingMessage.message = (uint8_t*)malloc(1);
    pendingMessage.messageSize = 1;
    renit();
}

MqTcpClient::~MqTcpClient()
{
    for(int m = 0; m < messagesQueued.size(); m++)
    {
        free(messagesQueued[m]);
    }
}

void MqTcpClient::connect(IPAddress ip, uint16_t port)
{
    serverIp = ip;
    serverPort = port;
    connect();
}

bool MqTcpClient::publish(String topic, String msg)
{
    return publish(topic, (uint8_t*)msg.c_str(), msg.length());
}

bool MqTcpClient::publish(String topic, uint8_t* msg, uint32_t msgSize)
{
    //2 bytes for Message ID + '\0'
    //bufferSizeReduced = bufferSize - 4 (message size)
    const uint32_t bufferSizeReduced(2 + topic.length() + msgSize);

    #if MQTCP_SECURE_MODE
        if(bufferSizeReduced >= (0xFF000000 - clientName - 1)) //Also take into account the server publish
            return false;
    #endif

    uint8_t* buffer = (uint8_t*)malloc(bufferSizeReduced + 4);

    //Write message size
    writeMessageSize(buffer, bufferSizeReduced);

    //Write message type ID
    buffer[4] = ClientPublish;

    //Write message MQTCP topic
    for (unsigned int i = 5; i < topic.length() + 5; i++)
    {
        buffer[i] = topic[i - 5];
    }
    buffer[topic.length() + 5] = '\0';

    //Write message
    for (unsigned int i = topic.length() + 6; i < bufferSizeReduced + 4; i++)
    {
        buffer[i] = msg[i - topic.length() - 6];
    }
    //messagesQueued.push_back('\0');
    
    messagesQueued.push_back(buffer);

    return true;
}

void MqTcpClient::subscribe(String topic)
{
    subUnsub(topic, ClientSubscribe);
}

void MqTcpClient::unsubscribe(String topic)
{
    subUnsub(topic, ClientUnsubscribe);
}


MqTcpClient::SendPendingReturnedValue MqTcpClient::sendPending()
{
    //Are we connected?
    if(!isConnected() || (waitingForAcknowledgement && millis() > waitingForAcknowledgementUntil))
    {
        //No: Try to connect
        connect();
        if(!isConnected()) //Can't connect, we stop
            return NotConnected;
    }
    if(sendAcknowledgementSoon && nextTcpWriteOffset == 0)
        sendAcknowledgement();
    if(waitingForAcknowledgement)
        return WaitingAcknowledgement;
    //Do we have a message to send?
    if(messagesQueued.size() == 0)
        return NothingToSend;
    uint32_t messageSize;
    //Copy message size found at start of the message
    std::reverse_copy(messagesQueued[0], messagesQueued[0]+4, &messageSize); //std::memcpy(&messageSize, messagesQueued[0], 4);
    //Add 4 bytes for messageSize ; Remove what has already been sent
    messageSize += 4 - nextTcpWriteOffset;
    uint32_t written(client.write(messagesQueued[nextTcpWriteOffset], messageSize));
    if(written == messageSize) {
        //Reset offset for next time
        nextTcpWriteOffset = 0;
        waitingForAcknowledgement = true;
        waitingForAcknowledgementUntil = millis() + MQTCP_COMMON_TIMEOUT;
        return FinishedSending;
    } else {
        //Add offset : part of message which has already been sent
        nextTcpWriteOffset += written;
        return PartiallySended;
    }
}

void MqTcpClient::sendAcknowledgement()
{
    uint32_t timeout = millis() + MQTCP_COMMON_TIMEOUT;
    while (client.write(uint8_t(0xFF)) == 0)
    {
        if(millis() > timeout)
        {
            client.stop();
            return;
        }
    }
    sendAcknowledgementSoon = false;
}

bool MqTcpClient::isConnected()
{
    return client.connected();
}

bool MqTcpClient::hasMessage()
{
    readWifi();
    return hasPendingMessage;
}

MqTcpMessage MqTcpClient::readMessage()
{
    readWifi();
    hasPendingMessage = false;
    return pendingMessage;
}

void MqTcpClient::renit()
{
    nextMessageSize = 0;
    nextMessageSizePart = 0;
    hasPendingMessage = false;
    nextTcpWriteOffset = 0;
    waitingForAcknowledgement = false;
    sendAcknowledgementSoon = false;
    client.flush();
}

bool MqTcpClient::connect()
{
    if(serverPort == 0)
        return false;
    if(!client.connect(serverIp, serverPort))
        return false;
    renit();

    //Allocate memory for two messages : version and login
    uint32_t baseMessageSize = MQTCP_INITIAL_MESSAGE_SIZE + clientName.length();
    uint8_t buffer[baseMessageSize];

    //Version message
    writeMessageSize(buffer, 6);
    buffer[4] = ClientVersion;
    buffer[5] = MQTCP_PROTOCOL_VERSION;

    //Login message
    writeMessageSize(buffer + 6, baseMessageSize - 4 - 6);
    buffer[10] = ClientLogin;
    WiFi.macAddress(buffer + 11);
    std::memcpy(&buffer[MQTCP_INITIAL_MESSAGE_SIZE], clientName.c_str(), clientName.length());

    //Write the two messages
    uint32_t timeout = millis() + MQTCP_COMMON_TIMEOUT;
    size_t written = 0;
    while (written < baseMessageSize)
    {
        if(timeout < millis())
        {
            client.stop();
            return false;
        }
        written += client.write(buffer + written, baseMessageSize - written);
    }
    waitingForAcknowledgement = true;
    waitingForAcknowledgementUntil = millis() + MQTCP_COMMON_TIMEOUT;
    return true;
}

void MqTcpClient::readWifi()
{
    if(hasPendingMessage == true)
        return;
    if(nextMessageSize == 0)
    {
        while (client.available() > 0 && nextMessageSizePart < 4)
        {
            nextMessageSizeSplited[nextMessageSizePart] = client.read();
            if(nextMessageSizeSplited[0] == 0xFF) //Acknowledgement
            {
                if(waitingForAcknowledgement)
                {
                    //Delete message from client
                    free(messagesQueued[0]);
                    messagesQueued.erase(messagesQueued.begin(), messagesQueued.begin() + 1);
                    waitingForAcknowledgement = false;
                }
                return;
            }
            nextMessageSizePart++;
        }
        if(nextMessageSizePart == 4)
        {
            std::reverse_copy(nextMessageSizeSplited, nextMessageSizeSplited + 4, (uint8_t*)(&nextMessageSize));
            nextMessageSizePart = 0;
        }
        else
        {
            return;
        }
    }
    if(sendAcknowledgementSoon)
        return;
    if(nextMessageSize != 0)
    {
        if(client.available() >= nextMessageSize)
        {
            uint8_t* buffer = (uint8_t*)malloc(nextMessageSize);
            client.read((uint8_t*)(buffer), nextMessageSize);
            /*Serial.print("Debug infos: ");
            for(int b=0; b < nextMessageSize; b++)
            {
                Serial.print(buffer[b]);
            }
            Serial.println("");*/
            uint32_t bufferReading = 1;
            switch (buffer[0])
            {
            case ServerPublish:
                readNullTerminatedString((char*)buffer, bufferReading, nextMessageSize, pendingMessage.author);
                bufferReading++;
                readNullTerminatedString((char*)buffer, bufferReading, nextMessageSize, pendingMessage.topic);
                bufferReading++;
                free(pendingMessage.message);
                pendingMessage.message = (uint8_t*)malloc(nextMessageSize - bufferReading);
                pendingMessage.messageSize = nextMessageSize - bufferReading;
                readData((char*)buffer, bufferReading, nextMessageSize, pendingMessage.message);
                hasPendingMessage = true;
                nextMessageSize = 0;
                sendAcknowledgementSoon = true;
                break;
            case ServerKick:
                switch (buffer[1])
                {
                case KickUnspecified:
                    Serial.println("MQTCP Server kicked us for unspecified reasons.");
                    break;
                case KickStringSpecified:
                {
                    String reason;
                    uint32_t bufferRead = 2;
                    readNonTerminatedString((char*)buffer, bufferRead, nextMessageSize, reason);
                    Serial.println("MQTCP Server kicked us for following reason:");
                    Serial.println(reason);
                }
                    break;
                case KickVersionNonMatching:
                    Serial.println("MQTCP Server kicked us for non-matching protocol version:");
                    Serial.print("Client version = ");
                    Serial.println(MQTCP_PROTOCOL_VERSION);
                    Serial.print("Server version = ");
                    Serial.println(int(buffer[2]));
                    break;
                case KickVersionNotReceived: //This should never happen using this library
                    Serial.println("MQTCP Server kicked us for not sending version value.");
                    break;
                case KickLoginNotReceived: //This should never happen using this library
                    Serial.println("MQTCP Server kicked us for not sending login infos.");
                    break;
                }
                break;
            }
            free(buffer);
        }
    }
}

void MqTcpClient::writeMessageSize(uint8_t* buffer, uint32_t messageSize)
{
    buffer[0] = uint8_t(messageSize >> 24);
    buffer[1] = uint8_t(messageSize >> 16);
    buffer[2] = uint8_t(messageSize >> 8);
    buffer[3] = uint8_t(messageSize);
}

void MqTcpClient::subUnsub(String topic, MessageMQTCPClientType type)
{
    //1 byte for Message ID
    //bufferSizeReduced = bufferSize - 4 (message size)
    const uint32_t bufferSizeReduced(1 + topic.length());

    uint8_t* buffer = (uint8_t*)malloc(bufferSizeReduced + 4);

    //Write message size
    writeMessageSize(buffer, bufferSizeReduced);

    //Write message type ID
    buffer[4] = type;

    //Write message MQTCP topic
    for (int i = 5; i < topic.length() + 5; i++)
    {
        buffer[i] = topic[i - 5];
    }
    
    messagesQueued.push_back(buffer);
}

void MqTcpClient::readNullTerminatedString(char* buffer, uint32_t& index, uint32_t size, String& string)
{
    string = "";
    while (index < size) {
        if(buffer[index] == '\0')
            break;
        string += buffer[index];
        index++;
    }
}

void MqTcpClient::readNonTerminatedString(char* buffer, uint32_t& index, uint32_t size, String& string)
{
    string = "";
    while (index < size) {
        string += buffer[index];
        index++;
    }
}

void MqTcpClient::readData(char* buffer, uint32_t& index, uint32_t size, uint8_t* bufferOut)
{
    int offset = index;
    while (index < size) {
        bufferOut[index - offset] = buffer[index];
        index++;
    }
}