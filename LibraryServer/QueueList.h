#pragma once

struct Node
{
	struct Node *m_next;
	CString m_item;
	CString m_addr;
};

class CQueueList
{
public:
	CQueueList(void);
	~CQueueList(void);

	void Clean(void);
	CString GetDel(void);//��ȡ��Ϣ����
	void Insert(CString data_intsert,CString data_addr);// ������в����ַ���
	bool IsEmpty(void);// �ж϶����Ƿ�Ϊ��
	CPtrList m_connectionList;////�ͻ����׽���ָ���б�
	int m_node_count;
	Node *m_tail;
	Node *m_head;
};

//struct staInfo
//{
//
//};

struct threadData
{
	CString recvTemp;//�ͻ�����Ϣ
	CString strSQL;//Ҫִ�е�SQL���
	CString sendTemp;//���ڷ�����Ϣ
	char sendBuffer[2048];
	char recBuffer[2048];//���ڽ�����Ϣ
	char temp[256];//ת��ʱ�õ���ʱ�ռ�
	char temp2[256];//
	CString temp3;//��ʱ����
	BSTR bSql;
	int recordCount;//��¼������
	_variant_t vFieldValue;//���ݿ��������ʱ����
	char passWord[32];//������
	CString patronName;//��������
	CString reType;//��������
	char readerCardNum[32];//����
	CString caState;//��������
	char bookID[32];//ͼ��ID
	bool boidCorrect;//ͼ��ID�Ƿ���ȷ
	CString boName;//ͼ������
	CString boState;//ͼ��״̬
	CString boWriter;//ͼ������
	CString bookEPC;//ͼ��EPC
	CString feeAmount;//���߷����ۼ�
	int borrowNumber;//���߽����ۼ�
	CString strNow;//��������
	CString strNew;//Ӧ������
	CString transactiondate;//����ʱ��
	int caNumber;//���߿ɽ�����Ŀ
	int caTimes;//���߿��������
	int reBonumber;//�������������
	int caDays;//���߿ɽ�������
	COleDateTime baDate;//Ӧ������
	int holdNumber;//ԤԼͼ������
	bool pidCorrect;//����ID�Ƿ���ȷ
	bool pwCorrect;//�����Ƿ���ȷ
	CString passWd;//����
	int sendLen;//���ͳ���
	int length;//ת��ʱ�ĳ���
	CString boID;	
	CString readerCN;
	//int feeAmount;//����Ƿ��
	CString chargedItems;//���߽���ͳ��
};
