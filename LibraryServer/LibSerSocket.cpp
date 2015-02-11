// LibSerSocket.cpp : 实现文件
//

#include "stdafx.h"
#include "LibraryServer.h"
#include "LibSerSocket.h"
#include "LibraryServerDlg.h"
#include "QueueList.h"

//外部变量
//extern CLibraryServerDlg  *librarydlg;
extern	CQueueList	 mylist;

// LibSerSocket
LibSerSocket::LibSerSocket()
: m_nLength(0)
{
	//成员变量初始化
	m_nLength=0;
	memset(m_szBuffer,0,sizeof(m_szBuffer));
}

LibSerSocket::~LibSerSocket()
{
	//关闭套接字
	if(m_hSocket!=INVALID_SOCKET)
		Close();
}


// LibSerSocket 成员函数

void LibSerSocket::OnAccept(int nErrorCode)
{
	// TODO: 在此添加专用代码和/或调用基类
	//侦听到连接请求，调用Accept函数
	LibSerSocket* pSocket = new LibSerSocket();
	if(Accept(*pSocket))//接受连接
	{
		pSocket->AsyncSelect(FD_READ);//触发通信Socket的Read函数读数据
		mylist.m_connectionList.AddHead(pSocket);//记录当前通信Socket
	}
	else
		delete pSocket;
	CAsyncSocket::OnAccept(nErrorCode);
}

void LibSerSocket::OnConnect(int nErrorCode)
{
	// TODO: 在此添加专用代码和/或调用基类
	if (nErrorCode==0)//连接成功
	{
		m_bConnected=TRUE;
	}
	CAsyncSocket::OnConnect(nErrorCode);
}

void LibSerSocket::OnReceive(int nErrorCode)
{
	// TODO: 在此添加专用代码和/或调用基类
	CString addr; 
	UINT port; 
	memset(m_szBuffer,0,sizeof(m_szBuffer));
	m_nLength=Receive(m_szBuffer,sizeof(m_szBuffer),0);
	GetPeerName(addr,port);//得到对方的地址 
	//把接收到的数据传送给队列：
	CString	Buf=CString(m_szBuffer);
	mylist.Insert(Buf,addr);
	CWinThread* pThread;
	pThread=AfxBeginThread(TraMsg,this);//开线程处理消息
	CAsyncSocket::OnReceive(nErrorCode);
}

void LibSerSocket::OnSend(int nErrorCode)
{
	// TODO: 在此添加专用代码和/或调用基类
	AsyncSelect(FD_READ);	//触发OnReceive函数
	CAsyncSocket::OnSend(nErrorCode);
}

void LibSerSocket::OnClose(int nErrorCode)
{
	// TODO: 在此添加专用代码和/或调用基类
	CString addr; 
	UINT port; 
	GetPeerName(addr,port);//得到对方的地址 
	//确定要关闭的socket
	sockaddr_in sa;
	int len=sizeof(sa);
	LibSerSocket *m_pSocket=NULL;//Socket临时指针
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
