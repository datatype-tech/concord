# ConcordScript 编译器高级功能规划

状态：设计规划

## 1. 目标

将 `concordc` 从“遍历全部 `.cx` 并透传大部分 C++ 的转译器”演进为可用于中型游戏项目的增量编译器，同时保持 C++23 互操作和 Vulkan 扩展能力。

目标能力：

- 可读的彩色终端诊断和机器可读诊断。
- 词法、语法、名称、导入、注解和可推断类型的编译期检查。
- 更自洽的 AST、语义模型和代码生成管线。
- 面向 ECS、系统、资源和场景的游戏语法糖。
- `std.xxx` 标准库包导入。
- 只重新生成发生变化或受依赖影响的模块。
- 内容寻址缓存、并行编译、稳定生成物和可复现构建。
- `check`、`watch`、`clean`、`format`、`explain` 等 CLI 能力。

非目标：

- 不在 CX 编译器中实现 C++ 编译器。
- 不复制 Clang 的全部 C++ 语义。
- 不在转译器中加载游戏资源或操作 Vulkan device。
- 不通过隐藏运行时机制取代 Concord ECS 的单线程和生命周期约束。

## 2. 当前基线

当前 `script/src/main.cpp` 的主流程是：

```text
解析 --project/--out
递归查找全部 .cx
逐文件 yyparse
TranslateFile
检查 entry、shader、sidecar 冲突
一次性写出所有 gen 文件和 manifest
```

当前 `Ast.h` 只建模 `use`、类、entry、注解和 raw code；类体、函数体、表达式和大部分 C++ 仍是 opaque text。`KeywordSubstitution` 通过受保护的文本扫描替换 `var/pub/priv/prot`，避免修改字符串和注释。

这个边界是合理的兼容基础，但也意味着：

- 当前不能可靠报告 C++ 类型错误，只能依赖后续 C++ 编译器。
- `use` 目标没有完整的项目依赖图和循环依赖诊断。
- 任何一个输入变化都会重写全项目生成物。
- CLI 参数不足，错误格式不适合 IDE 和 CI 消费。
- 生成代码、manifest、shader sidecar、registry 之间缺少统一的中间构建模型。

## 3. 总体架构

引入明确的编译管线：

```text
SourceManager
  -> Lexer
  -> Parser
  -> Syntax AST
  -> Semantic Index
  -> Dependency Graph
  -> Typed/Lowered IR
  -> CodeGen + Sidecars
  -> Atomic Output Writer
  -> Diagnostics/Cache Database
```

### 3.1 SourceManager

职责：

- 统一读取 UTF-8 源文件。
- 保存行偏移表，支持行列定位和 source excerpt。
- 通过内容 hash 判断文件是否改变。
- 维护文件 ID，诊断内部不直接复制路径字符串。
- 处理 `cx.toml`、标准包表和 include 根目录。

### 3.2 Syntax AST

Parser 只负责结构正确性，不做类型推导。每个节点保存：

```text
NodeId
SourceSpan { fileId, beginOffset, endOffset }
leading/trailing trivia
```

保留 raw text 的节点必须同时保存 source span，生成错误时才能指出原始位置。注解参数从当前的字符串拆分升级为带 span 的值节点，但仍允许未知注解携带 opaque body。

### 3.3 Semantic Index

索引每个项目中的：

- 模块、公开声明和私有声明。
- 类型、函数、字段、系统、组件、资源。
- `use` 导入和别名。
- 注解及其 schema。

索引阶段先处理所有声明，再解析引用，因此文件顺序不影响语义结果。符号查找遵循：局部作用域、当前模块、显式导入、标准包、C++ 命名空间。

### 3.4 Lowered IR

第一版 IR 只覆盖 CX 自己拥有的语义：

- 模块和 import。
- component、system、entity、spawn。
- match、属性和生命周期声明。
- 注解产生的生成请求。

Opaque C++ 保持 `RawCppNode`，不尝试修改其中的模板、宏或 Vulkan 代码。所有新语法都先降低到现有 Concord C++ API，再进入文本代码生成。

### 3.5 Output Writer

输出使用临时文件加原子替换：

1. 生成到同目录临时文件。
2. 内容 hash 与现有文件相同则不替换。
3. 先完成全部诊断和冲突验证，再提交输出。
4. 失败时不得留下半套 registry、manifest 或 sidecar。

## 4. 编译期检查

### 4.1 诊断级别

```text
error   阻止生成
warning 默认继续，可用 --warnings-as-errors 提升
note    关联位置或修复建议
hint    可选的风格和性能建议
```

统一格式：

```text
Main.cx:14:9: error CX201: unknown symbol 'Scnee'
  scene.Spawn<Object::Box>({});
  ^~~~~
Main.cx:14:9: note: did you mean 'Scene'?
```

稳定错误码按区域分配：

| 范围 | 内容 |
|---|---|
| CX0xx | CLI、配置、IO、缓存 |
| CX1xx | 词法和语法 |
| CX2xx | 名称、导入和类型 |
| CX3xx | ECS、资源和游戏语义 |
| CX4xx | 注解、shader、生成冲突 |
| CX5xx | C++ lowering 和工具链 |

### 4.2 必须检查的内容

第一阶段：

- 未闭合字符串、注解、body、括号和注释。
- 顶层结构顺序和重复 `@entry`。
- 非法模块名、重复模块、大小写冲突。
- `use` 路径是否存在、是否为允许的公共头或标准包。
- 项目模块循环依赖。
- `@register`、`@shader`、`@pass` 参数 schema。
- sidecar、shader、registry 保留路径冲突。

第二阶段：

- 重复类、字段、函数和组件成员。
- 继承基类和接口是否可解析。
- component 是否是 plain aggregate。
- system query 的组件是否存在、是否重复。
- `spawn` 的描述字段是否存在且类型可转换。
- `match` 分支是否覆盖枚举值。

第三阶段：

- 受支持 CX 表达式的类型推导。
- nullable/resource handle 的使用检查。
- ECS query 回调中的结构性变更检查。
- 生命周期和线程模型约束。
- `std.xxx` 类型与 C++ 标准库头的一致性检查。

无法可靠判断的普通 C++ 代码继续交给 C++ 编译器，不生成误导性的 CX 错误。

## 5. 彩色与机器诊断

显示层独立于诊断模型。诊断内部只含路径、span、级别、code、message、notes 和 fix-it。

CLI 选项：

```text
--color auto|always|never
--diagnostics text|json|sarif
--no-diagnostics-color
--warnings-as-errors
--max-errors <n>
```

规则：

- `auto` 只在交互终端启用 ANSI 色彩。
- CI、重定向和 JSON/SARIF 输出默认无 ANSI 控制序列。
- Windows 使用 ANSI virtual terminal；不可用时回退纯文本。
- 加粗只属于终端 presentation，不写入日志、JSON、缓存或生成代码。
- JSON/SARIF 必须包含文件、零基准或一基准列号约定，项目内固定为一基准行列。

## 6. CLI 设计

保留兼容调用：

```text
concordc --project <dir> --out <dir>
```

推荐子命令：

```text
concordc build [options]
concordc check [options]
concordc emit [options]
concordc graph [options]
concordc watch [options]
concordc format [options]
concordc clean [options]
concordc explain <code>
concordc version
```

通用选项：

```text
--project <dir>       项目根目录
--source <dir>        源码目录，默认 project/src
--out <dir>           生成目录
--cache <dir>         缓存目录，默认 project/.cx-cache
--config <file>       配置文件，默认 project/cx.toml
--jobs <n>             并行任务数
--color <mode>        颜色模式
--diagnostics <mode>  text/json/sarif
--verbose             输出阶段和缓存信息
--trace               输出依赖和失效原因
```

命令语义：

- `build`：检查、增量生成全部需要的 C++ 和 sidecar。
- `check`：只解析和语义检查，不写生成文件。
- `emit`：生成 C++，可选 `--stdout <module>` 查看单模块结果。
- `graph`：输出 `text`、`json` 或 `dot` 依赖图。
- `watch`：监听 `.cx`、配置、注解 schema 和 shader 输入，只重建受影响节点。
- `format`：只格式化 CX 自有语法；opaque C++ 默认原样保留。
- `clean`：删除指定输出和缓存，不触碰源码。
- `explain`：解释错误码、缓存失效原因或 lowering 规则。

配置文件示例：

```toml
language_version = "0.2"
source_roots = ["src"]
generated_dir = "generated"
cache_dir = ".cx-cache"
color = "auto"
diagnostics = "text"
warnings_as_errors = false
jobs = 0

[stdlib]
mode = "cxx23"

[checks]
unknown_annotations = "metadata"
query_order = "warning"
```

## 7. 增量编译与缓存

### 7.1 文件级依赖图

每个模块记录：

- 源文件内容 hash。
- 解析器版本和语言版本。
- 编译器版本及目标平台。
- 配置 hash。
- 直接 `use` 依赖。
- 生成 sidecar、shader、注解 schema 输入。
- 公开符号摘要 hash。
- 生成物内容 hash。

依赖边分为：

```text
syntax dependency       目标语法或声明影响当前模块
public API dependency    目标公开符号摘要变化影响当前模块
build asset dependency   shader/sidecar/manifest 影响构建输出
```

### 7.2 缓存键

模块翻译缓存键：

```text
hash(source bytes)
+ hash(transitive public API summaries)
+ hash(cx.toml)
+ language_version
+ concordc version
+ target profile
+ enabled feature flags
```

缓存值包含 AST 序列化、语义摘要、GeneratedFile、diagnostics 和依赖清单。缓存文件采用版本化目录和临时文件写入，损坏条目自动删除并重新生成。

### 7.3 失效规则

必须重新解析当前模块：

- 源文件内容改变。
- 编译器、语言版本或配置改变。
- 依赖的公开符号摘要改变。
- 被引用注解 schema 改变。

只改变依赖模块私有实现时，不重新转译依赖它的模块。registry、全局 manifest 和 CMake 汇总文件是聚合节点，只在输入摘要变化时重新生成。

### 7.4 并行策略

解析和独立模块的语义索引可以并行；代码生成可以并行；全局 registry 和 manifest 在所有模块完成后单线程确定性聚合。输出排序始终按规范化相对路径排序，不依赖线程完成顺序。

### 7.5 兼容与清理

首次使用新版本编译器时，缓存目录按编译器 ABI/IR 版本隔离。`clean --cache` 只清除 `.cx-cache`；`clean --generated` 才清除生成目录。编译器永不根据缓存删除用户源码。

## 8. `std.xxx` 标准库包

实现一个内置 `StdlibRegistry`，每个条目包含：

```text
package name
header
exported type hints
minimum language version
```

第一版只做头文件映射：

```text
std.vector -> <vector>
std.string -> <string>
std.optional -> <optional>
std.memory -> <memory>
std.algorithm -> <algorithm>
std.unordered_map -> <unordered_map>
```

第二版加入类型提示，使编译器能识别 `std.vector<T>`、`std.optional<T>` 等常用模板，但不重建完整 C++ 模板推导器。所有不确定的模板表达式仍由 C++ 编译器负责。

不允许任意 `std.xxx` 拼接为头文件，防止路径注入、拼写错误和不同工具链行为不一致。

## 9. 更高级语法的落地顺序

### 阶段 A：编译器基础设施

- SourceManager 和 SourceSpan。
- 统一 Diagnostic API、颜色输出、JSON/SARIF。
- 严格参数解析和 `--help`、`--version`。
- `check`、`emit`、`graph`。
- `std.xxx` 内置包表。
- 原子写入和确定性输出。

验收：现有 fixtures 输出保持兼容；非法输入有稳定错误码；CI 输出无 ANSI。

### 阶段 B：增量编译

- 模块依赖图。
- 文件/配置/版本 hash。
- 公开符号摘要。
- `.cx-cache` 持久化。
- 聚合输出节点。
- `--trace` 显示命中、未命中和失效原因。

验收：修改单个无依赖模块时只生成该模块；修改公开声明时准确重建反向依赖；输出字节稳定。

### 阶段 C：语义安全

- 符号索引和导入检查。
- duplicate declaration、继承和注解 schema 检查。
- fix-it 和 did-you-mean。
- `warnings-as-errors`。
- IDE 可消费的 JSON 诊断。

验收：错误位置含行列和 excerpt；opaque C++ 不被误报；所有现有注解测试继续通过。

### 阶段 D：游戏语法糖

按收益和风险排序：

1. `component`。
2. `system/query`。
3. `spawn/entity/add`。
4. `enum`、`match`。
5. 属性和资源引用。
6. 生命周期声明。

每项都必须有“CX 输入 -> C++ 输出”的 golden test，并验证生成代码直接使用 Concord 现有公共 API。

### 阶段 E：开发体验

- `watch`。
- `format`。
- `graph --dot`。
- 语言服务器协议适配器。
- `compile_commands` 与 C++ 错误回链。
- `--explain` 缓存和 lowering。

## 10. 测试策略

测试目录按能力分组：

```text
tests/lexer/
tests/parser/
tests/diagnostics/
tests/semantic/
tests/codegen/
tests/incremental/
tests/stdlib/
tests/cli/
tests/golden/
```

必须覆盖：

- 每个语法构造的成功和失败样例。
- 字符串、注释、raw string 中不发生关键字替换。
- Windows 路径、大小写冲突和路径逃逸。
- 缓存命中、源文件失效、公开 API 失效、配置失效、版本失效。
- 并行与串行生成结果完全一致。
- 终端、重定向、JSON、SARIF 四种诊断输出。
- 标准包白名单和未知包提示。
- 生成文件失败时不留下半套输出。

性能基准记录：冷启动全量构建、热缓存构建、单文件修改、公共 API 修改、千文件项目并行构建。

## 11. 风险与约束

- 继续透传 C++ 会限制 CX 的类型检查深度；必须明确诊断边界，不能假装拥有完整 C++ 语义。
- 生成文件和 manifest 的全局聚合是增量编译的主要复杂点，应建模成独立节点。
- 缓存必须包含编译器版本、语言版本、配置和目标平台，否则会产生隐蔽的陈旧代码。
- `std.xxx` 只能走白名单映射，不能接受用户构造任意头文件路径。
- 游戏语法糖不能引入与 Scene/World 平行的数据模型。
- `@vulkan`、`@cpp` 和 shader body 必须继续保留 escape hatch，但其内容不能被普通 CX 语义检查器重写。

## 12. 近期实现任务拆分

1. 把当前 `Options` 改为结构化 CLI 解析器，加入 `--help`、`--version`、严格未知参数错误。
2. 抽出 `Diagnostic`、`SourceManager` 和 ANSI/Text/JSON 渲染器。
3. 为 lexer/parser 的错误补充 `SourceSpan`，保留现有 bison 解析入口。
4. 增加 `StdlibRegistry`，先支持表驱动的 `std.vector`、`std.string`、`std.optional`、`std.memory`。
5. 将当前全局生成流程拆成模块结果和聚合结果两个阶段。
6. 为每个模块写 `.cx-cache` 索引，首版采用文件 hash + 配置 hash 的安全粗粒度失效。
7. 增加 `check` 和 `emit` 命令，补充 golden tests。
8. 在增量稳定后再加入 `component` 和 `system/query` 语法。

## 13. 成功标准

当以下条件全部满足时，认为高级编译器基础完成：

- 旧项目无需修改即可使用兼容命令构建。
- `check` 能在不生成文件的情况下报告结构和已支持语义错误。
- 终端错误有颜色、加粗位置和稳定错误码，重定向输出保持干净。
- 修改一个文件不会无条件重写所有模块。
- 缓存可解释、可失效、可清理，错误缓存不会污染后续构建。
- `use std.xxx` 只允许内置白名单并生成标准 C++ 头（已实现；别名与花括号导入仍是规划）。
- 新语法有明确 lowering，不改变 Concord 的 ECS、DLL、Vulkan 边界。
- 串行和并行构建生成完全一致的结果。
