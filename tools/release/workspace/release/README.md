# Concord Flash 0.1.0-preview.1

首个 Windows x64 本地预览版。API、CVM ABI 与产物格式尚不保证稳定。
预编译 SDK 使用 MSYS2 UCRT64 GCC 15.2、C++23、Release；不要与 MSVC 或不同
C++ 标准库 ABI 混用。运行演示需要支持 Vulkan 的驱动；光追取决于 GPU 能力。

## 只下载 CLI

下载 Release 的 `concord.exe`，执行 `concord init MyGame`、`concord build MyGame`、
`concord run MyGame`。CLI 自动下载并校验 SDK，内含 GCC、CMake、Ninja；
用户无需安装开发工具。首次下载后缓存可离线使用。发布步骤见 [PUBLISH.md](PUBLISH.md)。

## 运行与工具

在解压目录打开 PowerShell：

```powershell
.\bin\concordc.exe --version
.\bin\concordc.exe --project .\examples\script --check
.\bin\concordc.exe --project .\examples\script --out .\generated
.\bin\cc.exe --project .\examples\cvm --out .\cvm-output
.\bin\main.exe
```

演示支持 WASD、点击捕获鼠标、Esc 释放、Shift 加速、V 切换飞行/步行、
F3 显示统计、N/M 切换夜间/白天。可离线生成画面：

```powershell
.\bin\main.exe --still preview.png --width 1280 --height 720 --warmup 3
```

`concordc` 默认生成 C++；`cc` 默认通过 LLVM JIT 执行 CVM。`cc --cvm aot`
生成 `.ll/.bc/.obj`，还需要宿主链接，不能直接当成完整游戏 EXE。
CLI 参数支持 `--key=value`，路径含空格时仍须引号。

`--check` 仅验证 C++ 转译结构与注解，不写文件，不执行 C++ 类型检查或 shader
编译；CVM 使用 `--no-run` 编译而不运行。源码须为 UTF-8，可含 BOM。
扫描会排除指定输出目录、`.git`、`.svn`、`node_modules` 和目录符号链接。
没有变化的生成文件保留时间戳；I/O 失败返回非零退出码，可能留有部分输出。

## 使用预编译 SDK

需要 CMake 3.25+、Ninja、兼容的 MSYS2 UCRT64 GCC 15.2：

```powershell
cmake -S examples/cpp -B consumer-build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$PWD"
cmake --build consumer-build
.\consumer-build\preview.exe
```

`find_package(ConcordFlash CONFIG REQUIRED)` 提供 `concord::runtime`、
`concord::render`、`concord::concord` 和 `concord_stage_runtime()`。
该预览 SDK 面向普通公共 API；自定义 Vulkan SDK/shader 编译集成请使用源码构建。

## 内容与源码

- `bin/`：演示、两个引擎 DLL、编译器、运行库、SPIR-V。
- `include/`、`lib/`：SDK 头文件、导入库、CMake 配置和 CVM C 运行库。
- `examples/`：SDK、转译器和 CVM 最小示例。
- `source/`：本版引擎及 ConcordScript 源码、构建脚本与测试。
- `licenses/`：MPL-2.0、MIT 及第三方通知；见 `THIRD_PARTY.md`。
- `manifest.json`：文件 SHA-256、版本、构建信息；压缩包旁另有 `.sha256`。

源码中的 `setup_deps.ps1` 安装引擎第三方依赖；脚本编译器另需 Flex、Bison、
LLVM 22 开发包。预览包没有代码签名、安装器和自动更新。本次验证范围与缺口见
`VALIDATION.md`。第三方源代码不全部内嵌，获取方式见 `THIRD_PARTY.md`。
