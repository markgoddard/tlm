// TLMGUI.h : main header file for the TLMGUI application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CTLMGUIApp:
// See TLMGUI.cpp for the implementation of this class
//

class CTLMGUIApp : public CWinApp
{
public:
	CTLMGUIApp();


// Overrides
public:
	virtual BOOL InitInstance();

// Implementation
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CTLMGUIApp theApp;