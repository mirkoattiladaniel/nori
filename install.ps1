# Nori installer for Windows (PowerShell).
#
#   irm https://raw.githubusercontent.com/mirkoattiladaniel/nori/main/install.ps1 | iex
#
# Downloads the `roll` project tool into %USERPROFILE%\.nori\bin, adds it to your
# user PATH, then (unless -NoCompiler) runs `roll install stable` to fetch the
# matching compiler (noric). install.sh is the Unix equivalent.

[CmdletBinding()]
param(
    [string]$Version = "latest",          # roll release tag to fetch ("latest" or "vX.Y.Z")
    [switch]$NoCompiler,                   # skip the `roll install stable` step
    [switch]$NoModifyPath                  # don't touch PATH (you add ~/.nori/bin yourself)
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# --- config (override with env vars) -----------------------------------------
$Base   = if ($env:NORI_INSTALL_BASE) { $env:NORI_INSTALL_BASE } else { "https://github.com/mirkoattiladaniel/nori" }
$Target = "x86_64-windows"                # must match std/sys sys::target() on Windows
$Home_  = $env:USERPROFILE
$NoriHome = Join-Path $Home_ ".nori"
$BinDir   = Join-Path $NoriHome "bin"
$RollExe  = Join-Path $BinDir "roll.exe"

function Info($m) { Write-Host "nori: $m" -ForegroundColor Cyan }
function Warn($m) { Write-Host "nori: $m" -ForegroundColor Yellow }

# --- 1. fetch roll.exe -------------------------------------------------------
$asset = "roll-$Target.exe"
if ($Version -eq "latest") {
    $url = "$Base/releases/latest/download/$asset"
} else {
    $url = "$Base/releases/download/$Version/$asset"
}

New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
Info "downloading $asset"
Info "  from $url"
try {
    Invoke-WebRequest -Uri $url -OutFile $RollExe -UseBasicParsing
} catch {
    Warn "could not download roll. Is a Windows release published at:"
    Warn "  $url"
    throw
}
Info "installed roll -> $RollExe"

# --- 2. add ~/.nori/bin to the user PATH (persistent) ------------------------
if (-not $NoModifyPath) {
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$BinDir*") {
        $newPath = if ([string]::IsNullOrEmpty($userPath)) { $BinDir } else { "$userPath;$BinDir" }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Info "added $BinDir to your user PATH (restart your terminal to pick it up)"
    }
    # make it usable in this session too
    if ($env:Path -notlike "*$BinDir*") { $env:Path = "$env:Path;$BinDir" }
}

# --- 3. fetch the compiler via roll ------------------------------------------
if (-not $NoCompiler) {
    Info "installing the compiler (roll install stable) ..."
    & $RollExe install stable
    if ($LASTEXITCODE -ne 0) {
        Warn "roll install did not complete (rc=$LASTEXITCODE)."
        Warn "You can retry later with: roll install stable"
    }
}

Info "done. Open a new terminal and run:  roll --help"
Write-Host ""
Warn "Note: to COMPILE on Windows, noric also needs the LLVM/MLIR 22 backend tools"
Warn "on PATH (clang, lld, llvm-objcopy, mlir-opt, mlir-translate)."
