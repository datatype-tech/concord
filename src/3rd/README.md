# src/3rd/

Third-party **headers** used by Concord. Each SDK lives in its own folder;
the folder itself is added to the include path:

| Folder         | Include path expected          | Source                                                   |
|----------------|--------------------------------|----------------------------------------------------------|
| SDL3/          | <SDL3/SDL.h> (SDL3/ subfolder) | https://github.com/libsdl-org/SDL/releases (SDL3-devel-*-VC.zip, copy include/* here) |
| SteamAudio/    | <phonon.h>                     | https://github.com/ValveSoftware/steam-audio/releases (copy include/phonon.h here) |
| Vulkan/        | <vulkan/vulkan.h>              | https://github.com/KhronosGroup/Vulkan-Headers/releases (copy include/* here) |
| stb/           | <stb_image.h> etc.             | https://github.com/nothings/stb (single headers)        |
