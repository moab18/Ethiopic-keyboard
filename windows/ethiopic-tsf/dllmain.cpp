#include <windows.h>
#include <objbase.h>
#include <msctf.h>
#include <strsafe.h>
#include <cstdio>
#include <cstring>

#include "engine.h"

static HMODULE g_hModule = nullptr;
static LONG    g_dllRefCount = -1;

void DllAddRef()
{
    InterlockedIncrement(&g_dllRefCount);
}

void DllRelease()
{
    InterlockedDecrement(&g_dllRefCount);
}

HMODULE DllModuleHandle()
{
    return g_hModule;
}

static void reg_log(const char *msg);

class CEthiopicClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        DllAddRef();
        return (ULONG)(g_dllRefCount + 1);
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        DllRelease();
        return (ULONG)(g_dllRefCount + 1);
    }

    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override
    {
        reg_log("CreateInstance");
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        CEthiopicTextService *service = new (std::nothrow) CEthiopicTextService();
        if (!service) {
            reg_log("CreateInstance: new FAILED (OOM)");
            return E_OUTOFMEMORY;
        }

        HRESULT hr = service->QueryInterface(riid, ppv);
        service->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock)
            DllAddRef();
        else
            DllRelease();
        return S_OK;
    }
};

static CEthiopicClassFactory *g_pClassFactory = nullptr;

static const char *log_path()
{
    static char path[MAX_PATH];
    static bool init = false;
    if (!init) {
        GetTempPathA(sizeof(path), path);
        strcat_s(path, sizeof(path), "ethiopic_tsf.log");
        init = true;
    }
    return path;
}

static HANDLE get_log_mutex()
{
    static HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\EthiopicTSF_LogMutex");
    return hMutex;
}

static void log_lock()
{
    HANDLE h = get_log_mutex();
    if (h) WaitForSingleObject(h, 5000);
}

static void log_unlock()
{
    HANDLE h = get_log_mutex();
    if (h) ReleaseMutex(h);
}

static void reg_log(const char *msg)
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

    log_lock();
    FILE *f = fopen(log_path(), "a");
    if (f) {
        fprintf(f, "%02d:%02d:%02d.%03d [%lu %lu] %s\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                pid, tid, msg);
        fclose(f);
    }
    log_unlock();
}

static void reg_log_hr(const char *msg, HRESULT hr)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[EthiopicTSF] %02d:%02d:%02d.%03d [pid=%lu tid=%lu] %s (HRESULT=0x%08lX)\n",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     pid, tid, msg, (unsigned long)hr);
    if (n > 0) OutputDebugStringA(buf);

    log_lock();
    FILE *f = fopen(log_path(), "a");
    if (f) {
        fprintf(f, "%02d:%02d:%02d.%03d [%lu %lu] %s (HRESULT=0x%08lX)\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                pid, tid, msg, (unsigned long)hr);
        fclose(f);
    }
    log_unlock();
}

static void reg_log_reg(const char *msg, LONG result)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[EthiopicTSF] %02d:%02d:%02d.%03d [pid=%lu tid=%lu] %s (RegError=%ld)\n",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     pid, tid, msg, (long)result);
    if (n > 0) OutputDebugStringA(buf);

    log_lock();
    FILE *f = fopen(log_path(), "a");
    if (f) {
        fprintf(f, "%02d:%02d:%02d.%03d [%lu %lu] %s (RegError=%ld)\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                pid, tid, msg, (long)result);
        fclose(f);
    }
    log_unlock();
}

static const WCHAR TEXTSERVICE_DESC[] = L"Ethiopic (SERA)";
static const WCHAR TEXTSERVICE_MODEL[] = L"Apartment";

#define TEXTSERVICE_LANGID  0x045E  /* Amharic (Ethiopia) */

#ifndef TF_IPPMF_FORSESSION
#define TF_IPPMF_FORSESSION  0x00000001
#endif

static const GUID CLSID_EthiopicTSF = {
    0x7A5B3C1D, 0x9E2F, 0x4A6B,
    {0x8C, 0x3D, 0x1E, 0x5F, 0x7A, 0x9B, 0x2C, 0x4D}
};

static const GUID GUID_Profile = {
    0x8B6C4D2E, 0xAF30, 0x5B7C,
    {0x9D, 0x4E, 0x2F, 0x60, 0x8B, 0xAC, 0x3D, 0x5E}
};

#ifndef GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER
DEFINE_GUID(GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    0x046b8c84, 0x1647, 0x4ca7, 0x9a,0x4c, 0xac,0x5c,0xa6,0x6b,0x94,0x06);
#endif

static const GUID SupportCategories[] = {
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    GUID_TFCAT_TIPCAP_COMLESS,
    GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    GUID_TFCAT_TIPCAP_SECUREMODE,
    GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

static BOOL CLSIDToString(REFGUID guid, WCHAR *out)
{
    int n = swprintf(out, 39, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                     (unsigned long)guid.Data1,
                     (unsigned)guid.Data2, (unsigned)guid.Data3,
                     (unsigned)guid.Data4[0], (unsigned)guid.Data4[1],
                     (unsigned)guid.Data4[2], (unsigned)guid.Data4[3],
                     (unsigned)guid.Data4[4], (unsigned)guid.Data4[5],
                     (unsigned)guid.Data4[6], (unsigned)guid.Data4[7]);
    if (n > 0) return TRUE;

    reg_log("CLSIDToString: swprintf failed, trying StringFromGUID2");
    n = StringFromGUID2(guid, out, 39);
    return (n > 0);
}

static LONG RecurseDeleteKey(HKEY hParent, LPCWSTR lpszKey)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyW(hParent, lpszKey, &hKey) != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    WCHAR buf[256];
    DWORD size = ARRAYSIZE(buf);
    FILETIME ft;
    LONG res = ERROR_SUCCESS;
    while (RegEnumKeyExW(hKey, 0, buf, &size, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS) {
        buf[ARRAYSIZE(buf)-1] = L'\0';
        res = RecurseDeleteKey(hKey, buf);
        if (res != ERROR_SUCCESS) break;
        size = ARRAYSIZE(buf);
    }
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS ? RegDeleteKeyW(hParent, lpszKey) : res;
}

static BOOL RegisterServer()
{
    reg_log("RegisterServer begin");
    WCHAR clsid_str[39];
    if (!CLSIDToString(CLSID_EthiopicTSF, clsid_str)) {
        reg_log("RegisterServer: CLSIDToString FAILED");
        return FALSE;
    }

    WCHAR key_path[256];
    swprintf(key_path, ARRAYSIZE(key_path), L"CLSID\\%s", clsid_str);

    HKEY hKey = nullptr;
    DWORD disp = 0;

    LONG regResult = RegCreateKeyExW(HKEY_CLASSES_ROOT, key_path, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &hKey, &disp);

    if (regResult != ERROR_SUCCESS) {
        reg_log_reg("RegisterServer: HKCR write FAILED, trying HKCU", regResult);
        WCHAR cu_path[512];
        swprintf(cu_path, ARRAYSIZE(cu_path),
                 L"Software\\Classes\\CLSID\\%s", clsid_str);
        regResult = RegCreateKeyExW(HKEY_CURRENT_USER, cu_path, 0, nullptr,
                                    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                    &hKey, &disp);
        if (regResult != ERROR_SUCCESS) {
            reg_log_reg("RegisterServer: HKCU write FAILED", regResult);
            return FALSE;
        }
        reg_log("RegisterServer: wrote to HKCU");
    } else {
        reg_log("RegisterServer: wrote to HKCR");
    }

    RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                   (const BYTE *)TEXTSERVICE_DESC,
                   (DWORD)(wcslen(TEXTSERVICE_DESC) + 1) * sizeof(WCHAR));

    HKEY hSub = nullptr;
    if (RegCreateKeyExW(hKey, L"InProcServer32", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &hSub, &disp) == ERROR_SUCCESS) {
        WCHAR module_path[MAX_PATH];
        DWORD len = GetModuleFileNameW(g_hModule, module_path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            RegSetValueExW(hSub, nullptr, 0, REG_SZ,
                           (const BYTE *)module_path,
                           (DWORD)(len + 1) * sizeof(WCHAR));
        }
        RegSetValueExW(hSub, L"ThreadingModel", 0, REG_SZ,
                       (const BYTE *)TEXTSERVICE_MODEL,
                       (DWORD)(wcslen(TEXTSERVICE_MODEL) + 1) * sizeof(WCHAR));
        RegCloseKey(hSub);
    }
    RegCloseKey(hKey);
    reg_log("RegisterServer OK");
    return TRUE;
}

static void UnregisterServer()
{
    WCHAR clsid_str[39];
    if (!CLSIDToString(CLSID_EthiopicTSF, clsid_str))
        return;

    WCHAR key_path[256];
    swprintf(key_path, ARRAYSIZE(key_path), L"CLSID\\%s", clsid_str);
    RecurseDeleteKey(HKEY_CLASSES_ROOT, key_path);
}

static BOOL RegisterProfiles()
{
    reg_log("RegisterProfiles begin");
    ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
        nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, (void **)&pProfileMgr);
    if (FAILED(hr)) {
        reg_log_hr("RegisterProfiles: CoCreateInstance(profile mgr) FAILED", hr);
        return FALSE;
    }

    WCHAR icon_path[MAX_PATH] = {0};
    DWORD icon_len = GetModuleFileNameW(g_hModule, icon_path, MAX_PATH);
    if (icon_len >= MAX_PATH) icon_len = MAX_PATH - 1;
    icon_path[icon_len] = L'\0';

    size_t desc_len = 0;
    hr = StringCchLengthW(TEXTSERVICE_DESC, STRSAFE_MAX_CCH, &desc_len);
    if (FAILED(hr)) {
        pProfileMgr->Release();
        return FALSE;
    }

    hr = pProfileMgr->RegisterProfile(
        CLSID_EthiopicTSF,
        TEXTSERVICE_LANGID,
        GUID_Profile,
        TEXTSERVICE_DESC,
        (ULONG)desc_len,
        icon_path, (ULONG)icon_len,
        (UINT)-1,
        nullptr, 0,
        TRUE,
        0);

    if (SUCCEEDED(hr)) {
        hr = pProfileMgr->ActivateProfile(
            TF_PROFILETYPE_INPUTPROCESSOR,
            TEXTSERVICE_LANGID,
            CLSID_EthiopicTSF,
            GUID_Profile,
            nullptr,
            TF_IPPMF_FORSESSION);
        reg_log_hr("RegisterProfiles: ActivateProfile result", hr);
    }
    pProfileMgr->Release();
    if (FAILED(hr)) {
        reg_log_hr("RegisterProfiles FAILED", hr);
        return FALSE;
    }
    reg_log("RegisterProfiles OK");
    return TRUE;
}

static void UnregisterProfiles()
{
    ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
        nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, (void **)&pProfileMgr);
    if (SUCCEEDED(hr)) {
        pProfileMgr->UnregisterProfile(CLSID_EthiopicTSF, TEXTSERVICE_LANGID,
                                       GUID_Profile, 0);
        pProfileMgr->Release();
    }
}

static BOOL RegisterCategories()
{
    reg_log("RegisterCategories begin");
    ITfCategoryMgr *pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr,
        nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void **)&pCategoryMgr);
    if (FAILED(hr)) {
        reg_log_hr("RegisterCategories: CoCreateInstance(category mgr) FAILED", hr);
        return FALSE;
    }

    for (size_t i = 0; i < ARRAYSIZE(SupportCategories); i++) {
        HRESULT hrCat = pCategoryMgr->RegisterCategory(CLSID_EthiopicTSF,
            SupportCategories[i], CLSID_EthiopicTSF);
        if (FAILED(hrCat)) {
            reg_log_hr("RegisterCategories: RegisterCategory FAILED (non-fatal)", hrCat);
        }
    }

    pCategoryMgr->Release();
    reg_log("RegisterCategories OK");
    return TRUE;
}

static void UnregisterCategories()
{
    ITfCategoryMgr *pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr,
        nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void **)&pCategoryMgr);
    if (FAILED(hr)) return;

    for (size_t i = 0; i < ARRAYSIZE(SupportCategories); i++) {
        pCategoryMgr->UnregisterCategory(CLSID_EthiopicTSF,
            SupportCategories[i], CLSID_EthiopicTSF);
    }

    pCategoryMgr->Release();
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pvReserved)
{
    (void)pvReserved;

    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstance);
        g_hModule = hInstance;
        reg_log("DllMain: DLL_PROCESS_ATTACH  BUILD=" __DATE__ " " __TIME__);
        {
            char path[MAX_PATH];
            GetModuleFileNameA(hInstance, path, sizeof(path));
            reg_log(path);
        }
        break;

    case DLL_PROCESS_DETACH:
        MarkDllUnloading();
        if (g_pClassFactory) {
            delete g_pClassFactory;
            g_pClassFactory = nullptr;
        }
        break;
    }

    return TRUE;
}

STDAPI DllUnregisterServer(void);

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    reg_log("DllGetClassObject");
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (!g_pClassFactory) {
        g_pClassFactory = new (std::nothrow) CEthiopicClassFactory();
        if (!g_pClassFactory) return E_OUTOFMEMORY;
    }

    if (!IsEqualGUID(rclsid, CLSID_EthiopicTSF))
        return CLASS_E_CLASSNOTAVAILABLE;

    return g_pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return (g_dllRefCount < 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    OutputDebugStringA("EthiopicTSF: DllRegisterServer begin\n");

    if (!RegisterServer()) {
        OutputDebugStringA("EthiopicTSF: RegisterServer FAILED\n");
        DllUnregisterServer();
        return E_FAIL;
    }
    OutputDebugStringA("EthiopicTSF: RegisterServer OK\n");

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (SUCCEEDED(hrCom));
    if (FAILED(hrCom) && hrCom != RPC_E_CHANGED_MODE) {
        OutputDebugStringA("EthiopicTSF: CoInitializeEx FAILED\n");
        DllUnregisterServer();
        return E_FAIL;
    }

    if (!RegisterProfiles()) {
        OutputDebugStringA("EthiopicTSF: RegisterProfiles FAILED\n");
        if (needUninit) CoUninitialize();
        DllUnregisterServer();
        return E_FAIL;
    }
    OutputDebugStringA("EthiopicTSF: RegisterProfiles OK\n");

    if (!RegisterCategories()) {
        OutputDebugStringA("EthiopicTSF: RegisterCategories FAILED\n");
        if (needUninit) CoUninitialize();
        DllUnregisterServer();
        return E_FAIL;
    }
    OutputDebugStringA("EthiopicTSF: RegisterCategories OK\n");

    if (needUninit) CoUninitialize();
    OutputDebugStringA("EthiopicTSF: DllRegisterServer success\n");
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    UnregisterProfiles();
    UnregisterCategories();
    UnregisterServer();
    return S_OK;
}
