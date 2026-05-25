# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Moab
#
# Ethiopic TSF IME — Windows Install/Uninstall Script
# ====================================================
# Usage (run from project root):
#   powershell -ExecutionPolicy Bypass -File windows/install.ps1
#   powershell -ExecutionPolicy Bypass -File windows/install.ps1 -Uninstall
#
# The script auto-elevates to admin when needed (required for COM registration).

param(
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$ProductName    = "Ethiopic Keyboard"
$CompanyName    = "Moab"
$InstallDir     = "${env:ProgramFiles}\${CompanyName}\${ProductName}"
$DllName        = "ethiopic-tsf.dll"
$DataDir        = "data"
$RegSvrPath     = "regsvr32.exe"

# ── Auto-elevate to admin ────────────────────────────────────────
function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# ── Require 64-bit Windows ───────────────────────────────────────
if (-not [Environment]::Is64BitOperatingSystem) {
    Write-Host "[Ethiopic IME] ERROR: 64-bit Windows required. 32-bit is not supported."
    exit 1
}

if (-not (Test-Admin)) {
    Write-Host "[Ethiopic IME] Requesting administrator privileges..."
    $args = "-ExecutionPolicy Bypass -NoProfile -File `"$($MyInvocation.MyCommand.Path)`""
    if ($Uninstall) { $args += " -Uninstall" }
    $elevated = Start-Process powershell.exe -Verb RunAs -ArgumentList $args -Wait -PassThru
    exit $elevated.ExitCode
}

# ── Locate built DLL ─────────────────────────────────────────────
function Find-Dll {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $projectRoot = Resolve-Path "$scriptDir\.."

    $candidates = @(
        "$projectRoot\build-win\Release\ethiopic-tsf.dll"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    Write-Host "[Ethiopic IME] Built DLL not found. Searched:"
    foreach ($path in $candidates) {
        Write-Host "  $path"
    }
    Write-Host ""
    Write-Host "Build the DLL first:  .\build-tsf.sh  (from MSYS2 mingw64 shell)"
    return $null
}

# ── Uninstall ────────────────────────────────────────────────────
function Invoke-Uninstall {
    Write-Host "[Ethiopic IME] Uninstalling..."

    $dllPath = "$InstallDir\$DllName"
    if (Test-Path $dllPath) {
        Write-Host "  Unregistering DLL..."
        $result = Start-Process $RegSvrPath -ArgumentList "/u /s `"$dllPath`"" -Wait -PassThru -NoNewWindow
        if ($result.ExitCode -eq 0) {
            Write-Host "  DLL unregistered."
        } else {
            Write-Warning "  regsvr32 /u returned exit code $($result.ExitCode)"
        }
    }

    if (Test-Path $InstallDir) {
        Write-Host "  Removing $InstallDir..."
        Remove-Item -Recurse -Force $InstallDir
        Write-Host "  Removed."
    }

    Write-Host "[Ethiopic IME] Uninstall complete."
}

if ($Uninstall) {
    Invoke-Uninstall
    exit 0
}

# ── Install ──────────────────────────────────────────────────────
Write-Host "[Ethiopic IME] Installing..."
Write-Host "  Destination: $InstallDir"

$dllPath = Find-Dll
if (-not $dllPath) { exit 1 }

Write-Host "  DLL source: $dllPath"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path "$scriptDir\.."
$dataSource = "$projectRoot\$DataDir"

if (-not (Test-Path $dataSource)) {
    Write-Host "[Ethiopic IME] ERROR: data directory not found at $dataSource"
    exit 1
}

# Create install directory
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Stop and unregister any existing installation first
$existingDll = "$InstallDir\$DllName"
if (Test-Path $existingDll) {
    Write-Host "  Unregistering previous installation..."
    Start-Process $RegSvrPath -ArgumentList "/u /s `"$existingDll`"" -Wait -NoNewWindow | Out-Null
}

# Copy DLL
Write-Host "  Copying DLL..."
Copy-Item -Force $dllPath "$InstallDir\$DllName"

# Copy data files (DLL finds them via ./data/amharic/ relative to itself)
Write-Host "  Copying data files..."
$targetData = "$InstallDir\$DataDir"
if (Test-Path $targetData) {
    Remove-Item -Recurse -Force $targetData
}
Copy-Item -Recurse $dataSource $targetData

# Register DLL
Write-Host "  Registering IME..."
$installDllPath = "$InstallDir\$DllName"
$result = Start-Process $RegSvrPath -ArgumentList "/s `"$installDllPath`"" -Wait -PassThru -NoNewWindow

if ($result.ExitCode -ne 0) {
    Write-Host "[Ethiopic IME] ERROR: regsvr32 returned exit code $($result.ExitCode)"
    Write-Host "  Try running manually from an elevated command prompt:"
    Write-Host "    regsvr32 `"$installDllPath`""
    exit 1
}

# ── Verify ───────────────────────────────────────────────────────
Write-Host ""
Write-Host "[Ethiopic IME] Install complete."
Write-Host ""
Write-Host "  Install directory: $InstallDir"
Write-Host "  DLL:              $InstallDir\$DllName"
Write-Host "  Data:             $targetData"
Write-Host ""
Write-Host "  To activate:"
Write-Host "    1. Open Settings → Time & Language → Language → Amharic → Options → Keyboards"
Write-Host "    2. Add 'Ethiopic (SERA)'"
Write-Host "    3. Switch via language bar or Win+Space"
Write-Host ""
Write-Host "  To uninstall:"
Write-Host "    powershell -ExecutionPolicy Bypass -File `"$($MyInvocation.MyCommand.Path)`" -Uninstall"
