// Pre-compiled header for the AmiBroker UDP data plug-in.
// Mirrors the QT / Data_Template / ODBC samples: MFC core + extensions
// + WinINet (used for the HTTP backfill transport via WinHTTP wrappers).

#pragma once

#define VC_EXTRALEAN

#define _WIN32_WINNT 0x0601   // Win7+: WSAPoll, GetTickCount64, modern WinHTTP
#define WINVER       0x0601

#include <afxwin.h>           // MFC core and standard components
#include <afxext.h>           // MFC extensions (CDialog etc.)
#include <afxdtctl.h>
#include <afxcmn.h>
#include <afxinet.h>          // CInternetSession (legacy compatibility)
#include <afxtempl.h>         // CArray<>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
