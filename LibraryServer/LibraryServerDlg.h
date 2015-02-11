
// LibraryServerDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "LibSerSocket.h"
#define WM_USERMESSAGE WM_USER+100
#define WM_USER_NOTIFYICON WM_USER+101
// CLibraryServerDlg �Ի���
UINT TraMsg(LPVOID pParam);
class CLibraryServerDlg : public CDialog
{
// ����
public:
	CLibraryServerDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_LIBRARYSERVER_DIALOG };
public:
//����
	LibSerSocket m_srvrSocket;//�����׽���
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
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
	afx_msg LRESULT OnMsg(WPARAM wparam,LPARAM lparam);//�û��Զ�����Ϣ
	afx_msg LRESULT OnNotifyMsg(WPARAM wparam,LPARAM lparam); //��С����������Ϣ
	virtual void OnOK();
private:  
	NOTIFYICONDATA m_tnid;//
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	virtual void OnCancel();
};
