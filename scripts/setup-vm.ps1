<#
.SYNOPSIS
    Setup AutoKJ development environment on a bare Windows (Tiny11) VM.
    Run as Administrator.

.DESCRIPTION
    Installs everything needed to build and run Auto-KJ desktop app:
    - Chocolatey package manager
    - Visual Studio 2022 Build Tools (MSVC)
    - Qt 5.15.2 (via aqtinstall)
    - GStreamer SDK
    - OpenSSL 1.1
    - CMake, Git, Inno Setup
    - Opens SSH (OpenSSH server for remote access)
    - Clones the repo and configures the build
#>

$ErrorActionPreference = "Stop"

# ── Helper ────────────────────────────────────────────────────────────────
function Write-Step($msg) {
    Write-Host "`n=== $msg ===" -ForegroundColor Cyan
}

function Install-IfMissing($chocoPkg) {
    if (!(Get-Command $chocoPkg -ErrorAction SilentlyContinue)) {
        choco install $chocoPkg -y --no-progress
    }
}

# ── 1. Chocolatey ────────────────────────────────────────────────────────
Write-Step "Installing Chocolatey"
if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    refreshenv 2>$null; $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("PATH","User")
} else {
    Write-Host "Chocolatey already installed"
}

# ── 2. Core tools via Chocolatey ──────────────────────────────────────────
Write-Step "Installing core tools (git, cmake, python, innosetup)"
choco install git -y --no-progress
choco install cmake -y --no-progress --installargs '"ADD_CMAKE_TO_PATH=System"'
choco install python -y --no-progress
choco install innosetup -y --no-progress
choco install 7zip -y --no-progress
refreshenv 2>$null; $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("PATH","User")

# ── 3. Visual Studio 2022 Build Tools ─────────────────────────────────────
Write-Step "Installing Visual Studio 2022 Build Tools (MSVC)"
if (!(Test-Path "C:\Program Files\Microsoft Visual Studio\2022\BuildTools")) {
    choco install visualstudio2022buildtools -y --no-progress
    choco install visualstudio2022-workload-vctools -y --no-progress `
        --package-parameters "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22634 --passive"
} else {
    Write-Host "VS Build Tools already installed"
}

# ── 4. Qt 5.15.2 via aqtinstall ───────────────────────────────────────────
Write-Step "Installing Qt 5.15.2"
$QtDir = "C:\Qt\5.15.2\msvc2019_64"
if (!(Test-Path $QtDir)) {
    pip install aqtinstall --quiet
    aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -m qtwebengine -O C:\Qt
    if (!(Test-Path $QtDir)) {
        throw "Qt installation failed — $QtDir not found"
    }
} else {
    Write-Host "Qt already installed at $QtDir"
}
$env:Qt5_DIR = "$QtDir\lib\cmake\Qt5"

# ── 5. GStreamer SDK ──────────────────────────────────────────────────────
Write-Step "Installing GStreamer SDK"
$GstDir = "C:\gstreamer-sdk"
if (!(Test-Path "$GstDir\bin")) {
    $url = "https://github.com/pkircher29/gstreamer/releases/download/gst-sdk-1.26.9/gstreamer-sdk-1.26.9.zip"
    $zip = "$env:TEMP\gstreamer-sdk.zip"
    Write-Host "Downloading GStreamer SDK..."
    curl.exe -L --retry 5 --retry-delay 3 -o $zip $url
    if (!(Test-Path $zip)) { throw "GStreamer SDK download failed" }
    Expand-Archive -Path $zip -DestinationPath $GstDir -Force
    Remove-Item $zip -Force
    if (!(Test-Path "$GstDir\bin")) { throw "GStreamer SDK extract failed" }
} else {
    Write-Host "GStreamer SDK already installed at $GstDir"
}
$env:GST_BASE_PATH = $GstDir

# ── 6. OpenSSL 1.1 ────────────────────────────────────────────────────────
Write-Step "Installing OpenSSL 1.1"
$sslExe = Get-ChildItem -Path "C:\Program Files\OpenSSL*" -Filter "openssl.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (!$sslExe) {
    $url = "https://slproweb.com/download/Win64OpenSSL-1_1_1w.exe"
    $installer = "$env:TEMP\Win64OpenSSL-1_1_1w.exe"
    curl.exe -L --retry 3 -o $installer $url
    Start-Process -FilePath $installer -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-" -Wait
    $sslDll = Get-ChildItem -Path "C:\Program Files\OpenSSL*\bin" -Filter "libssl-1_1-x64.dll" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$sslDll) { throw "OpenSSL 1.1 installation failed" }
    Write-Host "OpenSSL installed at: $($sslDll.DirectoryName)"
} else {
    Write-Host "OpenSSL already installed"
}

# ── 7. spdlog ─────────────────────────────────────────────────────────────
Write-Step "Installing spdlog"
$SpdlogDir = "C:\spdlog"
if (!(Test-Path "$SpdlogDir\include\spdlog\spdlog.h")) {
    git clone --depth 1 https://github.com/gabime/spdlog.git $SpdlogDir
    cmake -B "$SpdlogDir\build" -S $SpdlogDir -G "Visual Studio 17 2022" -A x64
    cmake --build "$SpdlogDir\build" --config Release
    cmake --install "$SpdlogDir\build" --prefix "$SpdlogDir\install"
} else {
    Write-Host "spdlog already installed"
}

# ── 8. Clone repo ─────────────────────────────────────────────────────────
Write-Step "Cloning auto-kj repo"
$RepoDir = "C:\auto-kj"
if (!(Test-Path $RepoDir)) {
    git clone https://github.com/pkircher29/auto-kj.git $RepoDir
} else {
    Write-Host "Repo already cloned at $RepoDir"
    Push-Location $RepoDir
    git pull origin master
    Pop-Location
}

# ── 9. Configure build ────────────────────────────────────────────────────
Write-Step "Configuring CMake build"
$BuildDir = "$RepoDir\build"
if (!(Test-Path $BuildDir)) {
    Push-Location $RepoDir

    # Setup MSVC environment
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsPath = & $vsWhere -latest -products * -property installationPath 2>$null
    if ($vsPath) {
        Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
        Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments "-arch=amd64" -SkipAutomaticLocation
    }

    cmake -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
        "-DDEPLOY_DEPS=OFF" `
        "-DQT_INSTALL_PREFIX=$QtDir\..\..\.." `
        "-DGST_BASE_PATH=$GstDir" `
        "-Dspdlog_DIR=$SpdlogDir\install\lib\cmake\spdlog"

    Pop-Location
} else {
    Write-Host "Build already configured at $BuildDir"
}

# ── 10. Build ─────────────────────────────────────────────────────────────
Write-Step "Building Auto-KJ (Release)"
Push-Location $RepoDir
cmake --build $BuildDir --config Release --parallel
Pop-Location

if (Test-Path "$BuildDir\Release\auto-kj.exe") {
    Write-Host "`n✅ BUILD SUCCESSFUL" -ForegroundColor Green
    Write-Host "Executable: $BuildDir\Release\auto-kj.exe"
} else {
    Write-Host "`n⚠️  Build completed but auto-kj.exe not found" -ForegroundColor Yellow
    Write-Host "Check build output above for errors"
}

# ── 11. Enable SSH for remote access ─────────────────────────────────────
Write-Step "Enabling OpenSSH Server"
$sshdService = Get-Service sshd -ErrorAction SilentlyContinue
if (!$sshdService) {
    # Install OpenSSH
    Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
    Start-Service sshd
    Set-Service -Name sshd -StartupType Automatic
    Write-Host "SSH server installed and started on port 22"
} elseif ($sshdService.Status -ne "Running") {
    Start-Service sshd
    Write-Host "SSH server started"
} else {
    Write-Host "SSH server already running"
}

# ── 12. Enable RDP ────────────────────────────────────────────────────────
Write-Step "Enabling RDP"
Set-ItemProperty -Path 'HKLM:\System\CurrentControlSet\Control\Terminal Server' -name "fDenyTSConnections" -Value 0
Enable-NetFirewallRule -DisplayGroup "Remote Desktop"
Write-Host "RDP enabled"

# ── 13. Set persistent environment variables ──────────────────────────────
Write-Step "Setting environment variables"
[System.Environment]::SetEnvironmentVariable("Qt5_DIR", "$QtDir\lib\cmake\Qt5", "Machine")
[System.Environment]::SetEnvironmentVariable("GST_BASE_PATH", $GstDir, "Machine")
[System.Environment]::SetEnvironmentVariable("PATH", "$GstDir\bin;$QtDir\bin;" + [System.Environment]::GetEnvironmentVariable("PATH","Machine"), "Machine")

# ── Summary ───────────────────────────────────────────────────────────────
Write-Host "`n═══════════════════════════════════════════════" -ForegroundColor Green
Write-Host "  AutoKJ VM Setup Complete!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Installed:"
Write-Host "  - Visual Studio 2022 Build Tools (MSVC)"
Write-Host "  - Qt 5.15.2 at $QtDir"
Write-Host "  - GStreamer SDK at $GstDir"
Write-Host "  - OpenSSL 1.1"
Write-Host "  - spdlog"
Write-Host "  - Git, CMake, Python, Inno Setup, 7-Zip"
Write-Host ""
Write-Host "Repo: $RepoDir"
Write-Host "Build: $BuildDir\Release\auto-kj.exe"
Write-Host ""
Write-Host "Remote access:"
Write-Host "  - SSH: port 22"
Write-Host "  - RDP: port 3389"
Write-Host ""
Write-Host "To rebuild later:"
Write-Host "  cd $RepoDir"
Write-Host "  cmake --build $BuildDir --config Release --parallel"
Write-Host ""

# ── Firewall rule for Lisa access ─────────────────────────────────────────
Write-Step "Opening firewall for SSH and RDP"
New-NetFirewallRule -DisplayName "SSH-In" -Direction Inbound -Protocol TCP -LocalPort 22 -Action Allow -ErrorAction SilentlyContinue
New-NetFirewallRule -DisplayName "RDP-In" -Direction Inbound -Protocol TCP -LocalPort 3389 -Action Allow -ErrorAction SilentlyContinue