// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RayTracingTextureSlots.h"

#include <iostream>
#include <string>

namespace {

using Concord::RayTracingTextureSlots;
using Concord::u32;

bool TestUnarmedTableResolvesToFallback()
{
    const RayTracingTextureSlots slots;
    return !slots.IsReady() && slots.Count() == 0 && slots.Find("a") == 0 &&
           Concord::RayTracingTextureSlotKey(slots, 0).empty();
}

bool TestResetReservesOnlyTheFallback()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    return slots.IsReady() && slots.Count() == 1 && slots.keys[0].empty() &&
           Concord::RayTracingTextureSlotKey(slots, 0).empty();
}

bool TestEmptyKeyAlwaysResolvesToFallback()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    return slots.Acquire("") == 0 && slots.Find("") == 0 && slots.Count() == 1;
}

bool TestAcquireRegistersAndDeduplicates()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    const u32 first = slots.Acquire("assets/wall.png");
    if (first != 1 || slots.Count() != 2) return false;
    // Asking again must hand back the same slot instead of growing the table.
    if (slots.Acquire("assets/wall.png") != first || slots.Count() != 2) return false;
    return slots.Find("assets/wall.png") == first;
}

bool TestDistinctKeysGetDistinctSlots()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    const u32 wall = slots.Acquire("wall");
    const u32 floor = slots.Acquire("floor");
    const u32 crate = slots.Acquire("crate");
    if (wall != 1 || floor != 2 || crate != 3 || slots.Count() != 4) return false;
    return slots.Find("floor") == floor && slots.Find("crate") == crate &&
           Concord::RayTracingTextureSlotKey(slots, crate) == "crate";
}

bool TestUnknownKeysFindNothing()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    (void)slots.Acquire("wall");
    return slots.Find("not-registered") == 0;
}

bool TestCapacityOverflowFallsBackWithoutGrowing()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    for (u32 index = 1; index < Concord::kMaxRayTracingTextureSlots; ++index) {
        if (slots.Acquire("tex" + std::to_string(index)) != index) return false;
    }
    if (slots.Count() != Concord::kMaxRayTracingTextureSlots) return false;
    // The table is full: the next key must degrade to the fallback rather
    // than silently claim a slot the shader array does not have.
    if (slots.Acquire("one-too-many") != 0) return false;
    if (slots.Count() != Concord::kMaxRayTracingTextureSlots) return false;
    // A key that is already registered still resolves at capacity.
    return slots.Find("tex17") == 17;
}

bool TestSlotKeyIsBoundsChecked()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    (void)slots.Acquire("wall");
    return Concord::RayTracingTextureSlotKey(slots, 1) == "wall" &&
           Concord::RayTracingTextureSlotKey(slots, 99).empty() &&
           Concord::RayTracingTextureSlotKey(slots, Concord::kMaxRayTracingTextureSlots).empty();
}

bool TestResetDiscardsEntriesButKeepsTheFallback()
{
    RayTracingTextureSlots slots;
    Concord::ResetRayTracingTextureSlots(slots);
    (void)slots.Acquire("wall");
    Concord::ResetRayTracingTextureSlots(slots);
    return slots.Count() == 1 && slots.Find("wall") == 0 && slots.keys[0].empty();
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"unarmed table resolves to the fallback", TestUnarmedTableResolvesToFallback},
        {"reset reserves only the fallback", TestResetReservesOnlyTheFallback},
        {"empty key always resolves to the fallback", TestEmptyKeyAlwaysResolvesToFallback},
        {"acquire registers and deduplicates", TestAcquireRegistersAndDeduplicates},
        {"distinct keys get distinct slots", TestDistinctKeysGetDistinctSlots},
        {"unknown keys find nothing", TestUnknownKeysFindNothing},
        {"capacity overflow falls back without growing", TestCapacityOverflowFallsBackWithoutGrowing},
        {"slot key is bounds checked", TestSlotKeyIsBoundsChecked},
        {"reset discards entries but keeps the fallback", TestResetDiscardsEntriesButKeepsTheFallback},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " texture slot cases passed\n";
    return failures == 0 ? 0 : 1;
}
