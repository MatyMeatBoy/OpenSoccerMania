param(
    [string]$Python = '',
    [string]$Toolchain = 'D:\01Claude\toolchain\llvm-mingw-20261006-ucrt-x86_64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Python) { $Python = Join-Path $projectRoot '.venv\Scripts\python.exe' }
& $Python -m PyInstaller --version
if ($LASTEXITCODE -ne 0) { throw 'Install PyInstaller in the build environment first.' }
# Refuse incomplete or modified vendor components before freezing the launcher.
& $Python -c "import sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from tools.package_release import VENDOR_FILES,VENDOR_LICENSE,validate_vendor_file; root=Path(sys.argv[1]); [(validate_vendor_file(name,(root/name).read_bytes())) for name in VENDOR_FILES]; assert (root/VENDOR_LICENSE).is_file(), 'Missing graphics vendor license'" $projectRoot
if ($LASTEXITCODE -ne 0) { throw 'Pinned graphics vendor validation failed.' }
& (Join-Path $PSScriptRoot 'build_native.ps1') -Toolchain $Toolchain
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$distDir = Join-Path $projectRoot 'build\portable'
$workDir = Join-Path $projectRoot 'build\pyinstaller'
$pythonLicense = & $Python -c "import pathlib,sys; print(pathlib.Path(sys.base_prefix)/'LICENSE.txt')"
if (-not (Test-Path -LiteralPath $pythonLicense)) { throw 'Python runtime license is missing.' }
& $Python -m PyInstaller --noconfirm --clean --noupx --onedir --contents-directory . --windowed `
    --name OpenSoccerMania --distpath $distDir --workpath $workDir --specpath $workDir `
    --add-data "$projectRoot\assets;assets" --add-data "$projectRoot\licenses;licenses" `
    --add-data "$projectRoot\LICENSE;." --add-data "$projectRoot\THIRD_PARTY.md;." `
    --add-data "$projectRoot\README.md;." `
    --add-data "$projectRoot\third_party\minhook\LICENSE.txt;licenses/MinHook" `
    --add-data "$pythonLicense;licenses/Python" --copy-metadata Pillow --copy-metadata pyinstaller `
    --add-binary "$projectRoot\runtime\SDL3.dll;runtime" `
    --add-binary "$projectRoot\runtime\graphics\dgVoodoo\d3d8.dll;runtime/graphics/dgVoodoo" `
    --add-data "$projectRoot\runtime\graphics\dgVoodoo\dgVoodoo.conf;runtime/graphics/dgVoodoo" `
    --add-data "$projectRoot\runtime\graphics\dgVoodoo\manifest.json;runtime/graphics/dgVoodoo" `
    --add-binary "$projectRoot\build\native\d3d8.dll;native" `
    --add-binary "$projectRoot\build\native\dinput8.dll;native" `
    "$projectRoot\OpenSoccerMania.pyw"
if ($LASTEXITCODE -ne 0) { throw 'Portable launcher build failed.' }
Write-Output "Portable candidate: $distDir\OpenSoccerMania"
