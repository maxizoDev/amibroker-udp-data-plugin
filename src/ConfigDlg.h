// MFC dialog shown when the user clicks "Configure" in
// File -> Database Settings. Mirrors QT/ConfigDlg.h but bound to our
// Settings struct (see Settings.h). DDX_/DDV_ macros marshal values
// directly into the global g_oSettings, which is then persisted via
// AfxGetApp()->WriteProfileXxx in OnOK.

#pragma once

#ifndef __AFXWIN_H__
    #error include 'StdAfx.h' before including this file for PCH
#endif

#include "resource.h"

class CConfigDlg : public CDialog
{
public:
    CConfigDlg(CWnd* pParent = nullptr);

    enum { IDD = IDD_CONFIG_DIALOG };

    struct InfoSite* m_pSite{nullptr};

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    DECLARE_MESSAGE_MAP()
};
