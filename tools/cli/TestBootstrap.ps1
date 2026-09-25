# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
param([Parameter(Mandatory)][string]$Launcher, [Parameter(Mandatory)][string]$Work)
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Utility -Force
$Work = [IO.Path]::GetFullPath((Join-Path $Work ([guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Path $Work -Force | Out-Null
$name = 'ConcordFlash-0.1.0-preview.1-win64'
$fixture = Join-Path $Work $name
foreach ($file in @('bin/concordc.exe','tools/cmake/bin/cmake.exe','tools/mingw/bin/g++.exe',
    'tools/mingw/bin/ninja.exe','bin/ConcordFlashGameEngineRuntime.dll',
    'bin/ConcordFlashGameEngineRender.dll','lib/cmake/ConcordFlash/ConcordFlashConfig.cmake',
    'tools/cmake/share/cmake-4.3/Modules/CMakeExtraGeneratorDetermineCompilerMacrosAndIncludeDirs.cmake')) {
    $target = Join-Path $fixture $file
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $Launcher -Destination $target
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $Work 'fixture.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($fixture, $zip, 'Optimal', $true)
$checksum = Join-Path $Work 'fixture.sha256'
"$((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash)  $name.zip" | Set-Content -LiteralPath $checksum -Encoding ascii
$runner = Join-Path $Work 'Runner.ps1'
@'
param($Bootstrap,$Work,$Cache,$Mode)
$ErrorActionPreference = 'Stop'
function Invoke-WebRequest {
    param([switch]$UseBasicParsing,[string]$Uri,[string]$OutFile)
    if ($Mode -eq 'offline') { throw 'Network must not be used for a cached SDK' }
    if (-not $Uri.StartsWith('https://github.com/datatype-tech/concord/releases/download/v0.1.0-preview.1/')) { throw 'Unexpected release URL' }
    $source = if ($Uri.EndsWith('.sha256')) { 'fixture.sha256' } else { 'fixture.zip' }
    Copy-Item -LiteralPath (Join-Path $Work $source) -Destination $OutFile
}
& $Bootstrap -Action install -Cache $Cache
exit $LASTEXITCODE
'@ | Set-Content -LiteralPath $runner -Encoding utf8
$bootstrap = Join-Path $PSScriptRoot 'Bootstrap.ps1'
$powershell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
function Run-Case([string]$CacheName,[string]$Mode,[int]$Expected) {
    & $powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Bootstrap $bootstrap -Work $Work -Cache (Join-Path $Work $CacheName) -Mode $Mode
    if ($LASTEXITCODE -ne $Expected) { throw "Case $CacheName/$Mode returned $LASTEXITCODE, expected $Expected" }
}
Run-Case 'cache with spaces' 'online' 0
Run-Case 'cache with spaces' 'offline' 0
('0' * 64 + "  $name.zip") | Set-Content -LiteralPath $checksum -Encoding ascii
Run-Case 'bad-checksum' 'online' 1
if (Test-Path (Join-Path $Work 'bad-checksum/datatype-tech_concord-v0.1.0-preview.1')) { throw 'Corrupt download was cached' }
# A valid checksum must still not permit archive traversal.
$zipFile = [IO.Compression.ZipFile]::Open($zip, 'Update')
try { $entry = $zipFile.CreateEntry('../escape.txt') } finally { $zipFile.Dispose() }
"$((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash)  $name.zip" | Set-Content -LiteralPath $checksum -Encoding ascii
Run-Case 'bad-path' 'online' 1
if (Test-Path (Join-Path $Work 'bad-path/datatype-tech_concord-v0.1.0-preview.1')) { throw 'Unsafe download was cached' }
& $Launcher build --unknown
if ($LASTEXITCODE -ne 1) { throw 'Unknown option was accepted' }
Write-Host 'PASS: download, checksum, traversal, offline cache, paths with spaces, CLI errors'
