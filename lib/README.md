# lib/

Prebuilt dynamic/static libraries used by Concord.

| Library      | Files expected here                 | Source                                            |
|--------------|-------------------------------------|---------------------------------------------------|
| SDL3         | SDL3.lib / libSDL3.dll.a (import), SDL3.dll (runtime) | https://github.com/libsdl-org/SDL/releases (SDL3-devel-*-VC.zip or *-mingw.zip) |
| Steam Audio  | phonon.lib (import), phonon.dll (runtime) | https://github.com/ValveSoftware/steam-audio/releases (steamaudio_4.x.zip -> lib/windows-x64) |
| Vulkan       | vulkan-1.lib (import lib only)      | https://vulkan.lunarg.com/sdk/home (Vulkan SDK; vulkan-1.dll comes with GPU drivers) |

当前已安装：SDL3.dll + libSDL3.dll.a（SDL3 3.4.14 MinGW）、phonon.dll + phonon.lib（Steam Audio 4.8.1）。
vulkan-1.lib 尚未安装（需从 Vulkan SDK 复制），CMake 会跳过 Vulkan 链接并给出警告。

The `concord/CMakeLists.txt` imported targets are only created when the
corresponding file exists here, so the project configures fine while a
library is still missing.
