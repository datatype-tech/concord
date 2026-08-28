// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/app/Game.h"

#include "engine/app/GameState.h"
#include "engine/render/RenderBackendFactory.h"
#include "engine/window/Window.h"

#include <cstdio>
#include <exception>
#include <utility>

namespace Concord {

void Game::AttachWindow(Window& window)
{
    DetachWindow();

    if (!window.Open(m_impl->config.enableRendering)) {
        std::fprintf(stderr, "[Concord] failed to open window\n");
        return;
    }
    m_impl->window = &window;

    if (!m_impl->config.enableRendering) {
        return;
    }

    std::unique_ptr<IRenderBackend> backend;
    const auto rollback = [&]() {
        std::exception_ptr failure;
        try {
            if (backend) {
                backend->Shutdown();
            }
        } catch (...) {
            failure = std::current_exception();
        }
        try {
            DetachWindow();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
        if (failure) {
            std::rethrow_exception(failure);
        }
    };

    try {
        backend = CreateRenderBackend();
        if (!backend) {
            std::fprintf(stderr, "[Concord] no render backend is registered; "
                               "link the engine's render DLL to enable rendering\n");
        } else {
#if defined(NDEBUG)
            const bool validation = false;
#else
            const bool validation = m_impl->config.enableValidation;
#endif

            if (backend->Init(window, validation)) {
                m_impl->renderer = std::move(backend);
                return;
            }
            std::fprintf(stderr, "[Concord] renderer initialization failed\n");
        }
    } catch (...) {
        try {
            rollback();
        } catch (...) {
        }
        throw;
    }
    rollback();
}

void Game::DetachWindow()
{
    m_impl->quitRequested = true;
    std::unique_ptr<IRenderBackend> renderer = std::move(m_impl->renderer);
    Window* window = std::exchange(m_impl->window, nullptr);
    std::exception_ptr failure;
    try {
        if (renderer) {
            renderer->Shutdown();
        }
    } catch (...) {
        failure = std::current_exception();
    }
    try {
        if (window) {
            window->Close();
        }
    } catch (...) {
        if (!failure) {
            failure = std::current_exception();
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace Concord
