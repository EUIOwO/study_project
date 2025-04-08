#pragma once
#pragma pack(push)
#pragma pack(1)
#include "pch.h"
#include "framework.h"
#include<string>
#include <iostream>
#include<vector>
#include<list>
#include<map>
class CPacket {
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize, HANDLE hEvent) {
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
		this->hEvent = hEvent;
	}
	CPacket(const CPacket& pack) { //复制构造函数
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
		hEvent = pack.hEvent;
	}

	CPacket(const BYTE* pData, size_t& nSize):hEvent(INVALID_HANDLE_VALUE) {
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
			hEvent = pack.hEvent;
		}
		return *this;
	}
	int Size() {//获得包数据的大小
		return nLength + 6;
	}
	const char* Data(std::string& strOut) const{
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)(pData) = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
		return strOut.c_str();
	}
	WORD getCmd() {
		return sCmd;
	}
	WORD getHead() {
		return sHead;
	}

public:
	WORD sHead;//固定位FE FF
	DWORD nLength;//包长度（从控制命令开始，到和校验结束）
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	HANDLE hEvent;
};

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;//点击、移动、双击
	WORD nButton;//左键、右键、中键
	POINT ptXY;//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;//无效
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;//是否有效
	BOOL IsDirectory;//是否为目录 0 否 1 是
	BOOL HasNext;//是否还有后续 0 没有 1 有
	char szFileName[256];//文件名
}FILEINFO, * PFILEINFO;

std::string GetErrorInfo(int wsaErrCode);

class CClientSocket
{
public:
	static CClientSocket* getInstance() {
		if (m_instance == NULL) {//静态函数没有 this 指针，所以无法直接访问成员变量
			m_instance = new CClientSocket();
		}
		return m_instance;
	}
	bool InitSocket() {
		if (m_sock != INVALID_SOCKET) CloseSocket();
		m_sock = socket(PF_INET, SOCK_STREAM, 0);
		if (m_sock == -1) return false;
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;//地址族
		TRACE("addr %08x nIP %08x\r\n",inet_addr("127.0.0.1"), m_nIP);
		serv_addr.sin_addr.s_addr = htonl(m_nIP);
		serv_addr.sin_port = htons(m_nPort);
		if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
			AfxMessageBox("指定的IP地址，不存在！");
			return false;
		}
		int ret = connect(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
		if (ret == -1) {
			AfxMessageBox("连接失败！！");
			TRACE("连接失败：%d %s\r\n", WSAGetLastError(), GetErrorInfo(WSAGetLastError()).c_str());
			return false;
		}
		return true;
	}
	
#define BUFFER_SIZE 819200000
	int DealCommand() {
		if (m_sock == -1) return -1;
		char* buffer = m_buffer.data();//TODO:多线程发送命令时可能会出现冲突
		static int index = 0;
		while (true) {
			size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
			
			if ((int)len <= 0) {
				return -1;
			}
			TRACE("recv len = %d(0x%08X)index = %d(0x%08X)\r\n", 
				len, len, index, index);
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			if (index > 0) {
				_beginthread(&CClientSocket::threadEntry, 0, this);
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				if (m_packet.sCmd == 6) {
				}
				return m_packet.sCmd;
			}
		}

		return -1;
	}

	bool SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks,
		bool isAutoClosed = true);

	bool GetFilePath(std::string& strPath) {
		if ((m_packet.sCmd >= 2)&&(m_packet.sCmd <= 4)) {
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

	CPacket& GetPacket()
	{
		return m_packet;
	}
	void CloseSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;//置成无效的套接字
	}

	void UpdateAddress(int nIP, int nPort) {
		if ((m_nIP != nIP) || (m_nPort != nPort)) {
			m_nIP = nIP;
			m_nPort = nPort;
		}
 	}
private:
	bool m_bAutoClose;
	std::list<CPacket> m_lstSend;
	std::map<HANDLE, std::list<CPacket>&> m_mapAck;
	std::map<HANDLE, bool>m_mapAutoClosed;
	int m_nIP;//地址
	int m_nPort;//端口
	std::vector<char> m_buffer;
	SOCKET m_sock;
	CPacket m_packet;
	CClientSocket& operator = (const CClientSocket& ss) {}
	//复制构造函数
	CClientSocket(const CClientSocket& ss)
	{
		m_bAutoClose = ss.m_bAutoClose;
		m_sock = ss.m_sock;
		m_nIP = ss.m_nIP;
		m_nPort = ss.m_nPort;

	}
	//无参的默认构造函数
	CClientSocket() :
		m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true)
	{

		if (InitSocEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境, 请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_buffer.resize(BUFFER_SIZE);
		memset(m_buffer.data(), 0, BUFFER_SIZE);
		
	}
	~CClientSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup();
	}

	static void threadEntry(void* arg);
	void threadFunc();

	BOOL InitSocEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
			return FALSE;

		}
		return TRUE;
	}

	static void releaseInstance() {
		if (m_instance != NULL) {
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	bool Send(const char* pData, int nSize) {
		if (m_sock == -1) return false;
		return send(m_sock, pData, nSize, 0) > 0;
	}
	bool Send(const CPacket& pack);
	static CClientSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};


//声明外部变量
extern CClientSocket server;


