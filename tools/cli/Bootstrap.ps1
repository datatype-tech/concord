# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
param(
    [ValidateSet('init','install','build','run','doctor')][string]$Action,
    [string]$Project = '.',
    [string]$Release = 'v0.1.0-preview.1',
    [string]$Repository = 'datatype-tech/concord',
    [string]$Sdk = '',
    [string]$Cache = (Join-Path $env:LOCALAPPDATA 'Concord/sdk')
)
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Utility -Force
Set-StrictMode -Version Latest
$ProgressPreference = 'SilentlyContinue'

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with code $LASTEXITCODE" }
}

function Assert-Sdk([string]$Root) {
    foreach ($file in @('bin/concordc.exe','bin/ConcordFlashGameEngineRuntime.dll',
        'bin/ConcordFlashGameEngineRender.dll','tools/cmake/bin/cmake.exe',
        'tools/mingw/bin/g++.exe','tools/mingw/bin/ninja.exe',
        'lib/cmake/ConcordFlash/ConcordFlashConfig.cmake')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $file) -PathType Leaf)) {
            throw "Incomplete SDK: missing $file in $Root"
        }
    }
}

try {
    if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or
        $Repository.Contains('..')) { throw 'Expected --repo owner/repository' }
    if ($Release -notmatch '^v\d+\.\d+\.\d+(-[A-Za-z0-9.-]+)?$') { throw 'Expected a version tag such as v0.1.0-preview.1' }
    if (-not [Environment]::Is64BitProcess) { throw 'Windows x64 is required' }
    if (-not $Sdk) {
        $cacheRoot = [IO.Path]::GetFullPath($Cache)
        New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
        $key = $Repository.Replace('/','_') + '-' + $Release
        $Sdk = Join-Path $cacheRoot $key
        # A file lock serializes first installs; incomplete downloads never become the cache.
        $lock = [IO.File]::Open((Join-Path $cacheRoot "$key.lock"), 'OpenOrCreate', 'ReadWrite', 'None')
        try {
            if (-not (Test-Path -LiteralPath (Join-Path $Sdk '.complete'))) {
                if (Test-Path -LiteralPath $Sdk) { throw "Incomplete cache: remove $Sdk or select another --cache" }
                # Keep extraction below MAX_PATH on Windows PowerShell 5.1.
                # The archive already contains the release name and long CMake filenames.
                $stage = Join-Path $cacheRoot ('.d-' + [guid]::NewGuid().ToString('N').Substring(0,16))
                New-Item -ItemType Directory -Path $stage | Out-Null
                try {
                    $name = 'ConcordFlash-' + $Release.Substring(1) + '-win64'
                    $url = "https://github.com/$Repository/releases/download/$Release/$name.zip"
                    Write-Host "Downloading $Repository $Release (SDK + compiler + build tools)..."
                    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                    $zip = Join-Path $stage 'sdk.zip'
                    Invoke-WebRequest -UseBasicParsing -Uri "$url.sha256" -OutFile (Join-Path $stage 'sha256')
                    $checksum = (Get-Content -LiteralPath (Join-Path $stage 'sha256') -Raw).Trim()
                    if ($checksum -notmatch ('^([a-fA-F0-9]{64})\s+\*?' + [regex]::Escape("$name.zip") + '$')) { throw 'Invalid release checksum file' }
                    $expected = $Matches[1]
                    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip
                    if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $expected) { throw 'SDK SHA256 mismatch' }
                    Add-Type -AssemblyName System.IO.Compression.FileSystem
                    $extract = Join-Path $stage 'extract'
                    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
                    try {
                        $prefix = [IO.Path]::GetFullPath($extract) + [IO.Path]::DirectorySeparatorChar
                        foreach ($entry in $archive.Entries) {
                            $target = [IO.Path]::GetFullPath((Join-Path $extract $entry.FullName))
                            if ($entry.FullName.Contains(':') -or -not $target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
                                throw 'Unsafe path in SDK archive'
                            }
                        }
                    } finally { $archive.Dispose() }
                    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)
                    $root = Join-Path $extract $name
                    Assert-Sdk $root
                    Set-Content -LiteralPath (Join-Path $root '.complete') -Value $expected -Encoding ascii
                    Move-Item -LiteralPath $root -Destination $Sdk
                } finally {
                    $resolved = [IO.Path]::GetFullPath($stage)
                    if ($resolved.StartsWith($cacheRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                        Remove-Item -LiteralPath $resolved -Recurse -Force
                    }
                }
            }
        } finally { $lock.Dispose() }
    }
    $Sdk = [IO.Path]::GetFullPath($Sdk)
    Assert-Sdk $Sdk
    $compiler = Join-Path $Sdk 'bin/concordc.exe'
    $cmake = Join-Path $Sdk 'tools/cmake/bin/cmake.exe'
    $mingw = Join-Path $Sdk 'tools/mingw/bin'
    $env:PATH = "$mingw;$(Join-Path $Sdk 'bin');$env:SystemRoot/System32;$env:SystemRoot"
    if ($Action -eq 'install' -or $Action -eq 'doctor') {
        Invoke-Checked $compiler @('--version')
        Invoke-Checked $cmake @('--version')
        Invoke-Checked (Join-Path $mingw 'g++.exe') @('--version')
        Write-Host "SDK ready: $Sdk"
        exit 0
    }
    $Project = [IO.Path]::GetFullPath($Project)
    if ($Action -eq 'init') {
        Invoke-Checked $compiler @('--init', $Project)
        exit 0
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Project 'CMakeLists.txt'))) { throw 'No game project found; use concord init first' }
    $build = Join-Path $Project 'build-cli'
    Invoke-Checked $cmake @('-S', $Project, '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        "-DCMAKE_PREFIX_PATH=$Sdk", "-DConcordFlash_DIR=$Sdk/lib/cmake/ConcordFlash",
        "-DCONCORDSCRIPT_COMPILER=$compiler", "-DCMAKE_CXX_COMPILER=$mingw/g++.exe", "-DCMAKE_MAKE_PROGRAM=$mingw/ninja.exe")
    Invoke-Checked $cmake @('--build', $build, '--parallel', '4')
    if ($Action -eq 'run') {
        Push-Location $build
        try { Invoke-Checked (Join-Path $build 'game.exe') @() } finally { Pop-Location }
    } else { Write-Host "Game ready: $(Join-Path $build 'game.exe')" }
} catch {
    [Console]::Error.WriteLine("concord: " + $_.Exception.Message)
    exit 1
}
