# Packages a built FOHScope.exe into dist\FOHScope-<ver>-windows-x64-setup.exe (Inno Setup)
# and dist\FOHScope-<ver>-windows-x64.zip (portable).
# Usage: pwsh scripts/package-windows.ps1 -BuildDir build
param([string]$BuildDir = "build")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$ver = (cmake -P scripts/version.cmake).Trim()
$exe = Resolve-Path (Join-Path $BuildDir "FOHScope_artefacts/Release/FOHScope.exe")
$dist = New-Item -ItemType Directory -Force -Path dist

# Portable zip: the exe plus licence files.
$stage = Join-Path $env:RUNNER_TEMP "fohscope-zip"
if (-not $env:RUNNER_TEMP) { $stage = Join-Path ([IO.Path]::GetTempPath()) "fohscope-zip" }
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path "$stage/FOHScope" | Out-Null
Copy-Item $exe "$stage/FOHScope/"
Copy-Item LICENSE "$stage/FOHScope/LICENSE.txt"
Copy-Item THIRD_PARTY_NOTICES.md "$stage/FOHScope/"
Copy-Item Resources/fonts/OFL.txt "$stage/FOHScope/"
Compress-Archive -Path "$stage/FOHScope" -DestinationPath "dist/FOHScope-$ver-windows-x64.zip" -Force

# Installer.
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
& $iscc "/DAppVersion=$ver" "/DSourceExe=$exe" "/DOutputDir=$($dist.FullName)" packaging/windows/FOHScope.iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }
Get-ChildItem dist
