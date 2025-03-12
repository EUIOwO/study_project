#include "pch.h"
#include "ServerSocket.h"

//CServerSocket server;
CServerSocket* CServerSocket::m_instance = NULL;
CServerSocket::CHelper CServerSocket::* m_instance;

CServerSocket* pserver = CServerSocket::getInstance();