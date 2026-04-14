# Auto-KJ Windows Packaging Script
# This script automates the build, dependency deployment, and installer generation.

$ErrorActionPreference = "Stop"

$QtPath = "C:\Qt\5.15.2\msvc2019_64"
$GStreamerPath = "C:\Program Files\gstreamer\1.0\msvc_x86_64"
$BuildDir = "build_windows"
$OutputDir = "cd\output"
$VSPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"

function Write-Step($msg) {
    Write-Host "`n>>> $msg" -ForegroundColor Cyan
}

# 1. Setup Environment
Write-Step "Preparing Environment..."
if (!(Test-Path $QtPath)) { throw "Qt not found at $QtPath" }
if (!(Test-Path $GStreamerPath)) { throw "GStreamer not found at $GStreamerPath" }
if (!(Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir } else { Remove-Item -Path "$OutputDir\*" -Recurse -Force }

# 2. Configure and Build
Write-Step "Configuring CMake..."
& "cmake" -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
    "-DDEPLOY_DEPS=ON" `
    "-DQT_INSTALL_PREFIX=$QtPath" `
    "-DGST_BASE_PATH=$GStreamerPath"

Write-Step "Building Auto-KJ (Release)..."
& "cmake" --build $BuildDir --config Release --parallel

# 3. Collect Build Artifacts
Write-Step "Collecting Build Artifacts..."
Copy-Item "$BuildDir\Release\*" $OutputDir -Recurse -Force

# 4. Deploy Qt Dependencies
Write-Step "Running windeployqt..."
& "$QtPath\bin\windeployqt.exe" --release --no-translations "$OutputDir\auto-kj.exe"

# 5. Collect GStreamer Dependencies
Write-Step "Collecting GStreamer DLLs..."
$GstBin = "$GStreamerPath\bin"
Get-ChildItem -Path $GstBin -Filter "*.dll" | Copy-Item -Destination $OutputDir -Force

# 6. Collect Assets (Fonts, Redist)
Write-Step "Collecting Assets..."
$Fonts = @(
    "Roboto-Bold.ttf",
    "Roboto-Medium.ttf",
    "Roboto-Regular.ttf",
    "SourceCodePro-Medium.ttf"
)

foreach ($font in $Fonts) {
    $fontPath = "C:\Windows\Fonts\$font"
    if (Test-Path $fontPath) {
        Copy-Item $fontPath $OutputDir -Force
        Write-Host "Collected $font"
    } else {
        Write-Warning "Font $font not found in C:\Windows\Fonts"
    }
}

$RedistPath = Get-ChildItem -Path "$VSPath\VC\Redist\MSVC" -Filter "vc_redist.x64.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName
if ($RedistPath) {
    Copy-Item $RedistPath $OutputDir -Force
    Write-Host "Collected vc_redist.x64.exe"
} else {
    Write-Warning "vc_redist.x64.exe not found in $VSPath"
}

# 7. Collect License
if (Test-Path "LICENSE") {
    Copy-Item "LICENSE" "$OutputDir\LICENSE.txt" -Force
}

# 8. Final Installer Generation
Write-Step "Generating Installer..."
$InnoPaths = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)

$Iscc = $null
foreach ($p in $InnoPaths) { if (Test-Path $p) { $Iscc = $p; break } }

if ($Iscc) {
    & $Iscc "cd\auto-kj64.iss"
    Write-Host "`nSuccess! Installer generated in cd\Output" -ForegroundColor Green
} else {
    Write-Host "`nISCC.exe not found. Staging complete in $OutputDir." -ForegroundColor Yellow
    Write-Host "Please install Inno Setup 6 and run: & 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' cd\auto-kj64.iss" -ForegroundColor Yellow
}
