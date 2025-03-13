#pragma once
#include "pch.h"
#include "framework.h"

class CPacket {
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}

		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	CPacket(const CPacket& pack) { //复制构造函数
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}

	CPacket(const BYTE* pData, size_t& nSize) {
		size_t i = 0;
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		if (i + 8 > nSize) {//包数据可能不全，或者包头未能全部接收到
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;

		if (nLength + i > nSize) {//包没有完全接收到，就返回，解析失败
			nSize = 0;
			return;
		}

		sCmd = *(WORD*)(pData + i); i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 2 - 2);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
		}
		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum) {
			nSize = i;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator = (const CPacket& pack) {//等于号的运算符重载
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}
	int Size() {//获得包数据的大小
		return nLength + 6;
	}
	const char* Data() {
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)(pData) = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
		return strOut.c_str();
	}

public:
	WORD sHead;//固定位FE FF
	DWORD nLength;//包长度（从控制命令开始，到和校验结束）
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	std::string strOut;//整个包的数据
};

typedef struct MouseEvent{
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;//点击、移动、双击
	WORD nButton;//左键、右键、中键
	POINT ptXY;//坐标
}MOUSEEV, *PMOUSEEV;

class CServerSocket
{
public:
	static CServerSocket* getInstance() {
		if (m_instance == NULL) {//静态函数没有 this 指针，所以无法直接访问成员变量
			m_instance = new CServerSocket();
		}
		return m_instance;
	}
	bool initSocket() {

		if (m_socket == -1) return false;
		//TODO：校验
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;//地址族
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(9527);

		//绑定
		if (bind(m_socket, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
			return false;
		}
		if (listen(m_socket, 1) == -1) {
			return false;
		}
		return true;


	}
	bool AcceptClient() {
		sockaddr_in client_addr;
		char buffer[1024]{};
		int cli_sz = sizeof(client_addr);
		m_client = accept(m_socket, (sockaddr*)&client_addr, &cli_sz);
		if (m_client == -1) return false;
		return true;

		/*recv(client, buffer, sizeof(buffer), 0);
		send(client, buffer, sizeof(buffer), 0);*/
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_client == -1) return -1;
		//char buffer[1024] = "";
		char* buffer = new char[4096];
		memset(buffer, 0, 4096);
		size_t index = 0;
		while (1) {
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE - static_cast<size_t>(index), 0);
			if (len <= 0) {
				return -1;
			}
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, 4096 - len);
				index -= len;
				return m_packet.sCmd;
			}
		}
		return -1;
	}

	bool Send(const char* pData, size_t nSize) {
		if (m_client == -1) return false;
		return send(m_client, pData, (int)nSize, 0) > 0;
	}

	bool Send(CPacket& pack) {
		if (m_client == -1) return false;
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}

	bool GetFilePath(std::string& strPath) {
		if ((m_packet.sCmd == 2) || (m_packet.sCmd == 3) || (m_packet.sCmd == 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}

	bool GetMouseEvent(MOUSEEV& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}

private:
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
		SOCKET m_socket = socket(PF_INET, SOCK_STREAM, 0);
	}
	~CServerSocket() {
		closesocket(m_socket);
		WSACleanup();
	}

	BOOL InitSocEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
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
