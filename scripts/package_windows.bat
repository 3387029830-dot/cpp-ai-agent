@echo off
:: =============================================================================
:: cpp-ai-agent Windows 打包脚本
:: =============================================================================
:: 用法:
::   scripts\package_windows.bat
::
:: 输出: build-output\ai-agent-windows.zip
:: =============================================================================

setlocal enabledelayedexpansion

set "PROJECT_DIR=%~dp0.."
set "OUTPUT_DIR=%PROJECT_DIR%\build-output"
set "PACKAGE_DIR=%OUTPUT_DIR%\ai-agent-windows"
set "BUILD_DIR=%PROJECT_DIR%\build\msvc-vcpkg-debug"

echo === Building Debug ===
cmake --preset msvc-vcpkg-debug
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configure failed
    exit /b 1
)
cmake --build --preset msvc-vcpkg-debug
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo === Packaging ===
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%\bin"
mkdir "%PACKAGE_DIR%\config"
mkdir "%PACKAGE_DIR%\web"

copy "%BUILD_DIR%\Debug\ai-agent.exe" "%PACKAGE_DIR%\bin\" >nul 2>&1
if not exist "%PACKAGE_DIR%\bin\ai-agent.exe" (
    copy "%BUILD_DIR%\ai-agent.exe" "%PACKAGE_DIR%\bin\" >nul 2>&1
)
:: copy required DLLs
copy "%BUILD_DIR%\*.dll" "%PACKAGE_DIR%\bin\" >nul 2>&1
copy "%BUILD_DIR%\Debug\*.dll" "%PACKAGE_DIR%\bin\" >nul 2>&1
if not exist "%PACKAGE_DIR%\bin\ai-agent.exe" (
    echo [ERROR] Binary not found. Build may have failed.
    exit /b 1
)

copy "%PROJECT_DIR%\config\*.json" "%PACKAGE_DIR%\config\" >nul 2>&1
xcopy /s /y "%PROJECT_DIR%\web\*" "%PACKAGE_DIR%\web\" >nul 2>&1
copy "%PROJECT_DIR%\.env.example" "%PACKAGE_DIR%\.env.example" >nul 2>&1
copy "%PROJECT_DIR%\README.md" "%PACKAGE_DIR%\" >nul 2>&1
copy "%PROJECT_DIR%\AGENTS.md" "%PACKAGE_DIR%\" >nul 2>&1

:: Create a convenience launcher
(
echo @echo off
echo cd /d "%%~dp0"
echo echo.
echo echo   cpp-ai-agent v0.2.0
echo echo.
echo echo   用法:
echo echo     ai-agent.exe              ^# 终端 TUI 模式
echo echo     ai-agent.exe /web 8080   ^# 浏览器 Web 模式
echo echo     ai-agent.exe /doctor     ^# 诊断配置
echo echo     ai-agent.exe /demo       ^# 演示模式
echo echo     ai-agent.exe /skills     ^# 查看 Skill
echo echo.
echo echo   先编辑 .env 填入你的 API Key 再启动！
echo echo.
echo cmd /k
) > "%PACKAGE_DIR%\start-here.bat"

echo.
echo === Creating zip ===
powershell -Command "Compress-Archive -Path '%PACKAGE_DIR%\*' -DestinationPath '%OUTPUT_DIR%\ai-agent-windows.zip' -Force"

echo.
echo ==============================================
echo   Package ready
echo ==============================================
echo   %OUTPUT_DIR%\ai-agent-windows.zip
echo ==============================================
