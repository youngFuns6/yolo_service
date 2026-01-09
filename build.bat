@echo off
REM 视觉分析服务构建脚本 (Windows)
REM 支持多平台交叉编译

setlocal enabledelayedexpansion

REM 默认配置
set PLATFORM=windows
set ARCH=x64
set BUILD_TYPE=Release
set VCPKG_TRIPLET=
set CLEAN=false
set JOBS=4

REM 解析命令行参数
:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="-p" (
    set PLATFORM=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--platform" (
    set PLATFORM=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-a" (
    set ARCH=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--arch" (
    set ARCH=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-t" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--type" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-c" (
    set CLEAN=true
    shift
    goto parse_args
)
if /i "%~1"=="--clean" (
    set CLEAN=true
    shift
    goto parse_args
)
if /i "%~1"=="-j" (
    set JOBS=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--jobs" (
    set JOBS=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="--help" goto show_help
shift
goto parse_args

:show_help
echo 视觉分析服务构建脚本 (Windows)
echo.
echo 用法: %~nx0 [选项]
echo.
echo 选项:
echo     -p, --platform PLATFORM    目标平台 (windows^|linux^|macos) [默认: windows]
echo     -a, --arch ARCH              目标架构 (x64^|x86^|arm64) [默认: x64]
echo     -t, --type TYPE              构建类型 (Debug^|Release^|RelWithDebInfo) [默认: Release]
echo     -c, --clean                  清理构建目录
echo     -j, --jobs N                 并行编译任务数 [默认: 4]
echo     -h, --help                   显示此帮助信息
echo.
goto end

:end_parse

REM 确定 vcpkg triplet
if "%PLATFORM%-%ARCH%"=="windows-x64" set VCPKG_TRIPLET=x64-windows
if "%PLATFORM%-%ARCH%"=="windows-x86" set VCPKG_TRIPLET=x86-windows
if "%PLATFORM%-%ARCH%"=="windows-arm64" set VCPKG_TRIPLET=arm64-windows
if "%PLATFORM%-%ARCH%"=="linux-x64" set VCPKG_TRIPLET=x64-linux
if "%PLATFORM%-%ARCH%"=="linux-arm64" set VCPKG_TRIPLET=arm64-linux
if "%PLATFORM%-%ARCH%"=="macos-x64" set VCPKG_TRIPLET=x64-osx
if "%PLATFORM%-%ARCH%"=="macos-arm64" set VCPKG_TRIPLET=arm64-osx

if "%VCPKG_TRIPLET%"=="" (
    echo 错误: 不支持的平台-架构组合: %PLATFORM%-%ARCH%
    exit /b 1
)

REM 检查 vcpkg
if "%VCPKG_ROOT%"=="" (
    echo 警告: VCPKG_ROOT 环境变量未设置
    echo 尝试查找 vcpkg...
    if exist "%USERPROFILE%\vcpkg" (
        set VCPKG_ROOT=%USERPROFILE%\vcpkg
        echo 找到 vcpkg: %VCPKG_ROOT%
    ) else if exist "C:\vcpkg" (
        set VCPKG_ROOT=C:\vcpkg
        echo 找到 vcpkg: %VCPKG_ROOT%
    ) else (
        echo 错误: 未找到 vcpkg，请设置 VCPKG_ROOT 环境变量
        exit /b 1
    )
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo 错误: vcpkg 未正确安装: %VCPKG_ROOT%
    exit /b 1
)

echo 使用 vcpkg: %VCPKG_ROOT%

REM 清理构建目录
if "%CLEAN%"=="true" (
    echo 清理构建目录...
    if exist "build" (
        rmdir /s /q build
        echo 构建目录已清理
    ) else (
        echo 构建目录不存在
    )
    goto end
)

REM 创建构建目录
set BUILD_DIR=build\%PLATFORM%-%ARCH%-%BUILD_TYPE%
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
echo 构建目录: %BUILD_DIR%

REM 配置 CMake
echo 配置 CMake...
echo 平台: %PLATFORM%
echo 架构: %ARCH%
echo 构建类型: %BUILD_TYPE%
echo vcpkg triplet: %VCPKG_TRIPLET%

cd %BUILD_DIR%
cmake ..\.. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if errorlevel 1 (
    cd ..\..
    echo 错误: CMake 配置失败
    exit /b 1
)

REM 编译项目
echo 开始编译...
cmake --build . --config %BUILD_TYPE% -j %JOBS%
if errorlevel 1 (
    cd ..\..
    echo 错误: 编译失败
    exit /b 1
)

cd ..\..

echo.
echo ========================================
echo 构建成功！
echo ========================================
echo 平台: %PLATFORM%
echo 架构: %ARCH%
echo 构建类型: %BUILD_TYPE%
echo 构建目录: %BUILD_DIR%
echo.
if exist "%BUILD_DIR%\bin\detector_service.exe" (
    echo 可执行文件: %BUILD_DIR%\bin\detector_service.exe
)
if exist "%BUILD_DIR%\lib" (
    echo 库文件目录: %BUILD_DIR%\lib
)
echo.

:end
endlocal

