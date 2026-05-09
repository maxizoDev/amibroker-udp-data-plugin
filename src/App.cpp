#include "StdAfx.h"
#include "App.h"

BEGIN_MESSAGE_MAP(CPluginApp, CWinApp)
END_MESSAGE_MAP()

CPluginApp::CPluginApp()
{
}

BOOL CPluginApp::InitInstance()
{
    // SetRegistryKey routes AfxGetApp()->GetProfileInt / WriteProfileInt
    // to HKCU\Software\<key>\<plugin-name>\... — same approach as the QT
    // sample, which uses "TJP". Using a distinct vendor key here so our
    // settings don't collide with AmiBroker's own RT plug-ins.
    SetRegistryKey(_T("AmibrokerUDPData"));
    return CWinApp::InitInstance();
}

CPluginApp theApp;
