// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShaderModule.h"

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Concord {
namespace {

/** Returns the directory containing the running executable. */
std::filesystem::path ExecutableDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return length == 0 || length >= MAX_PATH ? std::filesystem::path{}
                                             : std::filesystem::path(buffer).parent_path();
}

/** Tries one path and validates its SPIR-V magic number. */
std::vector<u32> ReadCodeAt(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const std::streamoff size = file ? static_cast<std::streamoff>(file.tellg()) : 0;
    if (size < static_cast<std::streamoff>(sizeof(u32)) ||
        size % static_cast<std::streamoff>(sizeof(u32)) != 0) {
        return {};
    }
    std::vector<u32> code(static_cast<usize>(size / sizeof(u32)));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
    return file && code.front() == 0x07230203u ? code : std::vector<u32>{};
}

} // namespace

std::vector<u32> ReadVulkanShaderCode(const char* fileName)
{
    if (!fileName || *fileName == '\0') {
        return {};
    }
    std::vector<std::filesystem::path> paths;
    if (const char* directory = std::getenv("CONCORD_SHADER_DIR")) {
        paths.emplace_back(std::filesystem::path(directory) / fileName);
    }
    if (const std::filesystem::path directory = ExecutableDirectory(); !directory.empty()) {
        paths.emplace_back(directory / "Assets" / "Shaders" / fileName);
        paths.emplace_back(directory / "assets" / "shaders" / fileName);
    }
    paths.emplace_back(std::filesystem::path("Assets") / "Shaders" / fileName);
    paths.emplace_back(std::filesystem::path("assets") / "shaders" / fileName);
    for (const auto& path : paths) {
        if (std::vector<u32> code = ReadCodeAt(path); !code.empty()) {
            return code;
        }
    }
    return {};
}

VkShaderModule CreateVulkanShaderModule(const VulkanContext& context,
                                        const std::vector<u32>& code)
{
    if (context.device == VK_NULL_HANDLE || code.empty()) {
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size() * sizeof(u32);
    info.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    return vkCreateShaderModule(context.device, &info, nullptr, &module) == VK_SUCCESS
               ? module
               : VK_NULL_HANDLE;
}

} // namespace Concord
