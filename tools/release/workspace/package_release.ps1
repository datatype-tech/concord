#requires -Version 7.0
param(
    [string]$Version = '0.1.0-preview.1',
    [string]$EngineBuild = 'release-build',
    [string]$ScriptBuild = 'script/release-build',
    [string]$ToolchainBin = 'C:/msys64/ucrt64/bin',
    [string]$Workspace = $PSScriptRoot
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^\d+\.\d+\.\d+-preview\.\d+$') { throw 'Expected a preview version' }
$workspace = [IO.Path]::GetFullPath($Workspace)
$engine = [IO.Path]::GetFullPath((Join-Path $workspace $EngineBuild))
$compiler = [IO.Path]::GetFullPath((Join-Path $workspace $ScriptBuild))
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $workspace 'bin/releases'))
$name = "ConcordFlash-$Version-win64"
$stage = Join-Path $releaseRoot "$name-staging-$([guid]::NewGuid().ToString('N'))"
$package = Join-Path $releaseRoot $name
$archive = Join-Path $releaseRoot "$name.zip"
$launcherBuild = Join-Path $workspace 'build/cli'
& cmake -S (Join-Path $workspace 'concord/tools/cli') -B $launcherBuild -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CLI configure failed' }
& cmake --build $launcherBuild -j 4
if ($LASTEXITCODE -ne 0) { throw 'CLI build failed' }
& ctest --test-dir $launcherBuild --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CLI bootstrap tests failed' }

function Copy-Required([string]$From, [string]$To) {
    if (-not (Test-Path -LiteralPath $From -PathType Leaf)) { throw "Missing release input: $From" }
    New-Item -ItemType Directory -Force -Path (Split-Path $To -Parent) | Out-Null
    Copy-Item -LiteralPath $From -Destination $To -Force
}

function Copy-Tree([string]$From, [string]$To) {
    if (-not (Test-Path -LiteralPath $From -PathType Container)) { throw "Missing source tree: $From" }
    New-Item -ItemType Directory -Force -Path $To | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $From -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath($From, $file.FullName)
        if ($relative -match '(^|[\\/])(build[^\\/]*|node_modules|\.git)([\\/]|$)' -or
            $file.Extension -in @('.spv', '.exe', '.dll', '.o', '.obj', '.log')) { continue }
        Copy-Required $file.FullName (Join-Path $To $relative)
    }
}

function Remove-ReleaseArtifact([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($releaseRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw "Path outside release directory: $resolved" }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

foreach ($build in @($engine, $compiler)) {
    $cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
    if ($cache -notmatch 'CMAKE_BUILD_TYPE:STRING=Release' -or $cache -notmatch 'ucrt64') {
        throw "Release UCRT64 build required: $build"
    }
    & cmake --build $build -j 4
    if ($LASTEXITCODE -ne 0) { throw "Release build failed: $build" }
    & ctest --test-dir $build --output-on-failure --no-tests=error --output-junit (Join-Path $build 'release-results.xml')
    if ($LASTEXITCODE -ne 0) { throw "Release tests failed: $build" }
}
$objdump = Join-Path $ToolchainBin 'objdump.exe'
if (-not (Test-Path -LiteralPath $objdump)) { throw "Missing dependency inspector: $objdump" }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
try {
    foreach ($file in @('main.exe', 'ConcordFlashGameEngineRuntime.dll', 'ConcordFlashGameEngineRender.dll', 'SDL3.dll', 'phonon.dll')) {
        Copy-Required (Join-Path $engine $file) (Join-Path $stage "bin/$file")
    }
    foreach ($file in @('concordc.exe', 'cc.exe')) {
        Copy-Required (Join-Path $compiler $file) (Join-Path $stage "bin/$file")
    }
    Copy-Required (Join-Path $launcherBuild 'concord.exe') (Join-Path $stage 'bin/concord.exe')
    & (Join-Path $workspace 'concord/tools/cli/PackageToolchain.ps1') -Destination (Join-Path $stage 'tools') -Prefix (Split-Path $ToolchainBin -Parent)
    $shaderDir = Join-Path $engine 'Assets/Shaders'
    foreach ($shader in @('raygen.rgen.spv', 'rayhit.rchit.spv', 'mesh.vert.spv', 'solid.frag.spv', 'post.comp.spv')) {
        if (-not (Test-Path -LiteralPath (Join-Path $shaderDir $shader))) { throw "Missing compiled shader: $shader" }
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $stage 'bin/Assets/Shaders') | Out-Null
    Get-ChildItem -LiteralPath $shaderDir -Filter '*.spv' | Copy-Item -Destination (Join-Path $stage 'bin/Assets/Shaders')

    # Resolve the entire import closure instead of assuming three MinGW DLLs suffice.
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending = [Collections.Generic.Queue[string]]::new()
    Get-ChildItem -LiteralPath (Join-Path $stage 'bin') -File | ForEach-Object { $pending.Enqueue($_.FullName) }
    while ($pending.Count -gt 0) {
        $binary = $pending.Dequeue()
        if (-not $seen.Add([IO.Path]::GetFileName($binary))) { continue }
        $imports = & $objdump -p $binary
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect imports: $binary" }
        foreach ($line in $imports) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $dll = $Matches[1]
            if ($dll -match '^(api-ms-|ext-ms-)' -or (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$dll"))) { continue }
            $destination = Join-Path $stage "bin/$dll"
            if (-not (Test-Path -LiteralPath $destination)) {
                $found = $null
                foreach ($directory in @($engine, $compiler, $ToolchainBin)) {
                    $candidate = Join-Path $directory $dll
                    if (Test-Path -LiteralPath $candidate) { $found = $candidate; break }
                }
                if (-not $found) { throw "Unresolved dependency $dll required by $binary" }
                Copy-Required $found $destination
            }
            if (-not $seen.Contains($dll)) { $pending.Enqueue($destination) }
        }
    }

    Copy-Tree (Join-Path $workspace 'concord/include') (Join-Path $stage 'include')
    foreach ($dll in @('ConcordFlashGameEngineRuntime', 'ConcordFlashGameEngineRender')) {
        Copy-Required (Join-Path $engine "concord/lib$dll.dll.a") (Join-Path $stage "lib/$dll.dll.a")
    }
    Copy-Required (Join-Path $compiler 'libConcordCvmRuntime.a') (Join-Path $stage 'lib/libConcordCvmRuntime.a')
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $workspace 'script/runtime') -Filter '*.h') {
        Copy-Required $file.FullName (Join-Path $stage "include/cvm/$($file.Name)")
    }
    Copy-Required (Join-Path $workspace 'concord/cmake/ConcordFlashConfig.cmake') (Join-Path $stage 'lib/cmake/ConcordFlash/ConcordFlashConfig.cmake')
    Copy-Tree (Join-Path $workspace 'release') $stage
    Copy-Required (Join-Path $engine 'release-results.xml') (Join-Path $stage 'validation/engine.xml')
    Copy-Required (Join-Path $compiler 'release-results.xml') (Join-Path $stage 'validation/compiler.xml')

    foreach ($area in @('include', 'src/engine', 'cmake', 'assets/shaders', 'docs', 'tests', 'tools/cli')) {
        Copy-Tree (Join-Path $workspace "concord/$area") (Join-Path $stage "source/concord/$area")
    }
    foreach ($file in @('CMakeLists.txt', 'LICENSE', 'README.md', 'README.zh.md')) {
        Copy-Required (Join-Path $workspace "concord/$file") (Join-Path $stage "source/concord/$file")
    }
    foreach ($area in @('src', 'runtime', 'tests', 'examples', 'docs')) {
        Copy-Tree (Join-Path $workspace "script/$area") (Join-Path $stage "source/script/$area")
    }
    foreach ($file in @('CMakeLists.txt', 'CMakePresets.json', 'LICENSE', 'README.md', 'README.zh-CN.md')) {
        Copy-Required (Join-Path $workspace "script/$file") (Join-Path $stage "source/script/$file")
    }
    foreach ($file in @('CMakeLists.txt', 'CMakePresets.json', 'main.cpp', 'setup_deps.ps1', 'AGENTS.md', 'README.md', 'package_release.ps1')) {
        Copy-Required (Join-Path $workspace $file) (Join-Path $stage "source/$file")
    }
    Copy-Tree (Join-Path $workspace 'release') (Join-Path $stage 'source/release')
    Copy-Required (Join-Path $workspace 'concord/LICENSE') (Join-Path $stage 'licenses/Concord-MPL-2.0.txt')
    Copy-Required (Join-Path $workspace 'script/LICENSE') (Join-Path $stage 'licenses/ConcordScript-MIT.txt')
    Copy-Required (Join-Path $workspace 'concord/src/3rd/Jolt/LICENSE') (Join-Path $stage 'licenses/Jolt.txt')
    foreach ($entry in @(@('SDL3/SDL3/SDL.h', 'SDL.h'), @('SteamAudio/phonon.h', 'phonon.h'), @('stb/stb_image.h', 'stb_image.h'), @('stb/stb_image_write.h', 'stb_image_write.h'))) {
        Copy-Required (Join-Path $workspace "concord/src/3rd/$($entry[0])") (Join-Path $stage "licenses/$($entry[1])")
    }
    $prefix = Split-Path $ToolchainBin -Parent
    foreach ($license in @('gcc-libs', 'libwinpthread', 'winpthreads', 'llvm', 'llvm-libs', 'zlib', 'zstd', 'libxml2', 'libffi', 'libiconv', 'gettext-runtime', 'xz')) {
        $from = Join-Path $prefix "share/licenses/$license"
        if (Test-Path -LiteralPath $from) { Copy-Tree $from (Join-Path $stage "licenses/msys2/$license") }
    }
    $pacman = Join-Path (Split-Path $prefix -Parent) 'usr/bin/pacman.exe'
    if (Test-Path -LiteralPath $pacman) {
        & $pacman -Q | Set-Content -LiteralPath (Join-Path $stage 'licenses/msys2-packages.txt') -Encoding utf8
        if ($LASTEXITCODE -ne 0) { throw 'Cannot record MSYS2 package versions' }
    }

    $savedPath = $env:PATH
    try {
        $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
        $reported = & (Join-Path $stage 'bin/concordc.exe') --version
        if ($LASTEXITCODE -ne 0 -or "$reported" -notmatch [regex]::Escape($Version)) { throw "Compiler version mismatch: $reported" }
        & (Join-Path $stage 'bin/concordc.exe') --project (Join-Path $stage 'examples/script') --check
        if ($LASTEXITCODE -ne 0) { throw 'Packaged compiler smoke failed' }
        & (Join-Path $stage 'bin/cc.exe') --project (Join-Path $stage 'examples/cvm') --out (Join-Path $stage 'smoke-output')
        if ($LASTEXITCODE -ne 0) { throw 'Packaged CVM smoke failed' }
    } finally { $env:PATH = $savedPath }
    Remove-ReleaseArtifact (Join-Path $stage 'smoke-output')

    $files = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\', '/'); size = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    [ordered]@{ version = $Version; platform = 'Windows x64'; toolchain = 'MSYS2 UCRT64 GCC 15.2'; configuration = 'Release'; createdUtc = [DateTime]::UtcNow.ToString('o'); files = $files } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'manifest.json') -Encoding utf8
    Remove-ReleaseArtifact $package
    Move-Item -LiteralPath $stage -Destination $package
    Remove-ReleaseArtifact $archive
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($package, $archive, [IO.Compression.CompressionLevel]::Optimal, $true)
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name.zip" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
    Copy-Required (Join-Path $launcherBuild 'concord.exe') (Join-Path $releaseRoot 'concord.exe')
    $cliHash = (Get-FileHash -LiteralPath (Join-Path $releaseRoot 'concord.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    "$cliHash  concord.exe" | Set-Content -LiteralPath (Join-Path $releaseRoot 'concord.exe.sha256') -Encoding ascii
    Write-Host "Release ready: $archive"
} catch {
    Remove-ReleaseArtifact $stage
    throw
}
