#pragma once

#include <windows.h>
#include <objbase.h>
#include <msctf.h>

#include <string>

#include "ethio/engine.h"
#include "ethio/wordlist.h"

void DllAddRef();
void DllRelease();
HMODULE DllModuleHandle();
void MarkDllUnloading();
bool IsDllUnloading();

#ifndef __ITfTextInputProcessorEx_FWD_DEFINED__
#define __ITfTextInputProcessorEx_FWD_DEFINED__
typedef interface ITfTextInputProcessorEx ITfTextInputProcessorEx;
#endif

#ifndef __ITfTextInputProcessorEx_INTERFACE_DEFINED__
#define __ITfTextInputProcessorEx_INTERFACE_DEFINED__
EXTERN_C const IID IID_ITfTextInputProcessorEx;
MIDL_INTERFACE("6e4e2100-f9cd-433d-b46f-f3e7ed2d6259")
ITfTextInputProcessorEx : public ITfTextInputProcessor
{
    virtual HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr *pThreadMgr,
        TfClientId tfClientId, DWORD dwFlags) = 0;
};
#endif

#ifndef __ITfDisplayAttributeProvider_INTERFACE_DEFINED__
#define __ITfDisplayAttributeProvider_INTERFACE_DEFINED__
EXTERN_C const IID IID_ITfDisplayAttributeProvider;
MIDL_INTERFACE("fee47777-163c-4769-996a-6e9c504a2c4b")
ITfDisplayAttributeProvider : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(
        IEnumTfDisplayAttributeInfo **ppEnum) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(
        REFGUID guidInfo, ITfDisplayAttributeInfo **ppInfo) = 0;
};
#endif

#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM  ((TfGuidAtom)0xFFFFFFFF)
#endif

#ifndef GUID_TFCAT_TIPCAP_UIELEMENTENABLED
#ifndef _MSC_VER
DEFINE_GUID(GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    0xf2ef5d5f, 0x9f3a, 0x4d37, 0x88,0x90, 0x4e,0x33,0xad,0xb4,0x0d,0x2f);
#endif
#endif
#ifndef GUID_TFCAT_TIPCAP_SECUREMODE
#ifndef _MSC_VER
DEFINE_GUID(GUID_TFCAT_TIPCAP_SECUREMODE,
    0x4e7f8c6d, 0x5b2f, 0x4a3c, 0x9d,0x1e, 0x2a,0x5f,0x3c,0x8b,0x7d,0x9e);
#endif
#endif
#ifndef GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT
#ifndef _MSC_VER
DEFINE_GUID(GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    0x3a6d5e2f, 0x1c9b, 0x4b8f, 0xa2,0xe3, 0x5d,0x1c,0x6f,0x8a,0x9b,0x2e);
#endif
#endif
#ifndef GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT
#ifndef _MSC_VER
DEFINE_GUID(GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    0x13a50a5c, 0x2e5a, 0x4d8e, 0xb2,0xf3, 0x6e,0x4b,0x8c,0x1d,0x5a,0x9f);
#endif
#endif
#ifndef GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT
#ifndef _MSC_VER
DEFINE_GUID(GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    0x25504b2c, 0x5d1f, 0x4e3a, 0x8c,0xd2, 0x1b,0x6e,0x3d,0x7f,0x9a,0x5c);
#endif
#endif

class CEthiopicTextService;

class CEthiopicEditSession : public ITfEditSession {
public:
    CEthiopicEditSession(CEthiopicTextService *pService,
                         const std::string &commit_text,
                         const std::string &preedit_text);

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    LONG m_cRef;
    CEthiopicTextService *m_pService;
    std::string m_commitText;
    std::string m_preeditText;
};

class CEthiopicTextService : public ITfTextInputProcessorEx,
                              public ITfKeyEventSink,
                              public ITfTextEditSink,
                              public ITfThreadMgrEventSink,
                              public ITfCompositionSink,
                              public ITfDisplayAttributeProvider {
public:
    CEthiopicTextService();
    ~CEthiopicTextService();

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId) override
    {
        return ActivateEx(pThreadMgr, tfClientId, 0);
    }
    STDMETHODIMP ActivateEx(ITfThreadMgr *pThreadMgr, TfClientId tfClientId, DWORD dwFlags) override;
    STDMETHODIMP Deactivate() override;

    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten) override;

    STDMETHODIMP OnEndEdit(ITfContext *pic, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord) override;

    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr *pDim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr *pDim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr *pDimFocus, ITfDocumentMgr *pDimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext *pic) override;
    STDMETHODIMP OnPopContext(ITfContext *pic) override;

    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) override;

    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guidInfo, ITfDisplayAttributeInfo **ppInfo) override;

    TfClientId GetClientId() const { return m_tfClientId; }
    ITfThreadMgr *GetThreadMgr() const { return m_pThreadMgr; }
    bool HasComposition() const { return m_pComposition != nullptr; }
    void StartComposition(TfEditCookie ec, ITfContext *pContext, const std::string &text);
    void UpdateComposition(TfEditCookie ec, const std::string &text);
    void EndComposition(TfEditCookie ec, ITfContext *pContext);

    void SetTestMode(bool on) { m_testMode = on; }
    bool TestMode() const { return m_testMode; }
    std::string TestCommittedText() const { return m_testCommitted; }
    std::string TestPreeditText() const { return m_testPreedit; }
    void ResetTest();
    void ProcessKeyUtf8(const char *utf8);

private:
    BOOL _InitKeyEventSink();
    void _UninitKeyEventSink();
    BOOL _InitTextEditSink(ITfDocumentMgr *pDocMgr);
    void _UninitTextEditSink();
    BOOL _InitThreadMgrEventSink();
    void _UninitThreadMgrEventSink();
    BOOL _InitDisplayAttributeGuidAtom();
    BOOL _IsKeyboardOpen();

public:
    TfGuidAtom GetInputAttribute() const { return m_gaDisplayAttributeInput; }

private:
    LONG m_cRef;
    TfClientId m_tfClientId;
    DWORD m_dwActivateFlags;
    bool m_activated;
    DWORD m_dwThreadMgrCookie;
    DWORD m_dwTextEditCookie;
    ITfContext *m_pTextEditSinkContext;

    ITfThreadMgr *m_pThreadMgr;
    ITfComposition *m_pComposition;
    std::string m_currentPreedit;

    TfGuidAtom m_gaDisplayAttributeInput;

    bool m_testMode = false;
    std::string m_testCommitted;
    std::string m_testPreedit;

    ethio::Engine m_core;
    ethio::WordList m_wordlist;
    ethio::MappingFile m_mapping;
};
