#include "Util.hpp"

DWORD GetLongModuleFileName(HMODULE hModule, LPTSTR lpFileName, DWORD nSize)
{
    TCHAR longOrShortName[MAX_PATH];
    DWORD nRet = GetModuleFileName(hModule, longOrShortName, MAX_PATH);
    if (nRet && nRet < MAX_PATH) {
        nRet = GetLongPathName(longOrShortName, lpFileName, nSize);
        if (nRet < nSize) return nRet;
    }
    return 0;
}

unsigned int FileTimeToUnixTime(LONGLONG ll)
{
    return static_cast<unsigned int>((ll - 116444736000000000) / 10000000);
}

LONGLONG AribToFileTime(const BYTE *pData)
{
    if (pData[0] == 0xFF && pData[1] == 0xFF && pData[2] == 0xFF && pData[3] == 0xFF && pData[4] == 0xFF) {
        // 不指定
        return -1;
    }
    // 1858-11-17
    LONGLONG llft = 81377568000000000;
    // MJD形式の日付
    llft += (pData[0] << 8 | pData[1]) * FILETIME_MILLISECOND * 86400000;
    // BCD形式の時刻
    llft += ((pData[2] >> 4) * 10 + (pData[2] & 0x0F)) * FILETIME_MILLISECOND * 3600000;
    llft += ((pData[3] >> 4) * 10 + (pData[3] & 0x0F)) * FILETIME_MILLISECOND * 60000;
    llft += ((pData[4] >> 4) * 10 + (pData[4] & 0x0F)) * FILETIME_MILLISECOND * 1000;
    return llft;
}

#if 1 // TvtPlayのウィンドウを探す関数
namespace
{
BOOL CALLBACK FindTvtPlayFrameEnumProc(HWND hwnd, LPARAM lParam)
{
    TCHAR className[32];
    if (GetClassName(hwnd, className, _countof(className)) && !_tcscmp(className, TEXT("TvtPlay Frame"))) {
        *reinterpret_cast<HWND*>(lParam) = hwnd;
        return FALSE;
    }
    return TRUE;
}
}

HWND FindTvtPlayFrame()
{
    HWND hwnd = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), FindTvtPlayFrameEnumProc, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}
#endif

#if 1 // NicoJKのウィンドウを探す関数
namespace
{
BOOL CALLBACK FindNicoJKEnumProc(HWND hwnd, LPARAM lParam)
{
    TCHAR className[32];
    if (GetClassName(hwnd, className, _countof(className)) && !_tcscmp(className, TEXT("ru.jk.force"))) {
        *reinterpret_cast<HWND*>(lParam) = hwnd;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK FindNicoJKThreadTopEnumProc(HWND hwnd, LPARAM lParam)
{
    if (FindNicoJKEnumProc(hwnd, lParam)) {
        EnumChildWindows(hwnd, FindNicoJKEnumProc, lParam);
    }
    return *reinterpret_cast<HWND*>(lParam) == nullptr;
}
}

HWND FindNicoJKWindow()
{
    HWND hwnd = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), FindNicoJKThreadTopEnumProc, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}
#endif
