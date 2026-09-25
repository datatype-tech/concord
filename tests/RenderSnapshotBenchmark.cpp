// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CScene.h"
#include "engine/render/RenderSceneSnapshot.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

/** Compares identical extraction work with fresh and retained CPU storage. */
int main()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({});
    for (int index = 0; index < 4096; ++index) {
        scene.Spawn<Concord::Object::Box>({.transform = {.position = {float(index), 0, 0}}});
    }
    Concord::RenderSceneSnapshot retained;
    Concord::ExtractRenderScene(scene, 1.0f, retained);
    std::array<double, 7> freshTimes{}, retainedTimes{};
    constexpr int iterations = 120;
    Concord::usize consumed = 0;
    for (int round = 0; round < 7; ++round) {
        for (int pass = 0; pass < 2; ++pass) {
            const bool reuse = (round + pass) % 2 != 0;
            const auto start = std::chrono::steady_clock::now();
            for (int index = 0; index < iterations; ++index) {
                if (reuse) {
                    Concord::ExtractRenderScene(scene, 1.0f, retained);
                    consumed += retained.objects.size();
                } else {
                    const auto fresh = Concord::ExtractRenderScene(scene, 1.0f);
                    consumed += fresh.objects.size();
                }
            }
            const double micros = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count() / iterations;
            (reuse ? retainedTimes : freshTimes)[round] = micros;
        }
    }
    std::sort(freshTimes.begin(), freshTimes.end());
    std::sort(retainedTimes.begin(), retainedTimes.end());
    std::printf("4096 boxes, median of 7 x %d frames: fresh %.2f us, reused %.2f us, %.1f%% reduction\n",
                iterations, freshTimes[3], retainedTimes[3],
                100.0 * (1.0 - retainedTimes[3] / freshTimes[3]));
    return consumed == Concord::usize(4096) * iterations * 14 ? 0 : 1;
}
