#pragma once

#include "Util.hpp"
#include "JKStream.hpp"
#define TVTEST_PLUGIN_CLASS_IMPLEMENT
#define TVTEST_PLUGIN_VERSION TVTEST_PLUGIN_VERSION_(0, 0, 14)
#include "TVTestPlugin.h"

// プラグインクラス
class CNXKakolog : public TVTest::CTVTestPlugin
{
public:
    CNXKakolog();
    bool GetPluginInfo(TVTest::PluginInfo *pInfo);
    bool Initialize();
    bool Finalize();
private:
    bool EnablePlugin();
    bool DisablePlugin();
    bool CheckPlaying();
    void OnInitDialog();
    void OnDownload();
    static bool OpenXmlLog(const char *log, int openFlag);
    static INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK TVTestMainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, void *pClientData);
    INT_PTR ProcessMainDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2, void *pClientData);
    static BOOL CALLBACK WindowMsgCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult, void *pUserData);
    static BOOL CALLBACK StreamCallback(BYTE *pData, void *pClientData);

    HWND m_hDlg;
    bool m_fCheckSetStreamCallback;
    int m_openFlag;
    CJKStream m_getStream;
    std::vector<char> m_getBuf;
    recursive_mutex_ m_totLock;
    LONGLONG m_totTime;
};
