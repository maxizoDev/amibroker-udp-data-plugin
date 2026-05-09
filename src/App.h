// CWinApp instance for the plug-in DLL.
// Required so MFC has a hosting application object and so we can use
// AfxGetApp()->GetProfileInt / WriteProfileInt for settings persistence
// (the same idiom used by Data_Template, QT and ODBC samples).

#pragma once

#ifndef __AFXWIN_H__
    #error include 'StdAfx.h' before including this file for PCH
#endif

#include "resource.h"

class CPluginApp : public CWinApp
{
public:
    CPluginApp();

    BOOL InitInstance() override;

    DECLARE_MESSAGE_MAP()
};
