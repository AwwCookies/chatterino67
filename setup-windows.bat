@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: Chatterino7 Windows Build Setup Script
:: 
:: This script automates the setup of the Windows build environment for 
:: Chatterino7 using vcpkg for dependency management.
::
:: Requirements:
::   - Windows 10 or later
::   - Administrator privileges (for some installations)
::   - At least 30 GB of free disk space
::
:: Usage:
::   Run this script from the repository root directory:
::   > setup-windows.bat
:: ============================================================================

echo.
echo ============================================================
echo   Chatterino7 Windows Build Setup
echo ============================================================
echo.

:: Check if running from the correct directory
if not exist "CMakeLists.txt" (
    echo ERROR: This script must be run from the Chatterino repository root directory.
    echo Please cd to the repository root and run this script again.
    exit /b 1
)

:: ============================================================================
:: Check for Visual Studio
:: ============================================================================
echo [1/7] Checking for Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo.
    echo WARNING: Visual Studio is not installed.
    echo.
    echo Please install Visual Studio 2022 Community with the following:
    echo   - "Desktop development with C++" workload
    echo.
    echo Download from: https://visualstudio.microsoft.com/downloads/
    echo.
    echo After installation, run this script again.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo.
    echo WARNING: Visual Studio with C++ tools is not installed.
    echo.
    echo Please install Visual Studio 2022 Community with the following:
    echo   - "Desktop development with C++" workload
    echo.
    echo Download from: https://visualstudio.microsoft.com/downloads/
    echo.
    echo After installation, run this script again.
    exit /b 1
)

echo   Found Visual Studio at: %VS_PATH%

:: ============================================================================
:: Check for Git
:: ============================================================================
echo [2/7] Checking for Git...

where git >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo.
    echo WARNING: Git is not installed or not in PATH.
    echo.
    echo Please install Git from: https://git-scm.com/download/win
    echo Make sure to add Git to your PATH during installation.
    echo.
    echo After installation, restart your terminal and run this script again.
    exit /b 1
)

for /f "tokens=*" %%i in ('git --version') do set "GIT_VERSION=%%i"
echo   Found: %GIT_VERSION%

:: ============================================================================
:: Check for CMake
:: ============================================================================
echo [3/7] Checking for CMake...

where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo.
    echo WARNING: CMake is not installed or not in PATH.
    echo.
    echo Please install CMake from: https://cmake.org/download/
    echo Make sure to add CMake to your PATH during installation.
    echo.
    echo After installation, restart your terminal and run this script again.
    exit /b 1
)

for /f "tokens=*" %%i in ('cmake --version ^| findstr /r "cmake version"') do set "CMAKE_VERSION=%%i"
echo   Found: %CMAKE_VERSION%

:: ============================================================================
:: Initialize Git Submodules
:: ============================================================================
echo [4/7] Initializing Git submodules...

git submodule update --init --recursive
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Failed to initialize git submodules.
    echo Please check your internet connection and try again.
    exit /b 1
)
echo   Submodules initialized successfully.

:: ============================================================================
:: Setup vcpkg
:: ============================================================================
echo [5/7] Setting up vcpkg...

if not defined VCPKG_ROOT (
    if exist "%USERPROFILE%\vcpkg" (
        set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
        echo   Using existing vcpkg at: %USERPROFILE%\vcpkg
    ) else if exist "C:\vcpkg" (
        set "VCPKG_ROOT=C:\vcpkg"
        echo   Using existing vcpkg at: C:\vcpkg
    ) else (
        echo   vcpkg not found, cloning to %USERPROFILE%\vcpkg...
        git clone https://github.com/Microsoft/vcpkg.git "%USERPROFILE%\vcpkg"
        if %ERRORLEVEL% neq 0 (
            echo.
            echo ERROR: Failed to clone vcpkg.
            echo Please check your internet connection and try again.
            exit /b 1
        )
        set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    )
) else (
    echo   VCPKG_ROOT is set to: %VCPKG_ROOT%
)

:: Bootstrap vcpkg if needed
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo   Bootstrapping vcpkg...
    pushd "%VCPKG_ROOT%"
    call bootstrap-vcpkg.bat
    if %ERRORLEVEL% neq 0 (
        echo.
        echo ERROR: Failed to bootstrap vcpkg.
        popd
        exit /b 1
    )
    popd
)

:: Set environment variables for vcpkg
if not defined VCPKG_DEFAULT_TRIPLET (
    echo   Setting VCPKG_DEFAULT_TRIPLET to x64-windows...
    setx VCPKG_DEFAULT_TRIPLET x64-windows >nul
    set "VCPKG_DEFAULT_TRIPLET=x64-windows"
)

echo   vcpkg setup complete.

:: ============================================================================
:: Install Dependencies via vcpkg
:: ============================================================================
echo [6/7] Installing dependencies via vcpkg...
echo   This may take a while (30-60 minutes on first run)...

"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Failed to install dependencies.
    echo Please check the output above for errors.
    exit /b 1
)

echo   Dependencies installed successfully.

:: ============================================================================
:: Configure CMake Build
:: ============================================================================
echo [7/7] Configuring CMake build...

if not exist "build" mkdir build
pushd build

cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed.
    echo Please check the output above for errors.
    popd
    exit /b 1
)

popd

echo.
echo ============================================================
echo   Setup Complete!
echo ============================================================
echo.
echo To build Chatterino, run the following commands:
echo.
echo   cd build
echo   cmake --build . --parallel %NUMBER_OF_PROCESSORS% --config Release
echo.
echo After building, run the executable:
echo   .\bin\chatterino.exe
echo.
echo For more information, see:
echo   - BUILDING_ON_WINDOWS.md
echo   - BUILDING_ON_WINDOWS_WITH_VCPKG.md
echo.

endlocal
exit /b 0
