# ============================================================
# 本地静态分析一键脚本 (clang-tidy + cppcheck)
# 用法: .\scripts\static-analysis.ps1
# ============================================================
param(
    [switch]$SkipClangTidy,
    [switch]$SkipCppcheck,
    [string]$BuildPreset = "msvc-vcpkg-debug"
)

$ErrorActionPreference = "Continue"
$ProjectRoot = "$PSScriptRoot\.."
$ReportDir = "$ProjectRoot\build\analysis-reports"
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$ExitCode = 0

# ============================================================
# 1. 构建项目 (生成 compile_commands.json)
# ============================================================
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Step 1: 构建项目" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

Push-Location $ProjectRoot
cmake --preset $BuildPreset
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] CMake 配置失败" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

# ============================================================
# 2. cppcheck 分析
# ============================================================
if (-not $SkipCppcheck) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  Step 2: cppcheck 静态分析" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan

    $CppcheckPath = Get-Command cppcheck -ErrorAction SilentlyContinue
    if (-not $CppcheckPath) {
        Write-Host "[WARN] cppcheck 未安装，跳过。" -ForegroundColor Yellow
        Write-Host "       安装: choco install cppcheck  或  winget install cppcheck" -ForegroundColor Yellow
    } else {
        $CppcheckReport = "$ReportDir\cppcheck-report.xml"
        Write-Host "[INFO] 运行 cppcheck..." -ForegroundColor Green

        cppcheck `
            --enable=all `
            --inconclusive `
            --std=c++17 `
            --platform=win64 `
            --suppressions-list="$ProjectRoot\cppcheck-suppressions.txt" `
            --xml `
            --xml-version=2 `
            --output-file="$CppcheckReport" `
            -i "$ProjectRoot\vcpkg_installed" `
            -i "$ProjectRoot\build" `
            "$ProjectRoot\src" "$ProjectRoot\tests" 2>&1

        if (Test-Path $CppcheckReport) {
            Write-Host "[OK] cppcheck 报告: $CppcheckReport" -ForegroundColor Green
        }
    }
}

# ============================================================
# 3. clang-tidy 分析 (如果有 compile_commands.json)
# ============================================================
if (-not $SkipClangTidy) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  Step 3: clang-tidy 静态分析" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan

    $ClangTidyPath = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if (-not $ClangTidyPath) {
        Write-Host "[WARN] clang-tidy 未安装，跳过。" -ForegroundColor Yellow
        Write-Host "       安装: choco install llvm  或  winget install LLVM.LLVM" -ForegroundColor Yellow
    } else {
        $ClangTidyReport = "$ReportDir\clang-tidy-report.txt"
        Write-Host "[INFO] 运行 clang-tidy..." -ForegroundColor Green

        # 收集所有源文件
        $SourceFiles = Get-ChildItem -Path "$ProjectRoot\src" -Recurse -Include "*.cpp","*.h","*.hpp" `
            | ForEach-Object { $_.FullName }

        $TotalFiles = $SourceFiles.Count
        $Current = 0

        foreach ($File in $SourceFiles) {
            $Current++
            Write-Progress -Activity "clang-tidy 分析中" `
                -Status "$Current / $TotalFiles" `
                -PercentComplete (($Current / $TotalFiles) * 100)

            clang-tidy `
                --config-file="$ProjectRoot\.clang-tidy" `
                --extra-arg="-std=c++17" `
                "$File" 2>&1 | Out-File -Append -FilePath $ClangTidyReport
        }

        Write-Progress -Activity "clang-tidy 分析" -Completed
        Write-Host "[OK] clang-tidy 报告: $ClangTidyReport" -ForegroundColor Green

        # 统计发现问题数
        $WarningCount = (Select-String -Path $ClangTidyReport -Pattern "warning:" | Measure-Object).Count
        Write-Host "[INFO] 发现 $WarningCount 个警告" -ForegroundColor $(if ($WarningCount -gt 0) { "Yellow" } else { "Green" })
    }
}

# ============================================================
# 汇总
# ============================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  分析完成! 报告目录: $ReportDir" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Get-ChildItem $ReportDir | ForEach-Object { Write-Host "  - $($_.Name)" }

if ($ExitCode -ne 0) {
    Write-Host "[WARN] 部分分析发现问题，请查看报告。" -ForegroundColor Yellow
}
