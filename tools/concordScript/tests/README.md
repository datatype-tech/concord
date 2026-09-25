# ConcordScript regression fixtures

The CMake project registers `concordscript_annotation_fixtures` when
`BUILD_TESTING=ON` (the default from `include(CTest)`). The test invokes the
built `concordc` executable against small `.cx` projects and checks generated
C++, shader sidecars, JSON manifests, and the CMake integration fragment.

```sh
cmake -S script -B script/build -G Ninja
cmake --build script/build
ctest --test-dir script/build --output-on-failure
```

The positive fixture covers item-attached and stand-alone annotations,
argument nesting, header/source C++ and Vulkan blocks, `@pass` lifecycle
phases, every Vulkan ray-tracing stage alias, GLSL/HLSL and custom entry points,
inline `source=`/`code=` text, external shader references, and raw `@` text.
Negative fixtures cover duplicate arguments, invalid sections/stages, unsafe or
reserved paths, sidecar/name collisions, unterminated blocks, invalid pass
metadata, unknown or excess built-in arguments, conflicting shader stage
selectors, invalid include paths, and missing or duplicate `@entry` blocks.
Failed invocations are required to leave no generated files behind.

The fixtures intentionally do not require a GPU or a shader compiler. Compile
the generated shader list separately with `CONCORD_BUILD_SHADERS=ON` and a
supported compiler when validating a concrete graphics toolchain.

External shader paths in the positive fixture are intentionally not created:
the test verifies registration and exclusion from the inline list without
invoking `concordscript_attach_external_shaders`. A real project must provide
those files before calling that helper.
