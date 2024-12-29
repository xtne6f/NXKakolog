#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <algorithm>
#include <mutex>

using std::min;
using std::max;

typedef std::recursive_mutex recursive_mutex_;
typedef std::lock_guard<recursive_mutex_> lock_recursive_mutex;

static const LONGLONG FILETIME_MILLISECOND = 10000LL;

// TvtPlayから他プラグインに情報提供するメッセージ(From: TvtPlay.cpp)
static const int TVTP_REQUIRED_MSGVER = 1;
static const int TVTP_TOT_UNIX_MSGVER = 2;
static const UINT WM_TVTP_GET_MSGVER = WM_APP + 50;
static const UINT WM_TVTP_GET_POSITION = WM_APP + 52;
static const UINT WM_TVTP_GET_DURATION = WM_APP + 53;
static const UINT WM_TVTP_GET_TOT_TIME = WM_APP + 54;
static const UINT WM_TVTP_GET_TOT_UNIX = WM_APP + 62;

// NicoJKから他プラグインに情報提供するメッセージ(From: NicoJK.cpp)
static const int NICOJK_CURRENT_MSGVER = 1;
static const UINT WM_NICOJK_GET_MSGVER = WM_APP + 50;
static const UINT WM_NICOJK_GET_JKID_TO_GET = WM_APP + 52;
static const UINT WM_NICOJK_OPEN_LOGFILE = WM_APP + 53;
static const int NICOJK_OPEN_FLAG_XML = 0x04000000;
static const int NICOJK_OPEN_FLAG_RELATIVE = 0x10000000;
static const int NICOJK_OPEN_FLAG_ABSOLUTE = 0x20000000;

DWORD GetLongModuleFileName(HMODULE hModule, LPTSTR lpFileName, DWORD nSize);
LONGLONG UnixTimeToFileTime(unsigned int tm);
unsigned int FileTimeToUnixTime(LONGLONG ll);
LONGLONG AribToFileTime(const BYTE *pData);
HWND FindTvtPlayFrame();
HWND FindNicoJKWindow();
