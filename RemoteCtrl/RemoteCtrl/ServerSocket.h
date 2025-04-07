#pragma onc
#include <list>
#include "Packet.h"
#pragma comment(lib, "ws2_32.lib")

typedef void(*SOCKET_CALLBACK)(void*, int, std::list<CPacket>& lstPacket, CPacket& inPacket);

class CServerSocket
{
public:
	static CServerSocket* getInstance() {
		if (m_instance == NULL) {//静态函数没有 this 指针，所以无法直接访问成员变量
			m_instance = new CServerSocket();
		}
		return m_instance;
	}

	bool InitSocket(short port = 9527) {

		if (m_socket == INVALID_SOCKET) {
			return false;
		}
		//TODO：校验
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;//地址族
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(port);

		//绑定
		if (bind(m_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
			return false;
		}
		if (listen(m_socket, 1) == -1) {

			return false;
		}

		return true;
	}

	int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527) {
		//1.进度的可控性 2.对接的方便性 3.可行性评估，提早暴露风险
		//TODO:socket、bind、listen、accept、read、write、close
		//套接字初始化
		bool ret = InitSocket(port);
		if (ret == false) return -1;
		std::list<CPacket> lstPackets;
		m_callback = callback;
		m_arg = arg;
		int count = 0;
		while (true) {
			if (AcceptClient() == false) {
				if (count >= 3) {
					return -2;
				}
				count++;
			}
			int ret = DealCommand();
			if (ret > 0) {
				m_callback(m_arg, ret, lstPackets, m_packet);
				if (lstPackets.size() > 0) {
					Send(lstPackets.front());
					lstPackets.pop_front();
				}
			}
			CloseClient();
		}
		return 0;
	}

	bool AcceptClient() {
		TRACE("enter AcceptClient\r\n");
		sockaddr_in client_addr;
		char buffer[1024]{};
		int cli_sz = sizeof(client_addr);
		m_client = accept(m_socket, (sockaddr*)&client_addr, &cli_sz);
		TRACE("m_client = %d\r\n", m_client);
		if (m_client == -1) return false;
		return true;

		/*recv(client, buffer, sizeof(buffer), 0);
		send(client, buffer, sizeof(buffer), 0);*/
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_client == -1) return -1;
		char* buffer = new char[BUFFER_SIZE];
		if (buffer == NULL) {
			TRACE("内存不足！\r\n");
			return -2;
		}
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;

		while (true) {
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE - static_cast<size_t>(index), 0);//实际接收到的长度
			if (len <= 0) {
				delete[]buffer;
				return -1;
			}
			TRACE("server recv %d\r\n", len);
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);//解包
			if (index > 0) {
				memmove(buffer, buffer + len, 4096 - len);
				index -= len;
				delete[]buffer;
				return m_packet.sCmd;
			}
		}
		delete[]buffer;
		return -1;
	}

	bool Send(const char* pData, size_t nSize) {
		if (m_client == -1) return false;
		return send(m_client, pData, (int)nSize, 0) > 0;
	}

	bool Send(CPacket& pack) {
		if (m_client == -1) return false;
		//Dump((BYTE*)pack.Data(), pack.Size());
		size_t len = send(m_client, pack.Data(), pack.Size(), 0);
		return len > 0;
	}

	void CloseClient() {
		if (m_client != INVALID_SOCKET) {
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
	}
private:
	SOCKET_CALLBACK m_callback;
	void* m_arg;
	SOCKET m_client;
	SOCKET m_socket;
	CPacket m_packet;
	CServerSocket& operator = (const CServerSocket& ss) {}
	CServerSocket(const CServerSocket& ss) {
		m_socket = ss.m_socket;
		m_client = ss.m_client;
	}
	CServerSocket() {

		m_client = INVALID_SOCKET;
		if (InitSocEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境, 请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_socket = socket(PF_INET,  SOCK_STREAM,  0);

	}
	~CServerSocket() {
		closesocket(m_socket);
		WSACleanup();
	}

	BOOL InitSocEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			return FALSE;

		}
		return TRUE;
	}

	void releaseInstance() {
		if (m_instance != NULL) {
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}

	static CServerSocket* m_instance;
public:
	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {

		}
	};
	
};

//声明外部变量
extern CServerSocket server;
