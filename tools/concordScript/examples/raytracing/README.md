# ConcordScript ray-tracing showcase

This sample keeps the scene and animation logic in ConcordScript. Ray tracing
is implemented inside Concord's Render DLL: the application never includes
Vulkan headers, writes a shader, or records a ray-tracing pass.

At startup the renderer selects the built-in ray-generation pipeline when the
device exposes the required KHR features and the swapchain supports the output
composite. Otherwise it automatically uses the Forward+ raster path. The
scene and source code are identical in both cases.

From this directory, translate first:

```powershell
..\..\..\build\concordc.exe --project . --out generated
```

Then configure with the bundled engine shaders enabled (adjust the compiler path):

```powershell
cmake -S . -B build -G Ninja `
  -DCONCORD_BUILD_SHADERS=ON `
  -DCONCORD_SHADER_COMPILER=C:/msys64/ucrt64/bin/glslangValidator.exe
cmake --build build
./build/raytracing_example.exe
```
