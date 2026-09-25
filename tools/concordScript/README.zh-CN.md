# ConcordScript（中文说明）

ConcordScript 是 Concord Flash 的 C++ 超集转译器。`.cx` 文件会被转译为
`.gen.h`、`.gen.cpp`，并可由 C++23 编译器继续编译。它不是 C++ 类型检查器：
普通 C++、Vulkan 代码和 shader 文本由用户控制，错误通常在生成的 C++ 或
shader 编译阶段报告。

## 构建与使用

`0.1.0-preview.1` 的 CLI 支持 `--project=目录` / `--out=目录` 等号写法。
`concordc --project 项目 --check` 可验证转译结构与注解，不写入文件，也不需要
`--out`；它不执行 C++ 类型检查或 shader 编译。CVM 请用 `--no-run`。
源码为 UTF-8（允许 BOM），UTF-16 会明确报错。扫描忽略输出目录、`.git`、
`.svn`、`node_modules` 和目录符号链接；相同生成内容不改写时间戳。
参数、解析、验证和 I/O 错误统一返回 1，成功返回 0；CVM 执行结果沿用其后端契约。

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/concordc.exe --project MyGame --out MyGame/generated
ctest --test-dir build --output-on-failure
```

构建会产出共享同一套编译器核心的两个命令行工具：

- `concordc` —— 历史默认行为：生成普通 C++（`.gen.h`/`.gen.cpp`）与
  `ConcordScriptRegistry.gen.h/.cpp`；含 `@entry` 的 `.cx` 文件在自己的
  `.gen.cpp` 中持有生成的 `main()`。
- `cc` —— Concord Visual Machine 默认：把 `@cvm` 块直接降低到 LLVM，用
  ORC LLJIT 运行（`--cvm jit`，默认）或输出 LLVM IR/bitcode 与原生 COFF
  目标文件（`--cvm aot`）。显式传 `--cpp` 时 `cc` 与 `concordc` 行为一致。

两个工具接受相同的参数，仅“未指定模式时的默认值”不同。都支持 `--help` /
`-h`、`--version` / `-V`、`--color auto|always|never` 和 `--no-color`。
真正的控制台里，`error` 是加粗红色，`warning` 是加粗黄色，`note` 是加粗青色，
`--help` 里的选项名是加粗绿色。管道、CI 和 `--color never` 保持纯文本。

## Concord Visual Machine（CVM）

CVM 不是自研字节码解释器：它把一个受限的、可用 LLVM 表达的语句子集**直接
降低为 LLVM IR**，再通过 ORC LLJIT 进程内执行（JIT），或通过目标机器输出
原生产物（AOT）。

```sh
cc --project MyCvmGame --out out            # JIT：编译并运行（默认）
cc --project MyCvmGame --out out --cvm aot  # AOT：写出 .ll/.bc/.obj
cc --project AnyGame --out out --cpp        # 回退到普通 C++ 生成
concordc --project AnyGame --out out        # 普通 C++ 生成（默认）
```

CVM 代码写在 `@cvm` 块中。当前语言有 `i64` / `f64` / `str` 三种 ABI 值类型，
以及不过宿主边界的本地 `struct`；标准库含数学（`sin`/`clamp`/`lerp`）与容器，
引擎绑定见 `docs/CVM宿主绑定与标准库.md`（ABI 版本 12）。入口仍是名为 `main`
的无参 `@cvm` 块。可运行示例：`examples/cvm_game`。

CVM 产物为 `ConcordVisualMachine.ll`（可读 IR）、`ConcordVisualMachine.bc`
（bitcode），`--cvm aot` 时额外生成 `ConcordVisualMachine.obj`
（x86_64 COFF 目标文件）。AOT 目标文件引用外部 `print`，供宿主构建链接；
concordc 不会用它伪造完整可执行文件。`--no-run` 跳过 JIT 执行，仅写出产物。

**模式边界。** CVM 只接受 `@cvm` 块与注释。class、`use`、`@entry`、
`@cpp`/`@vulkan` 注入、shader 以及任何 opaque C++ 都会带文件/行号诊断被
拒绝——需要它们的项目请使用 `--cpp`。CVM 没有自定义字节码文件：`.bc`
就是标准 LLVM bitcode。

CVM 由 LLVM 22（MSYS2 UCRT64）支撑并**静态链接**；MSYS2 的共享
`libLLVM-22.dll` 对宿主侧 `llvm::Module` 析构处理有缺陷，因此工具内嵌
LLVM 组件。CMake 通过 `LLVM_DIR` 定位 LLVM（默认
`C:/msys64/ucrt64/lib/cmake/llvm`）。

`concordc` 会递归读取项目中的 `.cx`，按相对路径稳定排序，先完成全部解析和
校验，再写入输出目录。项目必须恰好包含一个 `@entry`；生成目录包含每个源文件
的 `.gen.h/.gen.cpp`、注册表，以及有注解时位于输出根目录的
`<模块>.annotations.json`、项目级
`ConcordScriptManifest.json` 和 `ConcordScriptShaders.cmake`。后者可在加入
`concord` 后 `include()`，用 `concordscript_attach_shaders(target)` 编译/部署内联
shader；有 `@vulkan` 时可调用 `concordscript_attach_vulkan(target)` 显式链接
`concord::vulkan_sdk`。

## `@` 注解语法

注解是面向扩展和工具链的通用语法，名称可以包含 `::`、`.`、`/` 和 `-`：

```cx
@tag;
@tag(name="value", count=2);
@tag { 任意文本，包括 { 嵌套大括号 } };
@tag(stage=fragment) { 任意文本 };
```

注解可以连续放在 `use`、`class` 或 `@entry` 前面（此时不单独写分号），也
可以作为独立顶层注解以分号结尾。参数按顶层逗号和等号拆分，支持引号、C++ 模板
尖括号及嵌套圆括号、方括号、大括号；键名不区分大小写，重复键或空参数会报告源代码
行号。C++ 原始字符串会整体保留，其中的花括号、逗号和 `@` 不会被解释；普通字符串
和注释中的 `@` 也不会被识别为注解。

未知注解不会丢失：它们会进入 JSON manifest，并以安全的元数据注释出现在生成
代码中。这样项目可以先定义自己的 `@binding`、`@pipeline` 或插件命名空间，
再由构建工具读取，而不必等待转译器内置支持。

内置 `@shader` 和 `@pass` 会在写文件前校验参数白名单，额外的位置参数或未知
键会直接报错；`@shader` 还支持 `defines`、`include_dirs`、`options`（以及对应
别名）来传递编译配置；阶段别名不能与冲突的 `stage=` 或位置阶段同时使用（如果 shader
名称本身是阶段单词，请加引号）。未知名称仍按元数据透传，不受这组白名单限制。

## C++ 与 Vulkan 自由度

`@cpp`、`@raw`、`@code` 会把 body 原样复制到生成 C++；`@vulkan` 是明确的低级
Vulkan 逃生舱。可用 `section=header` 或 `section=source` 指定输出位置：

```cx
@include(path="vulkan/vulkan.h", system=true);
@vulkan(section=source) {
    VkResult CreatePipeline(VkDevice device) {
        return VK_SUCCESS;
    }
};
```

转译器不管理用户自行创建的 Vulkan 句柄，也不暴露引擎内部的 device、swapchain
或 Runtime/Render 私有对象；需要更高层整合时，应通过正常的 Concord 公共 API
增加 adapter。注解本身不会改变双 DLL 依赖方向。

如果需要在 Concord 每帧命令缓冲仍处于录制状态时插入自己的 pass，可使用
`@pass`（别名 `@render_pass`、`@custom_pass`）。它生成对公开 Vulkan pass ABI
的注册，参数为 `name`、`callback`、`phase`、`order`：

```cx
@vulkan(section=source, include=["vulkan/vulkan.h"])
void DrawOverlay(const Concord::VulkanPassContext& context, void*) {
    VkCommandBuffer commandBuffer =
        Concord::VulkanHandle<VkCommandBuffer>(context.commandBuffer);
    (void)commandBuffer; // 这里只录制命令，不提交或结束引擎命令缓冲
}
@pass(name="overlay", callback=DrawOverlay, phase=after_scene, order=100);
```

`phase` 可选 `initialize`、`before_scene`、`after_scene`、`shutdown`，`order` 越小越先执行；同序号按注册
顺序执行。回调收到带版本和大小字段的 opaque `VulkanPassContext`，普通项目不需要
包含 Vulkan 头，只有实际使用原生句柄的回调才按需链接 `concord::vulkan_sdk`。

## Shader 注解

`@shader` 将 body 写入 shader sidecar，并把名称、阶段、入口点、语言、路径、
所属对象和源行号写入 manifest。建议使用命名参数；位置参数同时接受
`name, stage, entry, language, path` 和阶段在前的 `stage, name, entry, language, path`：

```cx
@shader(name="toon", stage=fragment, entry="main", language=glsl,
        path="shaders/toon.frag") {
#version 460
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
};
```

不使用大括号时，也可以用 `source="..."` 或 `code="..."` 携带内联 shader 文本
（换行写成 `\n`）。只有 `file=`/`path=` 而没有 body/source/code 时才是外部引用，
转译器不会覆盖该文件。

下面两种写法等价：

```cx
@shader("toon", fragment, "main") { /* GLSL */ };
@shader(fragment, "toon", "main") { /* GLSL */ };
```

也可以直接使用阶段别名：`@vertex`、`@fragment`、`@compute`、`@raygen`、
`@miss`、`@closesthit`、`@anyhit`、`@intersection`、`@callable`，以及
`@rgen`、`@rchit` 等规范后缀。阶段会归一化为 Vulkan 后缀：`vert`、`frag`、
`comp`、`geom`、`tesc`、`tese`、`rgen`、`rmiss`、`rchit`、`rahit`、`rint`、
`rcall`、`task`、`mesh`；短别名还包括 `vs/fs/ps/cs/gs/hs/ds/as/ms`。

`path` 必须是相对于输出目录的安全路径，不能是绝对路径、带盘符或包含 `..`。
省略路径时使用确定性的 `shaders/<模块>.<名称>.<阶段>[.<语言>]`；显式不安全
路径会直接报错，避免拼写错误悄悄改写输出位置。多个 sidecar 目标冲突也会在
写文件前拒绝。使用 `file=...`/`path=...` 且没有 body 时可引用外部 shader，
转译器会在项目 manifest 中标记 `external=true`，登记其路径但不会覆盖外部文件。

每个 shader 都可以在 `.cx` 中携带独立的编译选项：

```cx
@fragment(name="toon", defines=["TOON=1"],
          include_dirs=["shaders/include"], options=["-g"]) {
#version 460
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
};
```

这些字段可以写成一个带引号的值，也可以写成方括号列表。生成的 CMake 会把每项
作为独立进程参数传给 glslc、glslangValidator 或 DXC，不会按 shell 命令解释；同样
的配置也会出现在 JSON manifest 和逐 shader 的 CMake 调用中。

生成的 CMake 片段还提供可选的
`concordscript_attach_external_shaders(target)`，可以用同一个
`concord_add_shader_sources` helper 编译项目中已经存在的外部 shader；
调用前应确保这些文件已存在于生成目录或由项目构建系统准备好；文件缺失时
helper 会在 CMake 配置阶段报错。`concordc` 本身只登记路径，不会复制或修改外部文件。

除 JSON 外，`concordc` 还会生成 `ConcordScriptMetadata.gen.h`。这是无需 JSON
解析器即可使用的 C++20 `inline constexpr` 元数据表：

```cpp
#include "ConcordScriptMetadata.gen.h"
const auto* descriptor = Concord::Script::Metadata::FindAnnotation("Main", "descriptor");
if (descriptor != nullptr) {
    for (const auto& argument : Concord::Script::Metadata::Arguments(*descriptor)) {
        // argument.key/value 保留注解原始值
    }
}
const auto* fragment = Concord::Script::Metadata::FindShader("Main", "toon", "frag");
```

表中包含 `Version`，并按确定顺序保存模块、注解、参数和 shader，适合直接写入
descriptor/pipeline/resource 注册代码或编辑器工具。生成的
`ConcordScriptShaders.cmake` 同时提供 `CONCORDSCRIPT_METADATA_HEADER`、
`CONCORDSCRIPT_ANNOTATION_MANIFESTS` 和 `concordscript_attach_metadata(target)`，
可一键把头文件及 include 路径接到 CMake target。

## 内置预处理注解

```cx
@include(path="vector", system=true);       // #include <vector>
@include(path=["a.h", "b.h"]);             // 多个 include
@define(name="MAX_LIGHTS", value=64);      // #define MAX_LIGHTS 64
@pragma(value="once");                      // #pragma once
```

include 路径会在生成前检查。空路径以及会终止或改变 C++ `#include` 语法的字符
会报告带源行号的错误。

`section=header/source` 同样适用于 `@cpp` 和 `@vulkan`。其他注解只保留元数据，
不会被猜测性地转成 GPU 状态；构建系统可以用 manifest 实现 descriptor、pipeline、
resource、specialization、attachment、dispatch、trace、feature、requires 等
项目约定。

## 失败与安全边界

- `.cx` 文件名必须是合法且唯一的 C++ 标识符，否则会拒绝 namespace 冲突。
- `@entry` 多于一个、缺少一个、重复参数、未知 shader 阶段和不安全路径都会
  返回非零状态。
- 所有源文件解析成功且验证完成后才写出，失败不会留下本次生成的半套文件。
- shader 代码不会由 `concordc` 编译或反射；请在 CMake/CI 中调用
`glslangValidator`、DXC 或项目自己的工具。

`@shader` 也支持 `source="..."` 或 `code="..."` 内联文本；使用 `file=`/`path=`
且不带 body 时只登记外部文件，不会被覆盖。所有 sidecar 路径都会在写出前做
目录穿越、保留文件和冲突检查。

`script/tests/fixtures` 提供不依赖 GPU 的 CTest 夹具，覆盖参数嵌套、未知注解、
原始 C++ 中的 `@`、Vulkan 注入、全部 Vulkan 光追阶段、外部 shader、路径安全和
失败时不写出文件等行为。可用 `ctest --test-dir build --output-on-failure` 运行。

生成的 shader CMake helper 会按 `CONCORD_SHADER_COMPILER` 调用
`glslc`、`glslangValidator` 或 DXC。GLSL 支持 glslc/glslang；自定义 GLSL
入口会同时传递 `-e` 与 `--source-entrypoint main`。HLSL 可使用支持 SPIR-V
后端的 glslang 或 DXC。外部 shader 默认相对于生成目录解析，也可以在消费方
CMake 中覆盖 `CONCORDSCRIPT_EXTERNAL_SHADER_ROOT`。

完整的英文 API/语法说明见 [`README.md`](README.md)。
