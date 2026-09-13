// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The one translation unit in Render that compiles stb_image_write. Runtime
// has its own for stb_image (see src/engine/asset/StbImage.cpp); the two must
// stay in separate DLLs rather than sharing one, because a header-only library
// compiled into both would export the same symbols twice.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
