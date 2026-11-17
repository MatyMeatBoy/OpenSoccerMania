param([string]$Toolchain = 'D:\01Claude\toolchain\llvm-mingw-20261006-ucrt-x86_64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot 'build\native'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$branding = Join-Path $outputDir 'branding.o'
& "$Toolchain\bin\i686-w64-mingw32-windres.exe" -I "$projectRoot\assets\branding" -i "$projectRoot\native\branding.rc" -O coff -o $branding
if ($LASTEXITCODE -ne 0) { throw 'Branding resource compilation failed.' }
$hookObjects = @()
foreach ($source in @('buffer.c','hook.c','trampoline.c','hde\hde32.c')) {
    $objectPath = Join-Path $outputDir ((Split-Path $source -Leaf) + '.o')
    & "$Toolchain\bin\i686-w64-mingw32-clang.exe" -O2 -c "$projectRoot\third_party\minhook\src\$source" -o $objectPath
    if ($LASTEXITCODE -ne 0) { throw 'MinHook compilation failed.' }
    $hookObjects += $objectPath
}
& "$Toolchain\bin\i686-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -static -shared "$projectRoot\native\d3d8_proxy.cpp" "$projectRoot\native\backend_probe.cpp" "$projectRoot\native\d3d8.def" @hookObjects $branding -lgdi32 -ladvapi32 -ldxguid -o "$outputDir\d3d8.dll"
if ($LASTEXITCODE -ne 0) { throw 'Native bridge compilation failed.' }
& "$Toolchain\bin\i686-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -static -shared "$projectRoot\native\input_proxy.cpp" "$projectRoot\native\dinput8.def" $branding -o "$outputDir\dinput8.dll"
if ($LASTEXITCODE -ne 0) { throw 'Input bridge compilation failed.' }
