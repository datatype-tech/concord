# ConcordScript

ConcordScript is a thin C++ superset transpiler for Concord Flash. It compiles
`.cx` files to plain C++ (`.gen.h` / `.gen.cpp`) that any C++23 compiler can
build. It is a **transpiler, not a full C++ compiler**: the small set of
language constructs documented below is parsed, while ordinary C++ and
annotation bodies remain user-controlled text.

## Why

Writing game logic against Concord's engine API (`Scene::Spawn`, ECS
components, systems) is already close to plain C++. ConcordScript trims the
remaining ceremony: file-scoped namespaces, one-file-per-class with no
header/source split, a project-wide entry point, and a one-call registration
hook that wires `@register`-tagged classes straight into a `Scene`.

## File extension

`.cx`

## Toolchain

Built with [flex](https://github.com/westes/flex) 2.6 and
[GNU bison](https://www.gnu.org/software/bison/) 3.8 — the standard GNU
lexer/parser generator pair. The compiler core is shared by two CLI
personalities that are both produced by the build:

- `concordc` — the historical default: generates plain C++
  (`.gen.h`/`.gen.cpp`) plus a `ConcordScriptRegistry.gen.h/.cpp` pair; the
  `.cx` file containing `@entry` owns the generated `main()` in its own
  `.gen.cpp`.
- `cc` — the Concord Visual Machine default: lowers `@cvm` blocks straight
  to LLVM and runs them with the ORC LLJIT (`--cvm jit`, the default) or
  emits LLVM IR/bitcode plus a native COFF object (`--cvm aot`). Pass
  `--cpp` to make `cc` behave exactly like `concordc`.

`cc` and `concordc` accept the same arguments; only the mode assumed when no
mode flag is given differs. Both understand `--help` / `-h`, `--version` /
`-V`, `--color auto|always|never`, and `--no-color`. On a real console,
`error` is bold red, `warning` is bold yellow, `note` is bold cyan, and
flags in `--help` are bold green. Pipes, CI, and `--color never` stay
plain text so log matchers keep working. `NO_COLOR` disables colour under
`--color auto`; `--color always` paints anyway.

## Create a game project

`concordc --init MyGame` creates a starter `Main.cx`, CMake build, README
and ignore file. The destination must be new or empty; existing project
files are never overwritten. Initialization cannot be combined with
compilation options.

```powershell
concordc --init MyGame
cmake -S MyGame -B MyGame/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/path/to/ConcordFlash-sdk"
cmake --build MyGame/build
./MyGame/build/game.exe
```

The generated CMake project invokes `concordc` for root-level `.cx` files
and stages the SDK runtime next to the executable. Use the Windows x64
UCRT64 MinGW toolchain matching the preview SDK. Set
`-DCONCORDSCRIPT_COMPILER="C:/path/to/concordc.exe"` to override the compiler.

## Concord Visual Machine (CVM)

CVM is not a custom bytecode interpreter: it lowers a restricted,
LLVM-expressible statement subset **directly to LLVM IR** using the LLVM
C++ API, then either executes it in-process through ORC LLJIT (JIT) or
emits native artifacts through the target machine (AOT).

```sh
cc --project MyCvmGame --out out            # JIT: compile + run (default)
cc --project MyCvmGame --out out --cvm aot  # AOT: write .ll/.bc/.obj
cc --project AnyGame --out out --cpp        # fall back to plain C++ generation
concordc --project AnyGame --out out        # plain C++ generation (default)
```

CVM code lives in `@cvm` blocks. A block is one function; the one named
`main` is the entry, and it takes no parameters because the JIT calls it that
way.

## Types

CVM has three ABI value types, and a record type that never crosses the host
boundary. `i64` is for anything that names, counts or indexes something — a
handle, a key code, a comparison result. `f64` is for anything physical — a
position, an angle, a speed, a delta time. `str` is text, and a script never
sees it as a pointer: the only things it can do with one are what `str_*`
declares. A `struct` is a CVM-local value; host functions still only take
i64, f64 and str.

Widening an `i64` to an `f64` is implicit because it is lossless; narrowing
is not, because it silently drops a fraction, so it takes `to_i64(x)`. Strings
convert to nothing on their own — text parsing and formatting are `str_to_int` and
`str_from_int`, which say what they do.

A function declares its parameters with `args="x, y: f64"` and its result with
`ret="f64"`, both defaulting to `i64`. `@cvm_global` declares
module-level state shared by every function, and its type is inferred from the
initializer unless written as `let speed: f64 = 6.0;`. Globals are not sugar: a
per-frame callback cannot return its state to anyone and CVM has no closures, so
cross-frame state has to live in one.

Statements are `let` declarations, assignment (including `+= -= *= /= %=`),
`if`/`else`, `while`, C-style `for (init; cond; step)`, `break` / `continue`,
`print(...)` / `print_float(...)` / `print_str(...)` and `return ...`.
Expressions are `+ - * / %`, unary `-` and `!`, short-circuit `&&` / `||`,
the ternary `cond ? a : b`, comparisons (`< <= > >= == !=`, yielding 0/1),
parentheses, literals (`true` / `false` are i64 1 / 0), calls, and locals; a
literal is `f64` when it has a fraction or an exponent, and `str` when it is
quoted. Strings compare by content (`==` / `!=`) and concatenate with `+=`.

A bare name is a local when one exists, then a global, and otherwise a function
address — which is how a callback or a thread entry point is handed over. When
that argument is written as a bare name the compiler checks it against the
signature the host will call it through, so a mismatched callback is a compile
error rather than a wrong answer. A function must end on a `return`:

```cx
@cvm(name="sumTo") {
    let total = 0;
    let index = 1;
    while (index <= 10) {
        total = total + index;
        index = index + 1;
    }
    return total;
};

@cvm {
    print(sumTo());
    return sumTo() * 2;
};
```

### Host functions and the built-in standard library

A CVM module is LLVM IR, so the only thing it can call is a C ABI. Every API
reachable from .cx is therefore declared in one table
(`src/CvmHostRegistry.cpp`) that maps a CX-visible name to a C symbol and a
signature; `script/runtime/` implements that surface in plain C so the same
library serves the JIT and an AOT object.

The built-in standard library covers console output (`print`,
`print_float`, `print_str`); strings (`str_len`, `str_concat`,
`str_find`, `str_sub`, `str_from_int`, `str_to_float`, ...);
containers in both an i64 and an f64 flavour (`list_*`/`flist_*`,
`array_*`/`farray_*`); threads (`thread_spawn`,
`thread_spawn_arg`, `thread_join`, `mutex_*`, `counter_*`);
and the clock (`sys_time_ms`, `sys_sleep_ms`, `sys_random_*`),
and scalar maths (`sin`, `cos`, `sqrt`, `clamp`, `lerp`, `radians`, ...).

Containers, threads, mutexes and counters are opaque handles rather than
pointers, so a stale handle degrades to a documented failure value instead of
dereferencing garbage. Strings carry an ownership word, so freeing a literal is
a no-op rather than a crash. See `docs/CVM宿主绑定与标准库.md` for the full list,
the signature table, and the AOT link contract.

### Calling the engine

The Concord engine is a second host surface, but it is not linked into the
compiler: name a module with `--cvm-host` and the JIT binds whatever that
module exports, by name, at run time. `cc.exe` therefore keeps no dependency
on the engine, and an AOT object stays the real host's to link.

`--cvm-host` is repeatable, and the engine needs it twice: Runtime.dll carries
the C ABI, while a render backend only registers once Render.dll is loaded.

Jolt is the same solver C++ uses. `concord_add_physics_system()` registers it
for `concord_run_scene`; `concord_physics_step(scene, dt)` steps it without a
window (tests, tools). Dynamic boxes and spheres fall, static volumes stay,
`concord_raycast` hits Jolt when the solver is running, and
`concord_entity_set_wish_velocity` drives a walking capsule.

Steam Audio is the same mixer C++ uses. `concord_add_audio_system()` opens the
stereo mix; `concord_spawn_sound` loads a WAVE and places it in the world, and
`concord_entity_add_listener` puts ears on the camera.

```sh
cc --project examples/cvm_game --out examples/cvm_game/generated \
   --cvm-host build/ConcordFlashGameEngineRuntime.dll \
   --cvm-host build/ConcordFlashGameEngineRender.dll
```

```cx
@cvm_global {
    let player = 0;
    let speed = 6.0;        // metres per second
};

@cvm(name="update", args="deltaTime: f64") {
    let step = speed * deltaTime;   // seconds in, distance out
    let x = concord_entity_position(player, 0);
    if (concord_key_down(1)) { x = x - step; }   // A
    if (concord_key_down(4)) { x = x + step; }   // D
    concord_entity_set_position(player, x, 0.5, 0.0);
    return 0;
};

@cvm {
    let scene = concord_scene_new();
    concord_spawn_camera(scene, 0.0, 7.0, -15.0, 0.0, 0.5, 0.0);
    concord_spawn_sun(scene, 52.0, 210.0, 3.2);
    player = concord_spawn_box(scene, 0.0, 0.5, 0.0, 1.0, 1.0, 1.0, 224, 96, 64);
    concord_set_update(update);
    return concord_run_scene(scene, 1280, 720, "ConcordScript");
};
```

Lengths and angles cross the ABI as world units and degrees — the units an
author already reasons in — while handles, key codes and colour channels stay
integers and a window title crosses as text. A frame callback is `i64(f64)`,
because one frame is about 0.016 of a second and an integer would truncate
every one of them to zero. Key codes are a dense enum, so A is 1, Z is 26, Up
is 49, Space is 61 and Escape is 63. Calling an engine entry point without
`--cvm-host` is a compile-time error that names the missing symbols rather than a
link failure.

`examples/cvm_game/Main.cx` is a complete program: WASD moves, Shift sprints,
Space shoots, a score HUD is drawn, and Escape quits. The engine side of this
contract is `concord/include/Concord/CCvm.h`; the full surface and the ABI
version are in `docs/CVM宿主绑定与标准库.md`.

```cx
@cvm {
    print(6 * 7);
    return (2 + 3) * (10 - 4);
};

@cvm(name="levelUpCost") {
    return 100 * 2 + 50;
};
```

CVM output artifacts are `ConcordVisualMachine.ll` (human-readable IR),
`ConcordVisualMachine.bc` (bitcode), and — with `--cvm aot` —
`ConcordVisualMachine.obj` (x86_64 COFF object). The AOT object references
external `print` and is meant to be linked by a host build; concordc does
not produce a full executable from it. `--no-run` skips JIT execution and
only writes the artifacts.

**Mode boundaries.** CVM accepts *only* `@cvm` blocks and comments. Classes,
`use` directives, `@entry`, `@cpp`/`@vulkan` injections, shaders, and any
other opaque C++ are rejected with a file/line diagnostic — use `--cpp` for
projects that need them. There is no custom bytecode file: `.bc` is standard
LLVM bitcode.

LLVM 22 (MSYS2 UCRT64) backs CVM and is linked **statically**; the MSYS2
shared `libLLVM-22.dll` mis-handles host-side `llvm::Module` destruction, so
the tools embed the LLVM components instead. CMake locates LLVM via
`LLVM_DIR` (defaults to `C:/msys64/ucrt64/lib/cmake/llvm`).

## Syntax overview

### File-scoped namespace

Every `.cx` file is wrapped in a namespace named after the file. `Player.cx`
becomes `namespace Player { ... }` in the generated header — this is what
lets other files `use` it by name.

```cx
// Player.cx
use Concord.CObject;

pub class Player {
    pub var health = 100;

    pub void TakeDamage(int amount) {
        health -= amount;
    }
};
```

### `use`: dotted imports

Use dot-separated paths for engine modules and project modules. Engine imports
start with `Concord` and map to public facade headers; project imports map to
`.gen.h` files.

```cx
use Concord.CApplication;
use Concord.CScene;
use MyGame.Player;
```

Generated output:

```cpp
#include <Concord/CApplication.h>
#include <Concord/CScene.h>
#include "Player.gen.h"
```

The legacy slash form for engine headers remains accepted for compatibility,
but new code should use the dotted form. Header suffixes are omitted from CX
imports. Direct private engine headers are not part of the import contract.

### `var`

`var` transpiles to `auto`. Prefer it for local variables the way this
codebase's C++ prefers `auto`; it is not required — plain C++ declarations
still work everywhere, since ConcordScript is a superset.

```cx
var count = 0;      // auto count = 0;
int total = 0;       // int total = 0;   (plain C++ still works)
```

### Access specifiers: `pub` / `priv` / `prot`

Written per-member, Java-style, instead of C++'s colon-terminated sections.
A member with no specifier defaults to `pub`.

```cx
class Health {
    var current = 100;        // defaults to pub
    priv var max = 100;

    pub void Heal(int amount) { current += amount; }
};
```

transpiles to:

```cpp
class Health {
public:
    auto current = 100;
private:
    auto max = 100;
public:
    void Heal(int amount) { current += amount; }
};
```

### `extends` / `implements`

```cx
class Player extends Entity implements IDamageable {
};
```

transpiles to:

```cpp
class Player : public Entity, public IDamageable {
};
```

### `@register`: one call wires classes into a Scene

Tagging a class `@register` adds it to a project-wide registry. `concordc`
emits `Concord::Script::RegisterAll(Concord::Scene&)`, which the generated
`main()` — or your own code — calls once to spawn every tagged class into a
scene, without hand-writing a `Spawn` call per type.

```cx
@register
class Enemy {
    pub var health = 50;
};
```

A registered class must satisfy the same shape `Scene::Spawn` expects: a
public default-constructible type. `concordc` generates the boilerplate that
calls `scene.CreateEntity()` and default-constructs the type as a component;
give it real ECS components inside the class body as needed.

### `@entry`: the one main() in a project

Exactly one `.cx` file in a project may contain an `@entry` block. It
compiles to `int main() { ... }`. Zero or more than one `@entry` across the
project is a `concordc` error.

Top-level `@startup { ... };` code runs before `game.Run()`. Top-level
`@update { ... };` code becomes a per-frame `game.OnUpdate` callback and
receives `Concord::f32 deltaTime` in seconds. These lifecycle annotations must
be outside the opaque `@entry` body.

```cx
use Concord.CApplication;
use Concord.CScene;

@entry {
    Concord::Game game;
    Concord::Scene scene;
    Concord::Script::RegisterAll(scene);
    game.LoadScene(scene);
    game.Run();
};
```

### Extensible `@` annotations

Annotations are the escape hatch for engine-facing and build-facing code. The
lexer recognizes an annotation name without trying to understand its body, so
GLSL/HLSL preprocessor lines, Vulkan structs, templates, and nested braces are
preserved exactly. Names may contain `::`, `.`, `/`, and `-`, which leaves room
for project or plug-in namespaces.

The four forms are:

```cx
@tag;
@tag(name="value", count=2);
@tag { arbitrary text, including { nested } braces };
@tag(stage=fragment) { shader or other arbitrary text };
```

An annotation immediately before a `use`, `class`, or `@entry` is attached to
that item and does not have its own semicolon. A stand-alone annotation ends in
`;`. Argument splitting is shallow but quote-, template-, and delimiter-aware;
named keys are case-insensitive and duplicate or empty arguments are parse
errors. C++ raw string literals are preserved as a whole, so braces, commas,
and `@` characters in their payload are never interpreted. Text inside strings
and comments is never treated as an annotation.

Built-in annotations validate their contract before any output is written:
`@shader` accepts only `name`, `stage`, `entry`, `language`, `path`/`output`/`file`,
inline `source`/`code`, and compiler options (`defines`, `include_dirs`, and
`options`, including their aliases); `@pass` accepts only its lifecycle fields and rejects
extra positional values. Stage aliases cannot be combined with a conflicting
`stage=` or positional stage (quote a shader name if it is itself a stage word).
Unknown annotation names remain metadata and are not subject to this whitelist.

#### C++ and Vulkan escape hatches

`@cpp`, `@raw`, and `@code` copy their body into generated C++. `@vulkan` is the
same deliberately low-level path for user-owned Vulkan setup, resource
creation, and command recording. Use `section=header` or `section=source` to
choose the generated stream; the default is header for item-attached `@cpp`
and source for top-level/entry code (and for `@vulkan`).

```cx
@include(path="vulkan/vulkan.h", system=true);
@vulkan(section=source) {
    VkResult CreateMyPipeline(VkDevice device) {
        // The body is ordinary C++; concordc does not wrap or validate it.
        return VK_SUCCESS;
    }
};
```

The escape hatch intentionally does not expose Concord's private
`VkDevice`/swapchain objects or change Runtime's DLL dependency direction.
Advanced code owns the Vulkan handles it uses; an engine adapter can be added
as a normal public API without changing the annotation syntax.

#### Shader annotations

`@shader` writes the body to a sidecar shader source and records it in the
per-module `<Module>.annotations.json` manifest at the output root (the module
stem remains globally unique, including for nested source directories). The CLI also emits
`ConcordScriptManifest.json` and `ConcordScriptShaders.cmake`; include the
latter after `add_subdirectory(concord)` and call its
`concordscript_attach_shaders(target)` helper to compile and stage inline
shaders when `CONCORD_BUILD_SHADERS=ON`. Named arguments are preferred. The
positional form accepts either `name, stage, entry, language, path` or the
stage-first shorthand `stage, name, entry, language, path`:

```cx
@shader(name="toon", stage=fragment, entry="main", language=glsl,
        path="shaders/toon.frag") {
#version 460
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
};
```

For a declaration without a brace body, `source="..."` or `code="..."` can
carry inline shader text (escape newlines as `\n`). A `file=`/`path=` annotation
without either form is an external reference and is never overwritten.

These are equivalent:

```cx
@shader("toon", fragment, "main") { /* GLSL */ };
@shader(fragment, "toon", "main") { /* GLSL */ };
```

Stage aliases normalize to Vulkan suffixes: `vertex`/`vs` → `vert`,
`fragment`/`pixel`/`fs`/`ps` → `frag`, `compute`/`cs` → `comp`,
`geometry`/`gs` → `geom`, `tesscontrol`/`hull`/`hs` → `tesc`, and
`tesseval`/`domain`/`ds` → `tese`. Ray aliases (`raygen`/`rgen`,
`miss`/`raymiss`/`rmiss`, `closesthit`/`rchit`, `anyhit`/`rahit`,
`intersection`/`rint`, `callable`/`rcall`) map to their `r*` suffixes;
`task`/`amplification`/`as` and `mesh`/`ms` are also accepted. The shorter
stage annotations (`@vertex`, `@fragment`, `@compute`, `@raygen`, `@miss`,
`@closesthit`, `@anyhit`, `@intersection`, `@callable`, plus canonical
suffixes such as `@rgen` and `@rchit`) are equivalent to
`@shader(stage=...)`.

Sidecar paths must be relative to the output directory and may not contain
`..`, a drive, or a root. An omitted path receives a deterministic
`shaders/<module>.<name>.<stage>[.<language>]` fallback; an explicitly unsafe
path is rejected so a typo cannot silently redirect output. Collisions are
rejected before any output is written. A `file=`/`path=` shader with no body is
marked `external` in the project manifest and is only registered, never copied
or overwritten. The manifest also retains the original annotation name,
arguments, body, owner, and source line, allowing a project build tool to add
descriptor/pipeline generation without changing concordc.

Each shader can carry its own compiler configuration:

```cx
@fragment(name="toon", defines=["TOON=1"],
          include_dirs=["shaders/include"], options=["-g"]) {
#version 460
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
};
```

`defines`, `include_dirs`, and `options` accept one quoted value or a bracketed
list. Values are passed as individual process arguments through the generated
CMake helper, never as shell text; the same options are retained in JSON metadata
and the per-shader CMake calls.

`concordc` also emits `ConcordScriptMetadata.gen.h`. Include this generated
C++20 header to query all annotations and shaders without a JSON parser:

```cpp
#include "ConcordScriptMetadata.gen.h"

const auto* descriptor = Concord::Script::Metadata::FindAnnotation("Main", "descriptor");
if (descriptor != nullptr) {
    for (const auto& argument : Concord::Script::Metadata::Arguments(*descriptor)) {
        // argument.key/value are the original annotation values.
    }
}
const auto* fragment = Concord::Script::Metadata::FindShader("Main", "toon", "frag");
```

The tables are `inline constexpr`, deterministic, and carry a version number;
they can feed descriptor/pipeline/resource registration code or editor tools.
`ConcordScriptShaders.cmake` exposes `CONCORDSCRIPT_METADATA_HEADER`,
`CONCORDSCRIPT_ANNOTATION_MANIFESTS`, and `concordscript_attach_metadata(target)`
for attaching the generated header to a CMake target.

#### Preprocessor and metadata annotations

`@include(path="...", system=true)` emits an angle-bracket include (omit
`system` for a quoted include), `@define(name="KEY", value=VALUE)` emits a
`#define`, and `@pragma(value="...")` emits a `#pragma`. Their output follows
the same header/source placement as `@cpp`. Include paths are checked before
generation; empty paths and characters that would terminate or alter a C++
include directive are reported as errors.

All other names are retained as metadata comments in generated C++ and as JSON
records. This makes declarations such as `@binding`, `@descriptor`,
`@push_constant`, `@specialization`, `@pipeline`, `@resource`, `@sampler`,
`@attachment`, `@dispatch`, `@trace`, `@layout`, `@feature`, `@requires`, and
`@render` available to tooling today without pretending that concordc can
validate GPU layouts. Projects can therefore introduce their own annotation
vocabulary and migrate a directive to built-in lowering later without changing
the source grammar.

#### Vulkan boundary and build integration

`@vulkan` is an application-side translation-unit escape hatch: its body is
copied verbatim and may include Vulkan headers available to the consumer. It
does not reach into Concord's private renderer state or alter the Runtime →
Render DLL dependency direction. External `file=`/`path=` shader references
are recorded in the manifest but are never overwritten by `concordc`, so
checked-in shader files and inline blocks can coexist.

The generated fragment also exposes
`concordscript_attach_external_shaders(target)` for projects that want to
compile checked-in external files with the same `concord_add_shader_sources`
helper. Calling it is optional; `concordc` only records external paths and
never copies or edits them. Call it only after the referenced files have been
checked into the generated tree (or adjust the generated list in your build
system); the helper fails configuration when a listed source is missing.

For command recording or lifecycle hooks that should run through Concord's
public Vulkan pass ABI, use `@pass` (or `@render_pass`/`@custom_pass`). It emits
a registration thunk and accepts `name`, `callback`, `phase` and `order`.
The callback receives an opaque, versioned `VulkanPassContext`; include the
Vulkan SDK in the callback's own translation unit when native handle types are
needed. A body can be used as an inline callback when `callback=` is omitted:

```cx
@vulkan(section=source, include=["vulkan/vulkan.h"])
void DrawOverlay(const Concord::VulkanPassContext& context, void*) {
    VkCommandBuffer commandBuffer =
        Concord::VulkanHandle<VkCommandBuffer>(context.commandBuffer);
    // Record commands only; do not submit or end Concord's command buffer.
    (void)commandBuffer;
}

@pass(name="overlay", callback=DrawOverlay, phase=after_scene, order=100);
```

`phase` is `initialize`, `before_scene`, `after_scene`, or `shutdown`; lower
`order` values run first and registration order breaks ties. Pass callbacks are
optional and failure-isolated:
one callback returning `false` is logged while later callbacks still run.

```cmake
add_subdirectory("${CONCORD_ROOT}/concord" concord_build)
include("${CMAKE_CURRENT_SOURCE_DIR}/generated/ConcordScriptShaders.cmake")
concordscript_attach_shaders(my_game)
concordscript_attach_vulkan(my_game) # only needed when @vulkan is present
```

`concordscript_attach_vulkan` links the explicit `concord::vulkan_sdk`
interface target, which supplies the vendored Vulkan headers and loader import
library. Ordinary Concord consumers do not link that target and continue to
receive the Vulkan-free facade API.

The generated shader helper invokes `glslc`, `glslangValidator`, or DXC according
to `CONCORD_SHADER_COMPILER`. GLSL works with glslc/glslang; glslang receives
both `-e` and `--source-entrypoint main` for custom GLSL entry points. For HLSL,
glslang's `-e` names the source function directly, while DXC uses `-E`; both
paths preserve a custom HLSL entry point. External files resolve
under `CONCORDSCRIPT_EXTERNAL_SHADER_ROOT`, which defaults to the generated
directory and can be overridden from the consuming CMake project.

### Semicolons

Every statement, class body, and `@entry`/`@register` block ends with a
semicolon — this is ConcordScript grammar, not optional the way a bare
`{}` block is in C++.

### Everything else is plain C++

Method bodies, expressions, templates, operator overloads, lambdas — none of
it is reinterpreted. `concordc` passes it straight through to the generated
file, so any construct your C++ compiler accepts is legal ConcordScript.

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
# Optional, GPU-independent parser/code-generation regression suite
ctest --test-dir build --output-on-failure
```

Produces `build/concordc.exe`.

The CLI sorts `.cx` inputs by relative POSIX path, keeping generated include
and registry order stable across filesystems. It parses and validates the
whole project before writing new output, so a syntax or lexer error is
reported with its source line without partially writing the current run.
File stems must be valid C++ identifiers and unique across nested directories;
otherwise the command reports the namespace collision before generating code.

## Usage

```sh
concordc --project MyGame/ --out MyGame/generated/
```

Reads every `.cx` file under the project directory and writes, per file, a
matching `<Name>.gen.h`/`<Name>.gen.cpp` pair — plus two project-wide
files: `ConcordScriptRegistry.gen.h` (the `RegisterAll` declaration) and
`ConcordScriptRegistry.gen.cpp` (its definition, spawning every
`@register`-tagged class). The file containing `@entry` automatically
`#include`s the registry header, so calling `Concord::Script::RegisterAll`
from `@entry` needs no manual `use`. Feed the output directory to CMake
alongside `concord::concord` like any other C++ sources — see
`examples/hello/CMakeLists.txt` for a working setup.

Annotation sidecars are written under the requested output directory. Shader
bodies are emitted at the requested relative path (or the deterministic
`shaders/` fallback), and a module that contains annotations receives a
`<Name>.annotations.json` manifest at that directory's root. A failed parse, duplicate argument, unsafe
shader path, unsupported stage, or sidecar collision returns a non-zero status
before the current invocation writes generated output.

## Showcase example

`examples/showcase/` is a two-file ConcordScript sample that exercises the
engine's visible path as well as the translator: `Main.cx` owns the single
`@entry`, while `Showcase.cx` supplies a project `use`, a registered component,
an `ISystem`, moving point lights, materials, and a shadow-casting scene. The
generated program keeps the camera aspect automatic by omitting any fixed
aspect override.

```sh
build/concordc.exe --project examples/showcase \
    --out examples/showcase/generated
cmake -S examples/showcase -B examples/showcase/build -G Ninja \
    -DCONCORD_BUILD_SHADERS=ON \
    -DCONCORD_SHADER_COMPILER=/path/to/glslangValidator
cmake --build examples/showcase/build
```

`generated/` and the example build directories are ignored by Git. Running the
resulting `showcase_example` opens the same colored-light, depth, and
directional-shadow demonstration used by the native host sample.

The `tests/fixtures/` CTest harness is GPU-independent. It covers nested
annotation arguments, unknown names and raw `@` text, header/source Vulkan
injection, every Vulkan ray-tracing stage alias, inline and external shaders,
path safety, duplicate names, entry-count errors, and the no-partial-output
guarantee. Run it with `ctest --test-dir build --output-on-failure`.

`examples/annotations/` is the focused escape-hatch sample. It demonstrates
header/source C++ injection, raw Vulkan code, descriptor metadata, inline
vertex/fragment shaders, and the generated CMake asset fragment:

```sh
build/concordc.exe --project examples/annotations --out examples/annotations/generated
cmake -S examples/annotations -B examples/annotations/build -G Ninja \
    -DCONCORD_BUILD_SHADERS=ON
cmake --build examples/annotations/build
```

## Implementation layout

```
script/
├── CMakeLists.txt          # FLEX_TARGET / BISON_TARGET wiring + CTest
├── tests/                  # annotation fixtures, CTest driver, and fixture notes
├── src/
│   ├── lexer.l              # flex rules; keywords vs. RAW_CODE, brace-balanced body capture
│   ├── parser.y              # bison grammar; builds a ConcordScript::SourceFile
│   ├── Ast.h                 # SourceFile / TopLevelItem / ClassDecl / UseDirective
│   ├── KeywordSubstitution.h/.cpp  # var/pub/priv/prot -> auto/access-label text rewriting
│   ├── Render.h/.cpp         # use -> #include, extends/implements -> base-class list
│   ├── AnnotationCodeGen.h/.cpp # annotation dispatch and output aggregation
│   ├── AnnotationShader*.h/.cpp # stage aliases, shader metadata, validation
│   ├── AnnotationShaderOptions*.h/.cpp # per-shader defines/includes/flags
│   ├── AnnotationDirectives*.h/.cpp # @include/@define/@pragma and code blocks
│   ├── AnnotationText*.h/.cpp # opaque text, literals, templates, paths, comments
│   ├── AnnotationValues*.h/.cpp # argument lookup and value decoding
│   ├── AnnotationPass*.h/.cpp # @pass registration and validation helpers
│   ├── CodeGen.h/.cpp        # C++/sidecar assembly
│   ├── CodeGenManifest.cpp   # JSON manifest rendering
│   ├── MetadataCodeGen.h/.cpp # constexpr C++ metadata registry generation
│   ├── MetadataCmake.cpp      # generated metadata CMake integration
│   ├── Registry.h/.cpp       # RegisterAll declaration + definition generation
│   ├── main.cpp              # concordc CLI: validates and writes generated output
└── examples/hello/           # a project verified to build against the real engine
```

## Known limitations

- `concordc` is a transpiler, not a type checker: a `.cx` file with invalid
  C++ inside a captured body only fails when the *generated* C++ is
  compiled, with error locations pointing at the `.gen.cpp`/`.gen.h` file
  rather than the original `.cx` source.
- `@register`-tagged classes are spawned with `scene.CreateEntity().Add<T>()`
  — `T` must be default-constructible, the same requirement
  `Scene::Spawn` places on an archetype's components.
- Only one `extends` base is supported per class (C++ multiple inheritance
  from more than one `extends` clause is not modeled); `implements` accepts
  any number of comma-separated interfaces.
- Annotation bodies are intentionally opaque: `concordc` does not type-check
  Vulkan C++, compile GLSL/HLSL, reflect descriptor layouts, or verify that a
  declared pipeline/resource annotation matches a shader. Use the generated
  manifest as the stable input to your own build/reflection tooling.
- An annotation body is attached to one top-level item or emitted as a sidecar;
  it cannot currently be placed inside a captured C++ method body. Put a
  method-level attribute in ordinary C++ syntax when the target compiler
  supports one.
