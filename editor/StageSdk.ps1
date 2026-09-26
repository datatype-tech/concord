#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$BaseSdk,
    [Parameter(Mandatory)][string]$EngineBuild,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = [IO.Path]::GetFullPath($EngineBuild)
$baseRoot = [IO.Path]::GetFullPath($BaseSdk)
$sdkRoot = [IO.Path]::GetFullPath($Destination)
if ($sdkRoot -eq $baseRoot -or $sdkRoot -eq $buildRoot -or $sdkRoot -eq $sourceRoot) {
    throw 'Choose a separate destination for the development SDK.'
}
foreach ($required in @('bin/concordc.exe','tools/cmake/bin/cmake.exe','tools/mingw/bin/g++.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $baseRoot $required))) { throw "Base SDK is missing $required" }
}
if (-not (Test-Path -LiteralPath (Join-Path $baseRoot 'licenses') -PathType Container)) {
    throw 'Base SDK is missing its third-party license directory.'
}
foreach ($shader in @('ui_toolkit.vert.spv','ui_toolkit.frag.spv')) {
    if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "shaders/$shader") -PathType Leaf)) {
        throw "Engine build is missing the native UI shader: $shader"
    }
}
New-Item -ItemType Directory -Force -Path $sdkRoot | Out-Null
foreach ($folder in @('bin','lib','tools')) {
    if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot $folder))) {
        Copy-Item -LiteralPath (Join-Path $baseRoot $folder) -Destination $sdkRoot -Recurse
    }
}
Copy-Item -LiteralPath (Join-Path $baseRoot 'licenses') -Destination $sdkRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'include') -Destination $sdkRoot -Recurse -Force
foreach ($name in @('imgui.h','imgui_internal.h','imconfig.h','imstb_rectpack.h','imstb_textedit.h','imstb_truetype.h')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "src/3rd/imgui/$name") -Destination (Join-Path $sdkRoot 'include') -Force
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'src/3rd/ImGuizmo/ImGuizmo.h') -Destination (Join-Path $sdkRoot 'include') -Force
foreach ($folder in @('lib/cmake/ConcordFlash','bin/Assets/Shaders')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $sdkRoot $folder) | Out-Null
}
foreach ($name in @('ConcordFlashGameEngineRuntime','ConcordFlashGameEngineRender')) {
    Copy-Item -LiteralPath (Join-Path $buildRoot "$name.dll") -Destination (Join-Path $sdkRoot 'bin') -Force
    Copy-Item -LiteralPath (Join-Path $buildRoot "concord/lib$name.dll.a") -Destination (Join-Path $sdkRoot "lib/$name.dll.a") -Force
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'cmake/ConcordFlashConfig.cmake') -Destination (Join-Path $sdkRoot 'lib/cmake/ConcordFlash') -Force
Get-ChildItem -LiteralPath (Join-Path $buildRoot 'shaders') -Filter '*.spv' | Copy-Item -Destination (Join-Path $sdkRoot 'bin/Assets/Shaders') -Force
New-Item -ItemType Directory -Force -Path (Join-Path $sdkRoot 'licenses') | Out-Null
foreach ($pair in @(@('imgui','LICENSE.txt'),@('hello_imgui','LICENSE'),@('ImGuizmo','LICENSE'),
                    @('ImGuiColorTextEdit','LICENSE'),@('nanosvg','LICENSE.txt'))) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "src/3rd/$($pair[0])/$($pair[1])") -Destination (Join-Path $sdkRoot "licenses/$($pair[0]).txt") -Force
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'src/3rd/UI-VERSIONS.txt') -Destination (Join-Path $sdkRoot 'licenses/UI-VERSIONS.txt') -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'src/3rd/hello_imgui/README.concord.md') -Destination (Join-Path $sdkRoot 'licenses/hello_imgui-NOTICE.md') -Force
Set-Content -LiteralPath (Join-Path $sdkRoot 'DEVELOPMENT.txt') -Value 'Local editor development SDK. Not the tagged v0.1.0-preview.1 release.'
Write-Host "Development SDK staged: $sdkRoot"
