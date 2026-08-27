// Concord/SDL3.h - SDL3 wrapper.
// Requires: headers in src/3rd/SDL3, lib/SDL3.lib and lib/SDL3.dll.
#pragma once

#include <Concord/Core.h>

#include <SDL3/SDL.h>

namespace concord::sdl {

// Last SDL error as a C string (SDL3 functions return plain bool / int).
[[nodiscard]] inline const char* error_string() noexcept { return SDL_GetError(); }

} // namespace concord::sdl
