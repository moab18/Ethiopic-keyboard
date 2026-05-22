#define INITGUID
#include <initguid.h>
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "ethio/json.hpp"
#include "ethio/mapping.h"

static HANDLE get_log_mutex()
{
    static HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\EthiopicTSF_LogMutex");
    return hMutex;
}

static void elog(const char *msg)
{
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

#ifdef __MINGW32__
template<> const GUID &__mingw_uuidof<ITfTextInputProcessorEx>() { return IID_ITfTextInputProcessorEx; }
template<> const GUID &__mingw_uuidof<ITfDisplayAttributeProvider>() { return IID_ITfDisplayAttributeProvider; }
#endif

static void tlog(const char *msg)
{
    const char *path = std::getenv("ETHIOPIC_TSF_LOG");
    if (!path) return;
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
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

    std::string path = dir + "/data/amharic/am-sera.json";
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        elog("find_data_file: found at data/amharic");
        return path;
    }

    path = dir + "/../../data/amharic/am-sera.json";
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        elog("find_data_file: found at ../../data/amharic");
        return path;
    }

    elog("find_data_file: using hardcoded MAPPING_SOURCE_DIR");
    return MAPPING_SOURCE_DIR "/amharic/am-sera.json";
}

static void vk_to_utf8(WPARAM wParam, LPARAM lParam, char *out, int out_size)
{
    out[0] = '\0';
    BYTE key_state[256] = {};
    GetKeyboardState(key_state);

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
                                           const std::string &preedit_text)
    : m_cRef(1)
    , m_pService(pService)
    , m_commitText(commit_text)
    , m_preeditText(preedit_text)
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
    return ++m_cRef;
}

STDMETHODIMP_(ULONG) CEthiopicTextService::Release()
{
    LONG cr = --m_cRef;
    if (m_cRef == 0)
        delete this;
    return cr;
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

BOOL CEthiopicTextService::_SetKeyboardOpen(BOOL fOpen)
{
    if (!m_pThreadMgr) return FALSE;

    ITfCompartmentMgr *pCompMgr = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr,
            (void **)&pCompMgr)))
        return FALSE;

    ITfCompartment *pComp = nullptr;
    HRESULT hr = pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                           &pComp);
    pCompMgr->Release();
    if (FAILED(hr)) return FALSE;

    VARIANT var;
    var.vt = VT_I4;
    var.lVal = fOpen ? 1 : 0;
    hr = pComp->SetValue(m_tfClientId, &var);
    pComp->Release();
    return SUCCEEDED(hr);
}

BOOL CEthiopicTextService::_IsKeyboardOpen()
{
    if (!m_pThreadMgr || !m_activated) {
        elog("_IsKeyboardOpen: FALSE (no thread mgr or not activated)");
        return FALSE;
    }

    ITfCompartmentMgr *pCompMgr = nullptr;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr,
            (void **)&pCompMgr))) {
        elog("_IsKeyboardOpen: FALSE (QI failed)");
        return FALSE;
    }

    ITfCompartment *pComp = nullptr;
    HRESULT hr = pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                           &pComp);
    pCompMgr->Release();
    if (FAILED(hr)) {
        elog("_IsKeyboardOpen: FALSE (GetCompartment failed)");
        return FALSE;
    }

    VARIANT var;
    VariantInit(&var);
    hr = pComp->GetValue(&var);
    pComp->Release();
    if (FAILED(hr)) {
        elog("_IsKeyboardOpen: FALSE (GetValue failed)");
        return FALSE;
    }

    BOOL open = (var.vt == VT_I4 && var.lVal != 0);
    VariantClear(&var);
    if (!open) elog("_IsKeyboardOpen: FALSE (compartment says closed)");
    return open ? TRUE : m_activated;
}

STDMETHODIMP CEthiopicTextService::ActivateEx(ITfThreadMgr *pThreadMgr,
                                               TfClientId tfClientId,
                                               DWORD dwFlags)
{
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
    if (!_SetKeyboardOpen(TRUE)) {
        elog("ActivateEx: _SetKeyboardOpen FAILED");
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
    elog("Deactivate called");
    tlog("Deactivate");
    m_activated = false;

    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }

    _SetKeyboardOpen(FALSE);
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
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnSetFocus(BOOL fForeground)
{
    char b[64];
    snprintf(b, sizeof(b), "[EthiopicTSF] KeyEventSink::OnSetFocus fg=%d\n", (int)fForeground);
    OutputDebugStringA(b);
    elog(b);
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnTestKeyDown(ITfContext *, WPARAM wParam,
                                                  LPARAM lParam, BOOL *pfEaten)
{
    if (!_IsKeyboardOpen()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    char utf8[8] = {};
    vk_to_utf8(wParam, lParam, utf8, sizeof(utf8));

    if (utf8[0] == '\0') {
        *pfEaten = FALSE;
        return S_OK;
    }

    *pfEaten = TRUE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnTestKeyUp(ITfContext *, WPARAM,
                                                LPARAM, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CEthiopicTextService::OnKeyDown(ITfContext *pContext,
                                               WPARAM wParam, LPARAM lParam,
                                               BOOL *pfEaten)
{
    tlog("OnKeyDown begin");

    if (!_IsKeyboardOpen()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (wParam == VK_BACK) {
        if (!m_currentPreedit.empty()) {
            elog("OnKeyDown: VK_BACK with preedit -> reset composition");
            m_core.reset();
            m_currentPreedit.clear();
            CEthiopicEditSession *session = new CEthiopicEditSession(
                this, "", "");
            HRESULT hr = S_OK;
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

    char utf8[8] = {};
    vk_to_utf8(wParam, lParam, utf8, sizeof(utf8));
    if (utf8[0] == '\0') {
        *pfEaten = FALSE;
        return S_OK;
    }

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

    CEthiopicEditSession *session = new CEthiopicEditSession(
        this, produced, composing);
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
