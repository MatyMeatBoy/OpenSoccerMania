param(
    [string]$Toolchain = 'D:\01Claude\toolchain\llvm-mingw-20261006-ucrt-x86_64',
    [switch]$Gpu,
    [string]$Python = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot 'build\tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
foreach ($name in @('aspect','frame_stats','layout','screen_vertices','capture_pixels','camera_lens','buffer_mirror','private_keyboard','cursor_mode','interpolation','ui_state','settings','edition','portable','hook_chain','title_background','world_effects','menu_background','menu_preview','virtual_pad','pad_fallback')) {
    $executable = Join-Path $outputDir "$name.exe"
    & "$Toolchain\bin\i686-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -static "$projectRoot\tests\native_$name.cpp" -lgdi32 -o $executable
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw "Check failed: $name" }
    Write-Output "PASS $name"
}
$padFallback = Join-Path $outputDir 'pad_fallback.exe'
& $padFallback late
if ($LASTEXITCODE -ne 0) { throw 'Check failed: late pad producer' }
$inputBridge = Join-Path $projectRoot 'build\native\dinput8.dll'
if (Test-Path -LiteralPath $inputBridge) {
    & (Join-Path $outputDir 'virtual_pad.exe') $inputBridge
    if ($LASTEXITCODE -ne 0) { throw 'Check failed: actual input bridge factory' }
}
if ($Gpu) {
    if (-not $Python) {
        $Python = Join-Path $projectRoot '.venv\Scripts\python.exe'
    }
    & $Python (Join-Path $PSScriptRoot 'test_gpu.py') --toolchain $Toolchain
    if ($LASTEXITCODE -ne 0) { throw 'GPU integration checks failed.' }
}
