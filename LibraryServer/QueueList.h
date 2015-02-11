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
	CString GetDel(void);//获取消息内容
	void Insert(CString data_intsert,CString data_addr);// 向队列中插入字符串
	bool IsEmpty(void);// 判断队列是否为空
	CPtrList m_connectionList;////客户端套接字指针列表
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
	CString recvTemp;//客户端消息
	CString strSQL;//要执行的SQL语句
	CString sendTemp;//用于发送信息
	char sendBuffer[2048];
	char recBuffer[2048];//用于接收信息
	char temp[256];//转换时用的临时空间
	char temp2[256];//
	CString temp3;//临时变量
	BSTR bSql;
	int recordCount;//记录集变量
	_variant_t vFieldValue;//数据库操作的临时变量
	char passWord[32];//卡密码
	CString patronName;//读者姓名
	CString reType;//读者类型
	char readerCardNum[32];//卡号
	CString caState;//读者类型
	char bookID[32];//图书ID
	bool boidCorrect;//图书ID是否正确
	CString boName;//图书名称
	CString boState;//图书状态
	CString boWriter;//图书作者
	CString bookEPC;//图书EPC
	CString feeAmount;//读者罚款累计
	int borrowNumber;//读者借书累计
	CString strNow;//当天日期
	CString strNew;//应还日期
	CString transactiondate;//交易时间
	int caNumber;//读者可借书数目
	int caTimes;//读者可续借次数
	int reBonumber;//读者已续借次数
	int caDays;//读者可借书天数
	COleDateTime baDate;//应还日期
	int holdNumber;//预约图书数量
	bool pidCorrect;//读者ID是否正确
	bool pwCorrect;//密码是否正确
	CString passWd;//密码
	int sendLen;//发送长度
	int length;//转换时的长度
	CString boID;	
	CString readerCN;
	//int feeAmount;//读者欠费
	CString chargedItems;//读者借书统计
};
