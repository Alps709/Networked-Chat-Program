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

#ifndef __SERVER_H__
#define __SERVER_H__

// Library Includes
#include <Windows.h>
#include <map>
#include <time.h>

// Local Includes
#include "networkentity.h"
#include "WorkQueue.h"
#include "clock.h"

// Types

// Constants

//Forward Declaration
class CSocket;

//Structure to hold the details of all connected clients
struct TClientDetails
{
	sockaddr_in m_ClientAddress;
	bool m_bIsAlive;
	std::string m_strName;
	double m_timeSinceLastMessage; //in milliseconds
};

class CServer : public INetworkEntity
{
public:
	// Default Constructors/Destructors
	CServer();
	~CServer();

	// Virtual Methods from the Network Entity Interface.
	virtual bool Initialise(); //Implicit in the intialization is the creation and binding of the socket
	virtual bool SendData(char* _pcDataToSend);
	bool SendDataTo(char* _pcDataToSend, sockaddr_in _clientAdrress);
	virtual void ReceiveData(char* _pcBufferToReceiveData);
	virtual void ProcessData(std::pair<sockaddr_in, std::string> dataItem);
	virtual void GetRemoteIPAddress(char* _pcSendersIP);
	virtual unsigned short GetRemotePort();

	void GetDataAndProcess(CNetwork& _rNetwork, char* _cIPAddress);

	bool m_clientTimer = true;
	void ProcessClientLastMessageTimer();

	CWorkQueue<std::pair<sockaddr_in, std::string>>* GetWorkQueue();
	std::mutex* m_timerMutex = new std::mutex();

	bool AddClient(std::string _strClientName);

	//A Buffer to contain all packet data for the server
	char* m_pcPacketData;
	//A server has a socket object to create the UDP socket at its end.
	CSocket* m_pServerSocket;
	// Make a member variable to extract the IP and port number of the sender from whom we are receiving
	//Since it is a UDP socket capable of receiving from multiple clients; these details will change depending on who has sent the packet we are currently processing.
	sockaddr_in m_ClientAddress; 

	//Used to keep time for client keep alive messages
	CClock m_serverTimer;

	//The structure maps client addresses to client details
	std::map<std::string, TClientDetails>* m_pConnectedClients;

	//A workQueue to distribute messages between the main thread and Receive thread.
	CWorkQueue<std::pair<sockaddr_in, std::string>>* m_pWorkQueue;
};

#endif