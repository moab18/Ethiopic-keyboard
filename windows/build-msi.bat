@echo off
REM SPDX-License-Identifier: GPL-3.0-or-later
REM Copyright (C) 2026 Moab
REM
REM Ethiopic TSF IME — MSI Build Script
REM ====================================
REM Builds the Windows Installer MSI package using WiX Toolset v4+.
REM
REM Prerequisites:
REM   dotnet tool install --global wix
REM   (or install WiX from https://wixtoolset.org/)
REM
REM Usage (run from project root):
REM   windows\build-msi.bat
REM   windows\build-msi.bat C:\output\EthiopicKeyboard.msi

setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

set "VERSION=0.1.0"
set "OUTPUT=%PROJECT_ROOT%\build-win\EthiopicKeyboard-%VERSION%-x64.msi"
if not "%~1"=="" set "OUTPUT=%~1"

echo === Ethiopic Keyboard MSI Builder ===
echo.

REM ── Check for WiX ──────────────────────────────────────────────
set "WIX_CMD="

REM WiX v4/v5: the 'wix' CLI (dotnet tool or global install)
where wix >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "WIX_CMD=wix"
    goto :wix_found
)

REM WiX v3 fallback: candle + light
where candle >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "WIX_CMD=v3"
    goto :wix_found
)

REM Check dotnet tool path
if exist "%USERPROFILE%\.dotnet\tools\wix.exe" (
    set "WIX_CMD=%USERPROFILE%\.dotnet\tools\wix.exe"
    goto :wix_found
)

echo ERROR: WiX Toolset not found.
echo Install with: dotnet tool install --global wix
echo Or download from: https://wixtoolset.org/
exit /b 1

:wix_found
echo   WiX command: %WIX_CMD%

REM ── Check DLL is built ─────────────────────────────────────────
set "DLL_PATH=%PROJECT_ROOT%\build-win\ethiopic-tsf\msys-ethiopic-tsf.dll"
if not exist "%DLL_PATH%" (
    set "DLL_PATH=%PROJECT_ROOT%\build-win\Release\ethiopic-tsf.dll"
)
if not exist "%DLL_PATH%" (
    echo ERROR: Built DLL not found. Searched:
    echo   %PROJECT_ROOT%\build-win\ethiopic-tsf\msys-ethiopic-tsf.dll
    echo   %PROJECT_ROOT%\build-win\Release\ethiopic-tsf.dll
    echo.
    echo Build the DLL first:  .\build-tsf.sh  (from MSYS2 mingw64 shell^)
    exit /b 1
)
echo   DLL source: %DLL_PATH%

REM ── Check data files ───────────────────────────────────────────
if not exist "%PROJECT_ROOT%\data\amharic\am-sera.json" (
    echo ERROR: data\amharic\am-sera.json not found
    exit /b 1
)

REM ── Convert LICENSE to RTF for the installer ───────────────────
set "LICENSE_RTF=%PROJECT_ROOT%\windows\wix\license.rtf"
set "LICENSE_TXT=%PROJECT_ROOT%\LICENSE"

if exist "%LICENSE_TXT%" (
    echo   Creating license RTF...
    powershell -NoProfile -Command ^
        "$txt = Get-Content '%LICENSE_TXT%' -Raw; ^
         $rtf = '{\rtf1\ansi\deff0 {\fonttbl {\f0 Consolas;}} \f0\fs20 ' + ^
                $txt.Replace('\', '\\').Replace('{', '\{').Replace('}', '\}').Replace(\"`r`n\", '\par ') + ^
                '}'; ^
         [System.IO.File]::WriteAllText('%LICENSE_RTF%', $rtf)"
    if %ERRORLEVEL% NEQ 0 (
        echo WARNING: Could not create license RTF. Continuing without license page.
        del "%LICENSE_RTF%" 2>nul
    ) else (
        echo   License RTF created: %LICENSE_RTF%
    )
) else (
    echo WARNING: LICENSE file not found, skipping license page.
)

REM ── Create output directory ────────────────────────────────────
set "OUTDIR=%PROJECT_ROOT%\build-win"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ── Build MSI ──────────────────────────────────────────────────
echo.
echo === Building MSI ===

if "%WIX_CMD%"=="v3" (

    REM WiX v3: candle + light
    set "WIXOBJ=%OUTDIR%\wix\product.wixobj"
    if not exist "%OUTDIR%\wix" mkdir "%OUTDIR%\wix"

    echo   Compiling...
    candle.exe -nologo -arch x64 ^
        -dProjectRoot="%PROJECT_ROOT%" ^
        "%PROJECT_ROOT%\windows\wix\product.wxs" ^
        -out "%WIXOBJ%"
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: candle.exe failed
        exit /b 1
    )

    echo   Linking...
    light.exe -nologo ^
        -ext WixUIExtension ^
        -ext WixUtilExtension ^
        "%WIXOBJ%" ^
        -out "%OUTPUT%"
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: light.exe failed
        exit /b 1
    )

) else (

    REM WiX v4/v5: wix build
    "%WIX_CMD%" build -nologo -arch x64 ^
        -o "%OUTPUT%" ^
        "%PROJECT_ROOT%\windows\wix\product.wxs"
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: wix build failed
        exit /b 1
    )

)

echo.
echo === MSI built successfully ===
echo   %OUTPUT%
echo.
echo To install silently:
echo   msiexec /i "%OUTPUT%" /qn
echo.
echo To install interactively:
echo   msiexec /i "%OUTPUT%"
echo.
echo To uninstall:
echo   msiexec /x "%OUTPUT%"

endlocal
