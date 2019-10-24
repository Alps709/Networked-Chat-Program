//
// Bachelor of Software Engineering
// Media Design School
// Auckland
// New Zealand
//
// (c) 2015 Media Design School
//
// File Name	: 
// Description	: 
// Author		: Your Name
// Mail			: your.name@mediadesign.school.nz
//

//Library Includes
#include <Windows.h>
#include <cassert>
#include <vld.h>
#include <thread>

//Local Includes
#include "consoletools.h"
#include "network.h"
#include "client.h"
#include "server.h"
#include "InputLineBuffer.h"
#include <functional>

// make sure the winsock lib is included...
#pragma comment(lib,"ws2_32.lib")

int main()
{
	char* _pcPacketData = 0; //A local buffer to receive packet data info
	_pcPacketData = new char[MAX_MESSAGE_LENGTH];
	strcpy_s(_pcPacketData, strlen("") + 1, "");

	char _cIPAddress[MAX_ADDRESS_LENGTH]; // An array to hold the IP Address as a string
										  //ZeroMemory(&_cIPAddress, strlen(_cIPAddress));

	unsigned char _ucChoice;
	EEntityType _eNetworkEntityType;
	CInputLineBuffer _InputBuffer(MAX_MESSAGE_LENGTH);
	std::thread _ClientReceiveThread, _ServerReceiveThread, _ClientSendKeepAliveThread, _ServerProcessThread, _ServerClientTimerThread;

	//Get the instance of the network
	CNetwork& _rNetwork = CNetwork::GetInstance();
	_rNetwork.StartUp();

	//A pointer to hold a client instance
	CClient* _pClient = nullptr;
	//A pointer to hold a server instance
	CServer* _pServer = nullptr;

	// query, is this to be a client or a server?
	_ucChoice = QueryOption("Do you want to run a client or server (C/S)?", "CS");
	switch (_ucChoice)
	{
		case 'C':
		{
			_eNetworkEntityType = CLIENT;
			break;
		}
		case 'S':
		{
			_eNetworkEntityType = SERVER;
			break;
		}
		default:
		{
			std::cout << "This is not a valid option" << std::endl;
			return 0;
			break;
		}
	}

	if (!_rNetwork.GetInstance().Initialise(_eNetworkEntityType))
	{
		std::cout << "Unable to initialise the Network........Press any key to continue......";
		_getch();
		return 0;
	}

	//Run receive on a separate thread so that it does not block the main client thread.
	if (_eNetworkEntityType == CLIENT) //if network entity is a client
	{
		_pClient = static_cast<CClient*>(_rNetwork.GetInstance().GetNetworkEntity());
		_ClientReceiveThread = std::thread(&CClient::ReceiveData, _pClient, std::ref(_pcPacketData));
	}
	//Run receive of server also on a separate thread 
	else if (_eNetworkEntityType == SERVER) //if network entity is a server
	{
		_pServer = dynamic_cast<CServer*>(_rNetwork.GetInstance().GetNetworkEntity());
		_ServerReceiveThread = std::thread(&CServer::ReceiveData, _pServer, std::ref(_pcPacketData));
		_ServerProcessThread = std::thread(&CServer::GetDataAndProcess, _pServer, std::ref(_rNetwork), std::ref(_cIPAddress));
		_ServerClientTimerThread = std::thread(&CServer::ProcessClientLastMessageTimer, _pServer);
	}

	int counter = 0;
	while (_rNetwork.IsOnline())
	{
		if (_eNetworkEntityType == CLIENT) //if network entity is a client
		{
			//Only start the thread on the second loop
			if (counter == 1)
			{
				_ClientSendKeepAliveThread = std::thread(&CClient::SendKeepAlive, _pClient);
			}

			//Increment counter
			if (counter <= 1) ++counter;

			//Prepare for reading input from the user
			//_InputBuffer.PrintToScreenTop();

			//Get input from the user
			if (_InputBuffer.Update())
			{
				// we completed a message, lets send it:
				int _iMessageSize = static_cast<int>(strlen(_InputBuffer.GetString()));

				//Put the message into a packet structure
				TPacket _packet;

				if (_InputBuffer.GetString()[0] == '!' || (_InputBuffer.GetString()[0] == '!' && _InputBuffer.GetString()[1] == '!'))
				{
					_packet.Serialize(COMMAND, const_cast<char*>(_InputBuffer.GetString()));
				}
				else
				{
					_packet.Serialize(DATA, const_cast<char*>(_InputBuffer.GetString()));
				}

				_rNetwork.GetInstance().GetNetworkEntity()->SendData(_packet.PacketData);
				//Clear the Input Buffer
				_InputBuffer.ClearString();
				//Print To Screen Top
				//_InputBuffer.PrintToScreenTop();
			}
			if (_pClient != nullptr)
			{
				//If the message queue is not empty, process the messages
				if (!(_pClient->GetWorkQueue()->empty()))
				{
					//Retrieve off a message from the queue and process it
					std::string temp;
					_pClient->GetWorkQueue()->pop(temp);
					_pClient->ProcessData(const_cast<char*>(temp.c_str()));
				}
			}
		}
	}

	_rNetwork.m_bOnline = false;
	_ClientReceiveThread.join();
	_ClientSendKeepAliveThread.join();
	_ServerReceiveThread.join();
	_ServerProcessThread.join();
	_ServerClientTimerThread.join();

	//Shut Down the Network
	_rNetwork.ShutDown();
	_rNetwork.DestroyInstance();

	delete[] _pcPacketData;
	return 0;
}