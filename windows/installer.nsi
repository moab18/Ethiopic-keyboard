; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Moab
;
; Ethiopic TSF IME — NSIS Installer Script
; =========================================
; Build the DLL first, then run from project root:
;   makensis windows\installer.nsi
;
; Requires NSIS 3.0+:  https://nsis.sourceforge.io/Download

Unicode true
ManifestDPIAware true

; ── Product metadata ─────────────────────────────────────────────
!define PRODUCT_NAME    "Ethiopic Keyboard"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_PUBLISHER "Moab"
!define PRODUCT_URL      "https://github.com/moab/ethiopic-keyboard"
!define PRODUCT_DIRREGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\ethiopic-tsf.dll"
!define PRODUCT_UNINSTKEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

!define DLL_NAME         "ethiopic-tsf.dll"
!define DATA_DIR         "data"

SetCompressor /SOLID lzma

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\build-win\EthiopicKeyboard-${PRODUCT_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_PUBLISHER}\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIRREGKEY}" ""
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

; ── Interface ─────────────────────────────────────────────────────
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON   "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; License page
!define MUI_LICENSEPAGE_CHECKBOX
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install section ───────────────────────────────────────────────
Section "Install"
    SetOutPath "$INSTDIR"

    ; Stop and unregister any existing installation
    IfFileExists "$INSTDIR\${DLL_NAME}" 0 +2
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\${DLL_NAME}"'

    ; Copy DLL
    DetailPrint "Installing ${DLL_NAME}..."
    File /oname=${DLL_NAME} "..\build-win\ethiopic-tsf\msys-ethiopic-tsf.dll"

    ; Copy data files (DLL finds them via ./data/amharic/ relative to itself)
    DetailPrint "Installing mapping data..."
    SetOutPath "$INSTDIR\${DATA_DIR}\amharic"
    File "..\data\amharic\am-sera.json"
    File "..\data\amharic\wordlist.json"
    File "..\data\amharic\bigrams.json"
    File "..\data\amharic\names.json"

    SetOutPath "$INSTDIR"

    ; Register COM DLL
    DetailPrint "Registering IME..."
    ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\${DLL_NAME}"' $0
    ${If} $0 != 0
        MessageBox MB_ICONEXCLAMATION "DLL registration failed (exit code $0).$\n$\nTry running from an elevated command prompt:$\n  regsvr32 $\"$INSTDIR\${DLL_NAME}$\"" /SD IDOK
    ${EndIf}

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Registry: Add/Remove Programs
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "DisplayName"     "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "DisplayVersion"  "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "Publisher"       "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "DisplayIcon"     "$INSTDIR\${DLL_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "QuietUninstallString" "$INSTDIR\uninstall.exe /S"
    WriteRegStr HKLM "${PRODUCT_UNINSTKEY}" "URLInfoAbout"    "${PRODUCT_URL}"
    WriteRegDWORD HKLM "${PRODUCT_UNINSTKEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINSTKEY}" "NoRepair" 1

    ; Registry: App Paths
    WriteRegStr HKLM "${PRODUCT_DIRREGKEY}" "" "$INSTDIR\${DLL_NAME}"
    WriteRegStr HKLM "${PRODUCT_DIRREGKEY}" "Path" "$INSTDIR"
SectionEnd

; ── Uninstall section ─────────────────────────────────────────────
Section "Uninstall"
    ; Unregister DLL
    IfFileExists "$INSTDIR\${DLL_NAME}" 0 +2
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\${DLL_NAME}"'

    ; Remove files
    Delete "$INSTDIR\${DLL_NAME}"
    Delete "$INSTDIR\${DATA_DIR}\amharic\am-sera.json"
    Delete "$INSTDIR\${DATA_DIR}\amharic\wordlist.json"
    Delete "$INSTDIR\${DATA_DIR}\amharic\bigrams.json"
    Delete "$INSTDIR\${DATA_DIR}\amharic\names.json"
    RMDir  "$INSTDIR\${DATA_DIR}\amharic"
    RMDir  "$INSTDIR\${DATA_DIR}"
    Delete "$INSTDIR\uninstall.exe"
    RMDir  "$INSTDIR"

    ; Remove registry entries
    DeleteRegKey HKLM "${PRODUCT_UNINSTKEY}"
    DeleteRegKey HKLM "${PRODUCT_DIRREGKEY}"
SectionEnd
