#include "StdAfx.h"
#include "ConfigDlg.h"

#include "Logger.h"
#include "Settings.h"

CConfigDlg::CConfigDlg(CWnd* pParent)
    : CDialog(CConfigDlg::IDD, pParent)
{
}

void CConfigDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Text   (pDX, IDC_BIND_IP_EDIT,           g_oSettings.BindIp);
    DDX_Text   (pDX, IDC_PORT_EDIT,              g_oSettings.UdpPort);
    DDV_MinMaxInt(pDX,                           g_oSettings.UdpPort, 1, 65535);
    DDX_Text   (pDX, IDC_DEALER_ID_EDIT,         g_oSettings.DealerId);
    DDX_Text   (pDX, IDC_BACKFILL_URL_EDIT,      g_oSettings.BackfillUrl);
    DDX_Text   (pDX, IDC_REFRESH_INTERVAL_EDIT,  g_oSettings.RefreshIntervalMs);
    DDV_MinMaxInt(pDX,                           g_oSettings.RefreshIntervalMs, 50, 60000);
    DDX_Text   (pDX, IDC_SYMBOL_LIMIT_EDIT,      g_oSettings.SymbolLimit);
    DDV_MinMaxInt(pDX,                           g_oSettings.SymbolLimit, 1, 5000);
    DDX_CBIndex(pDX, IDC_LOG_LEVEL_COMBO,        g_oSettings.LogLevel);
    DDX_Check  (pDX, IDC_AUTOADD_CHECK,          g_oSettings.AutoAddSymbols);
}

BEGIN_MESSAGE_MAP(CConfigDlg, CDialog)
END_MESSAGE_MAP()

BOOL CConfigDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    if (CComboBox* combo =
            static_cast<CComboBox*>(GetDlgItem(IDC_LOG_LEVEL_COMBO)))
    {
        combo->ResetContent();
        combo->AddString(_T("trace"));
        combo->AddString(_T("debug"));
        combo->AddString(_T("info"));
        combo->AddString(_T("warn"));
        combo->AddString(_T("error"));
    }

    UpdateData(FALSE);
    return TRUE;
}

void CConfigDlg::OnOK()
{
    CDialog::OnOK();
    g_oSettings.SaveToRegistry();
    logger::SetLevel(g_oSettings.LogLevel);
    UDP_LOG_INFO("Configuration saved (port=%d limit=%d refresh=%dms)",
                 g_oSettings.UdpPort, g_oSettings.SymbolLimit,
                 g_oSettings.RefreshIntervalMs);
}
