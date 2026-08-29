// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ImageAsset.h"

#include <cstddef>
#include <string_view>

namespace {

constexpr std::string_view kPng =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAEUlEQVR4nGP4z8DQwPCf4T8ADn0Dfur2k8AAAAAASUVORK5CYII=";
constexpr std::string_view kJpeg =
    "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAABAAIDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDovBn/ACJ+hf8AXhB/6LWiiiv4mr/xZ+r/ADP4J4p/5H+P/wCv1X/0uR//2Q==";

} // namespace

int main()
{
    using namespace Concord;
    if (ImageAsset{}.IsValid()) return 1;
    const ImageLoadResult png = ImageLoader::LoadUri(kPng);
    if (!png.Succeeded() || png.image.width != 2 || png.image.height != 1 ||
        png.image.pixels.size() != 8 || png.image.pixels[0] != 255) {
        return 1;
    }
    const ImageLoadResult jpeg = ImageLoader::LoadUri(kJpeg);
    if (!jpeg.Succeeded() || jpeg.image.width != 2 || jpeg.image.height != 1 ||
        jpeg.image.pixels.size() != 8) {
        return 1;
    }
    ImageAssetCache cache;
    if (!cache.Ensure(kPng) || !cache.Ensure(kPng) || cache.entries.size() != 1 ||
        cache.Find(kPng) == nullptr || cache.Find("data:image/png;base64,bad") != nullptr) {
        return 1;
    }
    const ImageLoadResult bad = ImageLoader::LoadUri("data:image/png;base64,bad");
    return bad.Succeeded() || bad.error == ImageLoadError::None ? 1 : 0;
}
