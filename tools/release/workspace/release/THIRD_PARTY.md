# Third-party components

Concord engine source is MPL-2.0 (`licenses/Concord-MPL-2.0.txt`); ConcordScript
is MIT (`licenses/ConcordScript-MIT.txt`). The corresponding first-party source
for these binaries is included in `source/`. Vendor headers retain their notices.

| Component | License / source |
|---|---|
| SDL 3.4.14 | zlib; https://github.com/libsdl-org/SDL/releases/tag/release-3.4.14 |
| Jolt 5.6.0 (statically linked) | MIT; https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0 |
| Steam Audio 4.8.1 | Apache-2.0; https://github.com/ValveSoftware/steam-audio/releases/tag/v4.8.1 |
| stb (vendored headers) | Public domain / MIT, notice in included header; https://github.com/nothings/stb |
| Vulkan headers 1.4.357 | Apache-2.0 / MIT; https://github.com/KhronosGroup/Vulkan-Headers |
| LLVM 22.1.3 (statically linked) | Apache-2.0 with LLVM exceptions; https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.3 |
| MSYS2 GCC and runtime dependencies | See bundled `licenses/msys2/`; https://packages.msys2.org/ |

The Windows Vulkan loader and OS DLLs are supplied by the system/graphics driver,
not redistributed. MSYS2 package versions are recorded in `licenses/msys2-packages.txt`;
package source recipes are available at https://github.com/msys2/MINGW-packages.
