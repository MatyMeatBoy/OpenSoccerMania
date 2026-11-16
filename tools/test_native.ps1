param([string]$Toolchain = 'D:\01Claude\toolchain\llvm-mingw-20261006-ucrt-x86_64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot 'build\tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
foreach ($name in @('aspect','frame_stats','layout','screen_vertices','camera_lens','buffer_mirror','private_keyboard','interpolation','ui_state','settings','edition','hook_chain','title_background')) {
    $executable = Join-Path $outputDir "$name.exe"
    & "$Toolchain\bin\i686-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -static "$projectRoot\tests\native_$name.cpp" -lgdi32 -o $executable
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw "Check failed: $name" }
    Write-Output "PASS $name"
}
