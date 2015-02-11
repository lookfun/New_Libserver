// LibSerSocket.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "LibraryServer.h"
#include "LibSerSocket.h"
#include "LibraryServerDlg.h"
#include "QueueList.h"

//�ⲿ����
//extern CLibraryServerDlg  *librarydlg;
extern	CQueueList	 mylist;

// LibSerSocket
LibSerSocket::LibSerSocket()
: m_nLength(0)
{
	//��Ա������ʼ��
	m_nLength=0;
	memset(m_szBuffer,0,sizeof(m_szBuffer));
}

LibSerSocket::~LibSerSocket()
{
	//�ر��׽���
	if(m_hSocket!=INVALID_SOCKET)
		Close();
}


// LibSerSocket ��Ա����

void LibSerSocket::OnAccept(int nErrorCode)
{
	// TODO: �ڴ����ר�ô����/����û���
	//�������������󣬵���Accept����
	LibSerSocket* pSocket = new LibSerSocket();
	if(Accept(*pSocket))//��������
	{
		pSocket->AsyncSelect(FD_READ);//����ͨ��Socket��Read����������
		mylist.m_connectionList.AddHead(pSocket);//��¼��ǰͨ��Socket
	}
	else
		delete pSocket;
	CAsyncSocket::OnAccept(nErrorCode);
}

void LibSerSocket::OnConnect(int nErrorCode)
{
	// TODO: �ڴ����ר�ô����/����û���
	if (nErrorCode==0)//���ӳɹ�
	{
		m_bConnected=TRUE;
	}
	CAsyncSocket::OnConnect(nErrorCode);
}

void LibSerSocket::OnReceive(int nErrorCode)
{
	// TODO: �ڴ����ר�ô����/����û���
	CString addr; 
	UINT port; 
	memset(m_szBuffer,0,sizeof(m_szBuffer));
	m_nLength=Receive(m_szBuffer,sizeof(m_szBuffer),0);
	GetPeerName(addr,port);//�õ��Է��ĵ�ַ 
	//�ѽ��յ������ݴ��͸����У�
	CString	Buf=CString(m_szBuffer);
	mylist.Insert(Buf,addr);
	CWinThread* pThread;
	pThread=AfxBeginThread(TraMsg,this);//���̴߳�����Ϣ
	CAsyncSocket::OnReceive(nErrorCode);
}

void LibSerSocket::OnSend(int nErrorCode)
{
	// TODO: �ڴ����ר�ô����/����û���
	AsyncSelect(FD_READ);	//����OnReceive����
	CAsyncSocket::OnSend(nErrorCode);
}

void LibSerSocket::OnClose(int nErrorCode)
{
	// TODO: �ڴ����ר�ô����/����û���
	CString addr; 
	UINT port; 
	GetPeerName(addr,port);//�õ��Է��ĵ�ַ 
	//ȷ��Ҫ�رյ�socket
	sockaddr_in sa;
	int len=sizeof(sa);
	LibSerSocket *m_pSocket=NULL;//Socket��ʱָ��
	m_pSocket=(LibSerSocket*)mylist.m_connectionList.GetHead();
	POSITION pos = mylist.m_connectionList.GetHeadPosition();
	while(pos!=NULL)
	{
		if(!getpeername(*m_pSocket, (struct sockaddr *)&sa, &len))
		{
			CString ipAddress=(CString)inet_ntoa(sa.sin_addr);
			if(ipAddress==addr)
			{
				delete m_pSocket;
				mylist.m_connectionList.RemoveAt(pos);
				break;
			}
		}
		m_pSocket=(LibSerSocket*)mylist.m_connectionList.GetNext(pos);
	}
	CAsyncSocket::OnClose(nErrorCode);
}
