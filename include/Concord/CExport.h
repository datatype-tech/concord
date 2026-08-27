// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CEXPORT_H
#define CONCORD_CEXPORT_H

/**
 * Central home of the engine's symbol-visibility macros.
 *
 * This header is the deliberate exception to the facade re-export rule
 * (AGENTS.md §3): it defines macros directly rather than forwarding to a
 * module-private header, because the macros are not a type surface.
 *
 * Two DLLs, two macro pairs. `CENGINE_API` marks the runtime types every
 * consumer sees (Window, Game, Scene, IRenderBackend, ...), exported from
 * ConcordFlashGameEngineRuntime.dll. `CRENDER_API` marks the handful of
 * symbols ConcordFlashGameEngineRender.dll exports on its own — today just
 * its module-load anchor (see VulkanBackendRegistration.cpp), since the
 * Vulkan backend itself is created only through `CreateRenderBackend()` and
 * never named directly by a consumer.
 *
 * Both expand to nothing when `CONCORD_SHARED` is undefined, i.e. when the
 * engine is consumed as a static build.
 */

#if defined(CONCORD_SHARED)
#  if defined(CENGINE_EXPORTS)
#    define CENGINE_API __declspec(dllexport)
#  else
#    define CENGINE_API __declspec(dllimport)
#  endif
#  if defined(CRENDER_EXPORTS)
#    define CRENDER_API __declspec(dllexport)
#  else
#    define CRENDER_API __declspec(dllimport)
#  endif
#else
#  define CENGINE_API
#  define CRENDER_API
#endif

#endif // CONCORD_CEXPORT_H
