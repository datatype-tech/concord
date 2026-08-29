// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CRender.h"

#include <string>
#include <stdexcept>
#include <vector>

namespace {

struct State {
    std::vector<std::string> calls;
    std::uint32_t observedFrame = 0;
    bool contextValid = false;
};

bool Early(const Concord::VulkanPassContext& context, void* userData)
{
    auto& state = *static_cast<State*>(userData);
    state.calls.push_back("early");
    state.observedFrame = context.frameIndex;
    return context.phase == Concord::VulkanPassPhase::BeforeScene;
}

bool Late(const Concord::VulkanPassContext& context, void* userData)
{
    auto& state = *static_cast<State*>(userData);
    state.calls.push_back("late");
    return context.phase == Concord::VulkanPassPhase::AfterScene;
}

bool Failing(const Concord::VulkanPassContext&, void* userData)
{
    static_cast<State*>(userData)->calls.push_back("failing");
    return false;
}

bool Throwing(const Concord::VulkanPassContext&, void* userData)
{
    static_cast<State*>(userData)->calls.push_back("throwing");
    throw std::runtime_error("test callback");
}

bool Stable(const Concord::VulkanPassContext&, void* userData)
{
    static_cast<State*>(userData)->calls.push_back("stable");
    return true;
}

bool ContextCheck(const Concord::VulkanPassContext& context, void* userData)
{
    auto& state = *static_cast<State*>(userData);
    state.contextValid = context.version == Concord::VulkanPassAbiVersion &&
                         context.structSize == sizeof(Concord::VulkanPassContext) &&
                         context.phase == Concord::VulkanPassPhase::AfterScene &&
                         context.commandBufferRecording == 1 && context.commandBuffer != 0;
    return state.contextValid;
}

} // namespace

int main()
{
    Concord::ClearVulkanPasses();
    State state;
    if (!Concord::RegisterVulkanPass({
            .name = "late",
            .phase = Concord::VulkanPassPhase::AfterScene,
            .order = 20,
            .callback = Late,
            .userData = &state}) ||
        !Concord::RegisterVulkanPass({
            .name = "early",
            .phase = Concord::VulkanPassPhase::BeforeScene,
            .order = 10,
            .callback = Early,
            .userData = &state}) ||
        Concord::RegisterVulkanPass({
            .name = "early",
            .callback = Early,
            .userData = &state}) ||
        Concord::RegisterVulkanPass({
            .name = "INVALID_PHASE",
            .phase = static_cast<Concord::VulkanPassPhase>(99),
            .callback = Early,
            .userData = &state})) {
        return 1;
    }

    Concord::VulkanPassContext context{};
    context.frameIndex = 7;
    if (!Concord::RunVulkanPasses(Concord::VulkanPassPhase::BeforeScene, context) ||
        state.calls != std::vector<std::string>{"early"} || state.observedFrame != 7) {
        return 1;
    }
    if (!Concord::RunVulkanPasses(Concord::VulkanPassPhase::AfterScene, context) ||
        state.calls != std::vector<std::string>{"early", "late"}) {
        return 1;
    }
    if (!Concord::UnregisterVulkanPass("EARLY") ||
        Concord::UnregisterVulkanPass("early") ||
        Concord::UnregisterVulkanPass("missing")) {
        return 1;
    }

    Concord::ClearVulkanPasses();
    state.calls.clear();
    if (!Concord::RegisterVulkanPass({
            .name = "failing", .phase = Concord::VulkanPassPhase::AfterScene,
            .callback = Failing, .userData = &state}) ||
        !Concord::RegisterVulkanPass({
            .name = "throwing", .phase = Concord::VulkanPassPhase::AfterScene,
            .callback = Throwing, .userData = &state}) ||
        !Concord::RegisterVulkanPass({
            .name = "stable_a", .phase = Concord::VulkanPassPhase::AfterScene,
            .order = 10, .callback = Stable, .userData = &state}) ||
        !Concord::RegisterVulkanPass({
            .name = "stable_b", .phase = Concord::VulkanPassPhase::AfterScene,
            .order = 10, .callback = Stable, .userData = &state})) {
        return 1;
    }
    context.commandBufferRecording = 1;
    context.commandBuffer = 1;
    context.structSize = sizeof(Concord::VulkanPassContext);
    context.version = Concord::VulkanPassAbiVersion;
    if (Concord::RunVulkanPasses(Concord::VulkanPassPhase::AfterScene, context) ||
        state.calls != std::vector<std::string>{"failing", "throwing", "stable", "stable"}) {
        return 1;
    }
    if (Concord::RunVulkanPasses(
            static_cast<Concord::VulkanPassPhase>(99), context)) {
        return 1;
    }
    if (!Concord::UnregisterVulkanPass("failing") ||
        !Concord::UnregisterVulkanPass("throwing")) {
        return 1;
    }
    state.calls.clear();
    if (!Concord::RegisterVulkanPass({
            .name = "context", .phase = Concord::VulkanPassPhase::AfterScene,
            .order = 20, .callback = ContextCheck, .userData = &state}) ||
        !Concord::RunVulkanPasses(Concord::VulkanPassPhase::AfterScene, context) ||
        !state.contextValid) {
        return 1;
    }
    Concord::ClearVulkanPasses();
    return 0;
}
