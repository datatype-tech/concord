// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTextureKey.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

using Concord::MakeVulkanTextureCacheKey;

std::string Key(const char* uri, const char* base)
{
    return MakeVulkanTextureCacheKey(uri, std::filesystem::path(base));
}

bool TestKeyIsDeterministic()
{
    return Key("tex.png", "assets") == Key("tex.png", "assets") &&
           Key("", "") == Key("", "");
}

bool TestBaseDirectoryParticipates()
{
    // Two assets may reference the same relative URI from different folders;
    // those are different textures and must not share a cache entry.
    return Key("tex.png", "assets/a") != Key("tex.png", "assets/b");
}

bool TestUriParticipates()
{
    return Key("a.png", "assets") != Key("b.png", "assets");
}

bool TestBaseDirectoryIsNormalized()
{
    // Baked paths reach here spelled differently for the same folder, so the
    // key has to fold them together or the cache would decode twice.
    return Key("tex.png", "assets/./sub/..") == Key("tex.png", "assets") &&
           Key("tex.png", "assets//sub") == Key("tex.png", "assets/sub");
}

bool TestSeparatorDisambiguatesRealisticInputs()
{
    // A path and a URI never contain a newline, and for such inputs the
    // separator makes the split unique: without it these two would collide.
    return Key("sub/tex.png", "assets") != Key("tex.png", "assets/sub");
}

bool TestSeparatorIsADelimiterNotAnEscape()
{
    // Documented limitation rather than a guarantee: the separator is a plain
    // delimiter, so two newline-containing halves can still be confused for a
    // different split. Real paths and URIs never contain one, and escaping
    // them would cost more than it protects.
    return Key("b\nc", "a") == Key("c", "a\nb");
}

bool TestEmptyBaseLeavesOnlyTheUri()
{
    const std::string empty = Key("tex.png", "");
    const std::string withBase = Key("tex.png", "assets");
    if (empty == withBase) return false;
    // Whatever the empty base folds to, the URI must still be recoverable from
    // the tail of the key.
    return empty.size() > std::string("tex.png").size() &&
           empty.compare(empty.size() - std::string("tex.png").size(), std::string::npos,
                         "tex.png") == 0;
}

bool TestEmptyUriStillKeys()
{
    // A material with no texture must still produce a stable, distinct key so
    // the slot table can treat it as "no texture" rather than as a collision.
    return Key("", "assets") != Key("", "other") && Key("", "assets") != Key("x", "assets");
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"key is deterministic", TestKeyIsDeterministic},
        {"base directory participates", TestBaseDirectoryParticipates},
        {"uri participates", TestUriParticipates},
        {"base directory is normalized", TestBaseDirectoryIsNormalized},
        {"separator disambiguates realistic inputs", TestSeparatorDisambiguatesRealisticInputs},
        {"separator is a delimiter not an escape", TestSeparatorIsADelimiterNotAnEscape},
        {"empty base leaves only the uri", TestEmptyBaseLeavesOnlyTheUri},
        {"empty uri still keys", TestEmptyUriStillKeys},
    };
    // The key header is deliberately dependency-free, so the test uses plain
    // unsigned rather than pulling in the engine type aliases.
    unsigned failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const unsigned total = static_cast<unsigned>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " texture key cases passed\n";
    return failures == 0 ? 0 : 1;
}
