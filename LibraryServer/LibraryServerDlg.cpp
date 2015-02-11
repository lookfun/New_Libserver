
// LibraryServerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include <afx.h>
#include <afxcoll.h>
#include "LibraryServer.h"
#include "LibraryServerDlg.h"
#include "afxdialogex.h"
#include "QueueList.h"
#include "AESTransfer.h"

#define _AFXDLL
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//定义全局变量用于进程间的通信：
HWND hwnd;//主线程句柄
int opFlag;//消息开头
CString showStr;//屏显消息
//CLibraryServerDlg  *librarydlg;
CQueueList	mylist;  //接收队列
bool m_blinkDatabase=false;//连接数据库标志，默认连接数据库失败
bool m_blinkDatabaseSQL=false;//连接数据库标志，默认连接数据库失败
CString strLast(_T(""));///记录上次发送的信息
CString recLast(_T(""));///记录上次接收的信息
LibSerSocket *m_pSocket=NULL;//通信用的Socket临时指针
_ConnectionPtr m_Connection;			//连接对象指针
_RecordsetPtr m_pRecordset;				//记录集对象指针
_ConnectionPtr m_ConnectionSQL;			//连接对象指针
_RecordsetPtr m_pRecordsetSQL;				//记录集对象指针
bool thrFlag=true;//线程标志位
CString addr;//客户端地址

bool EncryptAndDecrypt(CString strIn,CString &strOut,bool EnOrDe);//EnOrDe：true为加密，false为解密
unsigned char HexToUChar(unsigned char ch );//将16进制字母转换为值

int Get_Time(char *time,int n)///by month按月计算
{
	if(n<0)
		n=0;
	if(n>=3)///续借顶多一个月
		n=1;
	COleDateTime curTime;
	curTime=COleDateTime::GetCurrentTime();//获取当前时间	
	sprintf_s(time,19,"%.4d%.2d%.2d0000%.2d%.2d%.2d",curTime.GetYear(),curTime.GetMonth(),curTime.GetDay(),curTime.GetHour(),curTime.GetMinute(),curTime.GetSecond());
	*(time+18)='\0';
	return strlen(time);
}

bool Get_Sub(const char *str,const char *FID,char result[])
{
	int length;//最后拷贝结果的长度
	/*const char *temp;*/
	const char *temp1;
	const char *temp2;
	temp1=strstr(str,FID);//找出str2字符串在str1字符串中第一次出现的位置（不包括str2的串结束符）。
	temp2=strstr(temp1,"|");//查找段标志后的最近的一个“|”分隔符
	if(NULL==temp1||NULL==temp2)//如果没有找到，要查找的内容，则返回失败
	{
		return false;
	}
	else
	{
		length=temp2-temp1-2;//拷贝的长度
		memcpy(result,temp1+2,length);//将查找到的内容拷入结果数组
		*(result+length)='\0';//添加结束符
		return true;
	}
}

bool Get_Sub2(const char *str,const char *FID,char result[])
{
	int length;//最后拷贝结果的长度
	/*const char *temp;*/
	const char *temp1;
	const char *temp2;
	temp1=strstr(str,FID);//找出str2字符串在str1字符串中第一次出现的位置（不包括str2的串结束符）。
	temp2=strstr(temp1,"/");//查找段标志后的最近的一个“/”分隔符
	if(NULL==temp1||NULL==temp2)//如果没有找到，要查找的内容，则返回失败
	{
		return false;
	}
	else
	{
		length=temp2-temp1-2;//拷贝的长度
		memcpy(result,temp1+2,length);//将查找到的内容拷入结果数组
		*(result+length)='\0';//添加结束符
		return true;
	}
}

int Add_Check(char *str)///获取发送数据包的校验码
{
	//首先判断str的长度，然后根据其生成校验码
	int i;
	int len=strlen(str);
	UINT32 sum=0,s=0x0000FFFF;
	for (i=0;i<len;i++)
	{
		sum+=str[i];///
	}
	sum=0-sum;//取反
	UINT16 Sum=(UINT16)sum&s;
	sprintf_s(str+len,6,"%.4X",Sum);
	*(str+len+4)='\n';
	*(str+len+5)='\0';
	return len+5;
}

bool LinkDatabase()//连接图书馆数据库
{
	//首先读取配置文件
	CString strSQL;//要执行的SQL语句
	CString strUserName;//连接数据库的用户名称
	CString strPW,strTemp;//连接数据库密码
	CString strIP;//数据库Ip地址
	CString strLibName;//数据库名称

	::GetPrivateProfileString(_T("LIB"),_T("User"),_T("libuser"),strUserName.GetBuffer(16),16,_T(".\\LibraryServer.ini"));
	strUserName.ReleaseBuffer();

	::GetPrivateProfileString(_T("LIB"),_T("Password"),_T("Password"),strTemp.GetBuffer(40),40,_T(".\\LibraryServer.ini"));
	strTemp.ReleaseBuffer();	   
	EncryptAndDecrypt(strTemp,strPW,false);//解密

	::GetPrivateProfileString(_T("LIB"),_T("IPAddress"),_T("192.168.1.99"),strIP.GetBuffer(30),30,_T(".\\LibraryServer.ini"));
	strIP.ReleaseBuffer();

	::GetPrivateProfileString(_T("LIB"),_T("Database"),_T("LIBRARY"),strLibName.GetBuffer(16),16,_T(".\\LibraryServer.ini"));
	strLibName.ReleaseBuffer();	
	strSQL.Format(_T("Provider=OraOLEDB.Oracle.1;User ID=%s;Password=%s;Data Source=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=%s)(PORT=1521))(CONNECT_DATA=(SERVICE_NAME = %s)));Persist Security Info=False"),strUserName,strTemp,strIP,strLibName);

	// m_Connection.CreateInstance(__uuidof(Connection));
	m_pRecordset.CreateInstance(__uuidof(Recordset));//创建记录集对象
	HRESULT hr; 
	try 
	{ 
		::CoInitialize(NULL); 
		hr = m_Connection.CreateInstance("ADODB.Connection");//创建Connection对象
		if(SUCCEEDED(hr))
		{
			m_Connection->ConnectionTimeout=8;
			//hr = m_Connection->Open("Provider=OraOLEDB.Oracle.1;User ID=libuser;Password=abc123;Data Source=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=192.168.1.99)(PORT=1521))(CONNECT_DATA=(SERVICE_NAME = library)));Persist Security Info=False","","",adOpenUnspecified);
			hr = m_Connection->Open((_bstr_t)strSQL,"","",adOpenUnspecified);
			///连接数据库，此处的红色字体是为CString类型语句，里面的IP、User ID、Password分别为服务器的IP、数据库用户名、密码
			m_blinkDatabase=true;
			return true;
		}
		else
		{
			m_blinkDatabase=false;
			return false;
		}
	}
	catch(_com_error e)///捕捉异常
	{ 
		::CoUninitialize();
		m_blinkDatabase=false;//出现异常时，连接数据库失败
		return false;
		//CString errorMessage;
		//errorMessage.Format(_T("连接数据库失败！^错误信息：%s"),e.ErrorMessage());
		//AfxMessageBox(errorMessage); 
	}
}

bool LinkSQL()//连接本地数据库
{
	//首先读取配置文件
	CString strSQL;//要执行的SQL语句
	CString strUserName;//连接数据库的用户名称
	CString strPW,strTemp;//连接数据库密码
	CString strIP;//数据库Ip地址
	CString strLibName;//数据库名称

	::GetPrivateProfileString(_T("LIBSTA"),_T("User"),_T("libuser"),strUserName.GetBuffer(16),16,_T(".\\LibraryServer.ini"));
	strUserName.ReleaseBuffer();

	::GetPrivateProfileString(_T("LIBSTA"),_T("Password"),_T("Password"),strPW.GetBuffer(40),40,_T(".\\LibraryServer.ini"));
	strPW.ReleaseBuffer();	   
	//EncryptAndDecrypt(strTemp,strPW,false);//解密
	
	::GetPrivateProfileString(_T("LIBSTA"),_T("IPAddress"),_T("192.168.1.99"),strIP.GetBuffer(30),30,_T(".\\LibraryServer.ini"));
	strIP.ReleaseBuffer();

	::GetPrivateProfileString(_T("LIBSTA"),_T("Database"),_T("LIBRARY"),strLibName.GetBuffer(16),16,_T(".\\LibraryServer.ini"));
	strLibName.ReleaseBuffer();	

	strSQL.Format(_T("Provider=sqloledb;Network Library=DBMSSOCN;Data Source=%s,1433;Initial Catalog=%s;User ID=%s;Password=%s"),strIP,strLibName,strUserName,strPW);//Provider = SQLOLEDB.1;Integrated Security=SSPI;Persist Security Info=False ;Initial Catalog=LibSVRSTA; Data Source=(local);

	m_pRecordsetSQL.CreateInstance(__uuidof(Recordset));//创建记录集对象
	HRESULT hr; 
	try 
	{ 
		::CoInitialize(NULL); 
		hr = m_ConnectionSQL.CreateInstance("ADODB.Connection");//创建Connection对象
		if(SUCCEEDED(hr))
		{
			m_ConnectionSQL->ConnectionTimeout=8;
			hr = m_ConnectionSQL->Open((_bstr_t)strSQL,"","",adModeUnknown);
			m_blinkDatabaseSQL=true;
			return true; 
		}
		else
		{
			m_blinkDatabase=false;
			return false;
		}
	}
	catch(_com_error e)///捕捉异常
	{ 
		::CoUninitialize();
		m_blinkDatabaseSQL=false;//出现异常时，连接数据库失败
		showStr.Format(_T("本地数据库错误001！^错误信息：%s"),e.ErrorMessage());
		::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
		/*CString errorMessage;
		errorMessage.Format(_T("连接数据库失败！^错误信息：%s"),e.ErrorMessage());
		AfxMessageBox(errorMessage); */
		return false;
	}
}

bool Verify(CString str)///检验收到的数据包是否是正确的
{
	//int n=str.GetLength();
	int m=str.ReverseFind('Z');
	CString s1=str.Left(m+1);
	CString s2=str.Mid(m+1);
	char cha[2048];
	WideCharToMultiByte(CP_ACP,0,(LPCTSTR)s1,-1,cha,2048,0,false);
	Add_Check(cha);
	if(cha==str)
		return true;
	else
		return false;
}

//将统计信息写入本地数据库。fucFlag：0,借书。1,还书。2,续借。3,报错。返回值：0,出错。1,借书返回。2,还书返回。3,续借返回。4,报错返回
int SavetoLocal(int fucFlag)
{
	CTime currenttime;
	CString strSQL;
	CString temp;//临时变量
	_variant_t vFieldValue;//数据库操作的临时变量
	currenttime=CTime::GetCurrentTime();
	temp=currenttime.Format(_T(" %Y%m%d"))+addr.Right(3);

	strSQL.Format(_T("select * from StaInfo where ID = '%s'" ),temp);
	//newData.strSQL.Format(_T("update StaInfo set Checkout=Checkout+1 where ID = '%s'" ),newData.temp3);
	try 
	{
		m_pRecordsetSQL->Open(_bstr_t(strSQL),m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
	}						
	catch(_com_error e)///捕捉异常
	{
		//CString errorMessage;
		showStr.Format(_T("本地数据库错误002！^错误信息：%s"),e.ErrorMessage());
		::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
		return 0;//出现异常，则返回0
	}

	int countemp=0;
	while(!(m_pRecordsetSQL->adoEOF))
	{
		countemp++;
		m_pRecordsetSQL->MoveNext();
	}
	m_pRecordsetSQL->Close();//关闭记录集

	switch(fucFlag)
	{
	case 0:
		{
			if(countemp!=0)//如果记录为不为空，则更新数据
			{
				strSQL.Format(_T("update StaInfo set Checkout=Checkout+1 where ID = '%s'" ),temp);
				try 
				{
					//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					//m_blinkDatabaseSQL=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误003！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
				//
			}
			else//记录为空则添加新数据
			{
				strSQL.Format(_T("insert into StaInfo(ID,Checkin,Checkout,Renew,Error) values ('%s','0','1','0','0')"),temp);	
				try 
				{
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误004！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
			}
		}
		return 1;
	case 1:
		{
			if(countemp!=0)//如果记录为不为空，则更新数据
			{
				strSQL.Format(_T("update StaInfo set Checkin=Checkin+1 where ID = '%s'" ),temp);
				try 
				{
					//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					//m_blinkDatabaseSQL=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误005！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
				//
			}
			else//记录为空则添加新数据
			{
				strSQL.Format(_T("insert into StaInfo(ID,Checkin,Checkout,Renew,Error) values ('%s','1','0','0','0')"),temp);	
				try 
				{
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误006！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
			}
		}
		return 2;
	case 2:
		{
			if(countemp!=0)//如果记录为不为空，则更新数据
			{
				strSQL.Format(_T("update StaInfo set Renew=Renew+1 where ID = '%s'" ),temp);
				try 
				{
					//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					//m_blinkDatabaseSQL=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误007！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
				//
			}
			else//记录为空则添加新数据
			{
				strSQL.Format(_T("insert into StaInfo(ID,Checkin,Checkout,Renew,Error) values ('%s','0','0','1','0')"),temp);	
				try 
				{
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误008！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
			}
		}
		return 3;
	case 3:
		{
			if(countemp!=0)//如果记录为不为空，则更新数据
			{
				strSQL.Format(_T("update StaInfo set Error=Error+1 where ID = '%s'" ),temp);
				try 
				{
					//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					//m_blinkDatabaseSQL=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误009！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
				//
			}
			else//记录为空则添加新数据
			{
				strSQL.Format(_T("insert into StaInfo(ID,Checkin,Checkout,Renew,Error) values ('%s','0','0','0','1')"),temp);	
				try 
				{
					m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
				}						
				catch(_com_error e)///捕捉异常
				{
					showStr.Format(_T("本地数据库错误010！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}
			}
		}
		return 4;
	default:
		return 0;
	}
};

int TraSIP2(CString Msg)//解析消息,返回值：0.通信错误或数据库错误，1.收到不兼容消息，2.解析消息失败，其他.sip2返回消息开头
{
	threadData newData;
	newData.recvTemp=Msg;
	switch(_tcstoul(static_cast<LPCTSTR>(newData.recvTemp.Left(2)), 0, 10))   //_tcstoul(static_cast<LPCTSTR>(newData.recvTemp.Left(1)), 0, 16)
	{
	case	99:///通信规则 接收到SC Status消息，返回98开头的ACS Status消息
		{
			newData.sendTemp.Format(_T("98YYYYNN600010"));
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中newData.sendTemp
			newData.temp3=newData.temp;
			newData.sendTemp+=newData.temp3;
			newData.sendTemp+=_T("2.00AOuestc|AM|BXYYYNYYNYYNYNNNNN|AY0AZ");
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("通信规则发送失败！");
				break;
			}
			else
				showStr=_T("通信规则发送成功！");

			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendBuffer;//记录发送信息
		}
		return 98; 
	case	35:///通信规则 接收到End Patron Session消息，返回36开头的End Session Response消息
		{
			newData.sendTemp.Format(_T("36Y"));
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中newData.sendTemp
			newData.temp3=newData.temp;
			newData.sendTemp+=newData.temp3;
			newData.sendTemp=newData.sendTemp+(_T("AOuestc|AA|AF|AG|AY0AZ"));
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);
				return 0;
			}
			else
				showStr=_T("读者已成功退出！");

			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 36;
	case	63:///读者信息查询 返回“读者信息查询反馈”64开头
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AA",newData.temp);//读者id
			Get_Sub(newData.recBuffer,"AD",newData.temp2);//读者密码
			memcpy(newData.readerCardNum,newData.temp,strlen(newData.temp)+1);
			memcpy(newData.passWord,newData.temp2,strlen(newData.temp2)+1);

			/***********************************************/
			newData.pwCorrect=false;//默认密码不正确
			newData.recordCount=0;//记录集数目设为0
			newData.readerCN=newData.readerCardNum;
			newData.strSQL.Format(_T("select * from readerinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();

			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败631！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}

			if(m_blinkDatabase)
			{
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1
					/***************先读取密码****************/
					CString PWD;
					newData.vFieldValue=m_pRecordset->GetCollect(_T("usercode"));//读取卡密码
					PWD=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					/***************判断密码是否正确****************/
					if(newData.passWord==PWD)//密码正确
					{
						newData.pwCorrect=true;						
					}
					else
					{
						newData.pwCorrect=false;
					}

					/******************查询读者姓名********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("name"));//读取读者姓名
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.patronName=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					/******************查询读者罚款情况********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("fine"));//读取读者罚款情况
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.feeAmount=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					/******************查询读者借书累计********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bonumber"));//读取读者借书累计
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.borrowNumber=(int)(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					/******************读者类型********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("retype"));
					newData.reType=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//读者卡状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("castate"));
					newData.caState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();//移动到下一条
				}

			}
			m_pRecordset->Close();//关闭数据库记录集对象指针
			if(0==newData.recordCount)//判断有没有找到该读者
			{
				newData.pidCorrect=false;//没有该读者
			}
			else//找到该读者
			{
				newData.pidCorrect=true;
			}

			/******************查询读者预约累计********************/
			newData.strSQL.Format(_T("select * from appointmentinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();
			m_blinkDatabase=false;
			newData.holdNumber=0;//默认预约0本
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败632！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示出错消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				while(!(m_pRecordset->adoEOF))
				{

					newData.vFieldValue=m_pRecordset->GetCollect(_T("boid"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.holdNumber++;//记录集数目加1
					}
					newData.vFieldValue.Clear();
					m_pRecordset->MoveNext();//移动到下一条
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			//读者类型表
			newData.strSQL.Format(_T("select * from readertypeinfo where retype= '%s'" ),newData.reType);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败633！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示出错消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					//读者可借书数目
					newData.vFieldValue=m_pRecordset->GetCollect(_T("canumber"));
					newData.caNumber=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			//借阅关系表
			newData.strSQL.Format(_T("select * from borrowinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
			//	CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败634！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}

			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//图书ID
					newData.chargedItems=newData.chargedItems+_T("|AU");
					newData.vFieldValue=m_pRecordset->GetCollect(_T("boid"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.temp3=(char*)_bstr_t(newData.vFieldValue);													
						newData.chargedItems=newData.chargedItems+newData.temp3;
						//ChargedItems=ChargedItems+_T("|");
					}
					newData.vFieldValue.Clear();
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			if(newData.pidCorrect&&(newData.caState==_T("正常")))//卡号正确
			{
				/***********************生成返回信息***************************/			
				//消息编号，语言
				newData.sendLen=0;
				newData.sendTemp.Format(_T("64              019"));

				//生成通信时间
				newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
				newData.temp3=newData.temp;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//预约书数目
				newData.temp3.Format(_T("%d"), newData.holdNumber);
				newData.sendTemp=newData.sendTemp+(_T("000"))+newData.temp3;

				//加入超期图书统计（0000）
				newData.sendTemp=newData.sendTemp+(_T("0000"));

				//加入已借图书统计
				newData.sendTemp=newData.sendTemp+(_T("000"));
				newData.temp3.Format(_T("%d"), newData.borrowNumber);
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//加入罚款图书、召回图书、预约失败图书统计（0000）
				newData.sendTemp=newData.sendTemp+(_T("000000000000"));

				//图书馆id
				newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

				//读者id
				newData.sendTemp=newData.sendTemp+(_T("|AA"));
				newData.temp3=newData.readerCardNum;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//读者姓名
				newData.sendTemp=newData.sendTemp+(_T("|AE"));
				newData.sendTemp=newData.sendTemp+newData.patronName;

				//预约限制、超期限制（00020002）
				newData.sendTemp=newData.sendTemp+(_T("|BZ0002|CA0002"));

				//借书限制
				newData.sendTemp=newData.sendTemp+(_T("|CB00"));
				newData.temp3.Format(_T("%d"), newData.caNumber);
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//bool PIDcorrect;//读者ID是否正确
				newData.sendTemp=newData.sendTemp+(_T("|BL"));
				if(newData.pidCorrect)
				{
					newData.sendTemp=newData.sendTemp+(_T("Y"));
				}else
				{
					newData.sendTemp=newData.sendTemp+(_T("N"));
				}

				//bool pwCorrect;//密码是否正确.
				newData.sendTemp=newData.sendTemp+(_T("|CQ"));
				if(newData.pwCorrect)
				{
					newData.sendTemp=newData.sendTemp+(_T("Y"));
				}else
				{
					newData.sendTemp=newData.sendTemp+(_T("N"));
				}

				//罚款状况
				newData.sendTemp=newData.sendTemp+(_T("|BV"));
				newData.sendTemp=newData.sendTemp+newData.feeAmount;

				//罚款限制
				newData.sendTemp=newData.sendTemp+(_T("|CC2.00"));

				//读者借书统计
				if(newData.chargedItems!=_T(""))
					newData.sendTemp=newData.sendTemp+newData.chargedItems;
				else
					newData.sendTemp=newData.sendTemp+_T("|AU");

				//验证码开头
				newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));

				//屏显信息
				showStr=_T("读者信息查询成功！读者号：")+newData.readerCN;
			}else//卡号不正确
			{
				newData.sendTemp.Format(_T("64              019"));

				//图书馆id
				newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

				//读者id
				newData.sendTemp=newData.sendTemp+(_T("|AA"));
				newData.temp3=newData.readerCardNum;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//卡号不正确
				newData.sendTemp=newData.sendTemp+(_T("|BLN|CQN"));

				//验证码开头
				newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));

				showStr=_T("读者信息查询失败！非法卡号。");
			}

			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("读者查询消息发送失败！");
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 64;
	case	17:///图书信息查询，返回图书信息18开头
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AB",newData.temp);//图书id
			memcpy(newData.bookID,newData.temp,strlen(newData.temp)+1);

			/********************图书信息表********************/
			newData.recordCount=0;//记录集数目设为0
			newData.boID=newData.bookID;
			newData.strSQL.Format(_T("select * from bookinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();

			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败171！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}

			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//图书名称
					newData.vFieldValue=m_pRecordset->GetCollect(_T("boname"));//书名
					newData.boName=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					//图书状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bostate"));
					newData.boState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//图书作者
					//vFieldValue=m_pRecordset->GetCollect(_T("writer"));
					//boWriter=(char*)_bstr_t(vFieldValue);
					//vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			if(0==newData.recordCount)//判断有没有找到该图书
			{
				newData.boidCorrect=false;//没有该图书
			}
			else//找到该图书
			{
				newData.boidCorrect=true;
			}

			//借阅关系表
			COleDateTime newRetDate;
			newData.strNew=_T("0");
			newData.strSQL.Format(_T("select * from borrowinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败172！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return 0;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//应还时间
					newData.vFieldValue=m_pRecordset->GetCollect(_T("badate"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newRetDate=newData.vFieldValue.date;	
						newData.strNew=newRetDate.Format(_T("%Y%m%d"));//应还日期
					}
					newData.vFieldValue.Clear();

					//借书时间
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bodate"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newRetDate=newData.vFieldValue.date;	
						newData.transactiondate=newRetDate.Format(_T("%Y%m%d"));//借书日期
					}
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();

				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			/****************************生成返回消息***************************/
			if(newData.boidCorrect)
			{
				newData.sendTemp.Format(_T("18"));

				//图书状态
				if(newData.boState==_T("可借"))
				{
					newData.sendTemp=newData.sendTemp+(_T("03"));
				}else if(newData.boState==_T("借出"))
				{
					newData.sendTemp=newData.sendTemp+(_T("04"));
				}else if(newData.boState==_T("注销"))
				{
					newData.sendTemp=newData.sendTemp+(_T("13"));
				}else if(newData.boState==_T("丢失"))
				{
					newData.sendTemp=newData.sendTemp+(_T("12"));
				}else if(newData.boState==_T("馆内阅读"))
				{
					newData.sendTemp=newData.sendTemp+(_T("10"));
				}else if(newData.boState==_T("预约"))
				{
					newData.sendTemp=newData.sendTemp+(_T("02"));
				}else if(newData.boState==_T("超期"))
				{
					newData.sendTemp=newData.sendTemp+(_T("05"));
				}

				//fee type
				newData.sendTemp=newData.sendTemp+(_T("0001"));

				//transaction date
				if(newData.boState==_T("借出"))
					newData.sendTemp=newData.sendTemp+newData.transactiondate+(_T("0000000000"));
				else
				{
					newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
					newData.temp3=newData.temp;
					newData.sendTemp=newData.sendTemp+newData.temp3;
				}

				//
				newData.sendTemp=newData.sendTemp+(_T("|CF 0"));

				//到期日期
				newData.sendTemp=newData.sendTemp+(_T("|AH"));
				if(newData.boState==_T("借出"))
					newData.sendTemp=newData.sendTemp+newData.strNew.Left(4)+_T("-")+newData.strNew.Mid(4,2)+_T("-")+newData.strNew.Right(2);

				//添加图书编号
				newData.sendTemp=newData.sendTemp+(_T("|AB"));
				newData.temp3=newData.bookID;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//添加书名、作者
				newData.sendTemp=newData.sendTemp+(_T("|AJ"));
				newData.sendTemp=newData.sendTemp+newData.boName;/*+boWriter*/

				//其他信息
				newData.sendTemp=newData.sendTemp+(_T("|BG|BV|CK001|AQsa|CH|AF|CSTP331-43 587 |CT|AY0AZ"));

				showStr=_T("图书信息查询成功！图书编号：")+newData.temp3;

			}else
			{
				newData.sendTemp.Format(_T("18130001CF 0|AB"));
				newData.temp3=newData.bookID;
				newData.sendTemp=newData.sendTemp+newData.temp3;
				newData.sendTemp=newData.sendTemp+(_T("|AJNo Title|BG|BV|CK001|AQsa|CH|AF|CSTP331-43 587 |CT|AY0AZ"));
				showStr=_T("图书信息查询失败！非法图书编号：")+newData.temp3;
			}

			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 18;
	case	11:         //borrow book request
		{
			COleDateTime currentTime;		
			COleDateTime newRetDate;
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AB",newData.temp);//图书信息
			Get_Sub(newData.recBuffer,"AA",newData.temp2);//读者id
			memcpy(newData.bookID,newData.temp,strlen(newData.temp)+1);
			memcpy(newData.readerCardNum,newData.temp2,strlen(newData.temp2)+1);

			/************************查询数据库*****************************/
			//图书信息表
			newData.boID=(CString)newData.bookID;
			newData.strSQL.Format(_T("select * from bookinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败111！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return 0;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//图书名称
					newData.vFieldValue=m_pRecordset->GetCollect(_T("boname"));//书名
					newData.boName=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					//图书状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bostate"));
					newData.boState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//图书作者
					newData.vFieldValue=m_pRecordset->GetCollect(_T("writer"));
					newData.boWriter=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			//读者信息表
			newData.readerCN=newData.readerCardNum;
			newData.strSQL.Format(_T("select * from readerinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败112！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1
					//读者借书累计
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bonumber"));
					newData.borrowNumber=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					//读者类型
					newData.vFieldValue=m_pRecordset->GetCollect(_T("retype"));
					newData.reType=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//读取读者罚款情况
					newData.vFieldValue=m_pRecordset->GetCollect(_T("fine"));
					newData.feeAmount=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//读者卡状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("castate"));
					newData.caState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			//读者类型表
			newData.strSQL.Format(_T("select * from readertypeinfo where retype= '%s'" ),newData.reType);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败113！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					//读者可借书数目
					newData.vFieldValue=m_pRecordset->GetCollect(_T("canumber"));
					newData.caNumber=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					//读者可借书天数
					newData.vFieldValue=m_pRecordset->GetCollect(_T("cadays"));
					newData.caDays=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			/*****************生成返回消息********************/
			newData.sendTemp.Format(_T("12"));  
			int feetemp;
			feetemp= _tstoi( LPCTSTR(newData.feeAmount));
			if((newData.boState==_T("可借"))&&(newData.borrowNumber<newData.caNumber)&&(feetemp<2.00)&&(newData.caState==_T("正常")))//图书状态可借，且读者没有超过可借书总数
			{
				newData.sendTemp=newData.sendTemp+(_T("1"));
				showStr=_T("读者借书成功！图书编号：")+newData.boID;
			}else
			{
				newData.sendTemp=newData.sendTemp+(_T("0"));
				showStr=_T("读者借书失败！");
			}

			//默认可续借，无磁盘
			newData.sendTemp=newData.sendTemp+(_T("YNN"));

			//生成通信时间
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
			newData.temp3=newData.temp;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//单位名称，uestc
			newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

			//读者ID
			newData.sendTemp=newData.sendTemp+(_T("|AA"));
			newData.temp3=newData.readerCardNum;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//添加图书编号
			newData.sendTemp=newData.sendTemp+(_T("|AB"));
			newData.temp3=newData.bookID;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//添加书名、作者
			newData.sendTemp=newData.sendTemp+(_T("|AJ"));
			newData.sendTemp=newData.sendTemp+newData.boName+newData.boWriter;

			//到期日期
			currentTime=COleDateTime::GetCurrentTime();//获得当前时间
			newRetDate.m_dt=currentTime.m_dt+(DATE)newData.caDays;//应还日期
			newData.strNew=newRetDate.Format(_T("%Y-%m-%d"));//应还日期
			newData.sendTemp+=(_T("|AH"))+newData.strNew;
			newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

			/***********************修改数据库信息*************************/

			if((newData.boState==_T("可借"))&&(newData.borrowNumber<newData.caNumber)&&(feetemp<2.00)&&(newData.caState==_T("正常")))//图书状态可借，且读者没有超过可借书总数
			{
				/************************修改图书状态*************************/
				newData.strSQL.Format(_T("select * from bookinfo where boid = '%s'" ),newData.boID);				
				newData.bSql=newData.strSQL.AllocSysString();
				 
				try
				{							
					m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}

				catch(_com_error e)///捕捉异常
				{				
					//CString errorMessage;				
					showStr.Format(_T("连接数据库失败114！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					m_blinkDatabase=false;
					return 0;//出现异常，则返回0
				}
				while(!(m_pRecordset->adoEOF))
				{
					/*****************修改图书状态*******************/			
					m_pRecordset->PutCollect("bostate",_variant_t(_T("借出")));//修改图书状态
					m_pRecordset->Update();//提交记录
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库

				/****************修改读者借书数*********************/
				newData.readerCN=newData.readerCardNum;
				newData.strSQL.Format(_T("select * from readerinfo where bocode = '%s'" ),newData.readerCN);				
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{							
					m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				}

				catch(_com_error e)///捕捉异常
				{				
					//CString errorMessage;
					showStr.Format(_T("连接数据库失败114！^错误信息：%s"),e.ErrorMessage());
					m_blinkDatabase=false;
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}
				while(!(m_pRecordset->adoEOF))
				{
					/*******************获取读者的已借图书数量*******************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bonumber"));
					if(VT_NULL!=newData.vFieldValue.vt)
					{
						newData.borrowNumber=newData.vFieldValue.intVal;
						newData.vFieldValue.Clear();
					}
					else
					{
						newData.borrowNumber=0;
					}

					newData.borrowNumber=newData.borrowNumber+1;//借书数量加1

					/*****************修改读者的已借图书数量*******************/			
					m_pRecordset->PutCollect("bonumber",_variant_t(newData.borrowNumber));
					m_pRecordset->Update();//提交记录
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库		

				/*****************在借阅信息表中添加记录******************/
				currentTime=COleDateTime::GetCurrentTime();//获得当前时间
				newRetDate.m_dt=currentTime.m_dt+(DATE)newData.caDays;//应还日期
				newData.strNow=currentTime.Format(_T("%Y%m%d"));//当天日期
				newData.strNew=newRetDate.Format(_T("%Y%m%d"));//应还日期
				CString readerCardnum(newData.readerCardNum);/*yyyy-mm-dd*/
				newData.strSQL.Format(_T("insert into borrowinfo(bocode,boid,bodate,badate,rebonumber) values ('%s','%s',to_date('%s','YYMMDD'),to_date('%s','YYMMDD'),'0')"),readerCardnum,newData.boID,newData.strNow,newData.strNew);				
				newData.bSql=newData.strSQL.AllocSysString();
				try
				{
					m_Connection->Execute(_bstr_t(newData.strSQL),&(newData.vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					m_blinkDatabase=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					//CString errorMessage;
					showStr.Format(_T("连接数据库失败115！^错误信息：%s"),e.ErrorMessage());
					m_blinkDatabase=false;
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}
				//m_pRecordset->Close();//关闭数据库
			}
			//本地数据库未连接则尝试连接
	 	/*	if(!m_blinkDatabaseSQL)
			{
				LinkSQL();
			}
			//已连接本地数据库则添加本地消息记录
			if(m_blinkDatabaseSQL)
			{
				SavetoLocal(0);
			} 
		*/

			/***************************发送信息****************************/
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 12;
	case	9:///还书返回，返回10开头
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AB",newData.temp);//图书信息
			memcpy(newData.bookID,newData.temp,strlen(newData.temp)+1);

			/**************************查询数据库*****************************/
			//图书信息表
			newData.boID=newData.bookID;
			newData.strSQL.Format(_T("select * from bookinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败91！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//图书名称
					newData.vFieldValue=m_pRecordset->GetCollect(_T("boname"));//书名
					newData.boName=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					//图书状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bostate"));
					newData.boState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//图书作者
					newData.vFieldValue=m_pRecordset->GetCollect(_T("writer"));
					newData.boWriter=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}
			//借阅关系表
			newData.strSQL.Format(_T("select * from borrowinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败92！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//借书证号
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bocode"));
					newData.readerCN=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			//借阅关系表
			newData.strSQL.Format(_T("select * from borrowinfo where boid= '%s'" ),newData.boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败93！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1
					//借书证号
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bocode"));
					newData.readerCN=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			/*******************生成返回消息********************/
			newData.sendTemp.Format(_T("10"));

			if(newData.boState==_T("借出"))//图书状态借出
			{
				newData.sendTemp=newData.sendTemp+(_T("1"));
				showStr=_T("还书成功！图书编号：")+newData.boID;
			}else
			{
				newData.sendTemp=newData.sendTemp+(_T("0"));
				showStr=_T("还书失败！图书未借出，图书编号：")+newData.boID;
			}

			//默认，无磁盘
			newData.sendTemp=newData.sendTemp+(_T("YNN"));

			//生成通信时间
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
			newData.temp3=newData.temp;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//单位名称，uestc
			newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

			//添加图书编号
			newData.sendTemp=newData.sendTemp+(_T("|AB"));
			newData.temp3=newData.bookID;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//permanent location
			newData.sendTemp=newData.sendTemp+(_T("|AQsa   "));

			//添加书名、作者
			newData.sendTemp=newData.sendTemp+(_T("|AJ"));
			newData.sendTemp=newData.sendTemp+newData.boName;
			newData.sendTemp=newData.sendTemp+newData.boWriter;
			newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

			/***********************修改数据库信息*************************/
			if(newData.boState==_T("借出"))//图书状态借出
			{
				/*******************修改读者借书数****************/
				newData.strSQL.Format(_T("update readerinfo set bonumber=bonumber-1 where bocode = '%s'" ),newData.readerCN);	//首先要获得读者证号			
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{							
					m_Connection->Execute(newData.bSql,&newData.vFieldValue,adCmdText);//执行SQL语句，将借书数减一
					m_blinkDatabase=true;
				}

				catch(_com_error e)///捕捉异常
				{
					//CString errorMessage;
					m_blinkDatabase=false;
					showStr.Format(_T("连接数据库失败94！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}

				/*******************在借阅信息表中删除记录***********************/
				newData.strSQL.Format(_T("delete from borrowinfo where boid='%s'"),newData.boID);//删除记录				
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{			
					m_Connection->Execute(newData.bSql,&newData.vFieldValue,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}						
				catch(_com_error e)///捕捉异常
				{
					//CString errorMessage;
					m_blinkDatabase=false;
					showStr.Format(_T("连接数据库失败95！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}			
				//pRecordset2->Close();//关闭数据库

				/************************修改图书状态*************************/
				newData.strSQL.Format(_T("select * from bookinfo where boid = '%s'" ),newData.boID);				
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{							
					m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}

				catch(_com_error e)///捕捉异常
				{				
					//CString errorMessage;
					m_blinkDatabase=false;
					showStr.Format(_T("连接数据库失败96！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}
				while(!(m_pRecordset->adoEOF))
				{
					/*****************修改图书状态*******************/			
					m_pRecordset->PutCollect("bostate",_variant_t(_T("可借")));//修改图书状态
					m_pRecordset->Update();//提交记录
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库
			}

			//本地数据库未连接则尝试连接
		/*	if(!m_blinkDatabaseSQL)
			{
				LinkSQL();
			}
			//已连接本地数据库则添加本地消息记录
			if(m_blinkDatabaseSQL)
			{
				SavetoLocal(1);
			}
		*/

			/***************************发送信息****************************/
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 10;
	case	29://续借，返回30开头
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AB",newData.temp);//图书信息
			Get_Sub(newData.recBuffer,"AA",newData.temp2);//读者id
			memcpy(newData.bookID,newData.temp,strlen(newData.temp)+1);
			memcpy(newData.readerCardNum,newData.temp2,strlen(newData.temp2)+1);

			//获得系统当前时间
			COleDateTime currentTime=COleDateTime::GetCurrentTime();//获得当前时间
			currentTime.m_dt=(long)currentTime.m_dt;//去掉时间的小数部分（小数表示时、分、秒）
			/************************查询数据库*****************************/

			//借阅关系表
			CString readerCN;
			CString boID;boID=newData.bookID;
			newData.strSQL.Format(_T("select * from borrowinfo where boid= '%s'" ),boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败291！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//借书证号
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bocode"));
					readerCN=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//读者已续借次数
					newData.vFieldValue=m_pRecordset->GetCollect(_T("rebonumber"));
					newData.reBonumber=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					//应还日期
					newData.vFieldValue=m_pRecordset->GetCollect(_T("badate"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.baDate=newData.vFieldValue.date;	
						//strNew=baDate.Format(_T("%Y%m%d"));//应还日期
					}
					newData.vFieldValue.Clear();
					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			//图书信息表
			boID=newData.bookID;
			newData.strSQL.Format(_T("select * from bookinfo where boid= '%s'" ),boID);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败292！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//图书名称
					newData.vFieldValue=m_pRecordset->GetCollect(_T("boname"));//书名
					newData.boName=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					//图书状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("bostate"));
					newData.boState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//图书作者
					newData.vFieldValue=m_pRecordset->GetCollect(_T("writer"));
					newData.boWriter=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库记录集对象指针
			}

			//读者信息表
			readerCN=newData.readerCardNum;
			newData.strSQL.Format(_T("select * from readerinfo where bocode= '%s'" ),readerCN);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败293！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1

					//读者类型
					newData.vFieldValue=m_pRecordset->GetCollect(_T("retype"));
					newData.reType=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					//读者卡状态
					newData.vFieldValue=m_pRecordset->GetCollect(_T("castate"));
					newData.caState=(char*)_bstr_t(newData.vFieldValue);
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			//读者类型表
			newData.strSQL.Format(_T("select * from readertypeinfo where retype= '%s'" ),newData.reType);
			newData.bSql=newData.strSQL.AllocSysString();
			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败294！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}
			if(m_blinkDatabase)
			{
				newData.recordCount=0;
				while(!(m_pRecordset->adoEOF))
				{

					//读者可续借次数
					newData.vFieldValue=m_pRecordset->GetCollect(_T("catimes"));
					newData.caTimes=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					//读者可借书天数
					newData.vFieldValue=m_pRecordset->GetCollect(_T("cadays"));
					newData.caDays=(int)newData.vFieldValue.intVal;
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}
			}
			m_pRecordset->Close();//关闭数据库记录集对象指针

			/************************生成返回消息**********************/
			if((readerCN==newData.readerCardNum)&&(newData.caTimes>newData.reBonumber)&&(newData.baDate>currentTime)&&(newData.caState==_T("正常")))
			{
				newData.sendTemp.Format(_T("30"));  
				newData.sendTemp=newData.sendTemp+(_T("1"));//renew ok
				if(newData.caTimes>(newData.reBonumber+1))
				{
					newData.sendTemp=newData.sendTemp+(_T("Y"));
				}else
				{
					newData.sendTemp=newData.sendTemp+(_T("N"));
				}
				newData.sendTemp=newData.sendTemp+(_T("NN"));

				//生成通信时间
				newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
				newData.temp3=newData.temp;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//单位名称，uestc
				newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

				//读者ID
				newData.sendTemp=newData.sendTemp+(_T("|AA"));
				newData.temp3=newData.readerCardNum;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//添加图书编号
				newData.sendTemp=newData.sendTemp+(_T("|AB"));
				newData.temp3=newData.bookID;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//添加书名、作者
				newData.sendTemp=newData.sendTemp+(_T("|AJ"));
				newData.sendTemp=newData.sendTemp+newData.boName+newData.boWriter;

				//到期日期
				newData.sendTemp=newData.sendTemp+(_T("|AH"));
				newData.baDate=newData.baDate.m_dt+(DATE)newData.caDays;//刷新还书时间续借天数与可借书天数一致
				newData.strNew=newData.baDate.Format(_T("%Y%m%d"));//应还日期
				newData.sendTemp=newData.sendTemp+newData.strNew.Left(4)+(_T("-"))+newData.strNew.Mid(4,2)+(_T("-"))+newData.strNew.Right(2);
				newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

				showStr=_T("续借成功！图书编号：")+newData.temp3;

				/********************修改数据库信息*********************/				
				newData.strSQL.Format(_T("select * from borrowinfo where boid= '%s'" ),boID);
				BSTR bSql=newData.strSQL.AllocSysString();
				try
				{
					m_pRecordset->Open((CComVariant)bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}
				catch(_com_error e)///捕捉异常
				{ 
					//CString errorMessage;
					m_blinkDatabase=false;//连接数据库出错变量赋值为false
					showStr.Format(_T("连接数据库失败295！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return false;//出现异常，则返回false
				}

				if(m_blinkDatabase)//l连接数据库成功
				{
					while(!(m_pRecordset->adoEOF))
					{

						newData.reBonumber=newData.reBonumber+1;//续借次数加1

						//取出应还日期					
						newData.vFieldValue=m_pRecordset->GetCollect(_T("badate"));//应还日期
						if(VT_NULL!=newData.vFieldValue.vt)
						{
							currentTime=newData.vFieldValue.date;
							newData.vFieldValue.Clear();
						}
						else
						{
							currentTime=COleDateTime::GetCurrentTime();
						}
						//重新刷新还书时间
						//baDate.m_dt=baDate.m_dt+(DATE)30;//前面生成消息时，还书时间已经加上了，所以注释掉
						newData.baDate.m_dt=(long)newData.baDate.m_dt;

						m_pRecordset->PutCollect("rebonumber",_variant_t(newData.reBonumber));//修改续借次数
						m_pRecordset->PutCollect("badate",_variant_t(newData.baDate));//修改应还日期
						m_pRecordset->Update();//提交记录
						m_pRecordset->MoveNext();
					}
					m_pRecordset->Close();//关闭记录集
				}
			}else
			{
				newData.sendTemp=newData.sendTemp+(_T("120YNN"));

				//生成通信时间
				newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
				newData.temp3=newData.temp;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//单位名称，uestc
				newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

				//读者ID
				newData.sendTemp=newData.sendTemp+(_T("|AA"));
				newData.temp3=newData.readerCardNum;
				newData.sendTemp=newData.sendTemp+newData.temp3;

				//添加图书编号
				newData.sendTemp=newData.sendTemp+(_T("|AB"));
				newData.temp3=newData.bookID;
				newData.sendTemp=newData.sendTemp+newData.temp3;
				  
				//添加书名、作者
				newData.sendTemp=newData.sendTemp+(_T("|AJ"));
				newData.sendTemp=newData.sendTemp+newData.boName+newData.boWriter;

				//到期日期
				newData.sendTemp=newData.sendTemp+(_T("|AH"));
				newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

				showStr=_T("续借失败！图书编号：")+newData.temp3;
			}

			//本地数据库未连接则尝试连接
		/*	if(!m_blinkDatabaseSQL)
			{
				LinkSQL();
			}
			//已连接本地数据库则添加本地消息记录 
			if(m_blinkDatabaseSQL)
			{
				SavetoLocal(2);
			}
       */
			/***************************发送信息****************************/
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 30;
	case	27:         //挂失
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AD",newData.temp);//读者密码
			Get_Sub(newData.recBuffer,"AA",newData.temp2);//读者id
			memcpy(newData.passWord,newData.temp,strlen(newData.temp)+1);
			memcpy(newData.readerCardNum,newData.temp2,strlen(newData.temp2)+1);

			/************************查询数据库*****************************/

			newData.pwCorrect=false;//默认密码不正确
			newData.recordCount=0;//记录集数目设为0
			newData.readerCN=newData.readerCardNum;
			newData.strSQL.Format(_T("select * from readerinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();

			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败271！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}

			if(m_blinkDatabase)
			{
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1
					/***************先读取密码****************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("usercode"));//读取卡密码
					newData.passWd=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					/***************判断密码是否正确****************/
					if(newData.passWord==newData.passWord)//密码正确
					{
						newData.pwCorrect=true;						
					}
					else
					{
						newData.pwCorrect=false;
					}

					/******************查询读者姓名********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("name"));//读取读者姓名
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.patronName=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					/******************查询读者卡状态********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("castate"));//查询读者卡状态
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.caState=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}

			}
			m_pRecordset->Close();//关闭数据库记录集对象指针
			if(0==newData.recordCount)//判断有没有找到该读者
			{
				newData.pidCorrect=false;//没有该读者
			}
			else//找到该读者
			{
				newData.pidCorrect=true;
			}

			/*****************生成返回消息********************/
			newData.sendTemp.Format(_T("28"));  
			if((newData.caState==_T("正常"))&&newData.pwCorrect&&newData.pidCorrect)//可挂失
			{
				newData.sendTemp=newData.sendTemp+(_T("1"));
				showStr=_T("读者卡挂失成功！卡号：")+newData.readerCN;
			}else
			{
				newData.sendTemp=newData.sendTemp+(_T("0"));
				showStr=_T("读者卡挂失失败！卡号：")+newData.readerCN;
			}

			//默认
			newData.sendTemp=newData.sendTemp+(_T("              019"));

			//生成通信时间
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
			newData.temp3=newData.temp;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//单位名称，uestc
			newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

			//读者ID
			newData.sendTemp=newData.sendTemp+(_T("|AA"));
			newData.temp3=newData.readerCardNum;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//读者名称
			newData.sendTemp=newData.sendTemp+(_T("|AE"));
			newData.sendTemp=newData.sendTemp+newData.patronName;

			//读者ID是否有效
			newData.sendTemp=newData.sendTemp+(_T("|BL"));
			if(newData.pidCorrect)
				newData.sendTemp=newData.sendTemp+(_T("Y"));
			else
				newData.sendTemp=newData.sendTemp+(_T("N"));

			//读者密码是否正确
			newData.sendTemp=newData.sendTemp+(_T("|CQ"));
			if(newData.pwCorrect)
				newData.sendTemp=newData.sendTemp+(_T("Y"));
			else
				newData.sendTemp=newData.sendTemp+(_T("N"));

			newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

			/***********************修改数据库信息*************************/

			if((newData.caState==_T("正常"))&&newData.pwCorrect&&newData.pidCorrect)//可挂失
			{
				/************************修改读者卡状态*************************/
				newData.strSQL.Format(_T("select * from readerinfo where bocode = '%s'" ),newData.readerCN);				
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{							
					m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}

				catch(_com_error e)///捕捉异常
				{				
					//CString errorMessage;
					m_blinkDatabase=false;
					showStr.Format(_T("连接数据库失败(挂失)4！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}
				while(!(m_pRecordset->adoEOF))
				{
					/*****************修改读者卡状态*******************/			
					m_pRecordset->PutCollect("castate",_variant_t(_T("挂失")));//修改读者卡状态
					m_pRecordset->Update();//提交记录
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库
			}
			/***************************发送信息****************************/
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 28;	

	case	25:         //解挂
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"AD",newData.temp);//读者密码
			Get_Sub(newData.recBuffer,"AA",newData.temp2);//读者id
			memcpy(newData.passWord,newData.temp,strlen(newData.temp)+1);
			memcpy(newData.readerCardNum,newData.temp2,strlen(newData.temp2)+1);

			/************************查询数据库*****************************/
			newData.pwCorrect=false;//默认密码不正确
			newData.recordCount=0;//记录集数目设为0
			newData.readerCN=newData.readerCardNum;
			newData.strSQL.Format(_T("select * from readerinfo where bocode= '%s'" ),newData.readerCN);
			newData.bSql=newData.strSQL.AllocSysString();

			try
			{
				m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				m_blinkDatabase=true;
			}
			catch(_com_error e)///捕捉异常
			{ 
				//CString errorMessage;
				m_blinkDatabase=false;//连接数据库出错变量赋值为false
				showStr.Format(_T("连接数据库失败251！^错误信息：%s"),e.ErrorMessage());
				::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
				return false;//出现异常，则返回false
			}

			if(m_blinkDatabase)
			{
				while(!(m_pRecordset->adoEOF))
				{
					newData.recordCount++;//记录集数目加1
					/***************先读取密码****************/
					CString PWD;
					newData.vFieldValue=m_pRecordset->GetCollect(_T("usercode"));//读取卡密码
					PWD=(char*)_bstr_t(newData.vFieldValue);					
					newData.vFieldValue.Clear();

					/***************判断密码是否正确****************/
					if(newData.passWord==PWD)//密码正确
					{
						newData.pwCorrect=true;						
					}
					else
					{
						newData.pwCorrect=false;
					}

					/******************查询读者姓名********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("name"));//读取读者姓名
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.patronName=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					/******************查询读者卡状态********************/
					newData.vFieldValue=m_pRecordset->GetCollect(_T("castate"));//查询读者卡状态
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.caState=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();

					m_pRecordset->MoveNext();
				}

			}
			m_pRecordset->Close();//关闭数据库记录集对象指针
			if(0==newData.recordCount)//判断有没有找到该读者
			{
				newData.pidCorrect=false;//没有该读者
			}
			else//找到该读者
			{
				newData.pidCorrect=true;
			}

			/*****************生成返回消息********************/
			newData.sendTemp.Format(_T("26"));  
			if((newData.caState==_T("挂失"))&&newData.pwCorrect&&newData.pidCorrect)//可解挂
			{
				newData.sendTemp=newData.sendTemp+(_T("1"));
				showStr=_T("读者卡解挂成功！卡号：");
			}else
			{
				newData.sendTemp=newData.sendTemp+(_T("0"));
				showStr=_T("读者卡解挂失败！卡号：");
			}
			showStr+=newData.readerCN;

			//默认可续借，无磁盘
			newData.sendTemp=newData.sendTemp+(_T("              019"));

			//生成通信时间
			newData.length=Get_Time(newData.temp,0);//生成时间，将时间存入临时数组中
			newData.temp3=newData.temp;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//单位名称，uestc
			newData.sendTemp=newData.sendTemp+(_T("AOuestc"));

			//读者ID
			newData.sendTemp=newData.sendTemp+(_T("|AA"));
			newData.temp3=newData.readerCardNum;
			newData.sendTemp=newData.sendTemp+newData.temp3;

			//读者名称
			newData.sendTemp=newData.sendTemp+(_T("|AE"));
			newData.sendTemp=newData.sendTemp+newData.patronName;

			//读者ID是否有效
			newData.sendTemp=newData.sendTemp+(_T("|BL"));
			if(newData.pidCorrect)
				newData.sendTemp=newData.sendTemp+(_T("Y"));
			else
				newData.sendTemp=newData.sendTemp+(_T("N"));

			//读者密码是否正确
			newData.sendTemp=newData.sendTemp+(_T("|CQ"));
			if(newData.pwCorrect)
				newData.sendTemp=newData.sendTemp+(_T("Y"));
			else
				newData.sendTemp=newData.sendTemp+(_T("N"));

			newData.sendTemp=newData.sendTemp+(_T("|AY0AZ"));//添加结束符

			/***********************修改数据库信息*************************/

			if((newData.caState==_T("挂失"))&&newData.pwCorrect&&newData.pidCorrect)//可解挂
			{
				/************************修改读者卡状态*************************/
				newData.strSQL.Format(_T("select * from readerinfo where bocode = '%s'" ),newData.readerCN);				
				newData.bSql=newData.strSQL.AllocSysString();

				try
				{							
					m_pRecordset->Open((CComVariant)newData.bSql,m_Connection.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
					m_blinkDatabase=true;
				}

				catch(_com_error e)///捕捉异常
				{				
					CString errorMessage;			
					m_blinkDatabase=false;
					showStr.Format(_T("连接数据库失败(挂失)4！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
					return 0;//出现异常，则返回0
				}
				while(!(m_pRecordset->adoEOF))
				{
					/*****************修改读者卡状态*******************/			
					m_pRecordset->PutCollect("castate",_variant_t(_T("正常")));//修改读者卡状态
					m_pRecordset->Update();//提交记录
					m_pRecordset->MoveNext();
				}
				m_pRecordset->Close();//关闭数据库
			}
			/***************************发送信息****************************/
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.sendTemp,-1,newData.sendBuffer,2048,0,false);
			newData.length=Add_Check(newData.sendBuffer);//将生成的校验码追加到数组最后
			strcat(newData.sendBuffer,"\n");

			int tp=send(*m_pSocket,newData.sendBuffer,strlen(newData.sendBuffer)+1,0);
			if(tp==-1)
			{
				showStr=_T("规则发送失败！");
				return 0;
			}
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 26;
	case 40://客户端出错消息,记录到本地数据库
		{
			WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)newData.recvTemp,-1,newData.recBuffer,2048,0,false);    //该函数映射一个unicode字符串到一个多字节字符串
			Get_Sub(newData.recBuffer,"DT",newData.temp);//错误详情
			newData.temp3=newData.temp;//temp3存储错误信息存
			newData.temp3+=_T("|");

			CTime currenttime;
			CString strSQL;
			//CString temp;//临时变量
			_variant_t vFieldValue;//数据库操作的临时变量
			currenttime=CTime::GetCurrentTime();
			//temp=currenttime.Format(_T(" %Y%m%d"))+addr.Right(3);
		/*	if (m_blinkDatabaseSQL)
			{
				strSQL.Format(_T("select * from ErrorMsg where ID = '%s'" ),addr.Right(3));
				//newData.strSQL.Format(_T("update StaInfo set Checkout=Checkout+1 where ID = '%s'" ),newData.temp3);
				try 
				{
					m_pRecordsetSQL->Open(_bstr_t(strSQL),m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
				}						
				catch(_com_error e)///捕捉异常
				{
					//CString errorMessage;
					showStr.Format(_T("本地数据库错误011！^错误信息：%s"),e.ErrorMessage());
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					return 0;//出现异常，则返回0
				}

				int countemp=0;
				while(!(m_pRecordsetSQL->adoEOF))
				{
					countemp++;
					newData.vFieldValue=m_pRecordsetSQL->GetCollect(_T("ErrorDetail"));
					if(VT_NULL!=newData.vFieldValue.vt)//如果不为空，则取出其值
					{
						newData.temp3+=(char*)_bstr_t(newData.vFieldValue);
					}
					newData.vFieldValue.Clear();
					m_pRecordsetSQL->MoveNext();
				}
				m_pRecordsetSQL->Close();//关闭记录集

				if(countemp!=0)//如果记录为不为空，则更新数据
				{
					strSQL.Format(_T("update ErrorMsg set ErrorFlag='True' where ID = '%s'" ),addr.Right(3));
					try 
					{
						//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
						m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
						//m_blinkDatabaseSQL=true;
					}						
					catch(_com_error e)///捕捉异常
					{
						showStr.Format(_T("本地数据库错误012！^错误信息：%s"),e.ErrorMessage());
						::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
						return 0;//出现异常，则返回0
					}

					strSQL.Format(_T("update ErrorMsg set ErrorDetail='%s' where ID = '%s'" ),newData.temp3,addr.Right(3));
					try 
					{
						//m_pRecordsetSQL->Open((CComVariant)newData.bSql,m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
						m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
						//m_blinkDatabaseSQL=true;
					}						
					catch(_com_error e)///捕捉异常
					{
						showStr.Format(_T("本地数据库错误013！^错误信息：%s"),e.ErrorMessage());
						::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
						return 0;//出现异常，则返回0
					}
					//
				}
				else//记录为空则添加新数据
				{
					strSQL.Format(_T("insert into ErrorMsg(ID,ErrorFlag,ErrorDetail) values ('%s','True','%s')"),addr.Right(3),newData.temp3);	
					try 
					{
						m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
					}						
					catch(_com_error e)///捕捉异常
					{
						showStr.Format(_T("本地数据库错误014！^错误信息：%s"),e.ErrorMessage());
						::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
						return 0;//出现异常，则返回0
					}
				}
			*/
			//}
		//	else
				return 0;
		}
		return 41;
	default :
		{
			showStr=_T("收到不兼容消息！未予以处理。 ");
			::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
			strLast=newData.sendTemp;
		}
		return 1;
	}
	showStr=_T("解析消息失败！");
	::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
	return 2;
}

//执行线程的实现部分DWORD WINAPI FunProc( LPVOID lpParameter  )  // thread data	
UINT TraMsg(LPVOID pParam)  // thread dataHandleSIP2	
{
	//while(1&&thrFlag)
	//{
		/***************************连接数据库************************************/
		if(!m_blinkDatabase)//数据库未连接
		{
			if (!LinkDatabase())//连接数据库
			{
				showStr=(_T("数据库未连接！")); 
				//Sleep(5000);//五秒后再次尝试连接
			}
		}

		CString recvTemp;
		char recBuffer[2048];//接收数组
		char sendBuffer[2048];//发送数组
		char temp[2048];//临时数组

		if (!mylist.IsEmpty())//消息链表不为空，且数据库已连接
		{
			recvTemp=mylist.GetDel();
			WideCharToMultiByte(CP_ACP,0,(LPCTSTR)recvTemp,-1,recBuffer,2048,0,false);  
			Get_Sub2(recBuffer,"DS",temp);//地址
			addr=temp;
			Get_Sub2(recBuffer,"MS",temp);//消息
			recvTemp=temp;
			
			//确定要发送的socket
			sockaddr_in sa;
			int len=sizeof(sa);
			m_pSocket=(LibSerSocket*)mylist.m_connectionList.GetHead();
			POSITION pos = mylist.m_connectionList.GetHeadPosition();
			while(pos!=NULL)
			{
				if(!getpeername(*m_pSocket, (struct sockaddr *)&sa, &len))
				{
					CString IPAddress=(CString)inet_ntoa(sa.sin_addr);
					if(IPAddress==addr)
						break;//找到了就退出循环
				}
				m_pSocket=(LibSerSocket*)mylist.m_connectionList.GetNext(pos);
			}

			if(recLast==recvTemp)
			{
				WideCharToMultiByte(CP_ACP,0,(LPCTSTR)strLast,-1,sendBuffer,2048,0,false);
				int tp=send(*m_pSocket,sendBuffer,strlen(sendBuffer)+1,0);
				if(tp==-1)
				{
					showStr=_T("规则发送失败！");
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
					//break;
				}
			}
			else
				if(Verify(recvTemp)&&m_blinkDatabase)//验证消息正确并且数据库已连接
				{
					recLast=recvTemp;///记录接收的信息
					int traResult=TraSIP2(recvTemp);//解析消息
					showStr+=_T("客户端IP：")+addr;
				}
				else
				{
					showStr=_T("收到非法消息或无法连接数据库，消息未处理未予以处理！消息内容：")+recvTemp+_T("客户端IP：")+addr;
					::PostMessage(hwnd,WM_USERMESSAGE,0,1);//向主线程发送消息
				}
		}
	//}
	return 0;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CLibraryServerDlg 对话框

CLibraryServerDlg::CLibraryServerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CLibraryServerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_ICON1);
}

void CLibraryServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RUN_LIST, m_ListWords);
}

BEGIN_MESSAGE_MAP(CLibraryServerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CLibraryServerDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_CLEAR, &CLibraryServerDlg::OnBnClickedClear)
	ON_BN_CLICKED(IDC_EXPORT, &CLibraryServerDlg::OnBnClickedExport)
	ON_BN_CLICKED(IDC_TESTDB, &CLibraryServerDlg::OnBnClickedTestdb)
	ON_MESSAGE(WM_USERMESSAGE,&CLibraryServerDlg::OnMsg)//自定义消息映射
	ON_MESSAGE(WM_USER_NOTIFYICON,OnNotifyMsg) //最小化到托盘消息映射
END_MESSAGE_MAP()


// CLibraryServerDlg 消息处理程序
BOOL CLibraryServerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	//ShowWindow(SW_MINIMIZE);

	// TODO: 在此添加额外的初始化代码
	hwnd=GetSafeHwnd();//获得线程句柄
	if (m_srvrSocket.m_hSocket==INVALID_SOCKET)
	{
		BOOL bFlag=m_srvrSocket.Create(7000,SOCK_STREAM,FD_ACCEPT);//创建监听套接字，激发FD_ACCEPT事件，默认端口7000
		if (!bFlag)//创建失败
		{
			AfxMessageBox(_T("Socket 创建失败！"));
			m_srvrSocket.Close();//关闭监听套接字
			PostQuitMessage(0);//退出窗口
			return 0;
		}
	}
	int tmp=m_srvrSocket.SetSockOpt(SO_OOBINLINE,NULL,sizeof(SO_OOBINLINE),0);

	//"侦听"成功，等待连接请求
	if (!m_srvrSocket.Listen(5))		//如果监听失败
	{
		int nErrorCode = m_srvrSocket.GetLastError();//检测错误信息
		if (nErrorCode!=WSAEWOULDBLOCK)//如果不是线程被阻塞
		{
			showStr=(_T("Socket错误!"));
			m_srvrSocket.Close();//关闭套接字
			PostQuitMessage(0);//关闭窗口
			return 0;
		}
	}

	// 初始化数据库,调用数据库类，建立数据库和数据库表文件
	BOOL CMyAdoTestApp：：InitInstance();
	{
		if(!AfxOleInit())//初始化COM库
		{
			showStr=(_T("OLE初始化出错!"));
			return FALSE;
		}
	}

	if (!LinkDatabase())//连接数据库
	{
		showStr=(_T("连接数据库失败！")); 
		//Sleep(5000);//五秒后再次连接
	}

/*	if (!LinkSQL())//连接数据库
	{
		showStr=(_T("连接本地数据库失败！程序部分功能将无效。")); 
		//Sleep(5000);//五秒后再次连接
	}
*/
	if (showStr==_T(""))
	{
		showStr=(_T("程序初始化完成，开始监听..."));
		::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
	}
	else
	{
		::PostMessage(hwnd,WM_USERMESSAGE,0,1);//提示消息
	}

	//托盘区
	m_tnid.cbSize=sizeof NOTIFYICONDATA;  
	m_tnid.hWnd=this->m_hWnd;    
	m_tnid.uID=IDI_ICON1;  
	m_tnid.hIcon=LoadIcon(AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_ICON1)); 
	MultiByteToWideChar(CP_ACP,0,(" LibraryServer 正在运行..."),-1,m_tnid.szTip,256);
	m_tnid.uCallbackMessage=WM_USER_NOTIFYICON;  
	m_tnid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP; 
	Shell_NotifyIcon(NIM_ADD,&m_tnid);

	//开一个线程，用于处理接收到的命令。
	/*CWinThread* pThread;
	pThread=AfxBeginThread(TraMsg,this);*/
	//HANDLE hThread;
	//hThread=CreateThread(NULL,0,FunProc,NULL,0,NULL);
	//CloseHandle(hThread);
	//this->ShowWindow(TRUE);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CLibraryServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CLibraryServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CLibraryServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

bool EncryptAndDecrypt(CString strIn,CString &strOut,bool EnOrDe)//返回值为加密或者解密后的数据，类型为CString
{

	CString strTemp;
	int i;
	unsigned char key[]={'u','e','s','t','c','k','b','1','1','2','s','h','e','n','k','e'};//加密密钥（解密密钥）
	unsigned char inBuff[17],outBuff[17];//加密/解密转换用数组
	char tempBuff[33];//加密/解密转换用临时数组
	//对所用数组进行初始化
	memset(inBuff,0,sizeof(inBuff));
	memset(outBuff,0,sizeof(outBuff));
	memset(tempBuff,0,sizeof(tempBuff));
	Aes aes(sizeof(key),key);

	switch(EnOrDe)
	{

	case true:
		//以下为加密部分		
		//将密码转换成字符串形式
		if(strIn.GetLength()>16)
			return false;
		WideCharToMultiByte(CP_ACP,NULL,strIn,strIn.GetLength(),tempBuff,strIn.GetLength(),NULL,NULL);//将宽字节转化为多字节来存储
		memcpy(inBuff,tempBuff,strlen(tempBuff));//将char型临时数组中的数据存入unsigned char数组（其中有一个隐形类型转换），以便转换		
		aes.Cipher(inBuff,outBuff); //加密
		strOut.Empty();//现将原先有的内容清空
		i=0;
		while ('\0'!=outBuff[i])//将加密后的内容逐一存入返回的CString 类型中
		{
			strTemp.Format(_T("%.2x"),outBuff[i]);
			strOut=strOut+strTemp;
			i++;
		}
		break;
	case false:		
		WideCharToMultiByte(CP_ACP,NULL,strIn,strIn.GetLength(),tempBuff,strIn.GetLength(),NULL,NULL);//将宽字节转化为多字节存储		
		for(i=0;i<16;i++)
		{
			inBuff[i]=HexToUChar(tempBuff[2*i])*16+HexToUChar(tempBuff[2*i+1]);
		}
		aes.InvCipher(inBuff,outBuff); 
		memcpy(tempBuff,outBuff,sizeof(outBuff));
		MultiByteToWideChar(CP_ACP,NULL,tempBuff,16,strOut.GetBuffer(16),16);
		strOut.ReleaseBuffer();
		break;
	}
	return true;
}

unsigned char HexToUChar(unsigned char ch )
{
	if(ch>='0'&&ch<='9')
	{
		return ch-'0';
	}
	if(ch>='a'&&ch<='f')
	{
		return ch-'a'+10;
	}
	if(ch>='A'&&ch<='F')
	{
		return ch-'A'+10;
	}
	return 0;
}

void CLibraryServerDlg::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	OnOK();
}


void CLibraryServerDlg::OnBnClickedClear()
{
	// TODO: 在此添加控件通知处理程序代码
	m_ListWords.ResetContent();
}

void CLibraryServerDlg::OnBnClickedExport()
{
	// TODO: 在此添加控件通知处理程序代码
	CString str;char str3[256];
	for(int i=0; i<m_ListWords.GetCount(); i++)
	{
		m_ListWords.GetText(i, str);
		str += _T("\r\n");		
		memset(str3,0,256);
		WideCharToMultiByte(CP_UTF8,0,(LPCTSTR)str,-1,str3,256,0,false);
		CFile file; 
		CFileException fileException;
		file.Close(); 
		//if(file.Open(_T("d:\\RunLog.txt"),CFile::modeWrite),&fileException)
		if(file.Open(_T("d:\\RunLog.txt"),CFile::modeReadWrite|CFile::modeCreate|CFile::modeNoTruncate),&fileException)
		{ 
			file.SeekToEnd(); 
			file.Write(str3, strlen(str3));
		}else
		{
			TRACE("Can't open file %s,error=%u\n","d:\\RunLog.txt",fileException.m_cause);
		}		
	}
	AfxMessageBox(_T("文件导出成功，位置：D:\\RunLog.txt"));
}

void CLibraryServerDlg::OnBnClickedTestdb()
{
	// TODO: 在此添加控件通知处理程序代码
	CString showmsg;
	if (m_Connection->State)
	{
		showmsg=(_T("图书馆数据库已连接"));
	}
	else
	{
		if (LinkDatabase())
		{
			showmsg=(_T("图书馆数据库连接成功！"));
		}
		else
		{
			showmsg=(_T("图书馆数据库连接失败！"));
		}
	}
/*	if (m_ConnectionSQL->State)
	{
		showmsg+=(_T(" 消息统计数据库已连接"));
	}
	else
	{
	/*	if (LinkSQL())
		{
			showmsg+=(_T(" 消息统计数据库连接成功！"));
		}
		else
		{
			showmsg+=(_T(" 消息统计数据库连接失败！"));
		}
*/
//	}
	AfxMessageBox(showmsg);
}

LRESULT CLibraryServerDlg::OnMsg(WPARAM wparam,LPARAM lparam) 
{
	CTime currenttime;
	CString temp3;
	CString msgtemp;
	CString strSQL;
	_variant_t vFieldValue;//数据库操作的临时变量
	currenttime=CTime::GetCurrentTime();
	temp3=currenttime.Format(_T(" %Y/%m/%d %H:%M:%S"));
	m_ListWords.AddString(temp3);//显示时间
	m_ListWords.AddString(showStr);//显示信息
	m_ListWords.AddString(_T("-------------------------------------------------------------------------------------------------------------------"));
	m_ListWords.SetTopIndex(m_ListWords.GetCount()-1);
  /*  if (m_blinkDatabaseSQL)
	{
		strSQL.Format(_T("select * from RunLog where Time = '%s'" ),temp3);
		//newData.strSQL.Format(_T("update StaInfo set Checkout=Checkout+1 where ID = '%s'" ),newData.temp3);
		try 
		{
			m_pRecordsetSQL->Open(_bstr_t(strSQL),m_ConnectionSQL.GetInterfacePtr(),adOpenDynamic,adLockOptimistic,adCmdText);//查询数据库
		}						
		catch(_com_error e)///捕捉异常
		{
			//CString errorMessage;
			showStr.Format(_T("本地数据库错误016！^错误信息：%s"),e.ErrorMessage());
			m_blinkDatabaseSQL=false;
			m_ListWords.AddString(showStr);//显示信息
			m_ListWords.AddString(_T("-------------------------------------------------------------------------------------------------------------------"));
			m_ListWords.SetTopIndex(m_ListWords.GetCount()-1);
			return 0;//出现异常，则返回0
		}

		int countemp=0;
		while(!(m_pRecordsetSQL->adoEOF))
		{
			countemp++;
			vFieldValue=m_pRecordsetSQL->GetCollect(_T("Detail"));//之前消息
			if(VT_NULL!=vFieldValue.vt)//如果不为空，则取出其值
			{
				msgtemp=(char*)_bstr_t(vFieldValue);	
			}
			vFieldValue.Clear();
			showStr+=msgtemp;
			m_pRecordsetSQL->MoveNext();
		}
		m_pRecordsetSQL->Close();//关闭记录集

		if (countemp==0)//记录为空
		{
			strSQL.Format(_T("insert into RunLog(Time,Detail) values ('%s','%s')"),temp3,showStr);	
			try
			{
				m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);//查询数据库，有问题m_pRecordset=
				m_blinkDatabaseSQL=true;
			}						
			catch(_com_error e)///捕捉异常
			{
				//CString errorMessage;
				showStr.Format(_T("本地数据库错误017！^错误信息：%s"),e.ErrorMessage());
				m_blinkDatabaseSQL=false;
				m_ListWords.AddString(showStr);//显示信息
				m_ListWords.AddString(_T("-------------------------------------------------------------------------------------------------------------------"));
				m_ListWords.SetTopIndex(m_ListWords.GetCount()-1);
				return 0;//出现异常，则返回0
			}
		}
		else//记录不为空
		{
			strSQL.Format(_T("update RunLog set Detail='%s' where Time = '%s'"),showStr,temp3);	
			try
			{
				m_ConnectionSQL->Execute(_bstr_t(strSQL),&(vFieldValue),adCmdText);
				m_blinkDatabaseSQL=true;
			}						
			catch(_com_error e)///捕捉异常
			{
				//CString errorMessage;
				showStr.Format(_T("本地数据库错误015！^错误信息：%s"),e.ErrorMessage());
				m_blinkDatabaseSQL=false;
				m_ListWords.AddString(showStr);//显示信息
				m_ListWords.AddString(_T("-------------------------------------------------------------------------------------------------------------------"));
				m_ListWords.SetTopIndex(m_ListWords.GetCount()-1);
				return 0;//出现异常，则返回0
			}
		}
	 */
	//}	
	return 1;
}

void CLibraryServerDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类
	INT_PTR Res;
	Res=MessageBox(_T("您确定要退出吗？退出后将使得所有客户端不能工作！"),_T("确定退出"),MB_OKCANCEL|MB_ICONQUESTION);
	if(IDCANCEL==Res)
		return;
	else
	{
		thrFlag=false;
		::Shell_NotifyIcon(NIM_DELETE,&m_tnid); //关闭时删除系统托盘图标  
		Sleep(100);
		CDialog::OnOK();
	}
}

LRESULT  CLibraryServerDlg::OnNotifyMsg(WPARAM wparam,LPARAM lparam)  
	//wParam接收的是图标的ID，而lParam接收的是鼠标的行为 
{  
	if(wparam!=IDI_ICON1)     
		return    1;     
	switch(lparam)     
	{     
	case  WM_RBUTTONUP://右键起来时弹出快捷菜单，这里只有一个“关闭”     
		{       
			LPPOINT    lpoint=new    tagPOINT;     
			::GetCursorPos(lpoint);//得到鼠标位置     
			CMenu    menu;     
			menu.CreatePopupMenu();//声明一个弹出式菜单     
			//增加菜单项“关闭”，点击则发送消息WM_DESTROY给主窗口（已     
			//隐藏），将程序结束。     
			menu.AppendMenu(MF_STRING,WM_DESTROY,_T("关闭"));       
			//确定弹出式菜单的位置     
			menu.TrackPopupMenu(TPM_LEFTALIGN,lpoint->x,lpoint->y,this);     
			//资源回收     
			HMENU    hmenu=menu.Detach();     
			menu.DestroyMenu();     
			delete    lpoint;     
		}     
		break;     
	case    WM_LBUTTONDBLCLK://双击左键的处理     
		{     
			this->ShowWindow(SW_SHOW);//简单的显示主窗口完事儿     
		}     
		break;     
	}      
	return 0;  
} 

LRESULT CLibraryServerDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO: 在此添加专用代码和/或调用基类
	switch(message) //判断消息类型  
	{   
	case WM_USER_NOTIFYICON:   
		//如果是用户定义的消息   
		if(lParam==WM_LBUTTONDBLCLK)  
		{   
			//鼠标双击时主窗口出现   
			if(AfxGetApp()->m_pMainWnd->IsWindowVisible()) //判断窗口当前状态  
			{  
				AfxGetApp()->m_pMainWnd->ShowWindow(SW_HIDE); //隐藏窗口  
			}  
			else  
			{  
				AfxGetApp()->m_pMainWnd->ShowWindow(SW_SHOW); //显示窗口  
			}  
		}   
		//else if(lParam==WM_RBUTTONDOWN)  
		//{ //鼠标右键单击弹出选单   
		//	CMenu menu;   
		//	//menu.LoadMenu(IDR_MENU1); //载入事先定义的选单   
		//	CMenu *pMenu=menu.GetSubMenu(0);   
		//	CPoint pos;   
		//	GetCursorPos(&pos);   
		//	pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON,pos.x,pos.y,AfxGetMainWnd());   
		//}   
		break;   
	case WM_SYSCOMMAND:   
		//如果是系统消息   
		if(wParam==SC_MINIMIZE)  
		{   
			//接收到最小化消息时主窗口隐藏   
			AfxGetApp()->m_pMainWnd->ShowWindow(SW_HIDE);   
			return 0;   
		}   
		if(wParam==SC_CLOSE)  
		{  
			OnOK();
			return 0;
			//::Shell_NotifyIcon(NIM_DELETE,&m_tnid); //关闭时删除系统托盘图标  
		}  
		break;  
	}
	return CDialog::WindowProc(message, wParam, lParam);
}


void CLibraryServerDlg::OnCancel()
{
	// TODO: 在此添加专用代码和/或调用基类
	OnOK();
}
