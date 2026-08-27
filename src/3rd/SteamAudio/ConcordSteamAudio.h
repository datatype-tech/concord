// Concord/SteamAudio.h - Steam Audio (phonon) wrapper.
// Requires: src/3rd/SteamAudio/phonon.h, lib/phonon.lib and lib/phonon.dll.
#pragma once

#include <Concord/Core.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <phonon.h>
#ifdef __cplusplus
}
#endif

namespace concord::audio {

// IPLerror -> bool
[[nodiscard]] constexpr bool ok(IPLerror err) noexcept { return err == IPL_STATUS_SUCCESS; }

} // namespace concord::audio
