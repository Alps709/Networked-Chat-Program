//
// (c) 2015 Media Design School
//
// File Name	: 
// Description	: 
// Author		: Your Name
// Mail			: your.name@mediadesign.school.nz
//

//Library Includes
#include <WS2tcpip.h>
#include <iostream>
#include <utility>
#include <thread>
#include <chrono>


//Local Includes
#include "utils.h"
#include "network.h"
#include "consoletools.h"
#include "socket.h"


//Local Includes
#include "server.h"

CServer::CServer()
	:m_pcPacketData(0),
	m_pServerSocket(0)
{
	ZeroMemory(&m_ClientAddress, sizeof(m_ClientAddress));
}

CServer::~CServer()
{
	delete m_timerMutex;
	m_timerMutex = 0;

	delete m_pConnectedClients;
	m_pConnectedClients = 0;

	delete m_pServerSocket;
	m_pServerSocket = 0;

	delete m_pWorkQueue;
	m_pWorkQueue = 0;
	
	delete[] m_pcPacketData;
	m_pcPacketData = 0;
}

bool CServer::Initialise()
{
	m_pcPacketData = new char[MAX_MESSAGE_LENGTH];
	
	//Create a work queue to distribute messages between the main  thread and the receive thread.
	m_pWorkQueue = new CWorkQueue<std::pair<sockaddr_in, std::string>>();

	//Create a socket object
	m_pServerSocket = new CSocket();

	//Get the port number to bind the socket to
	unsigned short _usServerPort = QueryPortNumber(DEFAULT_SERVER_PORT);

	//Initialise the socket to the local loop back address and port number
	if (!m_pServerSocket->Initialise(_usServerPort))
	{
		return false;
	}

	m_pConnectedClients = new std::map < std::string, TClientDetails >() ;

	m_serverTimer.Initialise();

	return true;
}

bool CServer::AddClient(std::string _strClientName)
{
	//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
	std::unique_lock<std::mutex> _lock(*m_timerMutex);

	for (auto it = m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
	{
		//Check to see that the client to be added does not already exist in the map, 
		if(it->first == ToString(m_ClientAddress))
		{
			return false;
		}
		//also check for the existence of the username
		if (it->second.m_strName == _strClientName)
		{
			return false;
		}
	}

	//Add the client to the map.
	TClientDetails _clientToAdd;
	_clientToAdd.m_strName = _strClientName;
	_clientToAdd.m_ClientAddress = this->m_ClientAddress;
	_clientToAdd.m_timeSinceLastMessage = 0.0;

	std::string _strAddress = ToString(m_ClientAddress);
	m_pConnectedClients->insert(std::pair < std::string, TClientDetails > (_strAddress, _clientToAdd));
	return true;
}

bool CServer::SendData(char* _pcDataToSend)
{
	int _iBytesToSend = (int)strlen(_pcDataToSend) + 1;
	
	int iNumBytes = sendto(
		m_pServerSocket->GetSocketHandle(),				// socket to send through.
		_pcDataToSend,									// data to send
		_iBytesToSend,									// number of bytes to send
		0,												// flags
		reinterpret_cast<sockaddr*>(&m_ClientAddress),	// address to be filled with packet target
		sizeof(m_ClientAddress)							// size of the above address struct.
		);
	//iNumBytes;
	if (_iBytesToSend != iNumBytes)
	{
		std::cout << "There was an error in sending data from client to server" << std::endl;
		return false;
	}
	return true;
}

bool CServer::SendDataTo(char* _pcDataToSend, sockaddr_in _clientAdrress)
{
	int _iBytesToSend = (int)strlen(_pcDataToSend) + 1;

	int iNumBytes = sendto(
		m_pServerSocket->GetSocketHandle(),				// socket to send through.
		_pcDataToSend,									// data to send
		_iBytesToSend,									// number of bytes to send
		0,												// flags
		reinterpret_cast<sockaddr*>(&_clientAdrress),	// address to be filled with packet target
		sizeof(_clientAdrress)							// size of the above address struct.
	);
	//iNumBytes;
	if (_iBytesToSend != iNumBytes)
	{
		std::cout << "There was an error in sending data from client to server" << std::endl;
		return false;
	}
	return true;
}

void CServer::ReceiveData(char* _pcBufferToReceiveData)
{
	int iSizeOfAdd = sizeof(m_ClientAddress);
	int _iNumOfBytesReceived;
	int _iPacketSize;

	//Make a thread local buffer to receive data into
	char _buffer[MAX_MESSAGE_LENGTH];

	while (true)
	{
		// pull off the packet(s) using recvfrom()
		_iNumOfBytesReceived = recvfrom // pulls a packet from a single source...
								(m_pServerSocket->GetSocketHandle(),				// client-end socket being used to read from
								_buffer,										// incoming packet to be filled
								MAX_MESSAGE_LENGTH,								// length of incoming packet to be filled
								0,												// flags
								reinterpret_cast<sockaddr*>(&m_ClientAddress),	// address to be filled with packet source
								&iSizeOfAdd);									// size of the above address struct.

		if (_iNumOfBytesReceived < 0)
		{
			int _iError = WSAGetLastError();
			ErrorRoutines::PrintWSAErrorInfo(_iError);
			//return false;
		}
		else
		{
			_iPacketSize = static_cast<int>(strlen(_buffer)) + 1;
			strcpy_s(_pcBufferToReceiveData, _iPacketSize, _buffer);
			char _IPAddress[100];
			inet_ntop(AF_INET, &m_ClientAddress.sin_addr, _IPAddress, sizeof(_IPAddress));

			std::string clientMsgName = "DEFAULT";

			//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
			std::unique_lock<std::mutex> _lock(*m_timerMutex);
			for (auto it = m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
			{
				if(ToString(it->second.m_ClientAddress) == ToString(m_ClientAddress))
				{
					clientMsgName = it->second.m_strName;
				}
			}
			_lock.unlock();

			std::cout << "Server Received \"" << _pcBufferToReceiveData << "\" from " <<
				_IPAddress << ":" << ntohs(m_ClientAddress.sin_port) << " Username: " << clientMsgName << std::endl;
			//Push this packet data into the WorkQ
			m_pWorkQueue->push(std::make_pair(m_ClientAddress,_pcBufferToReceiveData));
		}
		//std::this_thread::yield();
		
	} //End of while (true)
}

void CServer::GetRemoteIPAddress(char *_pcSendersIP)
{
	char _temp[MAX_ADDRESS_LENGTH];
	int _iAddressLength;
	inet_ntop(AF_INET, &(m_ClientAddress.sin_addr), _temp, sizeof(_temp));
	_iAddressLength = static_cast<int>(strlen(_temp)) + 1;
	strcpy_s(_pcSendersIP, _iAddressLength, _temp);
}

unsigned short CServer::GetRemotePort()
{
	return ntohs(m_ClientAddress.sin_port);
}

void CServer::GetDataAndProcess(CNetwork& _rNetwork, char* _cIPAddress)
{
	while (_rNetwork.IsOnline())
	{
		_rNetwork.GetInstance().GetNetworkEntity()->GetRemoteIPAddress(_cIPAddress);

		//Retrieve off a message from the queue and process it
		std::pair<sockaddr_in, std::string> dataItem;
		GetWorkQueue()->pop(dataItem);
		ProcessData(dataItem);
	}
}

//Update the time since the last message, every second
void CServer::ProcessClientLastMessageTimer()
{
	while(ClientTimer)
	{
		{
			//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
			std::lock_guard<std::mutex> _lock(*m_timerMutex);

			m_serverTimer.Process();
			if (!m_pConnectedClients->empty())
			{
				for (auto it = m_pConnectedClients->begin(); it != m_pConnectedClients->end();)
				{
					//Check if a message has not been sent within the last 10 seconds
					if (it->second.m_timeSinceLastMessage > 10000)
					{
						//Send the client a message saying it is being removed from the server 
						TPacket _packetToSend;
						std::string tempMessage = "You failed to send a keep alive message for over 10 seconds, you will now be disconnected from the server.";

						_packetToSend.Serialize(DATA, const_cast<char*>(tempMessage.c_str()));
						SendDataTo(_packetToSend.PacketData, it->second.m_ClientAddress);

						//Remove the client from the server
						m_pConnectedClients->erase(it);
					}
					else
					{
						//Increase the timer since last message
						it->second.m_timeSinceLastMessage += m_serverTimer.GetDeltaTick();

						//Only increment the iterator when erase hasn't been called
						++it;
					}
				}
			}
		}

		//Sleep for a second so it doesn't needlessly process it every  
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void CServer::ProcessData(std::pair<sockaddr_in, std::string> dataItem)
{
	TPacket _packetRecvd, _packetToSend;
	_packetRecvd = _packetRecvd.Deserialize(const_cast<char*>(dataItem.second.c_str()));

	switch (_packetRecvd.MessageType)
	{
		case HANDSHAKE:
		{
			std::cout << "Server received a handshake message " << std::endl;

			if (AddClient(_packetRecvd.MessageContent))
			{	//Handshake successful

				//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
				std::unique_lock<std::mutex> _lock(*m_timerMutex);

				//Get this client's name
				std::string clientMsgName = m_pConnectedClients->find(ToString(dataItem.first))->second.m_strName;

				std::string message = "Users in chatroom : ";

				//Add the names of the other connected users to the message
				//Append the first connected client
				message.append(ToString((m_pConnectedClients->begin())->second.m_strName));

				//Append the rest of the client names, but with a comma beforehand
				if (m_pConnectedClients->size() > 1)
				{
					for (auto& it = ++m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
					{
						message.append(", " + ToString(it->second.m_strName));
					}
				}

				//Append a fullstop at the end of the list of names
				message.append(".");

				_packetToSend.Serialize(HANDSHAKE_SUCCESS, const_cast<char*>(message.c_str()));
				SendDataTo(_packetToSend.PacketData, dataItem.first);

				//Tell all the other clients that this client has joined the chat
				for (auto& it = m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
				{
					if (ToString(it->second.m_strName) != clientMsgName)
					{
						std::string tempMessage = clientMsgName + " has joined the chat!";
						_packetToSend.Serialize(DATA, const_cast<char*>(tempMessage.c_str()));
						SendDataTo(_packetToSend.PacketData, it->second.m_ClientAddress);
					}
				}
				_lock.unlock();
			}
			else
			{	//Handshake failed
				std::string message = "Handshake with server failed!";
				_packetToSend.Serialize(HANDSHAKE_FAILURE, const_cast<char*>(message.c_str()));
				SendDataTo(_packetToSend.PacketData, dataItem.first);
			}
			break;
		}
		case DATA:
		{
			_packetToSend.Serialize(DATA, _packetRecvd.MessageContent);
			std::string temp = _packetRecvd.MessageContent;
			if (temp == " ")
			{
				break;
			}

			std::string clientMsgName;

			//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
			std::unique_lock<std::mutex> _lock(*m_timerMutex);

			if (m_pConnectedClients->find(ToString(dataItem.first)) != m_pConnectedClients->end())
			{
				clientMsgName = m_pConnectedClients->find(ToString(dataItem.first))->second.m_strName;
			}

			for (auto& it = m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
			{
				if (ToString(it->second.m_ClientAddress) != ToString(dataItem.first))
				{ 
					std::string tempMessage = clientMsgName + ": " + _packetToSend.MessageContent;
					_packetToSend.Serialize(DATA, const_cast<char*>(tempMessage.c_str()));
					SendDataTo(_packetToSend.PacketData, it->second.m_ClientAddress);
				}
			}
			_lock.unlock();
			break;
		}
		case KEEPALIVE:
		{
			//Reset the keep alive timer for the client, as the client has sent a keep alive message

			//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
			std::unique_lock<std::mutex> _lock(*m_timerMutex);

			std::cout << "Received a keepalive message from Username: " << m_pConnectedClients->find(ToString(dataItem.first))->second.m_strName << "\n";
			m_pConnectedClients->find(ToString(dataItem.first))->second.m_timeSinceLastMessage = 0.0;
			_lock.unlock();
			break;
		}
		case BROADCAST:
		{
			std::cout << "Received a broadcast packet" << std::endl;
			//Just send out a packet to the back to the client again which will have the server's IP and port in it's sender fields
			_packetToSend.Serialize(BROADCAST, "I'm here!");
			SendData(_packetToSend.PacketData);
			break;
		}
		case COMMAND:
		{
			std::string messageString = "No command found!";
			std::string commandString = _packetRecvd.MessageContent;

			/*if(commandString[0] == '!' && commandString[1] == '!')
			{
				commandString.erase(commandString.begin());
			}*/

			if (commandString[1] == '!')
			{
				std::string tempString;
				for (unsigned int i = 2; i < commandString.length(); ++i)
				{
					tempString += commandString[i];

					if(tempString.find('?') != std::string::npos)
					{
						messageString = "The possible commands are: ?, q";
						break;
					}
					else if(tempString.find('q') != std::string::npos)
					{
						messageString = "Are you sure you want to disconnect?";
						_packetToSend.Serialize(DISCONNECT, const_cast<char*>(messageString.c_str()));
						SendData(_packetToSend.PacketData);
						return;
					}
				}
			}

			_packetToSend.Serialize(DATA, const_cast<char*>(messageString.c_str()));
			SendData(_packetToSend.PacketData);
			break;
		}
		case DISCONNECT:
		{

			//The client has declared that it wants to be disconnected form the server (the client is shutting itself down)
			//Find the client and remove it from the client map

			//Lock the thread so a client can't be disconnected (in another process) while this thread is iterating over the map
			std::unique_lock<std::mutex> _lock(*m_timerMutex);
			for (auto& it = m_pConnectedClients->begin(); it != m_pConnectedClients->end(); ++it)
			{
				if (ToString(it->second.m_ClientAddress) != ToString(dataItem.first))
				{
					std::cout << "Disconnecting user: " << it->second.m_strName;
					m_pConnectedClients->erase(it);
					break;
				}
			}
			_lock.unlock();
			break;
		}
		default:
			break;
	}
}

CWorkQueue<std::pair<sockaddr_in, std::string>>* CServer::GetWorkQueue()
{
	return m_pWorkQueue;
}
