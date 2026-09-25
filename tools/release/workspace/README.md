# Concord Flash

宿主工程根目录。引擎本体、文档、许可证与完整说明都在 [`concord/`](concord/README.md) 下——
`concord/` 是一个完全自包含的项目，可以直接被其他工程 `add_subdirectory`。

- 引擎说明与语法示例：[concord/README.md](concord/README.md)
- 语法与架构文档：[concord/docs/](concord/docs/)
- 工程规范：[AGENTS.md](AGENTS.md)
- 安装第三方依赖：`./setup_deps.ps1`
- 打包可分发产物：`./package_bin.ps1`

## v0.1.0 预览版

可重建的首版发布流程（PowerShell 7、MSYS2 UCRT64 GCC 15.2、LLVM 22）：

```powershell
cmake --preset release-preview
cmake --build --preset release-preview
ctest --preset release-preview
cmake -S script -B script/release-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build script/release-build -j 4
ctest --test-dir script/release-build --output-on-failure
./package_release.ps1
```

产物位于 `bin/releases/`，包含 SDK、演示、ConcordScript/CVM、源码、文件清单和
SHA-256。使用方式见 [release/README.md](release/README.md)，验证记录见
[release/VALIDATION.md](release/VALIDATION.md)。现阶段仅本地准备，没有公开发布。

## 构建

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build
./build/main.exe
```
