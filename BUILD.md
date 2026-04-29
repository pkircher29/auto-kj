# Auto-KJ Build Guide

This document outlines how to build Auto-KJ from source for Windows, macOS, and Linux.

## Table of Contents
- [Prerequisites](#prerequisites)
  - [Windows](#windows)
  - [macOS](#macos)
  - [Linux](#linux)
- [Building](#building)
  - [CMake Configuration](#cmake-configuration)
  - [Build Commands](#build-commands)
- [Running Tests](#running-tests)
- [Packaging](#packaging)
  - [Windows](#windows-1)
  - [macOS](#macos-1)
  - [Linux](#linux-1)
- [CI/CD](#cicd)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Windows
- **Visual Studio 2022** (with C++ support)
- **Qt 5.15.2** (MSVC 2019 64-bit)
  - Install via [Qt Online Installer](https://www.qt.io/download) or `aqtinstall`:
    ```powershell
    pip install aqtinstall
    aqt install-qt windows desktop 5.15.2 win64_msvc2019_64
    ```
- **GStreamer 1.26**
  - Download from [GStreamer](https://gstreamer.freedesktop.org/download/) or use Chocolatey:
    ```powershell
    choco install gstreamer gstreamer-devel
    ```
- **OpenSSL 1.1**
  - Install via [Win64OpenSSL](https://slproweb.com/products/Win32OpenSSL.html) or Chocolatey:
    ```powershell
    choco install openssl.light
    ```
- **CMake** (3.14+)
  - Install via [CMake](https://cmake.org/download/) or Chocolatey:
    ```powershell
    choco install cmake
    ```
- **Inno Setup 6** (for installer)
  - Download from [Inno Setup](https://jrsoftware.org/isinfo.php) or use Chocolatey:
    ```powershell
    choco install innosetup
    ```

### macOS
- **Xcode** (with command-line tools)
  ```bash
  xcode-select --install
  ```
- **Qt 5.15.2**
  - Install via [Qt Online Installer](https://www.qt.io/download) or `aqtinstall`:
    ```bash
    pip install aqtinstall
    aqt install-qt mac desktop 5.15.2 clang_64
    ```
- **GStreamer**
  ```bash
  brew install gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav
  ```
- **TagLib**
  ```bash
  brew install taglib
  ```
- **CMake**
  ```bash
  brew install cmake
  ```

### Linux
- **Build Tools**
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake
  ```
- **Qt 5.15.2**
  - Install via [Qt Online Installer](https://www.qt.io/download) or `aqtinstall`:
    ```bash
    pip install aqtinstall
    aqt install-qt linux desktop 5.15.2 gcc_64
    ```
- **GStreamer**
  ```bash
  sudo apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav
  ```
- **TagLib**
  ```bash
  sudo apt-get install -y libtag1-dev
  ```
- **OpenSSL**
  ```bash
  sudo apt-get install -y libssl-dev
  ```

---

## Frontend
Auto-KJ uses **Qt Widgets** for its UI (no separate frontend like React, Vue, or Tauri). All UI code is built alongside the C++ backend.

---

## Building

### CMake Configuration
```bash
cmake -B build -G "<Generator>" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEPLOY_DEPS=ON \
  -DQT_INSTALL_PREFIX=<Qt_Path> \
  -DGSTREAMER_ROOT=<GStreamer_Path>
```

#### Generator Options:
- **Windows:** `"Visual Studio 17 2022"` (or `"Ninja"`)
- **macOS/Linux:** `"Unix Makefiles"` (or `"Ninja"`)

#### Platform-Specific Notes:
- **Windows:** Add `-A x64` for 64-bit builds.
- **macOS:** Add `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"` for universal binaries.

### Build Commands
```bash
cmake --build build --config Release --parallel
```

---

## Running Tests
Enable tests in CMake:
```bash
cmake -B build -DAUTOKJ_ENABLE_QT_TESTS=ON
```

Run tests:
```bash
cmake --build build --target test
```

---

## Packaging

### Windows
Use the `package_windows.ps1` script:
```powershell
./package_windows.ps1 \
  -Generator "Visual Studio 17 2022" \
  -Config Release \
  -QtPath "C:\Qt\5.15.2\msvc2019_64" \
  -GStreamerPath "C:\Program Files\gstreamer\1.0\msvc_x86_64"
```

Outputs:
- `Auto-KJ-Windows-x64.zip` (portable)
- `Auto-KJ-Windows-x64-Setup.exe` (installer)

### macOS
```bash
mkdir -p release
cp build/auto-kj release/
$Qt5_DIR/../../bin/macdeployqt release/auto-kj.app -always-overwrite
# Bundle GStreamer
mkdir -p release/auto-kj.app/Contents/Frameworks/GStreamer.framework
cp -r $(brew --prefix gstreamer)/lib/gstreamer-1.0 release/auto-kj.app/Contents/Frameworks/
cp -r $(brew --prefix gstreamer)/lib/libgst* release/auto-kj.app/Contents/Frameworks/
# Create DMG
hdiutil create -volname "Auto-KJ" -srcfolder release -ov -format UDZO Auto-KJ-macOS.dmg
```

### Linux
```bash
mkdir -p release
cp build/auto-kj release/
# Bundle Qt dependencies
$Qt5_DIR/../../bin/linuxdeployqt release/auto-kj -always-overwrite
# Create AppImage
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
./linuxdeploy-x86_64.AppImage --appdir release --executable release/auto-kj --output appimage
mv Auto_KJ*.AppImage Auto-KJ-Linux.AppImage
```

---

## CI/CD
Auto-KJ uses GitHub Actions for CI/CD. Workflows are defined in `.github/workflows/`:

| Workflow | Platform | Triggers | Artifacts |
|----------|----------|----------|-----------|
| `build-windows.yml` | Windows | Push, PR, Tag | Portable ZIP, Installer |
| `build-macos.yml` | macOS | Push, PR, Tag | DMG |
| `build-linux.yml` | Linux | Push, PR, Tag | AppImage |

### Manual Workflow Dispatch
To trigger a workflow manually:
1. Go to **Actions** in the GitHub repository.
2. Select the workflow (e.g., "Build Auto-KJ Windows").
3. Click **Run workflow**.

---

## Troubleshooting

### Windows
- **Missing OpenSSL DLLs:**
  - Ensure `libssl-1_1-x64.dll` and `libcrypto-1_1-x64.dll` are in `PATH` or the build directory.
  - Reinstall OpenSSL 1.1 if missing.

- **GStreamer Not Found:**
  - Set `GST_BASE_PATH` to the GStreamer installation directory (e.g., `C:\gstreamer-sdk`).

- **Qt Not Found:**
  - Set `QT_INSTALL_PREFIX` to the Qt installation directory (e.g., `C:\Qt\5.15.2\msvc2019_64`).

### macOS
- **GStreamer Not Found:**
  - Ensure GStreamer is installed via Homebrew:
    ```bash
    brew install gstreamer
    ```
  - Set `GSTREAMER_ROOT` to the Homebrew prefix:
    ```bash
    export GSTREAMER_ROOT=$(brew --prefix gstreamer)
    ```

- **Qt Not Found:**
  - Set `Qt5_DIR` to the Qt installation directory (e.g., `~/Qt/5.15.2/clang_64`).

### Linux
- **Missing Dependencies:**
  - Install all required packages:
    ```bash
    sudo apt-get install -y \
      build-essential \
      cmake \
      libgl1-mesa-dev \
      libgstreamer1.0-dev \
      libtag1-dev \
      libssl-dev
    ```

- **Qt Not Found:**
  - Set `Qt5_DIR` to the Qt installation directory (e.g., `~/Qt/5.15.2/gcc_64`).

---

## License
Auto-KJ is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.