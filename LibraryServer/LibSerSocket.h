#pragma once

// LibSerSocket ����Ŀ��

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
	//����
	unsigned int m_nLength;
	char	m_szBuffer[4096];
	BOOL m_bConnected;			//�Ƿ�����
	//LibSerSocket *m_connectionList;//�ͻ����׽���ָ������[128]
	virtual void OnClose(int nErrorCode);
};


