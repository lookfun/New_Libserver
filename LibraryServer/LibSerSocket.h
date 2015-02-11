#pragma once

// LibSerSocket 命令目标

class LibSerSocket : public CAsyncSocket
{
public:
	LibSerSocket();
	virtual ~LibSerSocket();
	virtual void OnAccept(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
public:
	//变量
	unsigned int m_nLength;
	char	m_szBuffer[4096];
	BOOL m_bConnected;			//是否连接
	//LibSerSocket *m_connectionList;//客户端套接字指针数组[128]
	virtual void OnClose(int nErrorCode);
};


