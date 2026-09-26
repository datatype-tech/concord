// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CWindow.h"

#include "engine/window/WindowState.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class EventFixture {
public:
    EventFixture()
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        m_state.handle = SDL_CreateWindow("Concord window close tests", 320, 200,
                                         SDL_WINDOW_HIDDEN);
        if (!m_state.handle) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            throw std::runtime_error(SDL_GetError());
        }
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    }

    ~EventFixture()
    {
        SDL_DestroyWindow(m_state.handle);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    Concord::WindowState& State() { return m_state; }

    void Send(Uint32 type, SDL_WindowID windowId = 0)
    {
        SDL_Event event{};
        event.type = type;
        if (type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            event.window.windowID = windowId ? windowId : SDL_GetWindowID(m_state.handle);
        Require(SDL_PushEvent(&event), "Could not queue close event");
        Concord::PumpWindowEvents(m_state);
    }

private:
    Concord::WindowState m_state;
};

void CheckNativeCloseRequests()
{
    EventFixture fixture;
    auto& state = fixture.State();

    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(state.shouldClose, "Default native close must be accepted");
    state.shouldClose = false;
    fixture.Send(SDL_EVENT_QUIT);
    Require(state.shouldClose, "Default process quit must be accepted");

    state.shouldClose = false;
    int requests = 0;
    bool approve = false;
    state.closeRequestHandler = [&] { ++requests; return approve; };
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(requests == 1 && !state.shouldClose, "Native close cancellation was ignored");
    fixture.Send(SDL_EVENT_QUIT);
    Require(requests == 2 && !state.shouldClose, "Process quit cancellation was ignored");
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED, SDL_GetWindowID(state.handle) + 100);
    Require(requests == 2 && !state.shouldClose, "Another window requested our close");

    approve = true;
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(requests == 3 && state.shouldClose, "Native close approval was ignored");
    approve = false;
    fixture.Send(SDL_EVENT_QUIT);
    Require(requests == 3 && state.shouldClose, "Confirmed close was reconsidered");

    state.shouldClose = false;
    approve = true;
    fixture.Send(SDL_EVENT_QUIT);
    Require(requests == 4 && state.shouldClose, "Process quit approval was ignored");

    state.shouldClose = false;
    state.closeRequestHandler = [&] {
        state.closeRequestHandler = {};
        ++requests;
        return false;
    };
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(requests == 5 && !state.shouldClose, "Replacing a handler interrupted its call");
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(requests == 5 && state.shouldClose, "Clearing the handler did not restore default");

    state.shouldClose = false;
    state.closeRequestHandler = [&] { state.shouldClose = true; return false; };
    fixture.Send(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    Require(state.shouldClose, "An explicit confirmation inside a handler was discarded");
}

void CheckExplicitConfirmation()
{
    Concord::Window window;
    int requests = 0;
    window.SetCloseRequestHandler([&] { ++requests; return false; });
    window.RequestClose();
    Require(window.ShouldClose(), "Explicit close did not confirm closure");
    Require(requests == 0, "Explicit close invoked the native-close handler");
    window.SetCloseRequestHandler({});
    Require(window.ShouldClose(), "Clearing a handler cleared confirmed closure");
}

} // namespace

int main()
{
    try {
        CheckNativeCloseRequests();
        CheckExplicitConfirmation();
        std::cout << "Window close request tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
