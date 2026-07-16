$ErrorActionPreference = "Continue"
$ProjectRoot = "$PSScriptRoot\.."
$ClangTidy = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
$ReportFile = "$ProjectRoot\build\analysis-reports\clang-tidy-report.txt"
$VcpkgInclude = "$ProjectRoot\build\msvc-vcpkg-debug\vcpkg_installed\x64-windows\include"

New-Item -ItemType Directory -Force -Path (Split-Path $ReportFile) | Out-Null
"" | Out-File $ReportFile

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  clang-tidy 静态分析" -ForegroundColor Cyan
Write-Host "  工具: $ClangTidy" -ForegroundColor Cyan
Write-Host "  配置: .clang-tidy" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

$sourceFiles = Get-ChildItem -Path "$ProjectRoot\src" -Recurse -Include "*.cpp" | ForEach-Object { $_.FullName }
$totalFiles = $sourceFiles.Count
$current = 0
$totalWarnings = 0
$filesWithWarnings = @()

Write-Host "分析 $totalFiles 个源文件..."
Write-Host ""

foreach ($file in $sourceFiles) {
    $current++
    $shortName = $file.Replace($ProjectRoot, "").TrimStart("\")
    Write-Progress -Activity "clang-tidy 分析中" -Status "$current/$totalFiles : $shortName" -PercentComplete (($current / $totalFiles) * 100)

    $output = & $ClangTidy `
        --config-file="$ProjectRoot\.clang-tidy" `
        --extra-arg="-std=c++17" `
        --extra-arg="-fms-compatibility" `
        --extra-arg="-fms-extensions" `
        --extra-arg="-fdelayed-template-parsing" `
        --extra-arg="-I$ProjectRoot\src" `
        --extra-arg="-I$VcpkgInclude" `
        --extra-arg="-D_DEBUG" `
        --extra-arg="-DWIN32" `
        --extra-arg="-D_CONSOLE" `
        "$file" 2>&1

    $warnings = ($output | Select-String -Pattern "warning:" | Measure-Object).Count
    $errors = ($output | Select-String -Pattern "error:" | Measure-Object).Count

    if ($warnings -gt 0 -or $errors -gt 0) {
        $totalWarnings += $warnings
        $filesWithWarnings += "$shortName ($warnings warnings, $errors errors)"
        Write-Host "[WARN] $shortName : $warnings warnings, $errors errors" -ForegroundColor Yellow
        $output | Out-File -Append $ReportFile
    } else {
        Write-Host "[OK]   $shortName" -ForegroundColor Green
        "=== $shortName : CLEAN ===" | Out-File -Append $ReportFile
    }
}

Write-Progress -Activity "clang-tidy 分析" -Completed

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  分析完成!" -ForegroundColor Cyan
Write-Host "  文件总数: $totalFiles" -ForegroundColor Cyan
Write-Host "  有警告的文件: $($filesWithWarnings.Count)" -ForegroundColor $(if ($filesWithWarnings.Count -gt 0) { "Yellow" } else { "Green" })
Write-Host "  警告总数: $totalWarnings" -ForegroundColor $(if ($totalWarnings -gt 0) { "Yellow" } else { "Green" })
Write-Host "  报告: $ReportFile" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

if ($filesWithWarnings.Count -gt 0) {
    Write-Host ""
    Write-Host "有警告的文件列表:" -ForegroundColor Yellow
    foreach ($f in $filesWithWarnings) {
        Write-Host "  $f" -ForegroundColor Yellow
    }
}
