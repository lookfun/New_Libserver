
// LibraryServer.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CLibraryServerApp:
// �йش����ʵ�֣������ LibraryServer.cpp
//

class CLibraryServerApp : public CWinApp
{
public:
	CLibraryServerApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CLibraryServerApp theApp;