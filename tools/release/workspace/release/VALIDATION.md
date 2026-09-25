# 0.1.0-preview.1 本地验证

日期：2026-09-25。平台：Windows x64，Intel Core i7-14650HX，
NVIDIA GeForce RTX 5060 Laptop GPU。工具链：MSYS2 UCRT64 GCC 15.2.0、
LLVM 22.1.3、CMake + Ninja。

| 检查 | 结果 |
|---|---|
| 独立 Release 全量构建及 22 个 shader 编译 | 通过 |
| 引擎 CTest | 50 通过，1 跳过，0 失败 |
| RT scene / model / pipeline GPU 测试 | 均通过 |
| ConcordScript CLI 回归 | 通过 |
| ConcordScript 注解、生成代码、CVM JIT/AOT、宿主绑定回归套件 | 通过 |
| Debug Vulkan validation smoke | 跳过：本机没有 `VK_LAYER_KHRONOS_validation` |
| 演示光追 PNG 输出 | 960×540 输出成功并检查画面 |
| 发布包编译器 `--version`、`--check`、CVM 执行 | 仅 Windows 系统 PATH 下通过 |
| 预编译 SDK 的独立 CMake 消费者 | 配置、链接通过，仅 Windows 系统 PATH 下运行通过 |
| 发布包演示 | 仅 Windows 系统 PATH 下生成 640×360 PNG 成功 |
| UI 绘制批次回归 | 超过 64 次绘制、同色合并及顺序保持通过 |
| 独立 concord.exe | 仅依赖 Windows 系统 DLL |
| CLI 下载流程 | 本地模拟 GitHub 响应：下载、SHA256、路径穿越拒绝、离线缓存通过 |
| 无预装工具构建 | 清空开发 PATH 后，使用随包工具链创建并构建模板与 Neon Courier 通过 |

打包脚本重新构建并执行两套工程的 CTest，任何失败或未注册测试都会终止打包。
当次 CTest JUnit 记录在 `validation/engine.xml`、`validation/compiler.xml`。
跳过不视为完成验证：验证层覆盖仍然缺失；未验证 MSVC、其他 GPU、其他 Windows
版本。没有公开发布或签名；真实 GitHub 下载需要上传 Release 后再验证。

## 性能测量

`concord_snapshot_benchmark` 在同一个 Release 可执行文件内比较相同场景的
“每帧新建存储”和“跨帧复用存储”。4096 个 Box，7 轮、每轮 120 帧，交替测量
顺序并取中位数：

| 快照存储 | CPU 时间/帧 |
|---|---:|
| 每帧新建 | 924.83 μs |
| 跨帧复用 | 596.03 μs |

该微基准减少约 **35.6%**。它只测 CPU 场景提取，不包含 GPU、present 或整帧，
不代表游戏 FPS 增幅，也不是跨机器保证。

粒子路径另外移除了两份临时顶点数组及合并复制；新增回归确认 Additive/Scatter
共享 8192 粒子上限，并确认快照切换到空场景后不会残留对象、相机、骨骼或水体数据。

## 重跑验证层检查

安装提供 `VK_LAYER_KHRONOS_validation` 的 Vulkan SDK 后：

```powershell
cmake -S . -B validation-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCONCORD_BUILD_GPU_TESTS=ON -DCONCORD_BUILD_SHADERS=ON
cmake --build validation-build --target concord_vulkan_smoke_tests -j 4
ctest --test-dir validation-build -R '^concord_vulkan_smoke_tests$' --output-on-failure
```
