#include "StdAfx.h"
#include "QueueList.h"


CQueueList::CQueueList(void)
: m_node_count(0)
, m_tail(NULL)
, m_head(NULL)
{
}


CQueueList::~CQueueList(void)
{
	Clean();
}

void CQueueList::Clean(void)
{
	while(m_head)
	{
		GetDel();
	}
	m_node_count = 0;
}

CString CQueueList::GetDel(void)
{
	CString data_del("MS");
	Node *pNode_del = m_head;

	//队列为空
	if(NULL == m_head)
	{
		//		AfxMessageBox(_T("EMPTY QUEUE!"));
		//		return -1;
	}
	else if(m_head == m_tail)//队列最后一个结点
	{
		m_head = 0;
		m_tail = 0;
	}
	else//队列中间
	{
		m_head = pNode_del->m_next;
	}

	data_del=data_del+pNode_del->m_item+_T("/DS")+pNode_del->m_addr+_T("/");
	pNode_del->m_item.Empty();//= "0";
	pNode_del->m_addr.Empty();//= "0";
	pNode_del->m_next = 0;

	delete pNode_del;

	m_node_count--;	

	return data_del;
}

// 向队列中插入字符串
void CQueueList::Insert(CString data_intsert,CString data_addr)
{
	Node *tmpNode = new Node;
	if(0 == tmpNode)
	{
		AfxMessageBox(_T("MEMORY_BUSY!"));
		return;
	}

	//如果队列为空
	if(0 == m_head)
	{
		m_head = tmpNode;
		m_tail = tmpNode;
	}
	else//如果队列不为空
	{
		m_tail->m_next = tmpNode;
		m_tail = tmpNode;
	}

	tmpNode->m_next = 0;
	tmpNode->m_item = data_intsert;
	tmpNode->m_addr = data_addr;

	m_node_count++;
}

// 判断队列是否为空
bool CQueueList::IsEmpty(void)
{
	return  m_tail==NULL&& m_head==NULL;
}