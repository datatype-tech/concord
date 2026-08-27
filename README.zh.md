<p align="center">
  <img src="assets/Concord.png" alt="Concord Flash" width="192">
</p>

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh.md">简体中文</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-MPL--2.0-2B3137?logo=mozilla&logoColor=white" alt="MPL 2.0">
  <img src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white" alt="C++23">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Vulkan-0078D4" alt="Windows and Vulkan">
  <img src="https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-064F8C?logo=cmake&logoColor=white" alt="CMake and Ninja">
</p>

# Concord Flash

Concord Flash 是 [Concord](https://github.com/lattice-tech/concord) 引擎的第二代：
一款面向 Windows 的原生 Forward+ Vulkan 3D 引擎。它继承了第一代面向用户的
语法设计，但重写了渲染后端、深化了 ECS 模型，并主动砍掉了不必要的复杂度。

> **当前状态**：引擎正在打地基阶段。窗口、Vulkan 帧生命周期、ECS 核心、
> 双 DLL 架构均已跑通并验证；渲染管线（深度预通道、光源分块裁剪、材质）
> 尚未实现。API、文件格式和运行时行为都可能在没有兼容性保证的情况下变化。

## 为什么要有第二代

第一代 Concord 建立在 [bgfx](https://github.com/bkaradzic/bgfx) 之上，
用一个跨平台渲染抽象层换来了 Windows/macOS/Linux 的可移植性。但这个抽象
本身带来了三个第二代想解决的问题：

1. **间接层的代价**。bgfx 的跨后端设计意味着任何 Vulkan 独有的能力
   （dynamic rendering、显式的帧同步、精确的内存布局）都要先问"bgfx 支不
   支持"，而不是直接写。第二代直接面向 Vulkan 编程，用第一代永远用不上的
   跨平台能力换取对渲染管线的完全控制。
2. **模块拆分过细**。第一代把引擎拆成 30+ 个 `C*.h` 门面头、多个独立 DLL
   （`CEngine.dll`/`CAudio.dll`/`CGUI.dll`/`CSystem.dll`/`CTime.dll`），
   很多模块在项目早期就已经存在门面却没有实质内容。第二代只在一个模块真正
   有代码时才为它建门面头，DLL 也只按真正的耦合边界（运行时 vs 渲染后端）
   一分为二。
3. **场景与 ECS 是两套平行状态**。第一代的 `Scene::Spawn` 生成的是持有
   自身数据的节点对象，`Ecs::World` 是完全独立的组件数据库——两者互不
   知晓对方的存在。想用 ECS 就没法用节点 API 生成的对象，反之亦然。

## 第二代解决了什么、进步在哪

### 场景本身就是 ECS World

这是最核心的改进。`Scene::Spawn<T>` 和 `Scene::Query<...>` 现在共享
同一份组件存储——面向对象的生成语法只是数据导向查询的一层薄外壳：

```cpp
// 面向对象视图：生成一个原型，再挂一个自定义组件。
scene.Spawn<Object::Box>({.material = {.albedo = COLOR_RGB(224, 64, 64)}})
     .Add<Spin>({.degreesPerSecond = 60.0f});

// 数据导向视图：逐组件拼装，完全不经过任何原型。
scene.CreateEntity()
     .Add<Transform>({.position = {4.0f, 1.5f, 1.0f}})
     .Add<MeshRenderer>({});

// 一次查询同时看到上面两个实体——因为它们本来就是同一份数据。
scene.Query<Transform, MeshRenderer>([](Entity, Transform& t, MeshRenderer& m) { ... });
```

详见 [docs/场景与ECS.md](docs/场景与ECS.md)。

### 语法更简洁，删掉的都是纯粹的样板

| 第一代 | Concord Flash |
|---|---|
| `scene.Spawn<Object::Box>(Object::BoxDesc{...})` 重复写类型名 | `scene.Spawn<Object::Box>({...})`，`T::Desc` 自动推导 |
| `.material = {.surface = {.albedo = ...}}` 嵌套两层 | `.material = {.albedo = ...}`，压平一层 |
| 30+ 个门面头，部分对应空模块 | 只为真正存在的模块建门面头，按需增加 |
| `Concord::Sleep(20000)` 让窗口保活 | `game.Run()`，正规的主循环 |
| 每个图元单独一套持有状态的 Desc + Node | Node 变成零状态的"配方"，状态全部在 ECS 里 |

### 渲染后端原生 Vulkan，双 DLL 而非多 DLL

引擎拆成两个 DLL：`ConcordFlashGameEngineRuntime.dll`（生命周期、ECS、
场景、窗口）和 `ConcordFlashGameEngineRender.dll`（Vulkan 后端）。
Runtime 通过一个自注册工厂获得渲染后端实例，从不直接引用 Vulkan 相关
符号——这让两个 DLL 之间的依赖保持单向，也让渲染后端未来可以独立替换或
升级，而不必重新编译整个运行时。详见 [docs/渲染架构.md](docs/渲染架构.md)。

## 语法示例

```cpp
#include <Concord/CApplication.h>
#include <Concord/CCamera.h>
#include <Concord/CLight.h>
#include <Concord/CObject.h>
#include <Concord/CRender.h>
#include <Concord/CScene.h>

int main()
{
    Concord::LinkVulkanRenderBackend();

    Concord::Game game;
    Concord::Window window({.title = "我的游戏", .resolution = {1280, 720}});
    game.AttachWindow(window);

    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({.position = {0.0f, 2.0f, -5.0f}});
    scene.Spawn<Concord::Object::SunLight>({.elevationDegrees = 45.0f});
    scene.Spawn<Concord::Object::Box>({.transform = {.position = {0.0f, 1.0f, 0.0f}}});

    game.LoadScene(scene);
    game.Run();
}
```

## 文档

完整的语法设计和架构说明见 [docs/](docs/)：

- [快速开始](docs/快速开始.md)
- [应用与窗口](docs/应用与窗口.md)
- [场景与 ECS](docs/场景与ECS.md)
- [组件与原型](docs/组件与原型.md)
- [系统与调度](docs/系统与调度.md)
- [渲染架构](docs/渲染架构.md)
- [构建与依赖](docs/构建与依赖.md)

工程规范（命名、文件布局、单文件行数上限等）见仓库根目录的 `AGENTS.md`。

## 许可证

Concord Flash 采用 [Mozilla Public License 2.0](LICENSE)：修改引擎自身的
源文件需要公开那些文件的修改，但用引擎制作的游戏完全不受影响，可以
自由闭源分发。第三方依赖各自遵循其原始许可证，见 `src/3rd/` 各子目录。

## 关于本项目

Concord Flash 由 **Datatype Team**（datatype.me）开发——这是一支由 Concord
原创始人 Simalth Wang 创立的综合开发团队。此前的维护方 Lattice Games
不再更新第一代引擎；此后的全部开发都在这里、由 Datatype Team 继续进行。

<br>

<p align="center">
  <img src="assets/DatatypeTeamLogo.png" alt="Datatype Team" width="64">
  <br>
  <sub>Developed by Datatype Team (datatype.me)</sub>
</p>
