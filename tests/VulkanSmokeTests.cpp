// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CApplication.h"
#include "Concord/CCamera.h"
#include "Concord/CObject.h"
#include "Concord/CScene.h"

#include <vulkan/vulkan.h>

#include <cstring>
#include <vector>

namespace {

/** Returns whether the validation layer required by this test is installed. */
bool HasValidationLayer()
{
    Concord::u32 count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkLayerProperties> layers(count);
    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
        return false;
    }
    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
#if defined(NDEBUG)
    return 77;
#else
    if (!HasValidationLayer()) {
        return 77;
    }

    Concord::Game game({.enableRendering = true, .enableValidation = true});
    Concord::Window window({
        .title = "Concord Vulkan smoke",
        .resolution = {.width = 320, .height = 200},
        .visible = false,
        .vsync = false,
    });
    game.AttachWindow(window);
    if (!window.IsOpen()) {
        return 77;
    }

    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({
        .position = {0.0f, 1.0f, -4.0f},
        .target = {0.0f, 0.0f, 0.0f},
    });
    scene.Spawn<Concord::Object::Box>({
        .transform = {.position = {0.0f, 0.0f, 0.0f}},
    });
    game.LoadScene(scene);
    game.OnUpdate([&](Concord::f32) {
        if (game.FrameCount() == 1) {
            window.Set({
                .title = "Concord Vulkan smoke resized",
                .resolution = {.width = 400, .height = 240},
                .visible = false,
                .vsync = false,
            });
        }
        if (game.FrameCount() >= 3) {
            game.Quit();
        }
    });
    game.Run();
    return game.FrameCount() >= 4 ? 0 : 1;
#endif
}
