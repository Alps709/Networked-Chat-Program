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

#ifndef __NETWORK_H__
#define __NETWORK_H__

//Types
enum EEntityType
{
	CLIENT = 1,
	SERVER
};

//constants
namespace
{
	unsigned const DEFAULT_SERVER_PORT = 80012;
	unsigned const DEFAULT_CLIENT_PORT = 60013;
	unsigned const MAX_MESSAGE_LENGTH = 256;
	unsigned const MAX_ADDRESS_LENGTH = 32;
}

namespace ErrorRoutines
{
	void PrintWSAErrorInfo(int iError);
}

//Forward Decalarations
class INetworkEntity;

class CNetwork
{
public:
	~CNetwork();
	
	bool Initialise(EEntityType _eType);
	void StartUp(); //A network has an ability to start up
	void ShutDown(); //& an ability to be shut down
	bool IsOnline();
	//Accessor methods
	INetworkEntity* GetNetworkEntity();

	// Singleton Methods
	static CNetwork& GetInstance();
	static void DestroyInstance();

	bool m_bOnline;

private:
	//Make the network class a singleton. There is only one instance of the network running
	CNetwork();
	CNetwork(const CNetwork& _kr);
	CNetwork& operator= (const CNetwork& _kr);

protected:
	//A network has a network entity
	INetworkEntity* m_pNetworkEntity;

	// Singleton Instance
	static CNetwork* s_pNetwork;
};
#endif