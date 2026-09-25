# 0.1.0-preview.1

- Windows x64 C++23 引擎：原生 Vulkan、Forward+ 与可选 KHR 光追，双 DLL。
- 同一 ECS 存储上的对象配方/查询 API；模型、骨骼动画、物理、空间音频、粒子、水体、昼夜和基础 UI。
- CPU 渲染快照跨帧保留容量；粒子直接分组写入最终数组，并对混合模式统一限流。
- ConcordScript 支持参数等号写法、C++ `--check`、明确的参数/I/O 诊断、UTF-8 BOM、未闭合文本诊断和输出目录排除。
- 未变化的生成文件不重写；两个 CLI 共用一次编译的静态编译器核心。
- 修复 shader 编译器自动探测、GPU 测试源依赖和模型 GPU 测试队列初始化。
- 提供 Release presets、可重建的本地打包脚本、预编译 CMake SDK、源码和 SHA-256 清单。
- 独立 `concord.exe` 支持 init/build/run/install/doctor；自动下载 GitHub Release、校验 SHA256、缓存 SDK 与 GCC/CMake/Ninja，用户无需预装开发工具。
- UI 按实际顶点缓冲容量绘制，移除 64 次绘制截断，合并相邻同色图元；修复像素标题及后续文字、按钮消失。
- 提供全部使用 `.cx` 编写的 Neon Courier 示例游戏。
