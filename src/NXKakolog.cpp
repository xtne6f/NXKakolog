#include "NXKakolog.hpp"
#include "JKIDNameTable.hpp"
#include "resource.h"
#include <shellapi.h>

namespace
{
const UINT WM_APP_STREAM = WM_APP;
const UINT WM_APP_SET_ZORDER = WM_APP + 1;

const TCHAR INFO_PLUGIN_NAME[] = TEXT("NXKakolog");
const TCHAR INFO_DESCRIPTION[] = TEXT("過去ログAPIから実況のログを取得 (ver.1.2)");

enum {
    TIMER_ID_CHECK_PLAYING,
    TIMER_ID_DONE_POSCHANGE,
};

enum {
    COMMAND_DOWNLOAD,
};
}

CNXKakolog::CNXKakolog()
    : m_hDlg(nullptr)
    , m_fCheckSetStreamCallback(false)
    , m_openFlag(0)
    , m_totTime(-1)
{
}

bool CNXKakolog::GetPluginInfo(TVTest::PluginInfo *pInfo)
{
    // プラグインの情報を返す
    pInfo->Type = TVTest::PLUGIN_TYPE_NORMAL;
    pInfo->Flags = TVTest::PLUGIN_FLAG_DISABLEONSTART;
    pInfo->pszPluginName = INFO_PLUGIN_NAME;
    pInfo->pszCopyright = L"Copyright (c) 2024 xtne6f";
    pInfo->pszDescription = INFO_DESCRIPTION;
    return true;
}

bool CNXKakolog::Initialize()
{
    int dpi = m_pApp->GetDPIFromWindow(m_pApp->GetAppWindow());
    int iconWidth = 16 * dpi / 96;
    int iconHeight = 16 * dpi / 96;
    m_pApp->GetStyleValuePixels(L"side-bar.item.icon.width", dpi, &iconWidth);
    m_pApp->GetStyleValuePixels(L"side-bar.item.icon.height", dpi, &iconHeight);
    bool bSmallIcon = iconWidth <= 16 && iconHeight <= 16;
    // アイコンを登録
    m_pApp->RegisterPluginIconFromResource(g_hinstDLL, MAKEINTRESOURCE(bSmallIcon ? IDB_ICON16 : IDB_ICON));

    // コマンドを登録
    TVTest::PluginCommandInfo ci;
    ci.Size = sizeof(ci);
    ci.Flags = TVTest::PLUGIN_COMMAND_FLAG_ICONIZE;
    ci.State = 0;
    ci.ID = COMMAND_DOWNLOAD;
    ci.pszText = L"Download";
    ci.pszDescription = ci.pszName = L"プラグイン有効→取得";
    ci.hbmIcon = static_cast<HBITMAP>(LoadImage(g_hinstDLL, MAKEINTRESOURCE(bSmallIcon ? IDB_DL16 : IDB_DL), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (!m_pApp->RegisterPluginCommand(&ci)) {
        m_pApp->RegisterCommand(ci.ID, ci.pszText, ci.pszName);
    }
    DeleteObject(ci.hbmIcon);

    // イベントコールバック関数を登録
    m_pApp->SetEventCallback(EventCallback, this);
    return true;
}

bool CNXKakolog::Finalize()
{
    // 終了処理
    DisablePlugin();
    return true;
}

bool CNXKakolog::EnablePlugin()
{
    if (!m_hDlg) {
        if (m_pApp->QueryMessage(TVTest::MESSAGE_SHOWDIALOG)) {
            TVTest::ShowDialogInfo info;
            info.Flags = TVTest::SHOW_DIALOG_FLAG_MODELESS;
            info.hinst = g_hinstDLL;
            info.pszTemplate = MAKEINTRESOURCE(IDD_MAIN);
            info.pMessageFunc = TVTestMainDlgProc;
            info.pClientData = this;
            info.hwndOwner = nullptr;
            m_pApp->ShowDialog(&info);
        }
        else {
            CreateDialogParam(g_hinstDLL, MAKEINTRESOURCE(IDD_MAIN), nullptr, MainDlgProc, reinterpret_cast<LPARAM>(this));
        }
    }
    return !!m_hDlg;
}

bool CNXKakolog::DisablePlugin()
{
    if (m_hDlg) {
        DestroyWindow(m_hDlg);
    }
    return true;
}

bool CNXKakolog::CheckPlaying()
{
    int jkIndex = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_JKID, CB_GETCURSEL, 0, 0));
    if (jkIndex < 0) {
        // NicoJKの現在の実況IDを調べる
        HWND hwndJK = FindNicoJKWindow();
        if (hwndJK && SendMessage(hwndJK, WM_NICOJK_GET_MSGVER, 0, 0) >= NICOJK_CURRENT_MSGVER) {
            int jkID = static_cast<int>(SendMessage(hwndJK, WM_NICOJK_GET_JKID_TO_GET, 0, 0));
            for (size_t i = 0; i < _countof(DEFAULT_JKID_NAME_TABLE); ++i) {
                if (DEFAULT_JKID_NAME_TABLE[i].jkID == jkID) {
                    jkIndex = static_cast<int>(i);
                    SendDlgItemMessage(m_hDlg, IDC_COMBO_JKID, CB_SETCURSEL, jkIndex, 0);
                    break;
                }
            }
        }
    }

    int year = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_YEAR, CB_GETCURSEL, 0, 0));
    int month = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MONTH, CB_GETCURSEL, 0, 0));
    int day = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_DAY, CB_GETCURSEL, 0, 0));
    int hour = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR, CB_GETCURSEL, 0, 0));
    int minute = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE, CB_GETCURSEL, 0, 0));
    int hourSpan = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR_SPAN, CB_GETCURSEL, 0, 0));
    int minuteSpan = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE_SPAN, CB_GETCURSEL, 0, 0));
    if (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0 || hourSpan < 0 || minuteSpan < 0) {
        // TvtPlayが現在開いているファイルの時刻や長さを調べる
        HWND hwndTvtp = FindTvtPlayFrame();
        int ver;
        if (hwndTvtp && (ver = static_cast<int>(SendMessage(hwndTvtp, WM_TVTP_GET_MSGVER, 0, 0))) >= TVTP_REQUIRED_MSGVER) {
            int totTime = -1;
            LONGLONG llft = -1;
            if (ver >= TVTP_TOT_UNIX_MSGVER) {
                // TOTの日付まで得られる
                unsigned int totUnixTime = static_cast<unsigned int>(SendMessage(hwndTvtp, WM_TVTP_GET_TOT_UNIX, 0, 0));
                if (totUnixTime) {
                    totUnixTime += 9 * 3600;
                    totTime = totUnixTime % (24 * 3600) * 1000;
                    if (year < 0 || month < 0 || day < 0) {
                        llft = UnixTimeToFileTime(totUnixTime);
                    }
                }
            }
            else {
                // ストリームコールバックを登録
                if (!m_fCheckSetStreamCallback) {
                    m_pApp->SetStreamCallback(0, StreamCallback, this);
                }
                totTime = static_cast<int>(SendMessage(hwndTvtp, WM_TVTP_GET_TOT_TIME, 0, 0));
                if (totTime >= 0 && (year < 0 || month < 0 || day < 0)) {
                    // TvtPlayから得られるのはTOTの「時刻」だけなのでストリームコールバックで得た日付と合成する
                    int position = static_cast<int>(SendMessage(hwndTvtp, WM_TVTP_GET_POSITION, 0, 0));
                    if (position >= 0) {
                        lock_recursive_mutex lock(m_totLock);
                        if (m_totTime >= 0) {
                            llft = m_totTime - position * FILETIME_MILLISECOND;
                            // ファイルのTOTが23時以上/1時未満でストリームコールバックから逆算したTOTが1時未満/23時以上のときは
                            // 誤差が日を跨いでしまったものとして補正する
                            if (totTime / 3600000 >= 23 && llft / (3600000 * FILETIME_MILLISECOND) % 24 < 1) {
                                // 前日にする
                                llft -= 3600000 * FILETIME_MILLISECOND;
                            }
                            else if (totTime / 3600000 < 1 && llft / (3600000 * FILETIME_MILLISECOND) % 24 >= 23) {
                                // 翌日にする
                                llft += 3600000 * FILETIME_MILLISECOND;
                            }
                        }
                    }
                }
            }
            m_fCheckSetStreamCallback = true;

            if (totTime >= 0) {
                if (hour < 0) {
                    hour = totTime / 3600000 % 24;
                    SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR, CB_SETCURSEL, hour, 0);
                }
                if (minute < 0) {
                    minute = totTime / 60000 % 60;
                    SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE, CB_SETCURSEL, minute, 0);
                }
                if (hourSpan < 0 || minuteSpan < 0) {
                    int duration = static_cast<int>(SendMessage(hwndTvtp, WM_TVTP_GET_DURATION, 0, 0));
                    if (duration >= 0) {
                        int hourRange = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR_SPAN, CB_GETCOUNT, 0, 0));
                        duration = min(duration + 60000 - 1, hourRange * 3600000 - 1);
                        if (hourSpan < 0) {
                            hourSpan = duration / 3600000;
                            SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR_SPAN, CB_SETCURSEL, hourSpan, 0);
                        }
                        if (minuteSpan < 0) {
                            minuteSpan = duration / 60000 % 60;
                            SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE_SPAN, CB_SETCURSEL, minuteSpan, 0);
                        }
                    }
                }
            }
            if (llft >= 0) {
                FILETIME ft;
                ft.dwLowDateTime = static_cast<DWORD>(llft);
                ft.dwHighDateTime = static_cast<DWORD>(llft >> 32);
                SYSTEMTIME st;
                if (FileTimeToSystemTime(&ft, &st)) {
                    int yearRange = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_YEAR, CB_GETCOUNT, 0, 0));
                    if (st.wYear >= 2009 && st.wYear < 2009 + yearRange) {
                        if (year < 0) {
                            year = st.wYear - 2009;
                            SendDlgItemMessage(m_hDlg, IDC_COMBO_YEAR, CB_SETCURSEL, year, 0);
                        }
                        if (month < 0) {
                            month = st.wMonth - 1;
                            SendDlgItemMessage(m_hDlg, IDC_COMBO_MONTH, CB_SETCURSEL, month, 0);
                        }
                        if (day < 0) {
                            day = st.wDay - 1;
                            SendDlgItemMessage(m_hDlg, IDC_COMBO_DAY, CB_SETCURSEL, day, 0);
                        }
                        SendMessage(m_hDlg, WM_COMMAND, MAKELONG(IDC_COMBO_YEAR, CBN_SELCHANGE), 0);
                    }
                }
            }
        }
    }

    if (jkIndex >= 0 && year >= 0 && month >= 0 && day >= 0 && hour >= 0 && minute >= 0 && hourSpan >= 0 && minuteSpan >= 0) {
        // コントロールがすべて選択されたので取得可能にする
        EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_DOWNLOAD), TRUE);
        EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_REL_DOWNLOAD), TRUE);
        SetDlgItemText(m_hDlg, IDC_STATIC_MESSAGE, TEXT(""));
        return true;
    }
    return false;
}

void CNXKakolog::OnInitDialog()
{
    TCHAR text[64];
    for (size_t i = 0; i < _countof(DEFAULT_JKID_NAME_TABLE); ++i) {
        _stprintf_s(text, TEXT("jk%d (%.40s)"), DEFAULT_JKID_NAME_TABLE[i].jkID, DEFAULT_JKID_NAME_TABLE[i].name);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_JKID, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    SYSTEMTIME stNow;
    GetSystemTime(&stNow);
    int utc9YearNow = stNow.wYear + (stNow.wMonth == 12 && stNow.wDay == 31 && stNow.wHour >= 15);
    for (int i = 2009; i <= utc9YearNow; ++i) {
        _stprintf_s(text, TEXT("%d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_YEAR, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    for (int i = 1; i <= 12; ++i) {
        _stprintf_s(text, TEXT("%d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_MONTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    for (int i = 1; i <= 31; ++i) {
        _stprintf_s(text, TEXT("%d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_DAY, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    for (int i = 0; i <= 23; ++i) {
        _stprintf_s(text, TEXT("%d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    for (int i = 0; i <= 11; ++i) {
        _stprintf_s(text, TEXT("%d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR_SPAN, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    for (int i = 0; i <= 59; ++i) {
        _stprintf_s(text, TEXT("%02d"), i);
        SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE_SPAN, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }
    if (!CheckPlaying()) {
        SetTimer(m_hDlg, TIMER_ID_CHECK_PLAYING, 1000, nullptr);
    }
    SendMessage(m_hDlg, WM_COMMAND, MAKELONG(IDC_COMBO_YEAR, CBN_SELCHANGE), 0);
    PostMessage(m_hDlg, WM_APP_SET_ZORDER, 0, 0);
}

void CNXKakolog::OnDownload()
{
    int jkIndex = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_JKID, CB_GETCURSEL, 0, 0));
    int year = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_YEAR, CB_GETCURSEL, 0, 0));
    int month = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MONTH, CB_GETCURSEL, 0, 0));
    int day = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_DAY, CB_GETCURSEL, 0, 0));
    int hour = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR, CB_GETCURSEL, 0, 0));
    int minute = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE, CB_GETCURSEL, 0, 0));
    int hourSpan = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_HOUR_SPAN, CB_GETCURSEL, 0, 0));
    int minuteSpan = static_cast<int>(SendDlgItemMessage(m_hDlg, IDC_COMBO_MINUTE_SPAN, CB_GETCURSEL, 0, 0));
    if (jkIndex >= 0 && jkIndex < static_cast<int>(_countof(DEFAULT_JKID_NAME_TABLE)) &&
        year >= 0 && month >= 0 && day >= 0 && hour >= 0 && minute >= 0 && hourSpan >= 0 && minuteSpan >= 0) {
        SYSTEMTIME st = {};
        st.wYear = static_cast<WORD>(year + 2009);
        st.wMonth = static_cast<WORD>(month + 1);
        st.wDay = static_cast<WORD>(day + 1);
        st.wHour = static_cast<WORD>(hour);
        st.wMinute = static_cast<WORD>(minute);
        FILETIME ft;
        if (SystemTimeToFileTime(&st, &ft)) {
            unsigned int startTime = FileTimeToUnixTime(ft.dwLowDateTime | static_cast<LONGLONG>(ft.dwHighDateTime) << 32) - 9 * 3600;
            unsigned int endTime = startTime + hourSpan * 3600 + minuteSpan * 60;
            char uri[128];
            sprintf_s(uri, "https://jikkyo.tsukumijima.net/api/kakolog/jk%d?starttime=%u&endtime=%u&format=xml",
                      DEFAULT_JKID_NAME_TABLE[jkIndex].jkID, startTime, endTime);
            if (m_getStream.Send(m_hDlg, WM_APP_STREAM, 'G', uri)) {
                m_getBuf.clear();
                SetDlgItemText(m_hDlg, IDC_STATIC_MESSAGE, TEXT("【Download: 0 bytes】"));
            }
            else {
                SetDlgItemText(m_hDlg, IDC_STATIC_MESSAGE, TEXT("【取得開始できませんでした】"));
            }
        }
        else {
            SetDlgItemText(m_hDlg, IDC_STATIC_MESSAGE, TEXT("【日付時刻に問題があります】"));
        }
    }
}

bool CNXKakolog::OpenXmlLog(const char *log, int openFlag)
{
    TCHAR path[MAX_PATH + 32];
    if (GetLongModuleFileName(g_hinstDLL, path, MAX_PATH)) {
        size_t len = _tcslen(path);
        while (len > 0 && !_tcschr(TEXT("/\\"), path[len - 1])) {
            --len;
        }
        DWORD processID = GetCurrentProcessId();
        _stprintf_s(path + len, _countof(path) - len, TEXT("NXK_%u.xml"), processID);
        FILE *fp;
        if (!_tfopen_s(&fp, path, TEXT("wN"))) {
            fputs(log, fp);
            fclose(fp);
            bool ok = false;
            HWND hwndJK = FindNicoJKWindow();
            if (hwndJK && SendMessage(hwndJK, WM_NICOJK_GET_MSGVER, 0, 0) >= NICOJK_CURRENT_MSGVER) {
                ok = !!SendMessage(hwndJK, WM_NICOJK_OPEN_LOGFILE, processID, 'N' | 'X' << 8 | 'K' << 16 | NICOJK_OPEN_FLAG_XML | openFlag);
            }
            DeleteFile(path);
            return ok;
        }
    }
    return false;
}

// ダイアログプロシージャ
INT_PTR CALLBACK CNXKakolog::MainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG) {
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
    }
    CNXKakolog *pThis = reinterpret_cast<CNXKakolog*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
    return pThis ? pThis->ProcessMainDlg(hDlg, uMsg, wParam, lParam) : FALSE;
}

// プラグインAPI経由のダイアログプロシージャ
INT_PTR CALLBACK CNXKakolog::TVTestMainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, void *pClientData)
{
    return static_cast<CNXKakolog*>(pClientData)->ProcessMainDlg(hDlg, uMsg, wParam, lParam);
}

INT_PTR CNXKakolog::ProcessMainDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static_cast<void>(lParam);
    switch (uMsg) {
    case WM_INITDIALOG:
        m_hDlg = hDlg;
        OnInitDialog();
        m_pApp->SetWindowMessageCallback(WindowMsgCallback, this);
        return TRUE;
    case WM_DESTROY:
        m_getStream.Close();
        m_pApp->SetWindowMessageCallback(nullptr);
        m_hDlg = nullptr;
        break;
    case WM_TIMER:
        switch (wParam) {
        case TIMER_ID_CHECK_PLAYING:
            if (CheckPlaying()) {
                KillTimer(hDlg, TIMER_ID_CHECK_PLAYING);
            }
            break;
        case TIMER_ID_DONE_POSCHANGE:
            KillTimer(hDlg, TIMER_ID_DONE_POSCHANGE);
            if (!m_pApp->GetFullscreen()) {
                SendMessage(hDlg, WM_APP_SET_ZORDER, 0, 0);
            }
            break;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            m_pApp->EnablePlugin(false);
            break;
        case IDC_COMBO_YEAR:
        case IDC_COMBO_MONTH:
        case IDC_COMBO_DAY:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                TCHAR text[16];
                _tcscpy_s(text, TEXT("(不明)"));
                int year = static_cast<int>(SendDlgItemMessage(hDlg, IDC_COMBO_YEAR, CB_GETCURSEL, 0, 0));
                int month = static_cast<int>(SendDlgItemMessage(hDlg, IDC_COMBO_MONTH, CB_GETCURSEL, 0, 0));
                int day = static_cast<int>(SendDlgItemMessage(hDlg, IDC_COMBO_DAY, CB_GETCURSEL, 0, 0));
                if (year >= 0 && month >= 0 && day >= 0) {
                    SYSTEMTIME st = {};
                    st.wYear = static_cast<WORD>(year + 2009);
                    st.wMonth = static_cast<WORD>(month + 1);
                    st.wDay = static_cast<WORD>(day + 1);
                    FILETIME ft;
                    if (SystemTimeToFileTime(&st, &ft) && FileTimeToSystemTime(&ft, &st)) {
                        static const LPCTSTR dayOfWeekText[] = {
                            TEXT("日"), TEXT("月"), TEXT("火"), TEXT("水"), TEXT("木"), TEXT("金"), TEXT("土")
                        };
                        _stprintf_s(text, TEXT("(%s)"), dayOfWeekText[st.wDayOfWeek % 7]);
                    }
                }
                SetDlgItemText(hDlg, IDC_STATIC_DAY_OF_WEEK, text);
            }
            break;
        case IDC_BUTTON_LINK:
            ShellExecute(nullptr, TEXT("open"), TEXT("https://jikkyo.tsukumijima.net/"), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDC_BUTTON_DOWNLOAD:
            m_openFlag = NICOJK_OPEN_FLAG_ABSOLUTE;
            OnDownload();
            break;
        case IDC_BUTTON_REL_DOWNLOAD:
            m_openFlag = NICOJK_OPEN_FLAG_RELATIVE;
            OnDownload();
            break;
        }
        break;
    case WM_APP_STREAM:
        {
            int ret = m_getStream.ProcessRecv(m_getBuf);
            if (ret < 0) {
                // 切断
                if (ret == -2) {
                    m_getBuf.push_back('\0');
                    // 成功なら1行目はxml宣言で2行目は"<packet>"で始まるはず
                    char *p = strchr(m_getBuf.data(), '\n');
                    if (p && strncmp(p + 1, "<packet>", 8) == 0 && OpenXmlLog(m_getBuf.data(), m_openFlag)) {
                        // 完了
                        m_pApp->EnablePlugin(false);
                    }
                    else {
                        SetDlgItemText(hDlg, IDC_STATIC_MESSAGE, TEXT("【取得データを開けませんでした】"));
                    }
                }
                else {
                    SetDlgItemText(hDlg, IDC_STATIC_MESSAGE, TEXT("【取得に失敗しました】"));
                }
            }
            else {
                TCHAR text[64];
                _stprintf_s(text, TEXT("【Download: %d bytes】"), static_cast<int>(m_getBuf.size()));
                SetDlgItemText(hDlg, IDC_STATIC_MESSAGE, text);
            }
        }
        break;
    case WM_APP_SET_ZORDER:
        // TVTestウィンドウの前面にもってくる
        SetWindowPos(hDlg, m_pApp->GetFullscreen() || m_pApp->GetAlwaysOnTop() ? HWND_TOPMOST : HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
        break;
    }
    return FALSE;
}

LRESULT CALLBACK CNXKakolog::EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2, void *pClientData)
{
    static_cast<void>(lParam2);
    CNXKakolog &this_ = *static_cast<CNXKakolog*>(pClientData);
    switch (Event) {
    case TVTest::EVENT_PLUGINENABLE:
        // プラグインの有効状態が変化した
        return lParam1 ? this_.EnablePlugin() : this_.DisablePlugin();
    case TVTest::EVENT_SERVICEUPDATE:
        // サービスの構成が変化した
        if (!this_.m_fCheckSetStreamCallback) {
            HWND hwndTvtp = FindTvtPlayFrame();
            if (hwndTvtp) {
                // 必要ならストリームコールバックを登録
                if (SendMessage(hwndTvtp, WM_TVTP_GET_MSGVER, 0, 0) < TVTP_TOT_UNIX_MSGVER) {
                    this_.m_pApp->SetStreamCallback(0, StreamCallback, &this_);
                }
                this_.m_fCheckSetStreamCallback = true;
            }
        }
        break;
    case TVTest::EVENT_FULLSCREENCHANGE:
        // 全画面表示状態が変化した
        if (this_.m_hDlg) {
            PostMessage(this_.m_hDlg, WM_APP_SET_ZORDER, 0, 0);
        }
        break;
    case TVTest::EVENT_COMMAND:
        // コマンドが選択された
        switch (lParam1) {
        case COMMAND_DOWNLOAD:
            if (!this_.m_hDlg && this_.m_pApp->EnablePlugin(true)) {
                // 取得可能でなければ無効に戻す
                if (this_.m_hDlg && IsWindowEnabled(GetDlgItem(this_.m_hDlg, IDC_BUTTON_DOWNLOAD))) {
                    this_.m_openFlag = NICOJK_OPEN_FLAG_ABSOLUTE;
                    this_.OnDownload();
                }
                else {
                    this_.m_pApp->EnablePlugin(false);
                }
            }
            break;
        }
        break;
    }
    return 0;
}

BOOL CALLBACK CNXKakolog::WindowMsgCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult, void *pUserData)
{
    static_cast<void>(hwnd);
    static_cast<void>(lParam);
    static_cast<void>(pResult);
    CNXKakolog &this_ = *static_cast<CNXKakolog*>(pUserData);
    switch (uMsg) {
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            SendMessage(this_.m_hDlg, WM_APP_SET_ZORDER, 0, 0);
        }
        break;
    case WM_WINDOWPOSCHANGED:
        // WM_ACTIVATEされないZオーダーの変化を捉える。フルスクリーンでもなぜか送られてくるので注意
        SetTimer(this_.m_hDlg, TIMER_ID_DONE_POSCHANGE, 1000, nullptr);
        break;
    }
    return FALSE;
}

// TOT時刻を取得するストリームコールバック(別スレッド)
BOOL CALLBACK CNXKakolog::StreamCallback(BYTE *pData, void *pClientData)
{
    int pid = (pData[1] & 0x1F) << 8 | pData[2];
    if (pid != 0x14) return TRUE;

    int unitStartIndicator = pData[1] >> 6 & 0x01;
    int adaptationControl  = pData[3] >> 4 & 0x03;
    if (!unitStartIndicator || adaptationControl == 0 || adaptationControl == 2) return TRUE;

    int payloadPos = 4;
    if (adaptationControl == 3) {
        // アダプテーションフィールドをスキップする
        int adaptationLength = pData[4];
        if (adaptationLength > 182) return TRUE;
        payloadPos += 1 + adaptationLength;
    }

    int pointerField = pData[payloadPos];
    int tablePos = payloadPos + 1 + pointerField;
    if (tablePos + 7 >= 188) return TRUE;

    int tableID = pData[tablePos];
    // TOT or TDT (ARIB STD-B10)
    if (tableID != 0x73 && tableID != 0x70) return TRUE;

    // TOTパケットは地上波の実測で6秒に1個程度
    // ARIB規格では最低30秒に1個

    CNXKakolog &this_ = *static_cast<CNXKakolog*>(pClientData);

    // TOT時刻を記録する
    lock_recursive_mutex lock(this_.m_totLock);
    this_.m_totTime = AribToFileTime(pData + tablePos + 3);
    return TRUE;
}

TVTest::CTVTestPlugin *CreatePluginClass()
{
    return new CNXKakolog;
}
