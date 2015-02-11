
// LibraryServerDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "LibSerSocket.h"
#define WM_USERMESSAGE WM_USER+100
#define WM_USER_NOTIFYICON WM_USER+101
// CLibraryServerDlg 对话框
UINT TraMsg(LPVOID pParam);
class CLibraryServerDlg : public CDialog
{
// 构造
public:
	CLibraryServerDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_LIBRARYSERVER_DIALOG };
public:
//变量
	LibSerSocket m_srvrSocket;//监听套接字
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CListBox m_ListWords;
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedClear();
	afx_msg void OnBnClickedExport();
	afx_msg void OnBnClickedTestdb();
	afx_msg LRESULT OnMsg(WPARAM wparam,LPARAM lparam);//用户自定义消息
	afx_msg LRESULT OnNotifyMsg(WPARAM wparam,LPARAM lparam); //最小化到托盘消息
	virtual void OnOK();
private:  
	NOTIFYICONDATA m_tnid;//
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	virtual void OnCancel();
};
