# ============================================================
# SonarQube 静态分析扫描脚本
# 支持: 自建 SonarQube / SonarCloud
#
# 用法:
#   自建 SonarQube:
#     .\scripts\sonarqube-scan.ps1 -SonarHostUrl "http://localhost:9000"
#
#   SonarCloud:
#     .\scripts\sonarqube-scan.ps1 -SonarCloud -Organization "your-org"
#
#   GitLab CI 中:
#     设置 CI/CD Variables: SONAR_TOKEN, SONAR_HOST_URL
# ============================================================
param(
    [string]$SonarHostUrl = $env:SONAR_HOST_URL,
    [string]$SonarToken = $env:SONAR_TOKEN,
    [switch]$SonarCloud,
    [string]$Organization = "",

    [string]$BuildWrapperUrl = "https://sonarcloud.io/static/cpp/build-wrapper-win-x86.zip",
    [string]$ToolsDir = "$PSScriptRoot\..\build\sonar-tools",
    [string]$BuildPreset = "msvc-vcpkg-debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = "$PSScriptRoot\.."

# ============================================================
# 参数校验
# ============================================================
if ($SonarCloud) {
    if (-not $Organization) {
        Write-Host "[ERROR] SonarCloud 需要 -Organization 参数" -ForegroundColor Red
        exit 1
    }
    if (-not $SonarHostUrl) {
        $SonarHostUrl = "https://sonarcloud.io"
    }
}

if (-not $SonarHostUrl) {
    $SonarHostUrl = "http://localhost:9000"
    Write-Host "[WARN] 未指定 SonarQube 地址，使用默认值: $SonarHostUrl" -ForegroundColor Yellow
}

if (-not $SonarToken) {
    Write-Host "[ERROR] SONAR_TOKEN 未设置。" -ForegroundColor Red
    Write-Host ""
    Write-Host "请通过以下方式之一提供 Token:" -ForegroundColor Yellow
    Write-Host "  1. 环境变量: set SONAR_TOKEN=sqa_xxx" -ForegroundColor Yellow
    Write-Host "  2. 命令行参数: .\scripts\sonarqube-scan.ps1 -SonarToken 'sqa_xxx'" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "获取 Token:" -ForegroundColor Yellow
    Write-Host "  自建 SonarQube: $SonarHostUrl/account/security" -ForegroundColor Yellow
    Write-Host "  SonarCloud: https://sonarcloud.io/account/security" -ForegroundColor Yellow
    exit 1
}

# ============================================================
# 准备工具
# ============================================================
New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

# Build Wrapper (下载)
$BuildWrapperDir = "$ToolsDir\build-wrapper-win-x86"
if (-not (Test-Path "$BuildWrapperDir\build-wrapper-win-x86-64.exe")) {
    Write-Host "[INFO] 下载 build-wrapper..." -ForegroundColor Cyan
    $ZipPath = "$ToolsDir\build-wrapper.zip"
    Invoke-WebRequest -Uri $BuildWrapperUrl -OutFile $ZipPath
    Expand-Archive -Path $ZipPath -DestinationPath $ToolsDir -Force
    Remove-Item $ZipPath
    Write-Host "[OK] build-wrapper 就绪" -ForegroundColor Green
}

# SonarScanner CLI (下载)
$ScannerDir = "$ToolsDir\sonar-scanner"
if (-not (Test-Path "$ScannerDir\bin\sonar-scanner.bat")) {
    Write-Host "[INFO] 下载 sonar-scanner CLI..." -ForegroundColor Cyan
    $ScannerZip = "$ToolsDir\sonar-scanner.zip"
    Invoke-WebRequest `
        -Uri "https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-6.2.1.4610-windows-x64.zip" `
        -OutFile $ScannerZip
    Expand-Archive -Path $ScannerZip -DestinationPath $ToolsDir -Force
    $ExtractedDir = Get-ChildItem -Path $ToolsDir -Directory | Where-Object { $_.Name -match "sonar-scanner" -and $_.Name -ne "sonar-scanner" } | Select-Object -First 1
    if ($ExtractedDir) {
        if (Test-Path $ScannerDir) { Remove-Item -Recurse -Force $ScannerDir }
        Rename-Item -Path $ExtractedDir.FullName -NewName "sonar-scanner"
    }
    Remove-Item $ScannerZip
    Write-Host "[OK] sonar-scanner 就绪" -ForegroundColor Green
}

# ============================================================
# 构建项目
# ============================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  SonarQube 静态分析扫描" -ForegroundColor Cyan
Write-Host "  服务器: $SonarHostUrl" -ForegroundColor Cyan
if ($SonarCloud) {
    Write-Host "  组织:   $Organization" -ForegroundColor Cyan
}
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# 清理旧的 build-wrapper 输出
$WrapperOutput = "$ProjectRoot\build-wrapper-output"
if (Test-Path $WrapperOutput) {
    Remove-Item -Recurse -Force $WrapperOutput
}

# 配置 + 构建
Write-Host "[1/3] CMake 配置..." -ForegroundColor Cyan
Push-Location $ProjectRoot
cmake --preset $BuildPreset

Write-Host "[2/3] build-wrapper 构建 (拦截编译命令)..." -ForegroundColor Cyan
& "$BuildWrapperDir\build-wrapper-win-x86-64.exe" `
    --out-dir build-wrapper-output `
    cmake --build --preset $BuildPreset --config Debug

# ============================================================
# 运行 SonarScanner
# ============================================================
Write-Host "[3/3] SonarScanner 分析..." -ForegroundColor Cyan

$ScannerArgs = @(
    "-Dsonar.projectKey=cpp-ai-agent",
    "-Dsonar.projectName=cpp-ai-agent",
    "-Dsonar.projectVersion=0.1.0",
    "-Dsonar.host.url=$SonarHostUrl",
    "-Dsonar.token=$SonarToken",
    "-Dsonar.sources=src",
    "-Dsonar.tests=tests",
    "-Dsonar.cfamily.build-wrapper-output=build-wrapper-output",
    "-Dsonar.sourceEncoding=UTF-8",
    "-Dsonar.exclusions=vcpkg_installed/**,build/**,.vs/**,logs/**,docs/**,config/**,examples/**"
)

if ($SonarCloud -and $Organization) {
    $ScannerArgs += "-Dsonar.organization=$Organization"
}

& "$ScannerDir\bin\sonar-scanner.bat" @ScannerArgs

Pop-Location

# ============================================================
# 完成
# ============================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  扫描完成!" -ForegroundColor Green
Write-Host "  查看结果: $SonarHostUrl/dashboard?id=cpp-ai-agent" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Green
