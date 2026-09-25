# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
param(
    [Parameter(Mandatory)][string]$Destination,
    [string]$Prefix = 'C:/msys64/ucrt64',
    [string]$CMakeRoot = (Split-Path (Split-Path (Get-Command cmake).Source -Parent) -Parent)
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Destination = [IO.Path]::GetFullPath($Destination)
$mingw = Join-Path $Destination 'mingw'
$msys = Split-Path $Prefix -Parent
$pacman = Join-Path $msys 'usr/bin/pacman.exe'
$packages = @('gcc','gcc-libs','binutils','crt','headers','winpthreads','libwinpthread','windows-default-manifest','ninja') |
    ForEach-Object { "mingw-w64-ucrt-x86_64-$_" }
New-Item -ItemType Directory -Force -Path $mingw | Out-Null
$versions = & $pacman -Q @packages
if ($LASTEXITCODE -ne 0) { throw 'Required UCRT64 toolchain packages are missing' }
$versions | Set-Content -LiteralPath (Join-Path $Destination 'toolchain-packages.txt') -Encoding utf8
$files = & $pacman -Qlq @packages
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate toolchain files' }
foreach ($file in $files) {
    if (-not $file.StartsWith('/ucrt64/') -or $file.EndsWith('/')) { continue }
    $relative = $file.Substring('/ucrt64/'.Length)
    $source = Join-Path $Prefix $relative
    $target = Join-Path $mingw $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
}
# Include the import closure of compiler subprocesses (cc1plus, assembler and linker).
$queue = [Collections.Generic.Queue[string]]::new()
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
Get-ChildItem -LiteralPath $mingw -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') } |
    ForEach-Object { $queue.Enqueue($_.FullName) }
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $seen.Add($binary)) { continue }
    $imports = & (Join-Path $Prefix 'bin/objdump.exe') -p $binary
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $binary" }
    foreach ($line in $imports) {
        if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
        $dll = $Matches[1]
        if ($dll -match '^(api-ms-|ext-ms-)' -or (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$dll"))) { continue }
        $target = Join-Path $mingw "bin/$dll"
        if (-not (Test-Path -LiteralPath $target)) {
            Copy-Item -LiteralPath (Join-Path $Prefix "bin/$dll") -Destination $target
            $queue.Enqueue($target)
        }
    }
}
foreach ($area in @('bin','share')) {
    $target = Join-Path $Destination "cmake/$area"
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item -Path (Join-Path $CMakeRoot "$area/*") -Destination $target -Recurse -Force
}
# Keep notices for dynamically loaded compiler dependencies as well as main packages.
Copy-Item -LiteralPath (Join-Path $Prefix 'share/licenses') -Destination (Join-Path $Destination 'licenses') -Recurse -Force
Write-Host "Bundled toolchain: $Destination"
