# 发布 v0.1.0-preview.1

这是 Windows x64 预览版。默认 GitHub 仓库为 `datatype-tech/concord`。
本地构建、commit、tag 不会自动推送或发布 GitHub Release。

## 用户只需要 concord.exe

从 Release 下载独立的 `concord.exe`，放到任意目录，执行：

```powershell
./concord.exe init MyGame
./concord.exe build MyGame
./concord.exe run MyGame
```

首次使用自动从对应 GitHub tag 下载 SDK 和工具链，校验 SHA256 后缓存到
`%LOCALAPPDATA%/Concord/sdk`。包含 ConcordScript、引擎 DLL、预编译 shader、
GCC、CMake 和 Ninja，无需 Visual Studio、MSYS2、Git 或 Vulkan SDK。
系统要求：Windows 10/11 x64、内置 Windows PowerShell 5.1；运行游戏还需要 Vulkan 显卡驱动。
首次下载需要网络，成功缓存后可离线创建和构建。

```powershell
./concord.exe install --release v0.1.0-preview.1
./concord.exe doctor
./concord.exe build MyGame --sdk 'D:/SDK/ConcordFlash-0.1.0-preview.1-win64'
./concord.exe install --repo datatype-tech/concord --cache 'D:/ConcordCache'
```

CLI 固定默认预览 tag，不依赖 GitHub 的 latest（它不包含预览版）。升级时显式指定
`--release`。下载失败、校验失败、损坏缓存、并发安装占用会返回非零错误；不会运行损坏包。
`--sdk` 是可信本地目录的离线覆盖选项，不执行下载校验。

## 构建发布产物

开发者构建环境使用 PowerShell 7、MSYS2 UCRT64 GCC 15.2、LLVM、flex、bison、
CMake、Ninja，以及 `setup_deps.ps1` 安装的引擎依赖。用户端不需要这些安装。

当前工作区：

```powershell
./package_release.ps1 -Version 0.1.0-preview.1
```

从 tag 重建时，将仓库检出到工作区的 `concord/`；把
`concord/tools/release/workspace/` 内容复制到工作区根目录，
把 `concord/tools/concordScript/` 复制为工作区的 `script/`。
`concord/tools/release/PackageRelease.ps1 -Workspace <工作区>` 是同一打包脚本的仓库副本。
先运行根目录 `setup_deps.ps1`，配置 Release 的 `release-build` 与 `script/release-build`，
再打包。引擎测试使用 `-DCONCORD_BUILD_TESTS=ON`。

打包脚本运行引擎、编译器及 CLI 下载流程测试，递归解析 DLL 依赖，带入匹配工具链、
第三方许可、源码和 SHA256 清单。工具链包版本记录在 `tools/toolchain-packages.txt`。
发布 GCC/binutils 等第三方工具时，需同时提供其对应源码和构建补丁，按包内许可证履行
源码提供义务；不要把“游戏源码可闭源”误套到编译器工具本身。

## 在 GitHub 创建 Release

发布前检查提交内容和本地 tag：

```powershell
git -C concord show --stat v0.1.0-preview.1
git -C concord push origin HEAD
git -C concord push origin v0.1.0-preview.1
```

GitHub 仓库页面打开 Releases → Draft a new release：

1. 选择 `v0.1.0-preview.1`，标题 `Concord Flash 0.1.0 Preview 1`。
2. 勾选 **Set as a pre-release**。
3. 上传 `bin/releases/` 下的四个文件（保持名字完全一致）：
   `concord.exe`、`concord.exe.sha256`、
   `ConcordFlash-0.1.0-preview.1-win64.zip`、
   `ConcordFlash-0.1.0-preview.1-win64.zip.sha256`。
4. 说明填写 `CHANGELOG.md` 的内容，并附所需第三方对应源码。
5. Publish release。

也可以在安装并登录 GitHub CLI 后执行：

```powershell
gh release create v0.1.0-preview.1 --repo datatype-tech/concord --verify-tag --prerelease --title 'Concord Flash 0.1.0 Preview 1' --notes-file release/CHANGELOG.md bin/releases/concord.exe bin/releases/concord.exe.sha256 bin/releases/ConcordFlash-0.1.0-preview.1-win64.zip bin/releases/ConcordFlash-0.1.0-preview.1-win64.zip.sha256
```

发布后用一个新的 `--cache` 路径运行 `concord.exe init`、`build`，检查真实 GitHub
下载链路。发布前只能验证本地包及模拟下载；尚不存在的 Release 链接会返回 404。
已发布的 tag 和资产不要覆盖；修订使用新的 preview 编号，并同步 CLI 默认版本。
