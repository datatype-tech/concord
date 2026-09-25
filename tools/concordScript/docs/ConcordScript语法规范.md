# ConcordScript（CX）语法规范

状态：设计基线

本文是 ConcordScript 的语言规范与演进约束。标记为“当前”的内容对应现有转译器；标记为“规划”的内容是后续实现目标。规划语法在编译器正式支持前不得用于生产项目。

## 1. 语言定位

ConcordScript 是面向 Concord Flash 的 C++23 转译语言。它的目标是减少游戏代码中的样板代码，同时保留 C++ 的性能、ABI、Vulkan 接入能力和现有库生态。

编译流程为：

```text
.cx -> 词法分析 -> CX AST -> 名称/类型/语义检查 -> C++ 代码生成 -> C++ 编译器
```

普通 C++ 表达式、函数体、模板和 Vulkan 代码默认保持原样。随着类型系统逐步完善，编译器会优先检查 CX 自己引入的结构，再检查可以可靠推断的 C++ 语义。

## 2. 文件与模块

### 2.1 文件扩展名

源文件扩展名为 `.cx`。一个文件对应一个模块，文件名去掉扩展名后必须是合法的 C++ 标识符，并且不能是 C++ 关键字。

```text
Player.cx -> namespace Player
```

规划中允许使用目录形成模块路径：

```text
game/player/Player.cx -> game.player.Player
```

生成文件保持目录结构：

```text
game/player/Player.gen.h
game/player/Player.gen.cpp
```

### 2.2 编码

源文件使用 UTF-8。标识符第一阶段保持 ASCII，以便与 C++、CMake、Windows 工具链保持一致。字符串字面量可以包含 UTF-8 文本。

## 3. 导入语法

### 3.1 引擎公共头

当前使用点号路径：

```cx
use Concord.CApplication;
use Concord.CScene;
use Concord.CObject;
```

转译为：

```cpp
#include <Concord/CApplication.h>
#include <Concord/CScene.h>
#include <Concord/CObject.h>
```

CX 代码只应通过 `<Concord/C*.h>` 使用引擎公共 API，不应直接引用 `include/engine` 私有头。

### 3.2 项目模块

当前支持点号路径：

```cx
use Game.Player;
```

转译为：

```cpp
#include "Player.gen.h"
```

规划中会根据模块路径生成完整的相对路径，并在编译期验证目标模块是否存在、是否发生循环依赖。

### 3.3 C++ 标准库包

当前语法：

```cx
use std.vector;
use std.string;
use std.unordered_map;
use std.optional;
use std.memory;
```

`std.xxx` 不是用户项目模块，而是由编译器内置的标准库包表映射到标准头：

| CX 包 | C++ 头 |
|---|---|
| `std.array` | `<array>` |
| `std.algorithm` | `<algorithm>` |
| `std.cmath` | `<cmath>` |
| `std.filesystem` | `<filesystem>` |
| `std.functional` | `<functional>` |
| `std.memory` | `<memory>` |
| `std.optional` | `<optional>` |
| `std.span` | `<span>` |
| `std.string` | `<string>` |
| `std.string_view` | `<string_view>` |
| `std.unordered_map` | `<unordered_map>` |
| `std.utility` | `<utility>` |
| `std.vector` | `<vector>` |

标准包只负责导入头文件，不重命名 `std` 命名空间中的类型。未知的 `std.xxx` 必须报错，并给出相近包名建议。以后增加映射时应同步更新包表和语言测试。

### 3.4 导入别名与选择性导入

规划语法：

```cx
use std.vector as Vec;
use std.{vector, string};
```

第一种生成标准头并生成别名；第二种生成多个标准头。`use std.*;` 默认不支持，以避免不可控的隐式依赖。

## 4. 生命周期注解

生命周期注解使用顶层 body，并由包含 `@entry` 的生成模块降低到现有 `Concord::Game` API：

```cx
@entry {
    Concord::Game game;
    Concord::Window window({.title = "CX Game"});
    game.AttachWindow(window);
    Concord::Scene scene;
    game.LoadScene(scene);

    @startup {
        Concord::Script::RegisterAll(scene);
    };

    @update {
        scene.FlushDeferred();
        (void)deltaTime;
    };

    game.Run();
};
```

`@startup` 的 body 在 `game.Run()` 前执行。`@update` 生成 `game.OnUpdate` 回调，每帧执行一次，并提供秒数类型的 `Concord::f32 deltaTime`。生命周期代码必须依赖 entry 中显式创建的对象，不会自动创建隐藏的 Game 或 Scene。

更常用的独立顶层形式也受支持：

```cx
@startup { LoadAssets(); };
@update { Tick(deltaTime); };
```

它们会在同一模块的 `@entry` 中按源文件顺序注入。没有 `@entry` 时，生命周期声明只作为自定义注解元数据保留，不产生可执行代码。

## 5. 自定义注解

任意未占用的 `@name`、`@name(args)` 或 `@name { body }` 都会被解析并记录到生成的 annotation manifest 和 metadata header/source。编译器不会猜测自定义注解的运行时语义，也不会擅自生成代码。

```cx
@replicated(channel="gameplay")
class Player {
};

@editor_property(category="Combat", range="0..100")
class Weapon {
};
```

自定义注解的参数支持位置参数、`key=value` 参数和 opaque body。后续可通过外部 schema 或编译器插件声明参数类型、允许的附着位置和 lowering 行为。未注册的自定义注解不会导致错误，但参数结构错误仍会被报告。

## 6. 顶层结构

```ebnf
file          = { use | declaration | annotation | raw_cpp } ;
use           = "use" import_path [ "as" identifier ] ";" ;
declaration   = class | struct | enum | component | system | function ;
class         = { annotation } "class" identifier [ extends ] [ implements ] body ";" ;
entry         = { annotation } "@entry" body ";" ;
annotation   = "@" identifier [ arguments ] [ body ] [ ";" ] ;
body          = "{" balanced_text "}" ;
```

当前版本只正式建模 `use`、`class`、`extends`、`implements`、`@entry`、`@register` 和通用注解；其它顶层 C++ 内容作为原始文本透传。

## 5. 类与访问控制

### 5.1 类

```cx
use Concord/CScene;

class Player {
    pub var health = 100;

    pub void TakeDamage(int amount) {
        health -= amount;
    }
};
```

生成的类默认带 `public:`。访问修饰符如下：

```cx
pub  int health;
priv int maxHealth;
prot void Reset();
```

它们分别生成 `public:`, `private:`, `protected:`。`var` 当前转译为 `auto`，只在代码区域替换，不修改字符串、字符字面量、注释和 C++ raw string。

### 5.2 继承与接口

```cx
class Player extends Entity implements IDamageable, IUpdatable {
};
```

生成：

```cpp
class Player : public Entity, public IDamageable, public IUpdatable {
};
```

规划中会验证：基类名称可解析、接口没有重复、`implements` 目标确实是接口类型。

### 5.3 `@register`

```cx
@register
class Enemy {
    pub var health = 50;
};
```

当前版本将类加入生成的 `Concord::Script::RegisterAll(Concord::Scene&)`。注册类型必须满足引擎注册器要求。规划中 `@register` 将区分组件注册、原型注册和资源注册，避免把普通 C++ 类误当成 ECS 组件。

## 9. ECS 游戏语法

### 6.1 直接使用引擎 API

```cx
use Concord/CApplication;
use Concord/CObject;
use Concord/CScene;

@entry {
    Concord::Game game;
    Concord::Window window({.title = "CX Game", .resolution = {.width = 1280, .height = 720}});
    game.AttachWindow(window);

    Concord::Scene scene;
    scene.Spawn<Concord::Object::Box>({
        .transform = {.position = {0.0f, 1.0f, 0.0f}},
        .material = {.albedo = COLOR_RGB(224, 64, 64)}
    });

    game.LoadScene(scene);
    game.Run();
};
```

### 6.2 规划中的组件声明

```cx
component Health {
    int current = 100;
    int maximum = 100;
};
```

生成一个满足 ECS 约束的 plain aggregate，不生成虚函数、继承关系或隐式行为。

### 6.3 规划中的系统声明

```cx
system DamageSystem {
    query (Health health, Damage damage) {
        health.current -= damage.amount;
    }
};
```

系统声明生成 `Concord::ISystem` 实现。查询的第一个组件必须是最稀有组件；编译器可以在静态信息足够时发出性能警告。

### 6.4 规划中的实体与原型糖

```cx
entity player = spawn Concord::Object::Box {
    transform.position = {0.0f, 1.0f, 0.0f};
    material.albedo = COLOR_RGB(64, 160, 224);
};

player.add Health { .current = 100 };
```

该语法只允许降低 `Scene::Spawn`、`CreateEntity` 和 `EntityHandle::Add` 的书写成本，最终必须生成等价的现有 API 调用。它不能创建第二份场景数据，也不能绕过世代句柄和延迟结构变更规则。

## 7. 游戏专用语法糖

以下功能按优先级规划，不代表当前可用。

### 7.1 属性

```cx
property float Speed = 4.0f;
```

生成显式 getter/setter 或公开字段，具体策略由配置决定。属性不允许隐式分配堆内存。

### 7.2 枚举与匹配

```cx
enum class WeaponType { Laser, Missile, Plasma };

match weapon {
    WeaponType::Laser   => FireLaser();
    WeaponType::Missile => FireMissile();
    _                   => FireDefault();
};
```

`match` 必须检查分支覆盖；生成嵌套 `if` 或 `switch`，并保留 C++ 类型语义。

### 7.3 生命周期

```cx
@startup
void LoadAssets() { }

@shutdown
void ReleaseAssets() { }
```

生命周期注解只能应用于明确允许的宿主位置，由生成器统一排序。禁止通过注解绕过 `Game`、Render backend 或 ECS 的生命周期约束。

### 7.4 资源引用

```cx
asset<Texture> playerTexture = asset("textures/player.png");
```

资源语法第一阶段只生成类型安全的资源描述，不在转译器中加载文件。路径检查、打包和运行时加载属于资产管线。

## 8. 注解

注解是编译器和构建工具的扩展点：

```cx
@tag;
@tag(name="value", count=2);
@tag { arbitrary text };
```

当前内建能力包括：

- `@entry`：生成项目唯一 `main()`。
- `@register`：加入项目注册表。
- `@shader` 及阶段别名：生成 shader sidecar 和元数据。
- `@pass`：注册 Vulkan pass。
- `@cpp`、`@raw`、`@code`：注入 C++。
- `@include`、`@define`、`@pragma`：生成预处理指令。
- `@vulkan`：在应用侧源文件中注入低级 Vulkan 代码。

未知注解保留到 JSON 和 `ConcordScriptMetadata.gen.h`，但不自动改变 C++ 行为。

规划中注解会带有声明位置、适用阶段和参数 schema，使错误从“未知文本”升级为可定位的契约错误。

## 9. 错误与诊断

诊断输出统一格式：

```text
path/to/Player.cx:12:8: error CX102: unknown type 'Helth'; did you mean 'Health'?
```

字段为：文件、行、列、级别、稳定错误码、消息。

终端输出（`cc` / `concordc` 已实现）：

- 检测到 TTY 时默认启用 ANSI 颜色（`--color auto`）。
- `error` 使用加粗红色，`warning` 使用加粗黄色，`note` 使用加粗青色；`--help` 的节标题为黄色、选项名为绿色。
- 非交互终端、CI 或 `--color never` / `--no-color` 时输出纯文本。
- `--color always` 强制颜色。`NO_COLOR` 在 auto 模式下关闭颜色。
- 颜色只用于显示层，不能出现在 JSON、SARIF、生成文件或缓存键中。

稳定错误码 `CXnnn` 与 JSON/SARIF 诊断仍为规划；当前文本格式是 `error: …` / `path:line: …`。

诊断级别：

```text
error   阻止生成
warning 不阻止生成，但可由 --warnings-as-errors 提升
note    提供上下文或修复建议
```

## 10. 兼容性原则

1. 所有新语法必须能确定地生成普通 C++23。
2. 不能改变现有 `use Concord/C*.h` 的含义。
3. 不能让公开引擎头传递 Vulkan 或 SDL 头。
4. 不能让语法糖引入隐藏线程、隐藏堆分配或第二份 ECS 状态。
5. 生成为 C++ 的代码必须稳定、可重复，并能通过 `--emit` 检查。
6. 任何不能可靠解析的 C++ 片段继续保留为 opaque text，不进行危险重写。
7. 语言版本通过 `cx.toml` 的 `language_version` 固定，禁止同一项目依赖未声明的编译器新语法。

## 11. 推荐项目结构

```text
MyGame/
  cx.toml
  src/
    Main.cx
    Player.cx
    Systems.cx
  shaders/
  generated/
  build/
```

`generated/` 和 `build/` 是构建产物，不应手工编辑或提交到版本库。
