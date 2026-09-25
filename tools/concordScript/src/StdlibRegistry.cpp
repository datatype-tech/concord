// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "StdlibRegistry.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ConcordScript {
namespace {

struct StdPackage {
    const char* target;
    const char* header;
};

constexpr StdPackage kPackages[] = {
    {"std.algorithm", "algorithm"},
    {"std.array", "array"},
    {"std.cmath", "cmath"},
    {"std.filesystem", "filesystem"},
    {"std.functional", "functional"},
    {"std.memory", "memory"},
    {"std.optional", "optional"},
    {"std.span", "span"},
    {"std.string", "string"},
    {"std.string_view", "string_view"},
    {"std.unordered_map", "unordered_map"},
    {"std.utility", "utility"},
    {"std.vector", "vector"},
};

std::size_t EditDistance(std::string_view left, std::string_view right)
{
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t column = 0; column <= right.size(); ++column) previous[column] = column;
    for (std::size_t row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (std::size_t column = 1; column <= right.size(); ++column) {
            const std::size_t substitution =
                previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);
            current[column] = std::min({previous[column] + 1, current[column - 1] + 1, substitution});
        }
        previous.swap(current);
    }
    return previous[right.size()];
}

} // namespace

std::string_view StdHeaderForPackage(std::string_view target)
{
    for (const StdPackage& package : kPackages) {
        if (target == package.target) return package.header;
    }
    return {};
}

std::string SuggestStdPackage(std::string_view target)
{
    std::string best;
    std::size_t bestDistance = 3;
    for (const StdPackage& package : kPackages) {
        const std::size_t distance = EditDistance(target, package.target);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = package.target;
        }
    }
    return best;
}

} // namespace ConcordScript
