# Auto-KJ Windows Packaging Script
# This script automates the build, dependency deployment, and installer generation.

param(
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Config = "Release",
    [string]$QtPath = "C:\Qt\5.15.2\msvc2019_64",
    [string]$GStreamerPath = "C:\Program Files\gstreamer\1.0\msvc_x86_64",
    [string]$BuildDir = "build_windows",
    [string]$OutputDir = "cd\output"
)

$ErrorActionPreference = "Stop"
$VSPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$IsMultiConfig = $Generator -match "Visual Studio|Xcode|Ninja Multi-Config"

function Write-Step($msg) {
    Write-Host "`n>>> $msg" -ForegroundColor Cyan
}

function Find-AndCopyRuntimeDll {
    param(
        [Parameter(Mandatory = $true)][string]$DllName,
        [Parameter(Mandatory = $true)][string]$DestinationDir,
        [Parameter(Mandatory = $true)][string[]]$SearchRoots
    )

    $destinationPath = Join-Path $DestinationDir $DllName
    if (Test-Path $destinationPath) {
        Write-Host "Found $DllName already staged."
        return $true
    }

    foreach ($root in $SearchRoots) {
        if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path $root)) { continue }

        $direct = Join-Path $root $DllName
        if (Test-Path $direct) {
            Copy-Item $direct $DestinationDir -Force
            Write-Host "Collected $DllName from $root"
            return $true
        }
    }

    foreach ($root in $SearchRoots) {
        if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path $root)) { continue }
        $candidate = Get-ChildItem -Path $root -Filter $DllName -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($candidate) {
            Copy-Item $candidate.FullName $DestinationDir -Force
            Write-Host "Collected $DllName from $($candidate.DirectoryName)"
            return $true
        }
    }

    Write-Warning "Optional runtime DLL not found during staging: $DllName"
    return $false
}

function Assert-ImportedRuntimeDllPresent {
    param(
        [Parameter(Mandatory = $true)][string]$DllName,
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)][string]$OutputDir
    )

    $dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if (-not $dumpbin) {
        Write-Warning "dumpbin.exe not found, skipping import validation for $DllName"
        return
    }

    $dependents = (& $dumpbin.Source /DEPENDENTS $ExecutablePath 2>$null) -join "`n"
    if ($dependents -notmatch [regex]::Escape($DllName)) { return }

    if (-not (Test-Path (Join-Path $OutputDir $DllName))) {
        throw "Missing required runtime DLL: $DllName. auto-kj.exe imports this DLL but it was not staged."
    }
}

# 1. Setup Environment
Write-Step "Preparing Environment..."
if (!(Test-Path $QtPath)) { throw "Qt not found at $QtPath" }
if (!(Test-Path $GStreamerPath)) { throw "GStreamer not found at $GStreamerPath" }
if (!(Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir } else { Remove-Item -Path "$OutputDir\*" -Recurse -Force }

# 2. Configure and Build
Write-Step "Configuring CMake..."
$ConfigureArgs = @(
    "-B", $BuildDir,
    "-G", $Generator,
    "-DDEPLOY_DEPS=ON",
    "-DQT_INSTALL_PREFIX=$QtPath",
    "-DGST_BASE_PATH=$GStreamerPath"
)
if ($Generator -match "Visual Studio") {
    $ConfigureArgs += @("-A", "x64")
}
if (-not $IsMultiConfig) {
    $ConfigureArgs += "-DCMAKE_BUILD_TYPE=$Config"
}
& "cmake" @ConfigureArgs

Write-Step "Building Auto-KJ ($Config)..."
$BuildArgs = @("--build", $BuildDir, "--parallel")
if ($IsMultiConfig) {
    $BuildArgs += @("--config", $Config)
}
& "cmake" @BuildArgs

# 3. Collect Build Artifacts
Write-Step "Collecting Build Artifacts..."
$BuildOutputDir = if ($IsMultiConfig) { Join-Path $BuildDir $Config } else { $BuildDir }
Copy-Item "$BuildOutputDir\*" $OutputDir -Recurse -Force

# 4. Deploy Qt Dependencies
Write-Step "Running windeployqt..."
& "$QtPath\bin\windeployqt.exe" --release --no-translations "$OutputDir\auto-kj.exe"

# 5. Collect GStreamer Dependencies
Write-Step "Collecting GStreamer DLLs..."
$GstBin = "$GStreamerPath\bin"
Get-ChildItem -Path $GstBin -Filter "*.dll" | Copy-Item -Destination $OutputDir -Force

# 6. Collect OpenSSL DLLs for Qt Network/TLS
Write-Step "Collecting OpenSSL DLLs..."
$OpenSslNames = @(
    "libssl-1_1-x64.dll",
    "libcrypto-1_1-x64.dll"
)
$OpenSslSearchDirs = @(
    "$QtPath\bin",
    "C:\Program Files\OpenSSL-Win64\bin"
)

$MissingOpenSsl = @()
foreach ($dll in $OpenSslNames) {
    $copied = $false
    foreach ($dir in $OpenSslSearchDirs) {
        $candidate = Join-Path $dir $dll
        if (Test-Path $candidate) {
            Copy-Item $candidate $OutputDir -Force
            Write-Host "Collected $dll from $dir"
            $copied = $true
            break
        }
    }
    if (-not $copied) {
        $MissingOpenSsl += $dll
    }
}

if ($MissingOpenSsl.Count -gt 0) {
    $missingList = $MissingOpenSsl -join ", "
    Write-Host ""
    Write-Host "Missing OpenSSL 1.1 runtime DLL(s): $missingList" -ForegroundColor Red
    Write-Host "Qt 5.15.2 requires OpenSSL 1.1 for HTTPS/TLS. Install it with:" -ForegroundColor Yellow
    Write-Host "  winget install ShiningLight.OpenSSL.Light" -ForegroundColor Cyan
    Write-Host "Then rerun this script." -ForegroundColor Yellow
    throw "Packaging aborted: missing OpenSSL 1.1 runtime."
}

# 7. Collect optional runtime DLLs used by some shared package-manager builds
Write-Step "Collecting optional runtime DLLs..."
$ExtraRuntimeDlls = @(
    "spdlog.dll",
    "qrencode.dll",
    "libqrencode.dll"
)

$ExtraSearchRoots = @(
    "$BuildDir\Release",
    "$BuildDir",
    "build\Release",
    "build",
    "build_win\Release",
    "build_win",
    "build_windows\Release",
    "build_windows",
    "$QtPath\bin",
    "$GStreamerPath\bin",
    "$env:VCPKG_ROOT\installed\x64-windows\bin",
    "$env:VCPKG_ROOT\installed\x64-windows-release\bin",
    "$env:USERPROFILE\vcpkg\installed\x64-windows\bin",
    "$env:USERPROFILE\vcpkg\installed\x64-windows-release\bin"
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

foreach ($dll in $ExtraRuntimeDlls) {
    Find-AndCopyRuntimeDll -DllName $dll -DestinationDir $OutputDir -SearchRoots $ExtraSearchRoots | Out-Null
}

Assert-ImportedRuntimeDllPresent -DllName "spdlog.dll" -ExecutablePath "$OutputDir\auto-kj.exe" -OutputDir $OutputDir
Assert-ImportedRuntimeDllPresent -DllName "qrencode.dll" -ExecutablePath "$OutputDir\auto-kj.exe" -OutputDir $OutputDir
Assert-ImportedRuntimeDllPresent -DllName "libqrencode.dll" -ExecutablePath "$OutputDir\auto-kj.exe" -OutputDir $OutputDir

# 7. Collect Assets (Fonts, Redist)
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

# 8. Collect License
if (Test-Path "LICENSE") {
    Copy-Item "LICENSE" "$OutputDir\LICENSE.txt" -Force
}

# 9. Final Installer Generation (64-bit only)
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
