// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#define INITGUID
#include <initguid.h>
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "ethio/json.hpp"
#include "ethio/mapping.h"

#ifndef NDEBUG
static HANDLE get_log_mutex()
{
    static HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\EthiopicTSF_LogMutex");
    return hMutex;
}
#endif

static void elog(const char *msg)
{
#ifndef NDEBUG
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[EthiopicTSF] %02d:%02d:%02d.%03d [pid=%lu tid=%lu] %s\n",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     pid, tid, msg);
    if (n > 0) OutputDebugStringA(buf);

    static char path[MAX_PATH];
    static bool path_init = false;
    if (!path_init) {
        GetTempPathA(sizeof(path), path);
        strcat_s(path, sizeof(path), "ethiopic_tsf.log");
        path_init = true;
    }

    HANDLE hMutex = get_log_mutex();
    if (hMutex) WaitForSingleObject(hMutex, 5000);

    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%02d:%02d:%02d.%03d [%lu %lu] v2|%s\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                pid, tid, msg);
        fclose(f);
    }

    if (hMutex) ReleaseMutex(hMutex);
#else
    (void)msg;
#endif
}

static bool g_dllUnloading = false;

void MarkDllUnloading()
{
    g_dllUnloading = true;
    elog("MarkDllUnloading: DLL is unloading");
}

bool IsDllUnloading()
{
    return g_dllUnloading;
}

#ifdef ETHIOPIC_TEST_STUBS
void DllAddRef() {}
void DllRelease() {}
HMODULE DllModuleHandle() { return GetModuleHandleA(nullptr); }
#endif

DEFINE_GUID(IID_ITfTextInputProcessorEx,
    0x6e4e2100, 0xf9cd, 0x433d, 0xb4,0x6f, 0xf3,0xe7,0xed,0x2d,0x62,0x59);
DEFINE_GUID(IID_ITfDisplayAttributeProvider,
    0xfee47777, 0x163c, 0x4769, 0x99,0x6a, 0x6e,0x9c,0x50,0x4a,0x2c,0x4b);

static const GUID GUID_DisplayAttributeInput = {
    0xD7A3E10F, 0xC8B2, 0x4F9D,
    {0xA1, 0xE3, 0x2D, 0x6F, 0x8C, 0x4B, 0x5A, 0x9E}
};

// GUID for the TSF candidate list UI element
static const GUID GUID_EthiopicCandidateWindow = {
    0xED70ECDE, 0xC8AA, 0x4170,
    {0x96, 0xCC, 0x00, 0x90, 0xDE, 0xA8, 0xAE, 0xC2}
};

#ifdef __MINGW32__
template<> const GUID &__mingw_uuidof<ITfTextInputProcessorEx>() { return IID_ITfTextInputProcessorEx; }
template<> const GUID &__mingw_uuidof<ITfDisplayAttributeProvider>() { return IID_ITfDisplayAttributeProvider; }
#endif

static void tlog(const char *msg)
{
#ifndef NDEBUG
    const char *path = std::getenv("ETHIOPIC_TSF_LOG");
    if (!path) return;
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
#else
    (void)msg;
#endif
}

static std::string find_data_file()
{
    HMODULE hModule = DllModuleHandle();
    if (!hModule) {
        hModule = GetModuleHandleA(nullptr);
    }

    char module_path[MAX_PATH];
    module_path[0] = '\0';
    GetModuleFileNameA(hModule, module_path, MAX_PATH);

    std::string dir(module_path);
    size_t last_sep = dir.find_last_of("\\/");
    if (last_sep != std::string::npos)
        dir = dir.substr(0, last_sep);

    elog(dir.c_str());

    // Candidates relative to the DLL directory, tried in order:
    //   1. ./data/amharic/am-sera.json       — installed alongside DLL
    //   2. ../../data/amharic/am-sera.json   — build tree
    //   3. ../data/amharic/am-sera.json      — DLL in a subfolder
    const char *candidates[] = {
        "/data/amharic/am-sera.json",
        "/../../data/amharic/am-sera.json",
        "/../data/amharic/am-sera.json",
    };

    for (int i = 0; i < 3; i++) {
        std::string path = dir + candidates[i];
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            elog("find_data_file: found data file");
            return path;
        }
    }

    // Try relative to the host executable as a last runtime resort.
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH)) {
        std::string exe_dir(exe_path);
        size_t sep = exe_dir.find_last_of("\\/");
        if (sep != std::string::npos)
            exe_dir = exe_dir.substr(0, sep);
        for (int i = 0; i < 3; i++) {
            std::string path = exe_dir + candidates[i];
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                elog("find_data_file: found data file (relative to exe)");
                return path;
            }
        }
    }

    elog("find_data_file: using hardcoded MAPPING_SOURCE_DIR");
    return MAPPING_SOURCE_DIR "/amharic/am-sera.json";
}

// Converts a Windows virtual-key event to a UTF-8 string using the active
// keyboard layout. Uses ToUnicodeEx to map vk→WCHAR, then WideCharToMultiByte
// for WCHAR→UTF-8. Returns empty string for dead keys, non-character input,
// or keys that don't produce text (arrows, modifiers, etc.).
static void vk_to_utf8(WPARAM wParam, LPARAM lParam, char *out, int out_size)
{
    out[0] = '\0';
    BYTE key_state[256] = {};
    GetKeyboardState(key_state);

    // Reconstruct scan code from lParam: bits 16-23 give the scan code,
    // KF_EXTENDED (bit 24) flags enhanced keys (right-alt, nav cluster, etc.)
    UINT scan_code = (LOBYTE(HIWORD(lParam)) & 0x7F) |
                     ((HIWORD(lParam) & KF_EXTENDED) ? 0x80 : 0);
    WCHAR wch[4] = {};
    int n = ToUnicodeEx((UINT)wParam, scan_code, key_state, wch, 4, 0,
                        GetKeyboardLayout(0));
    if (n > 0 && n < 4) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wch, n,
                                      out, out_size - 1, nullptr, nullptr);
        if (len > 0)
            out[len] = '\0';
        else
            out[0] = '\0';
    }
}

static std::wstring utf8_to_wstring(const std::string &s)
{
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                   nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], wlen);
    return w;
}

// ---- Candidate Popup Window ----
//
// The candidate list is rendered as a custom HWND popup (WS_POPUP with
// WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE). This is used
// alongside ITfCandidateListUIElementBehavior to support both TSF-aware
// apps (where TSF manages candidate UI lifetime) and non-TSF-aware apps
// (where the HWND provides a visible fallback).
//
// The popup is painted with GDI: themed text on an infotip-colored
// background, with the selected candidate drawn in highlight colors.
// Position is tracked via ITfContextView::GetTextExt during edit sessions
// so the popup sits just below the caret.

static const wchar_t *kPopupWndClass = L"EthiopicCandidatePopupWnd";
static HINSTANCE g_popupHInstance = nullptr;
static bool g_popupClassRegistered = false;

static HFONT g_popupFont = nullptr;
static HFONT _GetPopupFont()
{
    if (!g_popupFont) {
        NONCLIENTMETRICSW ncm = {sizeof(ncm)};
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            g_popupFont = CreateFontIndirectW(&ncm.lfMessageFont);
        if (!g_popupFont)
            g_popupFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    return g_popupFont;
}

void CEthiopicTextService::_EnsurePopupWindowClass(HINSTANCE hInstance)
{
    if (g_popupClassRegistered) return;
    g_popupHInstance = hInstance;

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_SAVEBITS | CS_DROPSHADOW;
    wc.lpfnWndProc = _PopupWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_INFOBK + 1);
    wc.lpszClassName = kPopupWndClass;
    RegisterClassExW(&wc);
    g_popupClassRegistered = true;
}

void CEthiopicTextService::_CreatePopupWindow()
{
    if (m_hwndCandidatePopup) return;

    _EnsurePopupWindowClass((HINSTANCE)DllModuleHandle());

    m_hwndCandidatePopup = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kPopupWndClass, L"",
        WS_POPUP | WS_DISABLED,
        0, 0, 10, 10,
        nullptr, nullptr, g_popupHInstance, nullptr);

    if (m_hwndCandidatePopup) {
        SetWindowLongPtrW(m_hwndCandidatePopup, GWLP_USERDATA, (LONG_PTR)this);
    }
}

LRESULT CALLBACK CEthiopicTextService::_PopupWndProc(HWND hwnd, UINT msg,
                                                      WPARAM wp, LPARAM lp)
{
    CEthiopicTextService *pThis =
        (CEthiopicTextService *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!pThis) { EndPaint(hwnd, &ps); break; }

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Background
        HBRUSH hBrBg = GetSysColorBrush(COLOR_INFOBK);
        FillRect(hdc, &rc, hBrBg);

        // Border
        HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_INFOTEXT));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldBr);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        // Candidate text
        HFONT hFont = _GetPopupFont();
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));

        auto &cands = pThis->m_candidates;
        int sel = pThis->m_candidateIndex;
        int lineH = pThis->m_candidatePopupLineHeight;
        if (lineH < 16) lineH = 20;

        int y = 4;
        for (int i = 0; i < (int)cands.size(); i++) {
            RECT itemRc = {4, y, rc.right - 4, y + lineH};

            if (i == sel) {
                // Selected item background
                HBRUSH hBrSel = GetSysColorBrush(COLOR_HIGHLIGHT);
                RECT selRc = {1, y, rc.right - 1, y + lineH};
                FillRect(hdc, &selRc, hBrSel);
                SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            } else {
                SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
            }

            // Build display text: index + candidate string
            wchar_t buf[256];
            std::wstring wcand = utf8_to_wstring(cands[i]);
            _snwprintf(buf, 256, L"%d. %s", i + 1, wcand.c_str());

            DrawTextW(hdc, buf, -1, &itemRc,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            y += lineH;
        }

        SelectObject(hdc, hOldFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

void CEthiopicTextService::ShowCandidatePopup(ITfContext *pContext)
{
    if (m_candidates.empty()) {
        HideCandidatePopup();
        return;
    }

    // Ensure window exists
    if (!m_hwndCandidatePopup) {
        _CreatePopupWindow();
    }
    if (!m_hwndCandidatePopup) return;

    // Calculate window size based on content
    HDC hdc = GetDC(m_hwndCandidatePopup);
    HFONT hFont = _GetPopupFont();
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    int lineH = m_candidatePopupLineHeight;
    if (lineH < 16) lineH = 20;
    // Measure text for minimum width
    TEXTMETRICW tm = {};
    GetTextMetricsW(hdc, &tm);
    int maxW = 80; // minimum
    for (auto &c : m_candidates) {
        std::wstring wc = utf8_to_wstring(c);
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, wc.c_str(), (int)wc.size(), &sz);
        // Account for "N. " prefix (~20px)
        int itemW = sz.cx + tm.tmAveCharWidth * 4 + 8;
        if (itemW > maxW) maxW = itemW;
    }
    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndCandidatePopup, hdc);

    int numCands = (int)m_candidates.size();
    int winW = maxW + 2;
    int winH = numCands * lineH + 8;

    // Default position (will be refined by edit session)
    POINT pt = m_candidatePopupPos;
    if (pt.x == 0 && pt.y == 0) {
        // No known position yet — use a rough estimate
        HWND hFg = GetForegroundWindow();
        if (hFg) {
            RECT fgRc;
            GetWindowRect(hFg, &fgRc);
            pt.x = fgRc.left + 100;
            pt.y = fgRc.top + 200;
        }
    }

    // Ensure within monitor work area
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfoW(hMon, &mi)) {
        if (pt.y + winH > mi.rcWork.bottom)
            pt.y = mi.rcWork.top + 100;
        if (pt.x + winW > mi.rcWork.right)
            pt.x = mi.rcWork.right - winW - 10;
        if (pt.x < mi.rcWork.left)
            pt.x = mi.rcWork.left + 10;
    }

    SetWindowPos(m_hwndCandidatePopup, HWND_TOPMOST,
                 pt.x, pt.y, winW, winH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_candidatePopupPos = pt;

    // Send WM_NCACTIVATE FALSE to prevent the window from looking "active"
    SendMessageW(m_hwndCandidatePopup, WM_NCACTIVATE, FALSE, 0);

    // Queue an edit session to get the accurate cursor position
    if (pContext && m_pThreadMgr) {
        CEthiopicEditSession *posSession = new CEthiopicEditSession(
            this, "", "", true);
        HRESULT hr = 0;
        pContext->RequestEditSession(m_tfClientId, posSession,
            TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        posSession->Release();
    }
}

void CEthiopicTextService::HideCandidatePopup()
{
    if (m_hwndCandidatePopup) {
        ShowWindow(m_hwndCandidatePopup, SW_HIDE);
        DestroyWindow(m_hwndCandidatePopup);
        m_hwndCandidatePopup = nullptr;
    }
    m_candidatePopupPos.x = 0;
    m_candidatePopupPos.y = 0;
}

void CEthiopicTextService::RefreshCandidatePopup()
{
    if (m_hwndCandidatePopup && IsWindowVisible(m_hwndCandidatePopup)) {
        // Recalculate size and position
        HDC hdc = GetDC(m_hwndCandidatePopup);
        HFONT hFont = _GetPopupFont();
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        TEXTMETRICW tm = {};
        GetTextMetricsW(hdc, &tm);
        int maxW = 80;
        for (auto &c : m_candidates) {
            std::wstring wc = utf8_to_wstring(c);
            SIZE sz = {};
            GetTextExtentPoint32W(hdc, wc.c_str(), (int)wc.size(), &sz);
            int itemW = sz.cx + tm.tmAveCharWidth * 4 + 8;
            if (itemW > maxW) maxW = itemW;
        }
        SelectObject(hdc, hOldFont);
        ReleaseDC(m_hwndCandidatePopup, hdc);

        int lineH = m_candidatePopupLineHeight;
        if (lineH < 16) lineH = 20;
        int numCands = (int)m_candidates.size();
        if (numCands < 1) { HideCandidatePopup(); return; }
        int winW = maxW + 2;
        int winH = numCands * lineH + 8;

        SetWindowPos(m_hwndCandidatePopup, nullptr,
                     0, 0, winW, winH,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        InvalidateRect(m_hwndCandidatePopup, nullptr, TRUE);
    }
}

// ---- End Candidate Popup Window ----

class CEthiopicDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CEthiopicDisplayAttributeInfo()
        : m_cRef(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
            *ppv = static_cast<ITfDisplayAttributeInfo *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() override
    {
        LONG ref = InterlockedDecrement(&m_cRef);
        if (ref == 0) delete this;
        return ref;
    }

    STDMETHODIMP GetGUID(GUID *pguid) override
    {
        if (!pguid) return E_INVALIDARG;
        *pguid = GUID_DisplayAttributeInput;
        return S_OK;
    }

    STDMETHODIMP GetDescription(BSTR *pbstrDesc) override
    {
        if (!pbstrDesc) return E_INVALIDARG;
        *pbstrDesc = SysAllocString(L"Ethiopic Input");
        return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
    }

    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE *pda) override
    {
        if (!pda) return E_INVALIDARG;
        memset(pda, 0, sizeof(TF_DISPLAYATTRIBUTE));
        pda->crText.type = TF_CT_COLORREF;
        pda->crText.cr = RGB(0, 0, 0);
        pda->crBk.type = TF_CT_NONE;
        pda->crLine.type = TF_CT_COLORREF;
        pda->crLine.cr = RGB(0, 0, 0);
        pda->lsStyle = TF_LS_DOT;
        pda->fBoldLine = FALSE;
        pda->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }

    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE *) override
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP Reset() override
    {
        return E_NOTIMPL;
    }

private:
    LONG m_cRef;
};

class CEthiopicEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
public:
    CEthiopicEnumDisplayAttributeInfo()
        : m_cRef(1), m_index(0) {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
            *ppv = static_cast<IEnumTfDisplayAttributeInfo *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() override
    {
        LONG ref = InterlockedDecrement(&m_cRef);
        if (ref == 0) delete this;
        return ref;
    }

    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo **ppEnum) override
    {
        if (!ppEnum) return E_INVALIDARG;
        auto *clone = new (std::nothrow) CEthiopicEnumDisplayAttributeInfo();
        if (!clone) return E_OUTOFMEMORY;
        clone->m_index = m_index;
        *ppEnum = clone;
        return S_OK;
    }

    STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo **rgInfo, ULONG *pcFetched) override
    {
        if (!rgInfo) return E_INVALIDARG;
        if (pcFetched) *pcFetched = 0;

        if (ulCount == 0) return S_OK;
        if (m_index >= 1) return S_FALSE;

        auto *info = new (std::nothrow) CEthiopicDisplayAttributeInfo();
        if (!info) return E_OUTOFMEMORY;

        rgInfo[0] = info;
        m_index++;
        if (pcFetched) *pcFetched = 1;
        return (ulCount == 1) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Reset() override { m_index = 0; return S_OK; }
    STDMETHODIMP Skip(ULONG ulCount) override { m_index += ulCount; return S_OK; }

private:
    LONG m_cRef;
    ULONG m_index;
};

CEthiopicEditSession::CEthiopicEditSession(CEthiopicTextService *pService,
                                           const std::string &commit_text,
                                           const std::string &preedit_text,
                                           bool position_popup)
    : m_cRef(1)
    , m_pService(pService)
    , m_commitText(commit_text)
    , m_preeditText(preedit_text)
    , m_positionPopup(position_popup)
{
}

STDMETHODIMP CEthiopicEditSession::QueryInterface(REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppv = static_cast<ITfEditSession *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEthiopicEditSession::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CEthiopicEditSession::Release()
{
    LONG ref = InterlockedDecrement(&m_cRef);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP CEthiopicEditSession::DoEditSession(TfEditCookie ec)
{
    if (!m_pService) return E_FAIL;

    // Position candidate popup if needed
    if (m_positionPopup && m_pService->IsCandidatePopupVisible()) {
        ITfContext *pContext = nullptr;
        ITfDocumentMgr *pDocMgr = nullptr;
        if (m_pService->GetThreadMgr() &&
            SUCCEEDED(m_pService->GetThreadMgr()->GetFocus(&pDocMgr)) && pDocMgr) {
            pDocMgr->GetTop(&pContext);
            pDocMgr->Release();
        }
        if (pContext) {
            ITfContextView *pView = nullptr;
            if (SUCCEEDED(pContext->GetActiveView(&pView)) && pView) {
                TF_SELECTION sel = {};
                ULONG fetched = 0;
                if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION,
                        1, &sel, &fetched)) && fetched > 0 && sel.range) {
                    RECT textRect = {};
                    BOOL clipped = FALSE;
                    if (SUCCEEDED(pView->GetTextExt(ec, sel.range, &textRect, &clipped))) {
                        m_pService->m_candidatePopupPos.x = textRect.left;
                        m_pService->m_candidatePopupPos.y = textRect.bottom;
                        m_pService->m_candidatePopupLineHeight = textRect.bottom - textRect.top;
                        if (m_pService->m_candidatePopupLineHeight < 16)
                            m_pService->m_candidatePopupLineHeight = 20;

                        // Reposition the popup window
                        if (m_pService->m_hwndCandidatePopup) {
                            RECT wr = {};
                            GetWindowRect(m_pService->m_hwndCandidatePopup, &wr);
                            int w = wr.right - wr.left;
                            int h = wr.bottom - wr.top;
                            POINT pt = {textRect.left, textRect.bottom};
                            // Check if there's room below; if not, flip above
                            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                            MONITORINFO mi = {sizeof(mi)};
                            if (GetMonitorInfoW(hMon, &mi)) {
                                if (pt.y + h > mi.rcWork.bottom)
                                    pt.y = textRect.top - h;
                                if (pt.x + w > mi.rcWork.right)
                                    pt.x = mi.rcWork.right - w;
                                if (pt.x < mi.rcWork.left)
                                    pt.x = mi.rcWork.left;
                            }
                            SetWindowPos(m_pService->m_hwndCandidatePopup, HWND_TOPMOST,
                                         pt.x, pt.y, 0, 0,
                                         SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
                            m_pService->m_candidatePopupPos.x = pt.x;
                            m_pService->m_candidatePopupPos.y = pt.y;
                        }
                    }
                    sel.range->Release();
                }
                pView->Release();
            }
            pContext->Release();
        }
    }

    // Position-only session: just reposition the popup, don't touch text/composition
    if (m_positionPopup && m_commitText.empty() && m_preeditText.empty()) {
        return S_OK;
    }

    char b[256];

    // 1. Handle commit text (may be combined with preedit)
    if (!m_commitText.empty()) {
        snprintf(b, sizeof(b), "DoEditSession: COMMIT text='%s'",
                 m_commitText.c_str());
        elog(b);

        bool hadComp = m_pService->HasComposition();
        if (hadComp) {
            elog("DoEditSession: had composition, replacing text before EndComposition");
            m_pService->UpdateComposition(ec, m_commitText);
        }
        m_pService->EndComposition(ec, nullptr);

        // Reposition cursor after committed text
        ITfContext *pContext = nullptr;
        ITfDocumentMgr *pDocMgr = nullptr;
        if (m_pService->GetThreadMgr() &&
            SUCCEEDED(m_pService->GetThreadMgr()->GetFocus(&pDocMgr)) && pDocMgr) {
            pDocMgr->GetTop(&pContext);
            pDocMgr->Release();
        }
        if (pContext) {
            if (hadComp) {
                TF_SELECTION sel = {};
                ULONG fetched = 0;
                HRESULT hrGetSel = pContext->GetSelection(ec, TF_DEFAULT_SELECTION,
                        1, &sel, &fetched);
                if (SUCCEEDED(hrGetSel) && fetched > 0 && sel.range) {
                    sel.range->Collapse(ec, TF_ANCHOR_END);
                    pContext->SetSelection(ec, 0, &sel);
                    sel.range->Release();
                    elog("DoEditSession: reposition cursor after composition commit");
                } else {
                    snprintf(b, sizeof(b),
                             "DoEditSession: GetSelection after commit FAILED hr=0x%08lX fetched=%lu",
                             (unsigned long)hrGetSel, (unsigned long)fetched);
                    elog(b);
                }
            } else {
                TF_SELECTION sel = {};
                ULONG fetched = 0;
                if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION,
                        1, &sel, &fetched)) && fetched > 0 && sel.range) {
                    std::wstring wtext = utf8_to_wstring(m_commitText);
                    snprintf(b, sizeof(b), "DoEditSession: SetText '%s' at selection",
                             m_commitText.c_str());
                    elog(b);
                    sel.range->SetText(ec, 0, wtext.c_str(), (LONG)wtext.size());
                    sel.range->Collapse(ec, TF_ANCHOR_END);
                    pContext->SetSelection(ec, 0, &sel);
                    sel.range->Release();
                }
            }
            pContext->Release();
        }
    }

    // 2. Handle preedit text (may be combined with commit above)
    if (!m_preeditText.empty()) {
        snprintf(b, sizeof(b), "DoEditSession: PREEDIT text='%s'",
                 m_preeditText.c_str());
        elog(b);
        m_pService->StartComposition(ec, nullptr, m_preeditText);
    } else if (m_commitText.empty()) {
        elog("DoEditSession: END-ONLY path (no preedit, no commit)");
        m_pService->EndComposition(ec, nullptr);
    }

    return S_OK;
}

// --- CEthiopicCandidateListUIElement ---

CEthiopicCandidateListUIElement::CEthiopicCandidateListUIElement(
    CEthiopicTextService *pService, ITfContext *pContext)
    : m_cRef(1), m_pService(pService), m_pContext(pContext), m_shown(false)
{
    if (m_pContext) m_pContext->AddRef();
}

void CEthiopicCandidateListUIElement::UpdateContext(ITfContext *pContext)
{
    if (m_pContext) m_pContext->Release();
    m_pContext = pContext;
    if (m_pContext) m_pContext->AddRef();
}

STDMETHODIMP CEthiopicCandidateListUIElement::QueryInterface(REFIID riid,
                                                              void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfUIElement) ||
        IsEqualIID(riid, IID_ITfCandidateListUIElement) ||
        IsEqualIID(riid, IID_ITfCandidateListUIElementBehavior)) {
        *ppv = static_cast<ITfCandidateListUIElementBehavior *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEthiopicCandidateListUIElement::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CEthiopicCandidateListUIElement::Release()
{
    LONG ref = InterlockedDecrement(&m_cRef);
    if (ref == 0) {
        if (m_pContext) m_pContext->Release();
        delete this;
    }
    return ref;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetDescription(BSTR *pbstr)
{
    if (!pbstr) return E_INVALIDARG;
    *pbstr = SysAllocString(L"Ethiopic Candidate");
    return *pbstr ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetGUID(GUID *pguid)
{
    if (!pguid) return E_INVALIDARG;
    *pguid = GUID_EthiopicCandidateWindow;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::Show(BOOL bShow)
{
    m_shown = (bShow != FALSE);
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::IsShown(BOOL *pbShow)
{
    if (!pbShow) return E_INVALIDARG;
    *pbShow = m_shown ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetUpdatedFlags(DWORD *pdwFlags)
{
    if (!pdwFlags) return E_INVALIDARG;
    *pdwFlags = TF_CLUIE_SELECTION | TF_CLUIE_COUNT |
                TF_CLUIE_CURRENTPAGE | TF_CLUIE_PAGEINDEX;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetDocumentMgr(
    ITfDocumentMgr **ppdim)
{
    if (!ppdim) return E_INVALIDARG;
    *ppdim = nullptr;
    if (!m_pContext) return E_FAIL;
    return m_pContext->GetDocumentMgr(ppdim);
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetCount(UINT *puCount)
{
    if (!puCount) return E_INVALIDARG;
    *puCount = (UINT)m_pService->m_candidates.size();
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetSelection(UINT *puIndex)
{
    if (!puIndex) return E_INVALIDARG;
    *puIndex = (UINT)m_pService->m_candidateIndex;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetString(UINT uIndex, BSTR *pstr)
{
    if (!pstr) return E_INVALIDARG;
    *pstr = nullptr;
    auto &cands = m_pService->m_candidates;
    if ((int)uIndex >= (int)cands.size()) return E_INVALIDARG;

    std::string &utf8 = cands[uIndex];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
                                    nullptr, 0);
    if (wlen <= 0) return E_FAIL;
    BSTR bstr = SysAllocStringLen(nullptr, wlen);
    if (!bstr) return E_OUTOFMEMORY;
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), bstr, wlen);
    *pstr = bstr;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetPageIndex(
    UINT *pIndex, UINT uSize, UINT *puPageCnt)
{
    if (!puPageCnt) return E_INVALIDARG;
    *puPageCnt = 1;
    if (pIndex && uSize > 0) pIndex[0] = 0;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::SetPageIndex(
    UINT *, UINT)
{
    return E_NOTIMPL;
}

STDMETHODIMP CEthiopicCandidateListUIElement::GetCurrentPage(UINT *puPage)
{
    if (!puPage) return E_INVALIDARG;
    *puPage = 0;
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::SetSelection(UINT nIndex)
{
    if ((int)nIndex >= (int)m_pService->m_candidates.size())
        return E_INVALIDARG;
    m_pService->m_candidateIndex = (int)nIndex;
    m_pService->_UpdateCandidateUI();
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::Finalize()
{
    std::string commitText = m_pService->FlushCandidate(
        m_pService->m_candidateIndex);
    m_pService->ShowSuggestions();

    // Update the TSF popup with new candidates (or dismiss if empty)
    if (!m_pService->m_candidates.empty()) {
        m_pService->_UpdateCandidateUI();
    } else {
        m_pService->_EndCandidateUI();
    }

    // Dispatch edit session to commit the chosen candidate text
    if (!commitText.empty() && m_pContext) {
        CEthiopicEditSession *session = new CEthiopicEditSession(
            m_pService, commitText, "", false);
        HRESULT hr = S_OK;
        m_pContext->RequestEditSession(m_pService->GetClientId(), session,
            TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        session->Release();
    }
    return S_OK;
}

STDMETHODIMP CEthiopicCandidateListUIElement::Abort()
{
    m_pService->HideSuggestions();
    return S_OK;
}

// --- CEthiopicTextService ---

CEthiopicTextService::CEthiopicTextService()
    : m_cRef(1)
    , m_tfClientId(0)
    , m_dwActivateFlags(0)
    , m_activated(false)
    , m_dwThreadMgrCookie(TF_INVALID_COOKIE)
    , m_dwTextEditCookie(TF_INVALID_COOKIE)
    , m_pTextEditSinkContext(nullptr)
    , m_pThreadMgr(nullptr)
    , m_pComposition(nullptr)
    , m_gaDisplayAttributeInput(TF_INVALID_GUIDATOM)
{
    elog("CEthiopicTextService ctor");
    DllAddRef();
    tlog("CEthiopicTextService ctor begin");
    try {
        // Load data files relative to DLL location. find_data_file() searches:
        //   1. ./data/amharic/am-sera.json (same dir as DLL)
        //   2. ../../data/amharic/am-sera.json (build tree layout)
        //   3. MAPPING_SOURCE_DIR fallback (compile-time path)
        // Names, wordlist, and bigrams are loaded from the same directory as
        // am-sera.json by substituting filename in the resolved path.
        std::string data_path = find_data_file();
        m_mapping = ethio::load_mapping_file(data_path);
        if (!m_mapping.states.empty()) {
            m_core.load_trie(m_mapping.states[0].trie);

            // Load names.json from the same directory as am-sera.json
            std::string names_path = data_path;
            size_t pos = names_path.rfind("am-sera.json");
            if (pos != std::string::npos)
                names_path.replace(pos, 13, "names.json");
            std::ifstream nfs(names_path);
            if (nfs.is_open()) {
                ethio::Json names_root;
                nfs >> names_root;
                const auto &states = names_root["states"];
                int count = 0;
                for (auto sit = states.begin(); sit != states.end(); ++sit) {
                    const auto &map_entries = sit.value()["map"];
                    for (auto mit = map_entries.begin();
                         mit != map_entries.end(); ++mit) {
                        std::string key = mit.key();
                        std::string val = mit.value();
                        if (val.empty())
                            m_mapping.states[0].trie.insert_commit(key);
                        else
                            m_mapping.states[0].trie.insert(key, val);
                        count++;
                    }
                }
                char b[64];
                snprintf(b, sizeof(b),
                         "CEthiopicTextService: loaded %d names", count);
                elog(b);
                m_core.load_trie(m_mapping.states[0].trie);
            }

            // Load wordlist.json for candidate suggestions
            std::string wordlist_path = data_path;
            size_t wp = wordlist_path.rfind("am-sera.json");
            if (wp != std::string::npos)
                wordlist_path.replace(wp, 13, "wordlist.json");
            try {
                m_wordlist.load(wordlist_path);
                char b[64];
                snprintf(b, sizeof(b),
                         "CEthiopicTextService: loaded %zu words from wordlist",
                         m_wordlist.size());
                elog(b);
            } catch (const std::exception &e) {
                char b[128];
                snprintf(b, sizeof(b),
                         "CEthiopicTextService: wordlist load error: %s", e.what());
                elog(b);
            }

            // Load bigrams.json for next-word prediction
            std::string bigrams_path = data_path;
            size_t bp = bigrams_path.rfind("am-sera.json");
            if (bp != std::string::npos)
                bigrams_path.replace(bp, 13, "bigrams.json");
            try {
                m_wordlist.load_bigrams(bigrams_path);
                elog("CEthiopicTextService: loaded bigrams");
            } catch (const std::exception &e) {
                char b[128];
                snprintf(b, sizeof(b),
                         "CEthiopicTextService: bigrams load error: %s", e.what());
                elog(b);
            }
        }
    } catch (...) {
        tlog("CEthiopicTextService ctor EXCEPTION");
    }
    tlog("CEthiopicTextService ctor end");
}

CEthiopicTextService::~CEthiopicTextService()
{
    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    if (m_pThreadMgr) {
        m_pThreadMgr->Release();
        m_pThreadMgr = nullptr;
    }
    DllRelease();
}

STDMETHODIMP CEthiopicTextService::QueryInterface(REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, __uuidof(ITfTextInputProcessor))) {
        *ppv = static_cast<ITfTextInputProcessor *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfTextInputProcessorEx))) {
        *ppv = static_cast<ITfTextInputProcessorEx *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfKeyEventSink))) {
        *ppv = static_cast<ITfKeyEventSink *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfTextEditSink))) {
        *ppv = static_cast<ITfTextEditSink *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfThreadMgrEventSink))) {
        *ppv = static_cast<ITfThreadMgrEventSink *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfCompositionSink))) {
        *ppv = static_cast<ITfCompositionSink *>(this);
    } else if (IsEqualIID(riid, __uuidof(ITfDisplayAttributeProvider))) {
        *ppv = static_cast<ITfDisplayAttributeProvider *>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CEthiopicTextService::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CEthiopicTextService::Release()
{
    LONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0)
        delete this;
    return (ULONG)cr;
}

BOOL CEthiopicTextService::_InitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
            (void **)&pKeystrokeMgr))) {
        elog("_InitKeyEventSink: QI for ITfKeystrokeMgr FAILED");
        return FALSE;
    }

    HRESULT hr = pKeystrokeMgr->AdviseKeyEventSink(
        m_tfClientId, static_cast<ITfKeyEventSink *>(this), TRUE);
    pKeystrokeMgr->Release();

    char buf[64];
    snprintf(buf, sizeof(buf), "_InitKeyEventSink: hr=0x%08lX", (unsigned long)hr);
    elog(buf);
    return SUCCEEDED(hr);
}

void CEthiopicTextService::_UninitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
            (void **)&pKeystrokeMgr)))
        return;

    pKeystrokeMgr->UnadviseKeyEventSink(m_tfClientId);
    pKeystrokeMgr->Release();
}

BOOL CEthiopicTextService::_InitTextEditSink(ITfDocumentMgr *pDocMgr)
{
    ITfSource *pSource = nullptr;

    if (m_dwTextEditCookie != TF_INVALID_COOKIE) {
        if (m_pTextEditSinkContext &&
            SUCCEEDED(m_pTextEditSinkContext->QueryInterface(IID_ITfSource,
                    (void **)&pSource))) {
            pSource->UnadviseSink(m_dwTextEditCookie);
            pSource->Release();
        }
        if (m_pTextEditSinkContext) {
            m_pTextEditSinkContext->Release();
            m_pTextEditSinkContext = nullptr;
        }
        m_dwTextEditCookie = TF_INVALID_COOKIE;
    }

    if (!pDocMgr)
        return TRUE;

    ITfContext *pContext = nullptr;
    if (FAILED(pDocMgr->GetTop(&pContext)))
        return FALSE;

    if (!pContext)
        return TRUE;

    BOOL ret = FALSE;
    if (SUCCEEDED(pContext->QueryInterface(IID_ITfSource, (void **)&pSource))) {
        HRESULT hr = pSource->AdviseSink(IID_ITfTextEditSink,
            static_cast<ITfTextEditSink *>(this), &m_dwTextEditCookie);
        if (SUCCEEDED(hr)) {
            m_pTextEditSinkContext = pContext;
            m_pTextEditSinkContext->AddRef();
            ret = TRUE;
            char buf[64];
            snprintf(buf, sizeof(buf), "_InitTextEditSink: hr=0x%08lX", (unsigned long)hr);
            elog(buf);
        } else {
            m_dwTextEditCookie = TF_INVALID_COOKIE;
            char buf[64];
            snprintf(buf, sizeof(buf), "_InitTextEditSink FAILED: hr=0x%08lX", (unsigned long)hr);
            elog(buf);
        }
        pSource->Release();
    }

    pContext->Release();
    return ret;
}

void CEthiopicTextService::_UninitTextEditSink()
{
    if (m_dwTextEditCookie != TF_INVALID_COOKIE && m_pTextEditSinkContext) {
        ITfSource *pSource = nullptr;
        if (SUCCEEDED(m_pTextEditSinkContext->QueryInterface(IID_ITfSource,
                (void **)&pSource))) {
            pSource->UnadviseSink(m_dwTextEditCookie);
            pSource->Release();
        }
        m_pTextEditSinkContext->Release();
        m_pTextEditSinkContext = nullptr;
        m_dwTextEditCookie = TF_INVALID_COOKIE;
    }
}

BOOL CEthiopicTextService::_InitThreadMgrEventSink()
{
    ITfSource *pSource = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfSource,
            (void **)&pSource)))
        return FALSE;

    HRESULT hr = pSource->AdviseSink(
        IID_ITfThreadMgrEventSink,
        static_cast<ITfThreadMgrEventSink *>(this),
        &m_dwThreadMgrCookie);
    pSource->Release();
    return SUCCEEDED(hr);
}

void CEthiopicTextService::_UninitThreadMgrEventSink()
{
    if (m_dwThreadMgrCookie == TF_INVALID_COOKIE)
        return;

    ITfSource *pSource = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfSource,
            (void **)&pSource)))
        return;

    pSource->UnadviseSink(m_dwThreadMgrCookie);
    pSource->Release();
    m_dwThreadMgrCookie = TF_INVALID_COOKIE;
}

BOOL CEthiopicTextService::_InitDisplayAttributeGuidAtom()
{
    ITfCategoryMgr *pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
            CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
            (void **)&pCategoryMgr);
    if (FAILED(hr)) {
        elog("_InitDisplayAttributeGuidAtom: CoCreateInstance FAILED");
        return FALSE;
    }

    hr = pCategoryMgr->RegisterGUID(
        GUID_DisplayAttributeInput, &m_gaDisplayAttributeInput);
    pCategoryMgr->Release();
    if (FAILED(hr)) {
        elog("_InitDisplayAttributeGuidAtom: RegisterGUID FAILED");
        return FALSE;
    }
    elog("_InitDisplayAttributeGuidAtom OK");
    return TRUE;
}

BOOL CEthiopicTextService::_IsKeyboardOpen()
{
    return m_activated;
}

STDMETHODIMP CEthiopicTextService::ActivateEx(ITfThreadMgr *pThreadMgr,
                                               TfClientId tfClientId,
                                               DWORD dwFlags)
{
    if (IsDllUnloading()) {
        elog("ActivateEx: DLL is unloading, refusing");
        return E_FAIL;
    }
    elog("ActivateEx begin");
    tlog("ActivateEx begin");
    m_pThreadMgr = pThreadMgr;
    m_pThreadMgr->AddRef();
    m_tfClientId = tfClientId;
    m_dwActivateFlags = dwFlags;
    m_activated = true;

    if (!_InitThreadMgrEventSink()) {
        elog("ActivateEx: _InitThreadMgrEventSink FAILED");
        goto ExitError;
    }

    {
        ITfDocumentMgr *pDocMgrFocus = nullptr;
        if (SUCCEEDED(m_pThreadMgr->GetFocus(&pDocMgrFocus)) && pDocMgrFocus) {
            _InitTextEditSink(pDocMgrFocus);
            pDocMgrFocus->Release();
        }
    }

    if (!_InitKeyEventSink()) {
        elog("ActivateEx: _InitKeyEventSink FAILED");
        goto ExitError;
    }
    if (!_InitDisplayAttributeGuidAtom()) {
        elog("ActivateEx: _InitDisplayAttributeGuidAtom FAILED");
        goto ExitError;
    }

    elog("ActivateEx OK");
    tlog("ActivateEx end");
    return S_OK;

ExitError:
    elog("ActivateEx FAILED");
    tlog("ActivateEx FAILED");
    Deactivate();
    return E_FAIL;
}

STDMETHODIMP CEthiopicTextService::Deactivate()
{
    if (IsDllUnloading()) {
        elog("Deactivate: DLL is unloading, skipping cleanup");
        return S_OK;
    }
    elog("Deactivate called");
    tlog("Deactivate");
    m_activated = false;

    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }

    if (m_core.passthrough()) {
        m_core.toggle_passthrough();
        elog("Deactivate: reset passthrough state to OFF");
    }

    _UninitKeyEventSink();
    _UninitTextEditSink();
    _UninitThreadMgrEventSink();

    if (m_pThreadMgr) {
        m_pThreadMgr->Release();
        m_pThreadMgr = nullptr;
    }

    m_tfClientId = 0;
    m_dwActivateFlags = 0;
    m_gaDisplayAttributeInput = TF_INVALID_GUIDATOM;
    m_currentPreedit.clear();
    m_core.reset();
    _EndCandidateUI();
    HideCandidatePopup();
    HideSuggestions();
    m_wordBuffer.clear();
    m_lastWord.clear();
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnSetFocus(BOOL fForeground)
{
#ifndef NDEBUG
    char b[64];
    snprintf(b, sizeof(b), "[EthiopicTSF] KeyEventSink::OnSetFocus fg=%d\n", (int)fForeground);
    OutputDebugStringA(b);
    elog(b);
#endif

    if (!fForeground) {
        HideCandidatePopup();
        HideSuggestions();
    }
    return S_OK;
}

static bool HasModifier()
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
           (GetAsyncKeyState(VK_MENU) & 0x8000) ||
           (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
           (GetAsyncKeyState(VK_RWIN) & 0x8000);
}

static bool is_key_eaten(WPARAM wParam, LPARAM lParam)
{
    if (HasModifier()) {
        char buf[80];
        snprintf(buf, sizeof(buf), "is_key_eaten: modifier pressed, pass through (wParam=0x%llX)",
                 (unsigned long long)wParam);
        elog(buf);
        return false;
    }

    char utf8[8] = {};
    vk_to_utf8(wParam, lParam, utf8, sizeof(utf8));

    if (utf8[0] == '\0') {
        return false;
    }

    unsigned char ch = (unsigned char)utf8[0];
    if (ch < 0x20 || ch == 0x7F) {
        char buf[80];
        snprintf(buf, sizeof(buf), "is_key_eaten: control char 0x%02X, pass through", ch);
        elog(buf);
        return false;
    }

    return true;
}

// TSF calls OnTestKeyDown before OnKeyDown to let the IME declare whether
// it wants to consume a key. Returning *pfEaten=TRUE reserves the key for
// OnKeyDown; FALSE lets TSF route it elsewhere first (app shortcuts, etc.).
// We pre-claim any key that could plausibly produce Ethiopic output so that
// candidate navigation (1-8, arrows, Tab, etc.) stays within the IME.
STDMETHODIMP CEthiopicTextService::OnTestKeyDown(ITfContext *, WPARAM wParam,
                                                  LPARAM lParam, BOOL *pfEaten)
{
    if (!_IsKeyboardOpen()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (m_core.passthrough()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Eat candidate navigation keys when popup is visible
    if (IsCandidatePopupVisible()) {
        if (wParam == VK_TAB || wParam == VK_RETURN || wParam == VK_ESCAPE ||
            wParam == VK_UP || wParam == VK_DOWN ||
            wParam == VK_LEFT || wParam == VK_RIGHT) {
            *pfEaten = TRUE;
            return S_OK;
        }
        if (wParam >= '1' && wParam <= '8') {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    // Eat candidate navigation keys when suggestions are visible (inline mode).
    if (!m_candidates.empty() && !IsCandidatePopupVisible()) {
        if ((wParam == VK_TAB || wParam == VK_RETURN || wParam == VK_ESCAPE)) {
            if (IsCandidateUIActive()) {
                *pfEaten = FALSE;
            } else {
                *pfEaten = TRUE;
            }
            return S_OK;
        }
        if (wParam >= '1' && wParam <= '8') {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    *pfEaten = is_key_eaten(wParam, lParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnTestKeyUp(ITfContext *, WPARAM,
                                                LPARAM, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

// Main key-event pipeline for the TSF IME. Processing order:
//  1. Ctrl+Shift → toggle passthrough mode
//  2. Ctrl+Space  → trigger word suggestions
//  3. Candidate navigation (arrows, Tab, 1-8, Enter, Escape)
//  4. Passthrough check (pass key through if passthrough mode active)
//  5. VK_BACK     → reset composing preedit
//  6. vk_to_utf8  → libethio::Engine::filter() → commit + preedit update
// All text changes go through CEthiopicEditSession (TSF edit cookie contract).
STDMETHODIMP CEthiopicTextService::OnKeyDown(ITfContext *pContext,
                                               WPARAM wParam, LPARAM lParam,
                                               BOOL *pfEaten)
{
    tlog("OnKeyDown begin");

    if (!_IsKeyboardOpen()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    {
        bool ctrlHeld  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        bool shiftKey  = (wParam == VK_SHIFT  || wParam == VK_LSHIFT  || wParam == VK_RSHIFT);
        bool ctrlKey   = (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL);

        if ((shiftKey && ctrlHeld) || (ctrlKey && shiftHeld)) {
            m_core.toggle_passthrough();
            m_core.reset();
            m_currentPreedit.clear();
            HideCandidatePopup();
            HideSuggestions();
            if (m_pComposition) {
                CEthiopicEditSession *session = new CEthiopicEditSession(
                    this, "", "", false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }

            char b[96];
            snprintf(b, sizeof(b),
                     "OnKeyDown: Ctrl+Shift toggle -> passthrough=%s",
                     m_core.passthrough() ? "ON" : "OFF");
            elog(b);

            *pfEaten = TRUE;
            return S_OK;
        }

        // Ctrl+Space: finish composition and show suggestions
        if ((wParam == VK_SPACE && ctrlHeld) || (ctrlKey && (GetAsyncKeyState(VK_SPACE) & 0x8000))) {
            elog("OnKeyDown: Ctrl+Space -> show suggestions");
            m_core.finish_composition();
            std::string prod = m_core.flush();
            if (!prod.empty()) _TrackWord(prod);
            ShowSuggestions();
            std::string preedit;
            if (!m_candidates.empty()) {
                ShowCandidatePopup(pContext);
                _BeginCandidateUI(pContext);
                preedit = "";
            }
            CEthiopicEditSession *session = new CEthiopicEditSession(
                this, prod, preedit, IsCandidatePopupVisible());
            HRESULT hr = 0;
            pContext->RequestEditSession(m_tfClientId, session,
                TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
            session->Release();
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    if (m_core.passthrough()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Handle candidate suggestion navigation
    if (!m_candidates.empty()) {
        bool popup = IsCandidatePopupVisible();
        {
            char b[80];
            snprintf(b, sizeof(b), "OnKeyDown: candidate mode wParam=0x%x n=%zu idx=%d popup=%d",
                     (int)wParam, m_candidates.size(), m_candidateIndex, popup ? 1 : 0);
            elog(b);
        }

        // Arrow Down: next candidate
        if (wParam == VK_DOWN) {
            m_candidateIndex = (m_candidateIndex + 1) % (int)m_candidates.size();
            if (popup) {
                RefreshCandidatePopup();
            } else {
                std::string preedit = _BuildCandidateDisplay();
                CEthiopicEditSession *session = new CEthiopicEditSession(this, "", preedit, false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
            *pfEaten = TRUE;
            return S_OK;
        }
        // Arrow Up: previous candidate
        if (wParam == VK_UP) {
            m_candidateIndex = (m_candidateIndex - 1 + (int)m_candidates.size()) % (int)m_candidates.size();
            if (popup) {
                RefreshCandidatePopup();
            } else {
                std::string preedit = _BuildCandidateDisplay();
                CEthiopicEditSession *session = new CEthiopicEditSession(this, "", preedit, false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
            *pfEaten = TRUE;
            return S_OK;
        }

        // Tab: cycle through candidates
        if (wParam == VK_TAB) {
            if (IsCandidateUIActive() && !popup) {
                elog("OnKeyDown: candidate TAB -> let TSF handle");
                *pfEaten = FALSE;
                return S_OK;
            }
            m_candidateIndex = (m_candidateIndex + 1) % (int)m_candidates.size();
            if (popup) {
                RefreshCandidatePopup();
            }
            { char b[64]; snprintf(b, sizeof(b), "OnKeyDown: candidate TAB -> idx=%d", m_candidateIndex); elog(b); }
            if (!popup) {
                std::string preedit = _BuildCandidateDisplay();
                CEthiopicEditSession *session = new CEthiopicEditSession(this, "", preedit, false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
            *pfEaten = TRUE;
            return S_OK;
        }
        // Return/Enter: accept selected candidate
        if (wParam == VK_RETURN) {
            if (IsCandidateUIActive() && !popup) {
                elog("OnKeyDown: candidate RETURN -> let TSF handle");
                *pfEaten = FALSE;
                return S_OK;
            }
            { char b[64]; snprintf(b, sizeof(b), "OnKeyDown: RETURN -> AcceptCandidate(%d)", m_candidateIndex); elog(b); }
            AcceptCandidate(m_candidateIndex);
            std::string prod = m_core.flush();
            if (!prod.empty()) _TrackWord(prod);
            ShowSuggestions();
            if (!m_candidates.empty()) {
                if (popup) {
                    ShowCandidatePopup(pContext);
                }
                std::string preedit = popup ? "" : _BuildCandidateDisplay();
                CEthiopicEditSession *session = new CEthiopicEditSession(
                    this, prod, preedit, popup);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            } else {
                HideCandidatePopup();
                CEthiopicEditSession *session = new CEthiopicEditSession(
                    this, prod, "", false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
            *pfEaten = TRUE;
            return S_OK;
        }
        if (wParam >= '1' && wParam <= '8') {
            int idx = wParam - '1';
            { char b[64]; snprintf(b, sizeof(b), "OnKeyDown: DIGIT %d -> AcceptCandidate(%d)", idx + 1, idx); elog(b); }
            AcceptCandidate(idx);
            std::string prod = m_core.flush();
            if (!prod.empty()) _TrackWord(prod);
            ShowSuggestions();
            if (!m_candidates.empty()) {
                if (popup) {
                    ShowCandidatePopup(pContext);
                }
                std::string preedit = popup ? "" : _BuildCandidateDisplay();
                _BeginCandidateUI(pContext);
                CEthiopicEditSession *session = new CEthiopicEditSession(
                    this, prod, preedit, popup);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            } else {
                HideCandidatePopup();
                _EndCandidateUI();
                CEthiopicEditSession *session = new CEthiopicEditSession(
                    this, prod, "", false);
                HRESULT hr = 0;
                pContext->RequestEditSession(m_tfClientId, session,
                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
            *pfEaten = TRUE;
            return S_OK;
        }
        if (wParam == VK_ESCAPE) {
            elog("OnKeyDown: candidate ESCAPE -> dismiss");
            _EndCandidateUI();
            HideCandidatePopup();
            HideSuggestions();
            std::string preedit(m_core.composing());
            CEthiopicEditSession *session = new CEthiopicEditSession(
                this, "", preedit, false);
            HRESULT hr = 0;
            pContext->RequestEditSession(m_tfClientId, session,
                TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
            session->Release();
            *pfEaten = TRUE;
            return S_OK;
        }
        // Any other key: dismiss candidates and fall through
        { char b[64]; snprintf(b, sizeof(b), "OnKeyDown: other key 0x%x -> dismiss candidates", (int)wParam); elog(b); }
        _EndCandidateUI();
        HideCandidatePopup();
        HideSuggestions();
    }

    if (wParam == VK_BACK) {
        if (!m_currentPreedit.empty()) {
            elog("OnKeyDown: VK_BACK with preedit -> reset composition");
            m_core.reset();
            m_currentPreedit.clear();
            HideCandidatePopup();
            HideSuggestions();
            CEthiopicEditSession *session = new CEthiopicEditSession(
                this, "", "", false);
            HRESULT hr = 0;
            pContext->RequestEditSession(m_tfClientId, session,
                TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
            session->Release();
            *pfEaten = TRUE;
        } else {
            elog("OnKeyDown: VK_BACK, nothing composing -> pass through");
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    if (!is_key_eaten(wParam, lParam)) {
        *pfEaten = FALSE;
        return S_OK;
    }

    char utf8[8] = {};
    vk_to_utf8(wParam, lParam, utf8, sizeof(utf8));

    if (m_testMode) {
        ProcessKeyUtf8(utf8);
        *pfEaten = TRUE;
        return S_OK;
    }

    char b[256];
    snprintf(b, sizeof(b),
             "OnKeyDown: key='%s' pre-state: composing='%s' pending='%s'",
             utf8, m_core.composing().data(), m_core.produced_text().data());
    elog(b);

    bool handled = m_core.filter(std::string_view(utf8));
    if (!handled) {
        snprintf(b, sizeof(b), "OnKeyDown: filter returned FALSE for '%s', appending to produced",
                 utf8);
        elog(b);
        m_core.append_produced(utf8);
    }

    std::string produced = m_core.flush();
    std::string composing(m_core.composing());

    snprintf(b, sizeof(b),
             "OnKeyDown: result produced='%s' composing='%s'",
             produced.c_str(), composing.c_str());
    elog(b);

    // Track committed word for suggestion context
    if (!produced.empty()) {
        _TrackWord(produced);
    }

    // Build preedit: show composing text OR show candidate popup, never both
    std::string preedit;
    if (!composing.empty()) {
        // Still mid-composition - just show composing text, no candidates yet
        preedit = composing;
        HideCandidatePopup();
        HideSuggestions();
    } else {
        // Composition complete - generate and show suggestions
        ShowSuggestions();
        if (!m_candidates.empty()) {
            // Show popup window + optional TSF UI element
            ShowCandidatePopup(pContext);
            _BeginCandidateUI(pContext);
            preedit = ""; // popup handles display
        }
    }

    CEthiopicEditSession *session = new CEthiopicEditSession(
        this, produced, preedit, IsCandidatePopupVisible());
    HRESULT hr = S_OK;
    pContext->RequestEditSession(m_tfClientId, session,
        TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    session->Release();

    tlog("OnKeyDown end (committed)");
    *pfEaten = TRUE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnKeyUp(ITfContext *, WPARAM, LPARAM,
                                            BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnPreservedKey(ITfContext *, REFGUID,
                                                   BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnEndEdit(ITfContext *, TfEditCookie,
                                              ITfEditRecord *)
{
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnInitDocumentMgr(ITfDocumentMgr *)
{
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnUninitDocumentMgr(ITfDocumentMgr *)
{
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnSetFocus(ITfDocumentMgr *pDimFocus,
                                               ITfDocumentMgr *)
{
    _InitTextEditSink(pDimFocus);
    if (!pDimFocus) {
        HideCandidatePopup();
        HideSuggestions();
    }
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnPushContext(ITfContext *)
{
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnPopContext(ITfContext *)
{
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnCompositionTerminated(TfEditCookie,
                                                            ITfComposition *)
{
    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    m_currentPreedit.clear();
    m_core.reset();
    _EndCandidateUI();
    HideCandidatePopup();
    HideSuggestions();
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo **ppEnum)
{
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = new (std::nothrow) CEthiopicEnumDisplayAttributeInfo();
    return *ppEnum ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CEthiopicTextService::GetDisplayAttributeInfo(
    REFGUID guidInfo, ITfDisplayAttributeInfo **ppInfo)
{
    if (!ppInfo) return E_INVALIDARG;
    *ppInfo = nullptr;

    if (IsEqualGUID(guidInfo, GUID_DisplayAttributeInput)) {
        *ppInfo = new (std::nothrow) CEthiopicDisplayAttributeInfo();
        return *ppInfo ? S_OK : E_OUTOFMEMORY;
    }

    return E_INVALIDARG;
}

void CEthiopicTextService::StartComposition(TfEditCookie ec,
                                             ITfContext *pContext,
                                             const std::string &text)
{
    char b[256];
    ITfContext *pCtx = pContext;
    if (!pCtx) {
        ITfDocumentMgr *pDocMgr = nullptr;
        if (m_pThreadMgr &&
            SUCCEEDED(m_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr) {
            pDocMgr->GetTop(&pCtx);
            pDocMgr->Release();
        }
    }
    if (!pCtx) {
        elog("StartComposition: no context, returning");
        return;
    }

    if (!m_pComposition) {
        snprintf(b, sizeof(b), "StartComposition: creating new composition for '%s'",
                 text.c_str());
        elog(b);
        ITfContextComposition *pContextComp = nullptr;
        if (SUCCEEDED(pCtx->QueryInterface(
                __uuidof(ITfContextComposition),
                (void **)&pContextComp)) && pContextComp) {
            TF_SELECTION sel = {};
            ULONG fetched = 0;
            if (SUCCEEDED(pCtx->GetSelection(ec, TF_DEFAULT_SELECTION,
                    1, &sel, &fetched)) && fetched > 0 && sel.range) {
                pContextComp->StartComposition(ec, sel.range, this,
                                               &m_pComposition);
                sel.range->Release();
                elog("StartComposition: StartComposition succeeded");
            } else {
                elog("StartComposition: GetSelection failed or empty");
            }
            pContextComp->Release();
        } else {
            elog("StartComposition: QI for ITfContextComposition FAILED");
        }
    }

    if (m_pComposition) {
        ITfRange *pRange = nullptr;
        if (SUCCEEDED(m_pComposition->GetRange(&pRange)) && pRange) {
            std::wstring wtext = utf8_to_wstring(text);
            snprintf(b, sizeof(b), "StartComposition: SetText '%s' on composition range",
                     text.c_str());
            elog(b);
            pRange->SetText(ec, 0, wtext.c_str(), (LONG)wtext.size());

            if (m_gaDisplayAttributeInput != TF_INVALID_GUIDATOM) {
                ITfProperty *pProp = nullptr;
                if (SUCCEEDED(pCtx->GetProperty(GUID_PROP_ATTRIBUTE,
                        &pProp)) && pProp) {
                    VARIANT var;
                    var.vt = VT_I4;
                    var.lVal = (LONG)m_gaDisplayAttributeInput;
                    pProp->SetValue(ec, pRange, &var);
                    pProp->Release();
                }
            }

            // Reposition cursor to end of preedit text
            pRange->Collapse(ec, TF_ANCHOR_END);
            TF_SELECTION selUpdate = {};
            selUpdate.range = pRange;
            selUpdate.style.ase = TF_AE_NONE;
            selUpdate.style.fInterimChar = FALSE;
            pCtx->SetSelection(ec, 1, &selUpdate);

            pRange->Release();
        }
        m_currentPreedit = text;
    }

    if (!pContext)
        pCtx->Release();
}

void CEthiopicTextService::UpdateComposition(TfEditCookie ec,
                                              const std::string &text)
{
    if (!m_pComposition) {
        elog("UpdateComposition: no composition, skipping");
        return;
    }

    char b[256];
    snprintf(b, sizeof(b), "UpdateComposition: text='%s'",
             text.c_str());
    elog(b);

    ITfRange *pRange = nullptr;
    if (SUCCEEDED(m_pComposition->GetRange(&pRange)) && pRange) {
        pRange->SetText(ec, 0, L"", 0);
        if (!text.empty()) {
            std::wstring wtext = utf8_to_wstring(text);
            pRange->SetText(ec, 0, wtext.c_str(), (LONG)wtext.size());
        }
        pRange->Release();
    }
    m_currentPreedit = text;
}

void CEthiopicTextService::EndComposition(TfEditCookie ec, ITfContext *)
{
    if (m_pComposition) {
        elog("EndComposition: ending composition");
        m_pComposition->EndComposition(ec);
        m_pComposition->Release();
        m_pComposition = nullptr;
    } else {
        elog("EndComposition: no composition to end");
    }
    m_currentPreedit.clear();
}

void CEthiopicTextService::ResetTest()
{
    m_testCommitted.clear();
    m_testPreedit.clear();
    m_core.reset();
}

void CEthiopicTextService::ProcessKeyUtf8(const char *utf8)
{
    if (!utf8 || utf8[0] == '\0') return;

    bool handled = m_core.filter(std::string_view(utf8));
    if (!handled) {
        m_core.append_produced(utf8);
    }

    std::string produced = m_core.flush();
    std::string composing(m_core.composing());

    m_testCommitted += produced;
    m_testPreedit = composing;
}

// Ethiopic punctuation characters (፡ through ፦, U+1361–U+1366) act as word
// boundaries alongside ASCII space and newline. This is used by _TrackWord to
// segment the accumulated word buffer for bigram-based next-word prediction.
bool CEthiopicTextService::_IsWordBoundary(const std::string &s)
{
    return s == " " || s == "\n" ||
           s == "\xe1\x8d\xa1" ||  // ፡ (ethiopic wordspace)
           s == "\xe1\x8d\xa2" ||  // ።
           s == "\xe1\x8d\xa3" ||  // ፣
           s == "\xe1\x8d\xa4" ||  // ፤
           s == "\xe1\x8d\xa5" ||  // ፥
           s == "\xe1\x8d\xa6";     // ፦
}

void CEthiopicTextService::_TrackWord(const std::string &text)
{
    if (_IsWordBoundary(text)) {
        m_lastWord = m_wordBuffer;
        m_wordBuffer.clear();
    } else if (!text.empty() && text.back() == ' ') {
        std::string word = text.substr(0, text.size() - 1);
        if (!word.empty())
            m_wordBuffer += word;
        m_lastWord = m_wordBuffer;
        m_wordBuffer.clear();
    } else {
        m_wordBuffer += text;
    }
}

void CEthiopicTextService::ShowSuggestions()
{
    std::vector<std::string> results;

    if (!m_wordBuffer.empty()) {
        results = m_wordlist.suggest(m_wordBuffer, 8);
    } else if (!m_lastWord.empty()) {
        results = m_wordlist.suggest_next(m_lastWord, 8);
    }

    m_candidates = std::move(results);
    m_candidateIndex = 0;

    char b[128];
    snprintf(b, sizeof(b), "ShowSuggestions: wb='%s' lw='%s' count=%zu",
             m_wordBuffer.c_str(), m_lastWord.c_str(), m_candidates.size());
    elog(b);
}

void CEthiopicTextService::HideSuggestions()
{
    m_candidates.clear();
    m_candidateIndex = 0;
    HideCandidatePopup();
}

HRESULT CEthiopicTextService::_BeginCandidateUI(ITfContext *pContext)
{
    if (m_candidates.empty()) {
        _EndCandidateUI();
        return S_FALSE;
    }

    if (!pContext) return E_POINTER;
    if (!m_pThreadMgr) return E_FAIL;

    ITfUIElementMgr *pUIMgr = nullptr;
    HRESULT hr = m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr,
        (void **)&pUIMgr);
    if (FAILED(hr) || !pUIMgr) {
        elog("_BeginCandidateUI: QI for ITfUIElementMgr FAILED");
        return hr;
    }

    // If element already exists, just update it
    if (m_pCandidateUIElement && m_uiElementId != TF_INVALID_UIELEMENTID) {
        elog("_BeginCandidateUI: element exists, updating");
        pUIMgr->UpdateUIElement(m_uiElementId);
        pUIMgr->Release();
        return S_OK;
    }

    // Destroy old element if any (stale state)
    _EndCandidateUI();

    // Update context on existing element or create new one
    if (m_pCandidateUIElement) {
        m_pCandidateUIElement->UpdateContext(pContext);
    } else {
        m_pCandidateUIElement = new (std::nothrow)
            CEthiopicCandidateListUIElement(this, pContext);
        if (!m_pCandidateUIElement) {
            pUIMgr->Release();
            return E_OUTOFMEMORY;
        }
    }

    BOOL show = FALSE;
    DWORD newId = TF_INVALID_UIELEMENTID;
    hr = pUIMgr->BeginUIElement(
        static_cast<ITfCandidateListUIElementBehavior *>(m_pCandidateUIElement),
        &show, &newId);
    m_uiElementId = newId;
    pUIMgr->Release();

    if (FAILED(hr)) {
        char b[80];
        snprintf(b, sizeof(b),
                 "_BeginCandidateUI: BeginUIElement FAILED hr=0x%08lX",
                 (unsigned long)hr);
        elog(b);
        m_uiElementId = TF_INVALID_UIELEMENTID;
        return hr;
    }

    m_pCandidateUIElement->Show(show);
    char b[128];
    snprintf(b, sizeof(b),
             "_BeginCandidateUI: OK, elementId=%lu, count=%zu, show=%d",
             (unsigned long)m_uiElementId, m_candidates.size(), (int)show);
    elog(b);
    return S_OK;
}

HRESULT CEthiopicTextService::_UpdateCandidateUI()
{
    if (m_uiElementId == TF_INVALID_UIELEMENTID)
        return S_FALSE;
    if (!m_pThreadMgr)
        return E_FAIL;

    ITfUIElementMgr *pUIMgr = nullptr;
    HRESULT hr = m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr,
        (void **)&pUIMgr);
    if (FAILED(hr) || !pUIMgr) return hr;

    hr = pUIMgr->UpdateUIElement(m_uiElementId);
    pUIMgr->Release();
    elog("_UpdateCandidateUI: OK");
    return hr;
}

void CEthiopicTextService::_EndCandidateUI()
{
    if (m_uiElementId != TF_INVALID_UIELEMENTID && m_pThreadMgr) {
        ITfUIElementMgr *pUIMgr = nullptr;
        HRESULT hr = m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr,
            (void **)&pUIMgr);
        if (SUCCEEDED(hr) && pUIMgr) {
            pUIMgr->EndUIElement(m_uiElementId);
            pUIMgr->Release();
            elog("_EndCandidateUI: OK");
        }
    }
    m_uiElementId = TF_INVALID_UIELEMENTID;
    if (m_pCandidateUIElement) {
        m_pCandidateUIElement->Release();
        m_pCandidateUIElement = nullptr;
    }
}

void CEthiopicTextService::AcceptCandidate(int index)
{
    if (index < 0 || index >= (int)m_candidates.size()) return;

    std::string chosen = m_candidates[index];
    std::string prefix = m_wordBuffer;

    if (!prefix.empty() &&
        chosen.size() >= prefix.size() &&
        chosen.compare(0, prefix.size(), prefix) == 0) {
        std::string suffix = chosen.substr(prefix.size());
        m_core.append_produced(suffix);
    } else {
        m_core.append_produced(chosen);
    }
    m_core.append_produced(" ");

    m_wordBuffer.clear();
    HideSuggestions();
}

std::string CEthiopicTextService::FlushCandidate(int index)
{
    if (index < 0 || index >= (int)m_candidates.size()) return "";

    std::string chosen = m_candidates[index];
    std::string prefix = m_wordBuffer;

    if (!prefix.empty() &&
        chosen.size() >= prefix.size() &&
        chosen.compare(0, prefix.size(), prefix) == 0) {
        std::string suffix = chosen.substr(prefix.size());
        m_core.append_produced(suffix);
    } else {
        m_core.append_produced(chosen);
    }
    m_core.append_produced(" ");
    m_wordBuffer.clear();

    std::string produced = m_core.flush();
    if (!produced.empty()) {
        _TrackWord(produced);
    }
    return produced;
}

std::string CEthiopicTextService::_BuildCandidateDisplay() const
{
    if (m_candidates.empty()) return "";

    std::string display = "[";
    for (int i = 0; i < (int)m_candidates.size(); i++) {
        if (i == m_candidateIndex) display += ">";
        char num[8];
        snprintf(num, sizeof(num), "%d:", i + 1);
        display += num;
        display += m_candidates[i];
        if (i < (int)m_candidates.size() - 1)
            display += " | ";
    }
    display += "]";
    return display;
}
