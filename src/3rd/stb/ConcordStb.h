// Concord/stb.h - stb single-file libraries (header-only).
//
// In exactly ONE translation unit, define CONCORD_STB_IMPLEMENTATION
// before including this header to compile the implementations:
//
//   #define CONCORD_STB_IMPLEMENTATION
//   #include <Concord/stb.h>
//
#pragma once

#ifdef CONCORD_STB_IMPLEMENTATION
#  define STB_IMAGE_IMPLEMENTATION
#  define STB_IMAGE_WRITE_IMPLEMENTATION
#  define STB_IMAGE_RESIZE2_IMPLEMENTATION
#  define STB_TRUETYPE_IMPLEMENTATION
#  define STB_RECT_PACK_IMPLEMENTATION
#endif

// NOTE: stb_rect_pack.h MUST come before stb_truetype.h so that
// stb_truetype.h skips its embedded copy of the rectangle packer
// (guarded by STB_RECT_PACK_VERSION).
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize2.h>
#include <stb_rect_pack.h>
#include <stb_truetype.h>
