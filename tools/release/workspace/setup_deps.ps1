# setup_deps.ps1 - downloads and installs third-party dependencies for Concord.
#   headers -> src/3rd  (SDL3 / SteamAudio / Vulkan / stb)
#   prebuilt libs -> lib  (.lib / .dll / .a)
# Idempotent: safe to run repeatedly.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }
Set-Location $root

$tmp = Join-Path $env:TEMP 'concord_dl'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$ua = 'concord-setup/1.0'
$dlCount = 0

function Get-File($url, $out) {
    $dir = Split-Path $out -Parent
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    # 1) msys2 curl (OpenSSL)
    $msys = 'C:\msys64\usr\bin\curl.exe'
    if (Test-Path $msys) {
        & $msys -sSL --connect-timeout 20 --retry 2 -A $ua -o $out $url 2>$null
        if ($LASTEXITCODE -eq 0 -and (Test-Path $out) -and (Get-Item $out).Length -gt 0) { return $true }
    }
    # 2) system curl (schannel)
    & curl.exe -sSL --connect-timeout 20 --retry 2 -A $ua -o $out $url 2>$null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $out) -and (Get-Item $out).Length -gt 0) { return $true }
    # 3) Invoke-WebRequest
    try {
        Invoke-WebRequest -Uri $url -OutFile $out -UseBasicParsing -TimeoutSec 600
        return (Test-Path $out)
    } catch {
        Write-Host ('    IWR failed: ' + $_.Exception.Message)
        return $false
    }
}

function Get-GithubLatest($repo) {
    try {
        return Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $repo + '/releases/latest') -Headers @{ 'User-Agent' = $ua } -TimeoutSec 30
    } catch {
        Write-Host ('    GitHub API /releases/latest failed for ' + $repo + ': ' + $_.Exception.Message)
        return $null
    }
}

function Sync-Headers($srcDir, $dstRel) {
    $dst = Join-Path $root $dstRel
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    robocopy $srcDir $dst /E /PURGE /NFL /NDL /NJH /NJS | Out-Null
    return (Get-ChildItem $dst -Recurse -File | Measure-Object).Count
}

# ---------------------------------------------------------------- stb
Write-Host '==> stb'
$stbBase = 'https://raw.githubusercontent.com/nothings/stb/master/'
$stbFiles = @('stb_image.h','stb_image_write.h','stb_image_resize2.h','stb_truetype.h','stb_rect_pack.h','stb_vorbis.c')
foreach ($f in $stbFiles) {
    $out = Join-Path $root ('concord/src/3rd/stb/' + $f)
    if (Get-File ($stbBase + $f) $out) { $dlCount++; Write-Host ('    ok ' + $f) } else { Write-Host ('    FAIL ' + $f) }
}

# ---------------------------------------------------------------- Vulkan headers (tag zip)
Write-Host '==> Vulkan-Headers'
$vkTag = $null
try {
    $vkTags = Invoke-RestMethod -Uri 'https://api.github.com/repos/KhronosGroup/Vulkan-Headers/tags?per_page=1' -Headers @{ 'User-Agent' = $ua } -TimeoutSec 30
    if ($vkTags -and $vkTags.Count -gt 0) { $vkTag = $vkTags[0].name }
} catch { Write-Host ('    tags API failed: ' + $_.Exception.Message) }
$vkUrl = $null
if ($vkTag) { $vkUrl = 'https://codeload.github.com/KhronosGroup/Vulkan-Headers/zip/refs/tags/' + $vkTag }
else { $vkUrl = 'https://codeload.github.com/KhronosGroup/Vulkan-Headers/zip/refs/heads/main'; $vkTag = 'main' }
$zip = Join-Path $tmp 'vk.zip'
if (Get-File $vkUrl $zip) {
    $dest = Join-Path $tmp 'vk'
    Expand-Archive -Path $zip -DestinationPath $dest -Force
    $vkInc = Get-ChildItem $dest -Recurse -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'vulkan\vulkan.h') } | Select-Object -First 1
    if ($vkInc) {
        $n = Sync-Headers $vkInc.FullName 'concord/src/3rd/Vulkan'
        Write-Host ('    ok ' + $vkTag + ' -> src/3rd/Vulkan/ (' + $n + ' files)')
        $dlCount++
    } else { Write-Host '    FAIL include dir not found' }
} else { Write-Host '    FAIL download' }

# ---------------------------------------------------------------- SDL3 (MinGW dev package)
Write-Host '==> SDL3'
$sdl = Get-GithubLatest 'libsdl-org/SDL'
if ($sdl) {
    $asset = $sdl.assets | Where-Object { $_.name -match '^SDL3-devel-.*-mingw\.zip$' } | Select-Object -First 1
    if (-not $asset) { $asset = $sdl.assets | Where-Object { $_.name -match '^SDL3-devel-.*-VC\.zip$' } | Select-Object -First 1 }
    if ($asset) {
        $zip = Join-Path $tmp 'sdl.zip'
        if (Get-File $asset.browser_download_url $zip) {
            $dest = Join-Path $tmp 'sdl'
            Expand-Archive -Path $zip -DestinationPath $dest -Force
            # headers: the dir that CONTAINS SDL3/SDL.h is the include root
            $sdlInc = Get-ChildItem $dest -Recurse -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'SDL3\SDL.h') } | Select-Object -First 1
            if ($sdlInc) {
                $n = Sync-Headers $sdlInc.FullName 'concord/src/3rd/SDL3'
                Write-Host ('    headers ok (' + $asset.name + ', ' + $n + ' files)')
                $dlCount++
            } else { Write-Host '    FAIL include dir not found' }
            # import lib (prefer x86_64): MSVC SDL3.lib / MinGW libSDL3.dll.a
            $imp = Get-ChildItem $dest -Recurse -File | Where-Object { $_.FullName -match 'x86_64|x64' -and $_.Name -match 'SDL3.*\.(lib|dll\.a)$' } | Select-Object -First 1
            if (-not $imp) { $imp = Get-ChildItem $dest -Recurse -File | Where-Object { $_.Name -match 'SDL3.*\.(lib|dll\.a)$' } | Select-Object -First 1 }
            if ($imp) { Copy-Item $imp.FullName (Join-Path $root 'concord/lib/') -Force; Write-Host ('    lib: ' + $imp.Name) }
            # runtime dll (prefer x86_64)
            $dll = Get-ChildItem $dest -Recurse -File -Filter 'SDL3.dll' | Where-Object { $_.FullName -match 'x86_64|x64' } | Select-Object -First 1
            if (-not $dll) { $dll = Get-ChildItem $dest -Recurse -File -Filter 'SDL3.dll' | Select-Object -First 1 }
            if ($dll) { Copy-Item $dll.FullName (Join-Path $root 'concord/lib/') -Force; Write-Host ('    dll: ' + $dll.Name) }
        } else { Write-Host '    FAIL download' }
    } else { Write-Host '    FAIL no asset' }
} else { Write-Host '    FAIL api' }

# ---------------------------------------------------------------- Steam Audio (main package)
Write-Host '==> Steam Audio'
$sa = Get-GithubLatest 'ValveSoftware/steam-audio'
if ($sa) {
    $asset = $sa.assets | Where-Object { $_.name -match '^steamaudio_[0-9]' -and $_.name -notmatch 'wwise|unity|unreal|fmod' } | Sort-Object { $_.size } -Descending | Select-Object -First 1
    if ($asset) {
        $sizeMB = [math]::Round($asset.size / 1MB, 1)
        Write-Host ('    package: ' + $asset.name + ' (' + $sizeMB + ' MB)')
        $zip = Join-Path $tmp 'sa.zip'
        if (Get-File $asset.browser_download_url $zip) {
            $dest = Join-Path $tmp 'sa'
            Expand-Archive -Path $zip -DestinationPath $dest -Force
            # headers (all phonon_*.h from the release)
            $incDir = Get-ChildItem $dest -Recurse -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'phonon.h') } | Select-Object -First 1
            if ($incDir) { $n = Sync-Headers $incDir.FullName 'concord/src/3rd/SteamAudio'; Write-Host ('    headers ok (' + $n + ' files)'); $dlCount++ }
            # windows x64 release binaries: lib/windows-x64/ (phonon.lib + phonon.dll)
            $win = Get-ChildItem $dest -Recurse -Directory | Where-Object { $_.FullName -match 'windows-x64' -and $_.FullName -match 'lib' } | Select-Object -First 1
            if ($win) {
                Copy-Item -Path (Join-Path $win.FullName '*.lib') -Destination (Join-Path $root 'concord/lib/') -Force -ErrorAction SilentlyContinue
                Copy-Item -Path (Join-Path $win.FullName '*.dll') -Destination (Join-Path $root 'concord/lib/') -Force -ErrorAction SilentlyContinue
                Write-Host ('    libs ok from ' + $win.FullName.Replace($dest, '.'))
                $dlCount++
            } else { Write-Host '    WARN windows-x64 lib dir not found (install binaries manually)' }
        } else { Write-Host '    FAIL download' }
    } else { Write-Host '    no package asset' }
} else {
    Write-Host '    api unavailable, header-only fallback'
    $phonon = Join-Path $root 'concord/src/3rd/SteamAudio/phonon.h'
    if (Get-File 'https://raw.githubusercontent.com/ValveSoftware/steam-audio/main/include/phonon.h' $phonon) { Write-Host '    header ok'; $dlCount++ }
}

# ---------------------------------------------------------------- Jolt Physics (source, compiled by CMake)
Write-Host '==> Jolt Physics'
$joltMarker = Join-Path $root 'concord/src/3rd/Jolt/Build/CMakeLists.txt'
if (Test-Path $joltMarker) {
    Write-Host '    already vendored'
} else {
    $joltTag = 'v5.6.0'
    $joltUrl = 'https://codeload.github.com/jrouwe/JoltPhysics/zip/refs/tags/' + $joltTag
    $zip = Join-Path $tmp 'jolt.zip'
    if (Get-File $joltUrl $zip) {
        $dest = Join-Path $tmp 'jolt'
        Expand-Archive -Path $zip -DestinationPath $dest -Force
        $src = Get-ChildItem $dest -Directory | Select-Object -First 1
        if ($src) {
            $n = Sync-Headers $src.FullName 'concord/src/3rd/Jolt'
            Write-Host ('    ok ' + $joltTag + ' -> src/3rd/Jolt/ (' + $n + ' files)')
            $dlCount++
        } else { Write-Host '    FAIL extract dir not found' }
    } else { Write-Host '    FAIL download' }
}

# ---------------------------------------------------------------- summary
Write-Host ''
Write-Host ('Downloads completed: ' + $dlCount)
Write-Host '--- src/3rd ---'
if (Test-Path (Join-Path $root 'concord/src/3rd')) {
    Get-ChildItem (Join-Path $root 'concord/src/3rd') -Directory | ForEach-Object { Write-Host ('  ' + $_.Name + ' (files: ' + (Get-ChildItem $_.FullName -File -Recurse | Measure-Object).Count + ')') }
}
Write-Host '--- lib ---'
if (Test-Path (Join-Path $root 'concord/lib')) { Get-ChildItem (Join-Path $root 'concord/lib') -File | ForEach-Object { Write-Host ('  ' + $_.Name) } }