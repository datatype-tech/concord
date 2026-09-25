# AGENTS.md — Concord Flash Development Standards

> This document defines the development standards for the C++ engine core (`concord/`) and the host application code at the repository root (e.g. `main.cpp`). Its purpose is to keep the codebase industrial-grade, consistent, and maintainable as the engine grows. Read the relevant section before adding or modifying code.
>
> **Lineage:** Concord Flash is the second generation of [Concord](https://github.com/lattice-tech/concord) (first generation at `E:\Concord`). It keeps the first generation's public API syntax, but replaces the bgfx backend with a **native Forward+ Vulkan renderer**, targets **Windows only**, and deliberately trims the API surface (see §11).
>
> **Repository layout:** `concord/` is a fully self-contained project — headers, sources, vendored third-party code (`src/3rd/`) and prebuilt libraries (`lib/`) all live under it, so it can be dropped into any other project with `add_subdirectory(concord)`. The repository root holds only a thin host: a minimal `CMakeLists.txt` that registers `concord/` and an executable that consumes it, plus `main.cpp`. Never place engine sources, headers, or vendored dependencies outside `concord/`.
>
> **License:** Concord Flash is Mozilla Public License 2.0 (`concord/LICENSE`) — file-level copyleft: modifying an engine source file requires publishing that file's changes, but a game built with the engine stays fully closed-source. Every new source file under `concord/` must carry the Exhibit A notice from the license (see §16).

## 1. Guiding Principles

- **Engineering rigor over expedience.** Favor explicit, predictable, well-structured code over quick hacks, even during early prototyping.
- **Maintainability first.** Code should be easy for another engineer — or an AI agent — to read, extend, and safely modify later without extra context.
- **Consistency over local preference.** Follow the conventions below exactly; a uniform codebase is more valuable than any individual stylistic preference.

## 2. Namespace & Naming Conventions

| Subject | Rule | Example |
|---|---|---|
| Namespace | Top-level namespace is always `Concord::`; nested namespaces also use PascalCase | `Concord::Game`, `Concord::Object::Box` |
| Classes / structs / enums | PascalCase with **no** `C` prefix. A type name never carries the `C` marker — that marker lives only on public header *filenames* in `include/Concord/` (see §3) | `Game`, `GameConfig`, `VulkanRenderBackend` |
| Abstract interfaces (pure virtual base classes) | PascalCase with an `I` prefix | `IRenderBackend` |
| Header / source file names | PascalCase, matching the primary type they define. Public forwarding headers in `include/Concord/` additionally take a `C` prefix — that is the **only** place the `C` prefix ever appears | `CApplication.h` (public) vs `Game.h` / `Game.cpp` (module) |
| Folder names | camelCase — with one deliberate exception, `include/Concord/` (see §3) | `engine`, `render`, `window` |
| Functions (methods & free functions) | PascalCase — **every** function, no exceptions (mirroring Unreal-style engine APIs). Applies uniformly to member functions, public free functions, and internal file-local helpers | `Init()`, `Run()`, `Shutdown()`, `AttachWindow()`, `Spawn()` |
| Local variables & parameters | camelCase | `keepOpen`, `configPath` |
| Private member variables | `m_` prefix + camelCase | `m_context` |
| Export macros | `<MODULE>_API` / `<MODULE>_EXPORTS`, defined centrally in `CExport.h` | `CENGINE_API` |
| Include guards | `CONCORD_<FILE_NAME>_H` (no `#pragma once`, for consistency with existing headers) | `CONCORD_CAPPLICATION_H` |

## 3. Header Placement

- All header files live under `concord/include/` — never colocated with `.cpp` files in `src/`.
- Public API headers, meant to be consumed by other modules or the application layer, live in `concord/include/Concord/`. This is the single folder that breaks the camelCase rule: its first letter is capitalized because it mirrors the `Concord::` namespace it represents.
- Module-private headers (internal-only, not part of the public API) live in `concord/include/<module>/`, matching the module's `src/<module>/` folder — never inside `Concord/`, to keep the public API surface clean.
- **Organize a module into sub-areas — don't pile files flat.** A module folder (e.g. `include/engine/`) must not accumulate a flat heap of unrelated headers. As soon as it holds more than a handful of files, group them into sub-directories by concern (e.g. `include/engine/render/`, `include/engine/window/`, `include/engine/scene/`). Be liberal about adding a new sub-area rather than dropping yet another file into the module root.
  - **`include/<module>/` and `src/<module>/` mirror each other one-to-one.** Every source sub-area has the same relative path on both sides: `include/engine/render/VulkanRenderBackend.h` pairs with `src/engine/render/VulkanRenderBackend.cpp`. A reader must be able to derive one path from the other by swapping `include`↔`src` and `.h`↔`.cpp`.
  - Includes always spell the full relative path from `include/`, e.g. `#include "engine/render/IRenderBackend.h"`. Never rely on the compiler finding a header by a shorter path.
  - Public facade headers under `include/Concord/` stay flat (they are the flat, curated API surface) and simply forward into whatever nested module sub-area now holds the real declaration.
- **Facade re-export pattern (mandatory for new headers).** The real class/struct/function declarations for a feature live in its module-private header(s) under `include/<module>/` (PascalCase names, no `C` prefix). The matching public header under `include/Concord/` is `C`-prefixed and contains **only** `#include` directives that pull those module-private headers back in — it never declares anything itself. Consumers always `#include <Concord/CXxx.h>` and never reach into `include/<module>/` directly. `CExport.h` is a deliberate exception: it is the central home of the export macros (§2), not a type facade, so it defines those macros directly rather than forwarding.
- `.cpp` include order: own public header → same-module internal headers → third-party headers → C++ standard library headers, with a blank line between groups.

## 4. Adding a New Engine Module

1. Add the module's `.cpp` sources under `concord/src/<module>/` (camelCase folder), grouped into sub-areas by concern once there is more than a handful (see §3).
2. Put the real declaration in a module-private header `<ClassName>.h` under `concord/include/<module>/` (PascalCase, no `C` prefix), in the sub-area that mirrors the source's location, inside `namespace Concord { ... }`.
3. Add a public forwarding header `C<Name>.h` under `concord/include/Concord/` that contains **only** `#include` directives pulling in the module-private header(s).
4. Add a matching `<MODULE>_EXPORTS` / `<MODULE>_API` macro pair to `CExport.h`.
5. Register the sources in `concord/CMakeLists.txt`.
6. Consumers `#include <Concord/C<Name>.h>` and link against the aggregate `concord` target.

## 5. File Splitting & Reusability

- **Single responsibility.** One header/source pair should hold one class (or one tightly related group of free functions). Keep files small, and split by responsibility as soon as a file starts accumulating unrelated concerns.
- **No god classes.** Build complex subsystems by composing several small, well-defined components rather than stacking every capability onto one class.
- **Clear module boundaries.** Modules interact only through the public headers in `include/Concord/`; never reach into another module's `src/` implementation details or private headers.
- **Design for reuse.** Generic capabilities (math, logging, containers, etc.) should be implemented as small, module-agnostic components. If no existing module is the right home, add a small dedicated module rather than bolting the capability onto an unrelated one.
- **No circular dependencies.** Dependency direction is strictly one-way: `core ← window ← scene ← render ← app`. If module A needs functionality from module B, decouple through an interface or callback — B must never include A back.
- **On-demand subsystem activation.** Lifecycle/facade classes that own multiple subsystems take a small config struct and construct/initialize each subsystem only when its flag is enabled.
- **Vulkan stays behind pimpl.** No public header may transitively include `vulkan.h` or `SDL3/SDL.h`. Types that own graphics or platform state hide it behind `std::unique_ptr<Impl>`, so application code including `<Concord/CApplication.h>` never pays for those headers.
- **Render backend lifecycle is ordered.** Backends follow `Init()` → zero or more `BeginFrame()` / `EndFrame()` calls → `Shutdown()`. Swapchain recreation on resize is the backend's own responsibility and must not leak to callers.

## 6. Comment Style

- **Documentation comments are Javadoc/Doxygen-style block comments only** (`/** ... */`, using `@brief`, `@param`, `@return`, etc. where useful). Every public class, struct, and non-trivial member function gets one directly above its declaration.
- Comments explain **intent, trade-offs, or constraints** the code can't express by itself — never restate what the next line obviously does.
- **No ASCII banners or separators** (e.g. `// ====`, `// ----`) to visually divide sections of a file. If a file feels like it needs visual sections, split it instead (see §5).
- **No changelog-style comments** in code (e.g. `// added`, `// fixed`). History belongs in commit messages / version control, never inline comments.

## 7. Third-Party Code

- Vendored third-party headers live under `src/3rd/<library>/`; precompiled libraries/DLLs live under `lib/`. Both keep their original upstream naming and are exempt from the conventions above.
- Currently vendored: SDL3 3.4.14 (windowing and input only — never rendering), Vulkan 1.4.357 headers, Steam Audio 4.8.1, Jolt Physics 5.6.0 (CPU simulation only — see §14), stb.
- `setup_deps.ps1` at the repository root installs and refreshes all of them; it is idempotent.

## 8. Repository & Commit Standards

- **Repository contents.** Do not add IDE settings, CMake build directories, temporary shader outputs, logs, or other local artifacts to commits.
- **Quality before commit.** Review the staged diff for unrelated edits, preserve existing line endings and third-party sources, and run the smallest relevant build or verification step. Record any verification gap in the commit body instead of implying that an unchecked change passed.
- **Commit language.** Commit subjects and bodies are written in English. Use specific engineering terms that describe the behavioral change, such as `render: add clustered light culling` or `scene: validate serialized node counts`; avoid generic wording such as `update`, `change`, `fix`, or `misc`.
- **Commit scope.** One commit represents one independently reviewable change.

## 9. Public API Syntax Contract

Application code only ever writes this shape — identical in spirit to the first generation:

```cpp
#include <Concord/CApplication.h>
#include <Concord/CObject.h>
#include <Concord/CScene.h>

int main()
{
    Concord::Game game;
    Concord::Window window({.title = "My Game", .resolution = {1280, 720}});
    game.AttachWindow(window);

    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({
        .position = {0.0f, 2.0f, -5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
    });
    scene.Spawn<Concord::Object::Box>({
        .transform = {.position = {0.0f, 1.0f, 0.0f}},
        .material  = {.albedo = COLOR_RGB(224, 64, 64)},
    });

    game.LoadScene(scene);
    game.Run();
}
```

Four rules that never bend:

1. **Desc aggregates + designated initializers.** Every spawnable type has a matching `XxxDesc` plain aggregate whose every field carries a default, so callers name only what they care about.
2. **`Spawn<T>(desc)`** is how archetypes come into being; it returns a chainable `EntityHandle` over the entity it created (§10).
3. **One namespace, one level of nesting.** `Concord::`, with at most `Concord::Object::`-style categories beneath it.
4. **Consumers include only `<Concord/C*.h>`.**

## 10. Entity-Component-System Model

A Scene **is** a component store. The object-oriented API and the
data-oriented API are two views of that one store, never two copies — a
change made through either is immediately visible to the other. This is the
central departure from the first generation, where `Scene::Spawn` and
`Ecs::World` were parallel systems holding separate state.

### The three layers

| Layer | Type | Role |
|---|---|---|
| Data | `World` | Sparse-set component storage keyed by `Entity` |
| Object view | `Scene::Spawn<T>` → `EntityHandle` | Archetype recipes and chainable per-entity access |
| Behaviour | `ISystem` / `SystemSchedule` | Bulk logic over queried components, ticked per frame |

### Archetypes are recipes, not containers

A spawnable type such as `Object::Box` holds **no state**. It declares
`using Desc = BoxDesc;` and a static `Build(World&, Entity, const Desc&)`
that attaches the components constituting that archetype. Adding a new
archetype means writing those two members — nothing registers with the
renderer or the scene.

```cpp
struct Box {
    using Desc = BoxDesc;

    static void Build(World& world, Entity entity, const BoxDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<MeshRenderer>(entity, MeshRenderer{.size = desc.size});
        world.Add<Material>(entity, desc.material);
    }
};
```

### Both views, one store

```cpp
// Object view: spawn an archetype, then chain a custom component onto it.
scene.Spawn<Object::Box>({.material = {.albedo = COLOR_RGB(224, 64, 64)}})
     .Add<Spin>({.degreesPerSecond = 60.0f});

// Data view: compose an entity component by component, no archetype at all.
scene.CreateEntity()
     .Add<Transform>({.position = {4.0f, 1.5f, 1.0f}})
     .Add<MeshRenderer>({});

// One query sees both.
scene.Query<Transform, MeshRenderer>([](Entity, Transform& t, MeshRenderer& m) { ... });
```

### Rules

- **Components are plain aggregates.** No inheritance, no virtuals, no
  behaviour — behaviour belongs to systems.
- **Name the rarest component first** in `Query<A, B, C>`: iteration is
  driven by the first type's dense array and the rest are membership tests.
- **Never create or destroy entities inside a query callback.** Collect the
  handles and act after the loop; structural change invalidates the dense
  arrays being walked.
- **Component references are borrowed, not owned.** A `T*` from `Get` is
  invalidated by the next structural change to that component type.
- **Generation counters make staleness safe.** A handle to a destroyed
  entity resolves to `nullptr` rather than to the slot's new occupant.
- **`World` is single-threaded by design.** The first generation locked a
  mutex per call, which cost more than it bought; parallelism will arrive as
  an explicit scheduler, not as per-operation locks.

## 11. Second-Generation Simplifications

Deliberate departures from the first generation, all in service of "less ceremony":

| First generation | Concord Flash |
|---|---|
| `scene.Spawn<Object::Box>(Object::BoxDesc{...})` repeats the type name | `scene.Spawn<Object::Box>({...})` — `T::Desc` is deduced |
| `.material = {.surface = {.albedo = ...}}` nests twice | `.material = {.albedo = ...}` — flattened to one level |
| 30+ `C*.h` facade headers, some for empty modules | Start with the few that are real; add one only when its module exists |
| Separate Desc + Node type for every primitive up front | Only the primitives actually implemented |
| `Concord::Sleep(20000)` to keep the window alive | `game.Run()` — a real main loop |
| bgfx static library, per-module DLL split (`CEngine.dll`, `CAudio.dll`, ...) | Native Vulkan backend, two DLLs split by coupling, not by module (§12) |
| `Scene::Spawn` and `Ecs::World` hold separate state | Scene **is** the World; both APIs share one store (§10) |
| Entities are typed node objects owning their data | Entities are handles; archetypes are component recipes |

## 12. Two-DLL Packaging

The engine builds as two DLLs rather than the single aggregate target
described in earlier drafts of this document, mirroring the first
generation's per-module DLL split (`CEngine.dll`, `CAudio.dll`, ...) at a
coarser grain:

| DLL | Contents | Depends on |
|---|---|---|
| `ConcordFlashGameEngineRuntime.dll` | `app`, `ecs`, `scene`, `window`, `physics`, `audio` | SDL3, Jolt (static), Steam Audio (`phonon.dll`, loaded at runtime) |
| `ConcordFlashGameEngineRender.dll` | all of `render/`, including every `render/vulkan/` unit | Runtime (for `Window`), SDL3, Vulkan |

The dependency between them runs one way only. `Game` (Runtime) needs a
renderer, but Runtime must never name `VulkanRenderBackend` or include
anything under `vulkan.h` — doing so would make Runtime depend on Render
while Render already depends on Runtime for `Window`, forming a cycle.

- **The factory, not a direct constructor, is how Runtime gets a backend.**
  `RenderBackendFactory.h` declares a registration slot
  (`RegisterRenderBackend`) and an accessor (`CreateRenderBackend`) that
  `Game` calls; Render.dll is the only unit that ever calls
  `RegisterRenderBackend`.
- **Linking a backend in is a forced link anchor, not application code.**
  Render.dll self-registers via a static initializer in
  `VulkanBackendRegistration.cpp` the moment it is loaded; a zero-argument
  `extern "C" CRENDER_API` anchor function in the same file, force-resolved
  by `target_link_options` on `concord::render`, is what makes the Windows
  loader actually pull the DLL in. Application code never calls anything to
  wire up rendering.
- **A second export macro pair, `CRENDER_API`/`CRENDER_EXPORTS`, lives
  beside `CENGINE_API`/`CENGINE_EXPORTS` in `CExport.h`.** Use
  `CENGINE_API` for anything Runtime.dll exports, `CRENDER_API` for the
  handful of symbols Render.dll exports on its own.
- **`IRenderBackend` is exported `CENGINE_API`** even though its only
  implementation lives in Render.dll: a vtable crossing a DLL boundary needs
  consistent visibility on both sides.
- Every DLL's `OUTPUT_NAME` is set explicitly in `concord/CMakeLists.txt`
  and its MinGW `lib` prefix is stripped (`PREFIX ""`), so the artifact
  name matches exactly across MSVC and MinGW builds.
- `concord::concord` remains available as an `INTERFACE` target linking
  both DLLs, for consumers that don't need to reason about the split.

## 13. Vulkan Backend Conventions

- Target architecture is **Forward+**: depth pre-pass → tiled light culling → forward shading. The foundation stage lands instance/device/surface/swapchain/frame synchronization and a clear pass; the pipeline and light culling come next.
- Validation layers (`VK_LAYER_KHRONOS_validation`) are enabled in Debug builds and compiled out in Release.
- `MAX_FRAMES_IN_FLIGHT` is 2; each frame in flight owns its own command buffer, semaphores, and fence.
- SDL3 provides the surface via `SDL_Vulkan_CreateSurface` and nothing else — it never participates in rendering.
- The `vulkan-1.lib` import library lives in `lib/`; the loader DLL ships with the GPU driver.

## 14. Physics Conventions

- Simulation is **Jolt Physics 5.6**, compiled as a static library and linked
  privately into Runtime. Public headers must never include `<Jolt/Jolt.h>`
  or name a Jolt type — `PhysicsWorld` hides the solver behind
  `std::unique_ptr<Impl>`.
- **`JPH_USE_VK` / `JPH_USE_DX12` / `JPH_USE_MTL` stay OFF.** Jolt 5.6 can
  build a GPU compute path that would pull `vulkan.h` into Runtime and
  break the two-DLL split (§12).
- Components are `Collider`, `RigidBody` and `CharacterMotor` — plain
  aggregates, no behaviour. `PhysicsSystem` owns the world and is the only
  unit that steps it.
- Archetypes: `Object::StaticBody` (invisible volume), `Object::DynamicBox`,
  `Object::DynamicSphere`. Size is the same full-extent number
  `MeshRenderer` uses; sphere radius is `0.5 * size.x`.
- Dynamic bodies write pose and velocity back onto `Transform` /
  `RigidBody` each tick. Static and kinematic bodies read `Transform` and
  push it into Jolt.
- `FirstPersonController` with `flyMode = false` writes
  `CharacterMotor::wishVelocity`; the system steps a Jolt
  `CharacterVirtual`. Fly mode never touches the motor.
- CVM reaches the same world through `PhysicsWorld::Find` — `concord_raycast`
  hits Jolt when a `PhysicsSystem` is running, and falls back to AABB
  otherwise. Scripts enable the solver with `concord_add_physics_system`.

## 15. Audio Conventions

- Spatial mix is **Steam Audio 4.8.1** (CPU HRTF) plus an equal-power stereo
  pan fallback. `phonon.dll` is loaded with `LoadLibraryW` / `GetProcAddress`;
  do **not** link `phonon.lib` from MinGW. A missing DLL or HRTF is not fatal.
- Public headers must never include `<phonon.h>` or `SDL3/SDL.h` —
  `AudioWorld` hides both behind `std::unique_ptr<Impl>`.
- The mix is stereo float at 48 kHz. SDL3 may open a playback device
  (`AudioSettings::playOnDevice`); tests turn that off and call `MixStereo`.
- HRTF (`iplBinauralEffectApply`) runs only when the mix chunk size equals
  `frameSize` (default 512). Other sizes pan.
- Components are `AudioListener` and `AudioSource` — plain aggregates.
  `AudioSystem` owns the world and is the only unit that steps it.
- Archetypes: `Object::Listener`, `Object::Sound`. A missing listener uses
  the scene's main camera. Steam Audio axes match Concord: +Y up, ahead is
  the camera's local −Z.
- CVM reaches the same mix through `AudioWorld::Find`. Scripts enable it with
  `concord_add_audio_system` and spawn voices with `concord_spawn_sound`.

## 16. Licensing

- Concord Flash is distributed under the Mozilla Public License 2.0. The
  full text lives at `concord/LICENSE`; nowhere else in the repository
  duplicates it.
- New source files under `concord/` (both `include/` and `src/`) should
  carry the Exhibit A notice near the top, above the include guard:
  ```cpp
  // This Source Code Form is subject to the terms of the Mozilla Public
  // License, v. 2.0. If a copy of the MPL was not distributed with this
  // file, You can obtain one at http://mozilla.org/MPL/2.0/.
  ```
- Vendored third-party code under `src/3rd/` keeps its own upstream
  license; MPL-2.0 governs Concord Flash's own source only.
- `bin/`, the output of `package_bin.ps1`, is a build artifact like
  `build/` — never committed.

## 17. Pre-Commit Checklist

- [ ] New headers are under `include/Concord/` or `include/<module>/`, never alongside `.cpp` files
- [ ] A module's headers/sources are grouped into mirrored sub-areas (`include/<module>/<area>/` ↔ `src/<module>/<area>/`), not piled flat (§3)
- [ ] New public headers under `include/Concord/` contain only `#include` directives (facade pattern, §3)
- [ ] Type names are PascalCase with no `C` prefix (interfaces use `I`); the `C` prefix appears only on public header *filenames* under `include/Concord/`; folder names are camelCase (except `include/Concord/`)
- [ ] Every function is PascalCase — methods, free functions and internal helpers alike (§2); only local variables, parameters and `m_` members stay camelCase
- [ ] Include guards follow `CONCORD_<NAME>_H`; no `#pragma once`
- [ ] No public header transitively includes `vulkan.h`, `SDL3/SDL.h` (§5), `<Jolt/Jolt.h>` (§14), or `<phonon.h>` (§15)
- [ ] Documentation comments use Javadoc-style block comments; no ASCII banner separators or changelog-style notes (§6)
- [ ] New components are plain aggregates with no behaviour; logic lives in a system (§10)
- [ ] New archetypes expose `using Desc = ...` plus a static `Build()` and store no state of their own (§10)
- [ ] No query callback creates or destroys entities (§10)
- [ ] Runtime.dll sources never name `VulkanRenderBackend` or include `vulkan.h`; a new render backend registers through `RenderBackendFactory` instead (§12)
- [ ] New source files under `concord/` carry the MPL-2.0 Exhibit A notice above the include guard (§16)
- [ ] Physics stays in Runtime; Jolt GPU backends stay disabled; a new collider is a component or archetype, never a Jolt type in a public header (§14)
- [ ] Audio stays in Runtime; `phonon.dll` is loaded dynamically; a new voice is a component or archetype, never a Steam Audio type in a public header (§15)