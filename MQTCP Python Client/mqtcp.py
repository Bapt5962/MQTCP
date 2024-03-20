'''
mqtcp.py

Version: 1.0.0
Protocol Version: 1

This file is part of MQTCP Python Client.

MQTCP Python Client is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MQTCP Python Client is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MQTCP Python Client. If not, see <http://www.gnu.org/licenses/>.

Author: Bapt59
'''

import socket
from uuid import getnode as get_mac

'''
Message tuple:
(
    str -> author
    str -> topic
    bytearray -> message
)
'''

class MqTcpClient:
    def __get_mqtcp_version(self):
        return 1

    def __get_initial_message_length(self):
        return 7

    def __init__(self, client_name:str, timeout:float = 1):
        if not isinstance(client_name, str):
            raise TypeError("client_name must be a str")
        self.client_name = client_name
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.settimeout(timeout)
        self.__renit()
        
    def __renit(self):
        self.queued_msg = []
        self.next_offset = 0
        self.waiting_for_acknowledgement = False
        self.connected = False
        self.pending_msg = None
        self.next_msg_length = 0
        self.next_msg_length_parts = []
        self.send_acknowledgement_soon = False
        self.msg_buffer = bytearray()
        
    def __del__(self):
        self.client_socket.close()
        
    def connect(self, ip:str, port:int = 4283):
        if not isinstance(ip, str):
            raise TypeError("ip must be a str")
        if not isinstance(port, int):
            raise TypeError("port must be an int")
        if port < 1 or port > 65536:
            raise ValueError("port must be in [1;65536]")
        self.ip = ip
        self.port = port
        self.__connect()
        
    def __connect(self):
        if self.port == 0:
            return False
        if self.client_socket.connect_ex((self.ip, self.port)) != 0:
            return False
        self.__renit()
        packet = bytearray()
        #Version message
        packet += int(6).to_bytes(4, byteorder = 'big')
        packet += bytearray([MqTcpClient.MessageMQTCPClientType.ClientVersion])
        packet += bytearray([self.__get_mqtcp_version()])

        #Login message
        encoded_name = self.client_name.encode()
        packet += int(self.__get_initial_message_length() + len(encoded_name)).to_bytes(4, byteorder = 'big')
        packet += bytearray([MqTcpClient.MessageMQTCPClientType.ClientLogin])
        packet += bytearray(get_mac().to_bytes(6, byteorder = 'big'))
        packet += bytearray(encoded_name)
    
        self.queued_msg.append(packet)
        self.connected = True
        return True
        
    def publish(self, topic:str, msg):
        if isinstance(msg, str):
            return self.publish(topic, msg.encode())
        if not isinstance(topic, str):
            raise TypeError("topic must be a str")
        if not (isinstance(msg, bytes) or isinstance(msg, bytearray)):
            raise TypeError("msg must be a str, a bytearray, or bytes")
        
        encoded_topic = topic.encode()
        packet = bytearray()
        packet += int(len(encoded_topic) + len(msg) + 2).to_bytes(4, byteorder = 'big')
        packet += bytearray([MqTcpClient.MessageMQTCPClientType.ClientPublish])
        packet += bytearray(encoded_topic)
        packet += bytearray([0])
        packet += bytearray(msg)
        self.queued_msg.append(packet)
        
    def subscribe(self, topic:str):
        if not isinstance(topic, str):
            raise TypeError("topic must be a str")
        self.__subunsub(topic, True)
    
    def unsubscribe(self, topic:str):
        if not isinstance(topic, str):
            raise TypeError("topic must be a str")
        self.__subunsub(topic, False)
    
    def __subunsub(self, topic:str, sub:bool):
        encoded_topic = topic.encode()
        packet = bytearray()
        packet += int(len(encoded_topic) + 1).to_bytes(4, byteorder = 'big')
        if sub:
            packet += bytearray([MqTcpClient.MessageMQTCPClientType.ClientSubscribe])
        else:
            packet += bytearray([MqTcpClient.MessageMQTCPClientType.ClientUnsubscribe])
        packet += bytearray(encoded_topic)
        self.queued_msg.append(packet)
        
    def is_connected(self):
        return self.connected
        
    def send_pending(self):
        if not self.connected:
            if not self.__connect():
                return MqTcpClient.SendPendingReturnedValue.NotConnected
        if self.send_acknowledgement_soon and self.next_offset == 0:
            self.__send_acknowledgement()
        if len(self.queued_msg) == 0:
            return MqTcpClient.SendPendingReturnedValue.NothingToSend
        if self.waiting_for_acknowledgement:
            return MqTcpClient.SendPendingReturnedValue.WaitingAcknowledgement
        try:
            sent = self.client_socket.send(self.queued_msg[0][self.next_offset:len(self.queued_msg[0])])
        except:
            self.connected = False
            return MqTcpClient.SendPendingReturnedValue.NotConnected
        if sent == len(self.queued_msg[0]) - self.next_offset:
            self.next_offset = 0
            self.waiting_for_acknowledgement = True
            return MqTcpClient.SendPendingReturnedValue.FinishedSending
        self.next_offset += sent
        return MqTcpClient.SendPendingReturnedValue.PartiallySended
    
    def __send_acknowledgement(self):
        self.client_socket.sendall(bytearray([0xFF]))
        self.send_acknowledgement_soon = False
    
    def has_message(self):
        self.__read_wifi()
        return self.pending_msg != None
    
    def read_message(self):
        self.__read_wifi()
        temp_msg = self.pending_msg
        self.pending_msg = None
        return temp_msg
    
    def __read_wifi(self):
        if self.pending_msg != None:
            return
        if self.next_msg_length == 0:
            while len(self.next_msg_length_parts) < 4:
                try:
                    buffer = self.client_socket.recv(1)
                    '''
                    print("Debug infos 1: ")
                    print(buffer)
                    #'''
                except:
                    break
                self.next_msg_length_parts.append(buffer[0])
                if self.next_msg_length_parts[0] == 255: #Acknowledgement
                    if self.waiting_for_acknowledgement:
                        #Delete message from client
                        self.queued_msg.pop(0)
                        self.waiting_for_acknowledgement = False
                    self.next_msg_length_parts.pop(0)
                    return

            if len(self.next_msg_length_parts) < 4:
                return
            self.next_msg_length = (self.next_msg_length_parts[0] << 24) | (self.next_msg_length_parts[1] << 16) | (self.next_msg_length_parts[2] << 8) | self.next_msg_length_parts[3]
            self.next_msg_length_parts = []
        if self.send_acknowledgement_soon:
            return
        if self.next_msg_length != 0:
            print(self.msg_buffer)
            print(self.next_msg_length)
            while len(self.msg_buffer) < self.next_msg_length:
                try:
                    data = self.client_socket.recv(1)
                    print(data)
                    if not data:  # If no data is received, break the loop
                        break
                    self.msg_buffer += bytearray(data)
                    '''
                    print("Debug infos 2: ")
                    print(self.msg_buffer)
                    #'''
                except:
                    break
            if len(self.msg_buffer) >= self.next_msg_length:
                '''
                print("Debug infos 3: ")
                print(self.msg_buffer)
                #'''
                buffer_reading = 1
                match self.msg_buffer[0]:
                    case MqTcpClient.MessageMQTCPServerType.ServerPublish:
                        (b_author, buffer_reading) = self.__read_null_terminated_string(self.msg_buffer, buffer_reading)
                        (b_topic, buffer_reading) = self.__read_null_terminated_string(self.msg_buffer, buffer_reading + 1)
                        b_msg = self.msg_buffer[buffer_reading+1:]
                        self.pending_msg = (b_author, b_topic, b_msg)
                        self.send_acknowledgement_soon = True
                    case MqTcpClient.MessageMQTCPServerType.ServerKick:
                        match self.msg_buffer[1]:
                            case MqTcpClient.KickReason.KickUnspecified:
                                print("MQTCP Server kicked us for unspecified reasons.")
                            case MqTcpClient.KickReason.KickStringSpecified:
                                (reason, buffer_reading) = self.__read_non_terminated_string(self.msg_buffer, buffer_reading+1)
                                print("MQTCP Server kicked us for following reason:")
                                print(reason)
                            case MqTcpClient.KickReason.KickVersionNonMatching:
                                print("MQTCP Server kicked us for non-matching protocol version:")
                                print("Client version = ")
                                print(self.__get_mqtcp_version())
                                print("Server version = ")
                                print(int(buffer[2]))
                            case MqTcpClient.KickReason.KickVersionNotReceived: #This should never happen using this library
                                print("MQTCP Server kicked us for not sending version value.")
                            case MqTcpClient.KickReason.KickLoginNotReceived: #This should never happen using this library
                                print("MQTCP Server kicked us for not sending login infos.")
                self.next_msg_length = 0
                self.msg_buffer = bytearray()

    def __read_null_terminated_string(self, buffer:bytearray, from_index:int):
        null_index = buffer.find(0, from_index) 
        if null_index == -1:
            null_index = len(buffer)
        return (buffer[from_index:null_index].decode('ascii'), null_index)
    
    def __read_non_terminated_string(self, buffer:bytearray, from_index:int):
        return (buffer[from_index:].decode('ascii'), len(buffer))
        

    def __str__(self):
        return f"MQTCP Client object with name: {self.client_name}"

    def __repr__(self):
        return f"MqTcpClient('{self.client_name}')"
    
    class SendPendingReturnedValue:
        NothingToSend = 0 #No messages were to be sent
        WaitingAcknowledgement = 1 #Can't send next message as the previous one wasn't acknowledged yet
        NotConnected = 2 #Couldn't connect to the server
        FinishedSending = 3 #Message was fully sent to the server
        PartiallySended = 4 #Message was partly sent to the server, next part will be sent upon next call of the function
    class MessageMQTCPClientType:
        ClientPublish = 0
        ClientSubscribe = 1
        ClientLogin = 2
        ClientUnsubscribe = 3
        ClientVersion = 4
    class MessageMQTCPServerType:
        ServerPublish = 0
        ServerKick = 1
    class KickReason:
        KickUnspecified = 0
        KickStringSpecified = 1
        KickVersionNonMatching = 2
        KickVersionNotReceived = 3
        KickLoginNotReceived = 4
    
    '''
    Attributes:
    client_name : str
    client_socket : socket
    ip : str
    port : int
    queued_msg : list<bytearray>
    next_offset : int
    waiting_for_acknowledgement : bool
    connected : bool
    pending_msg : None or Message Tuple
    next_msg_length : int (0 = Don't know)
    next_msg_length_parts : list<byte> (size between 0 and 4, all included)
    send_acknowledgement_soon : bool
    msg_buffer : bytearray
    '''