// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMAPI_H
#define CONCORD_CVMAPI_H

/**
 * The C ABI through which ConcordScript's LLVM backend drives the engine.
 *
 * ConcordScript's Concord Visual Machine lowers @cvm blocks to LLVM IR, and IR
 * can call a C ABI and nothing else: no classes, no templates, no overloads, no
 * exceptions, no object layout. Scene::Spawn<Object::Box>({...}) is therefore
 * unreachable from a .cx file directly -- reaching it would mean reimplementing
 * the C++ ABI. This header is the boundary instead.
 *
 * ## Three types
 *
 * CVM has three value types, and this ABI uses all of them:
 *
 * - \c std::int64_t for anything that names, counts or indexes something: a
 *   scene or entity handle, an axis, a key code, a colour channel, a pixel
 *   dimension.
 * - \c double for anything physical: a position, a size, an angle, an
 *   intensity, a mouse delta.
 * - \c const char* for text: a window title, an entity name, UI labels, and
 *   the diagnostic from a failed model load.
 *
 * An earlier revision passed every quantity as a scaled integer (millimetres,
 * tenths of a degree, thousandths) because the language had no float. That is
 * gone: a script writes metres and degrees, which is what an author actually
 * reasons in, and no call site has to remember a scale factor.
 *
 * ## Handles
 *
 * Scenes and entities are identified by opaque handles, never pointers, and
 * every entry point validates them. A stale or forged handle returns the
 * documented failure value instead of dereferencing freed memory. An entity
 * handle also records which scene it came from, so an entity read through a
 * recycled scene handle fails rather than addressing a foreign world.
 *
 * ## Threading
 *
 * The handle tables are locked, so creating and destroying scenes and entities
 * from different threads is safe. A scene's own contents are not: spawning into
 * one scene from two threads at once is a data race, exactly as it would be in
 * C++. The frame callback and the input queries run on the main thread.
 */

#include "Concord/CExport.h"

#include <cstdint>

extern "C" {

/**
 * Returns the version of this C ABI.
 *
 * ConcordScript declares the signatures it expects independently, so that the
 * compiler never has to link or include the engine. This number is how a
 * mismatch between the two sides is detected instead of failing silently.
 */
CENGINE_API std::int64_t ConcordCvmVersion(void);

/** Creates an empty scene. Returns a handle, or 0 when allocation fails. */
CENGINE_API std::int64_t ConcordCvmSceneCreate(void);

/**
 * Releases a scene.
 *
 * Every entity handle that belonged to it is retired too, so nothing can later
 * resolve against a recycled scene handle. Returns 1 on success.
 */
CENGINE_API std::int64_t ConcordCvmSceneDestroy(std::int64_t scene);

/** Returns the number of live entities, or 0 for an invalid handle. */
CENGINE_API std::int64_t ConcordCvmSceneEntityCount(std::int64_t scene);

/** Returns 1 when the scene has a complete, enabled camera, 0 otherwise. */
CENGINE_API std::int64_t ConcordCvmSceneHasMainCamera(std::int64_t scene);

/*
 * Spawns.
 *
 * Every one returns an entity handle, or 0 when the scene handle is not live.
 * Positions are world units, sizes are world units, angles are degrees, and
 * red, green and blue are 0..255 channels. Colour stays integral on purpose: a
 * channel byte is exact, and a script has no reason to write 0.878431.
 */

/** Spawns a look-at camera. Position and target are world units. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnCamera(std::int64_t scene, double positionX,
                                                    double positionY, double positionZ,
                                                    double targetX, double targetY,
                                                    double targetZ);

/** Spawns a directional sun. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnSun(std::int64_t scene, double elevationDegrees,
                                                 double azimuthDegrees, double intensity);

/** Spawns a box with an independent extent on every axis. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnBox(std::int64_t scene, double positionX,
                                                 double positionY, double positionZ, double sizeX,
                                                 double sizeY, double sizeZ, std::int64_t red,
                                                 std::int64_t green, std::int64_t blue);

/** Spawns a sphere of the given diameter. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnSphere(std::int64_t scene, double positionX,
                                                    double positionY, double positionZ,
                                                    double diameter, std::int64_t red,
                                                    std::int64_t green, std::int64_t blue);

/** Spawns a horizontal plane of the given extent. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnPlane(std::int64_t scene, double positionX,
                                                   double positionY, double positionZ,
                                                   double sizeX, double sizeZ, std::int64_t red,
                                                   std::int64_t green, std::int64_t blue);

/** Invisible static volume. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnStaticBody(std::int64_t scene, double positionX,
                                                        double positionY, double positionZ,
                                                        double sizeX, double sizeY,
                                                        double sizeZ);

/** Drawable box the solver is allowed to throw. \p mass in kilograms. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnDynamicBox(std::int64_t scene, double positionX,
                                                        double positionY, double positionZ,
                                                        double sizeX, double sizeY, double sizeZ,
                                                        std::int64_t red, std::int64_t green,
                                                        std::int64_t blue, double mass);

/** Drawable sphere the solver is allowed to throw. \p mass in kilograms. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnDynamicSphere(std::int64_t scene, double positionX,
                                                           double positionY, double positionZ,
                                                           double diameter, std::int64_t red,
                                                           std::int64_t green, std::int64_t blue,
                                                           double mass);

/** Spawns a point light. \p range is its reach, in world units. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnPointLight(std::int64_t scene, double positionX,
                                                        double positionY, double positionZ,
                                                        std::int64_t red, std::int64_t green,
                                                        std::int64_t blue, double intensity,
                                                        double range);

/**
 * Spawns a spot light aimed by elevation and azimuth, the same two numbers a
 * directional light uses.
 *
 * \p angle is the cone half-angle in degrees. Elevation 90 aims straight down.
 */
CENGINE_API std::int64_t ConcordCvmSceneSpawnSpotLight(std::int64_t scene, double positionX,
                                                       double positionY, double positionZ,
                                                       double elevationDegrees,
                                                       double azimuthDegrees, std::int64_t red,
                                                       std::int64_t green, std::int64_t blue,
                                                       double intensity, double range,
                                                       double angleDegrees);

/*
 * Entity edits. Each returns 1 when it applied, 0 when the handle is not live
 * or the entity does not carry the component being changed.
 */

/** Sets an entity's world position. */
CENGINE_API std::int64_t ConcordCvmEntitySetPosition(std::int64_t entity, double x, double y,
                                                     double z);

/** Sets an entity's Euler rotation, in degrees. */
CENGINE_API std::int64_t ConcordCvmEntitySetRotation(std::int64_t entity, double x, double y,
                                                     double z);

/**
 * Adds an offset to an entity's world position.
 *
 * Per-frame motion wants a relative move: a script that recomputes an absolute
 * position has to read it back first, and the read is the part that loses
 * precision.
 */
CENGINE_API std::int64_t ConcordCvmEntityTranslate(std::int64_t entity, double deltaX,
                                                   double deltaY, double deltaZ);

/**
 * Reads one axis of an entity's world position.
 *
 * \p axis is 0 for X, 1 for Y and 2 for Z; any other value, or an entity with
 * no transform, returns 0.
 */
CENGINE_API double ConcordCvmEntityPosition(std::int64_t entity, std::int64_t axis);

/** Reads one Euler axis in degrees. The axis convention matches Position. */
CENGINE_API double ConcordCvmEntityRotation(std::int64_t entity, std::int64_t axis);

/** Sets an entity's per-axis scale. */
CENGINE_API std::int64_t ConcordCvmEntitySetScale(std::int64_t entity, double x, double y,
                                                  double z);

/** Reads one axis of an entity's scale. */
CENGINE_API double ConcordCvmEntityScale(std::int64_t entity, std::int64_t axis);

/**
 * Shows or hides a drawable entity.
 *
 * Returns 1 when the entity carries a mesh or model renderer. An entity with
 * neither is not drawable, so hiding it would be a no-op reported as 0.
 */
CENGINE_API std::int64_t ConcordCvmEntitySetVisible(std::int64_t entity, std::int64_t visible);

/** Sets whether a drawable entity casts shadows. Same membership rule as visible. */
CENGINE_API std::int64_t ConcordCvmEntitySetShadow(std::int64_t entity, std::int64_t castShadow);

/** Sets an entity's albedo channels, 0..255 each. */
CENGINE_API std::int64_t ConcordCvmEntitySetColor(std::int64_t entity, std::int64_t red,
                                                  std::int64_t green, std::int64_t blue);

/**
 * Sets an entity's metallic-roughness surface.
 *
 * \p metallic and \p roughness are 0..1; \p emissive is a radiance
 * multiplier where 0 means the surface does not emit.
 */
CENGINE_API std::int64_t ConcordCvmEntitySetMaterial(std::int64_t entity, double metallic,
                                                     double roughness, double emissive);

/** Sets a camera entity's vertical field of view, in degrees. */
CENGINE_API std::int64_t ConcordCvmEntitySetFov(std::int64_t entity, double fovYDegrees);

/** Reads one albedo channel, 0..255. Axis 0/1/2 is R/G/B. */
CENGINE_API std::int64_t ConcordCvmEntityColor(std::int64_t entity, std::int64_t axis);

/**
 * Reads one metallic-roughness field.
 *
 * \p field is 0 metallic, 1 roughness, 2 emissive.
 */
CENGINE_API double ConcordCvmEntityMaterial(std::int64_t entity, std::int64_t field);

/** Reads a camera's vertical field of view in degrees, or 0 when it is not a camera. */
CENGINE_API double ConcordCvmEntityFov(std::int64_t entity);

/** Returns 1 while a drawable entity is visible. */
CENGINE_API std::int64_t ConcordCvmEntityVisible(std::int64_t entity);

/** Returns 1 while a drawable entity casts shadows. */
CENGINE_API std::int64_t ConcordCvmEntityShadow(std::int64_t entity);

/** Removes an entity from its scene and retires the handle. */
CENGINE_API std::int64_t ConcordCvmEntityDestroy(std::int64_t entity);

/** Returns 1 while \p entity still names a live entity. */
CENGINE_API std::int64_t ConcordCvmEntityAlive(std::int64_t entity);

/**
 * Aims an entity so its local -Z points at a world position.
 *
 * Spawn-time look-at is converted to a rotation and discarded, so later
 * aiming is a rotation write. Returns 1 when the entity has a transform.
 */
CENGINE_API std::int64_t ConcordCvmEntityLookAt(std::int64_t entity, double x, double y,
                                                double z);

/**
 * Reads one axis of the direction an entity faces (its local -Z).
 *
 * \p axis is 0/1/2 for X/Y/Z. An entity with no transform returns 0.
 */
CENGINE_API double ConcordCvmEntityForward(std::int64_t entity, std::int64_t axis);

/** Sets a primitive's extent, before Transform scale. Returns 1 on a mesh. */
CENGINE_API std::int64_t ConcordCvmEntitySetSize(std::int64_t entity, double x, double y,
                                                 double z);

/** Reads one axis of a primitive's extent. */
CENGINE_API double ConcordCvmEntitySize(std::int64_t entity, std::int64_t axis);

/**
 * Returns 1 when two drawable primitives' world AABBs overlap.
 *
 * The box is the mesh size scaled by the transform. Entities without a mesh
 * (cameras, lights, models) return 0: there is no authored extent to test.
 */
CENGINE_API std::int64_t ConcordCvmEntityOverlaps(std::int64_t left, std::int64_t right);

/** World-space distance between two entities' positions, or 0 if either is stale. */
CENGINE_API double ConcordCvmEntityDistance(std::int64_t left, std::int64_t right);

/** The scene's current camera as a handle, or 0 when none is enabled. */
CENGINE_API std::int64_t ConcordCvmSceneCamera(std::int64_t scene);

/** Sets a light's radiance multiplier. Returns 1 when the entity is a light. */
CENGINE_API std::int64_t ConcordCvmLightSetIntensity(std::int64_t entity, double intensity);

/** Sets a light's colour channels, 0..255. */
CENGINE_API std::int64_t ConcordCvmLightSetColor(std::int64_t entity, std::int64_t red,
                                                 std::int64_t green, std::int64_t blue);

/** Sets a directional or spot light's elevation and azimuth, in degrees. */
CENGINE_API std::int64_t ConcordCvmLightSetAngles(std::int64_t entity, double elevationDegrees,
                                                  double azimuthDegrees);

/** Reads a light's radiance multiplier, or 0 when the entity is not a light. */
CENGINE_API double ConcordCvmLightIntensity(std::int64_t entity);

/** Reads one light colour channel, 0..255. Axis 0/1/2 is R/G/B. */
CENGINE_API std::int64_t ConcordCvmLightColor(std::int64_t entity, std::int64_t axis);

/** Reads elevation (axis 0) or azimuth (axis 1) in degrees. */
CENGINE_API double ConcordCvmLightAngle(std::int64_t entity, std::int64_t axis);

/*
 * Listing, naming and picking.
 *
 * The engine cannot return a CVM list handle (those live in the script
 * runtime), so a script snapshots the live entities and then indexes the table.
 * Kind is a small integer rather than a string: 1 camera, 2 mesh, 3 model,
 * 4 light, 5 particles, 0 none.
 */

/**
 * Records every live entity and returns how many were recorded.
 *
 * Later ConcordCvmSceneSnapshotAt calls read this table. A new snapshot
 * replaces the previous one.
 */
CENGINE_API std::int64_t ConcordCvmSceneSnapshot(std::int64_t scene);

/** Returns the entity handle at \p index in the last snapshot, or 0. */
CENGINE_API std::int64_t ConcordCvmSceneSnapshotAt(std::int64_t index);

/** Returns the entity's kind, or 0 when the handle is stale. */
CENGINE_API std::int64_t ConcordCvmEntityKind(std::int64_t entity);

/** Sets the Name component, creating it when absent. */
CENGINE_API std::int64_t ConcordCvmEntitySetName(std::int64_t entity, const char* name);

/**
 * Returns the entity's name, or an empty string when it has none.
 *
 * The pointer is owned by the component and stays valid until the name is
 * changed or the entity is destroyed.
 */
CENGINE_API const char* ConcordCvmEntityName(std::int64_t entity);

/**
 * Casts a ray and returns the nearest hit, or 0.
 *
 * When a PhysicsSystem is running the test is Jolt; otherwise it walks mesh
 * AABBs. Models have no authored extent, so they are not tested on the AABB
 * path. The last hit's distance and point remain readable until the next raycast.
 */
CENGINE_API std::int64_t ConcordCvmRaycast(std::int64_t scene, double originX, double originY,
                                           double originZ, double directionX, double directionY,
                                           double directionZ, double maxDistance);

/** Distance of the last raycast hit, or 0 when the last ray missed. */
CENGINE_API double ConcordCvmRaycastDistance(void);

/** One axis of the last raycast hit point. Axis 0/1/2 is X/Y/Z. */
CENGINE_API double ConcordCvmRaycastPoint(std::int64_t axis);

/*
 * Scene environment. Each reads the current settings, changes the field it
 * names, and writes them back, so the setters are independent of one another.
 */

/** Sets the sRGB clear/sky colour, 0..255 per channel. */
CENGINE_API std::int64_t ConcordCvmSceneSetSky(std::int64_t scene, std::int64_t red,
                                               std::int64_t green, std::int64_t blue);

/** Sets the ambient colour and its multiplier. */
CENGINE_API std::int64_t ConcordCvmSceneSetAmbient(std::int64_t scene, std::int64_t red,
                                                   std::int64_t green, std::int64_t blue,
                                                   double intensity);

/** Sets exposure, contrast, saturation, vignette and bloom in one call. */
CENGINE_API std::int64_t ConcordCvmSceneSetGrade(std::int64_t scene, double exposure,
                                                 double contrast, double saturation,
                                                 double vignette, double bloomIntensity);

/**
 * Sets exponential height fog.
 *
 * \p density is an extinction coefficient per world unit, so a value much
 * above 0.05 makes everything beyond a few metres opaque.
 */
CENGINE_API std::int64_t ConcordCvmSceneSetFog(std::int64_t scene, double density,
                                               double heightFalloff, double baseHeight,
                                               double anisotropy);

/** Sets the volume cloud layer: coverage 0..1, extinction, and floor altitude. */
CENGINE_API std::int64_t ConcordCvmSceneSetClouds(std::int64_t scene, double coverage,
                                                  double density, double altitude);

/**
 * Registers the per-frame callback, and returns the callback it replaced.
 *
 * \p update is the address of a function taking the frame's delta time in
 * seconds and returning \c int64_t. Both parts are load-bearing: the engine
 * calls the callback through exactly that signature, so a @cvm function
 * registered here must be declared i64(f64) -- no ret=, and exactly one
 * parameter annotated f64. A delta time of about 0.016 seconds is precisely
 * why it cannot be an integer. A negative return asks the running game to
 * quit after the current frame. Pass 0 to clear the callback.
 */
CENGINE_API std::int64_t ConcordCvmSetUpdate(std::int64_t update);

/**
 * Asks the running game to leave Run() after this frame.
 *
 * Returning a negative number from the frame callback does the same thing.
 * Returns 1 when a game is running, 0 otherwise.
 */
CENGINE_API std::int64_t ConcordCvmQuit(void);

/**
 * Captures the cursor for mouse look (relative motion, cursor hidden).
 *
 * Pass 1 to capture, 0 to release. Returns 1 when a window is running.
 */
CENGINE_API std::int64_t ConcordCvmSetMouseCaptured(std::int64_t captured);

/** Returns 1 while the cursor is captured. */
CENGINE_API std::int64_t ConcordCvmMouseCaptured(void);

/** Client width in pixels, or 0 outside a running window. */
CENGINE_API std::int64_t ConcordCvmWindowWidth(void);

/** Client height in pixels, or 0 outside a running window. */
CENGINE_API std::int64_t ConcordCvmWindowHeight(void);

/** Shows or hides the debug overlay. Returns 1 when a game is running. */
CENGINE_API std::int64_t ConcordCvmSetOverlay(std::int64_t visible);

/** Returns 1 while the debug overlay is shown. */
CENGINE_API std::int64_t ConcordCvmOverlay(void);

/** Replaces the running window's caption. Returns 1 when a window is running. */
CENGINE_API std::int64_t ConcordCvmSetTitle(const char* title);

/**
 * Changes presentation mode: 0 windowed, 1 borderless, 2 exclusive fullscreen.
 *
 * Returns 1 when a window is running and \p mode is one of those three values.
 */
CENGINE_API std::int64_t ConcordCvmSetWindowMode(std::int64_t mode);

/** Current presentation mode, or 0 outside a running window. */
CENGINE_API std::int64_t ConcordCvmWindowMode(void);

/** Frame-loop iterations since Run() started, or 0 outside a running game. */
CENGINE_API std::int64_t ConcordCvmFrameCount(void);

/** Seconds between the two most recently processed frames, or 0 outside a game. */
CENGINE_API double ConcordCvmDeltaTime(void);

/** Seconds accumulated while the current Run() has been ticking, or 0 outside. */
CENGINE_API double ConcordCvmTime(void);

/*
 * Immediate-mode UI.
 *
 * The frame loop already Begin()s the canvas before the script callback, so
 * these are only valid inside that callback. Each returns 1 when a game is
 * running. A button returns 1 on the frame it is clicked.
 */

CENGINE_API std::int64_t ConcordCvmUiPanel(double x, double y, double width, double height);

CENGINE_API std::int64_t ConcordCvmUiLabel(double x, double y, const char* text);

CENGINE_API std::int64_t ConcordCvmUiButton(double x, double y, double width, double height,
                                            const char* text);

/**
 * Opens a window titled \p title and runs the scene until the window closes.
 *
 * \p width and \p height are pixel dimensions, clamped to a usable range. A
 * null or empty title falls back to a default. The scene stays owned by its
 * handle, so the caller may still destroy it afterwards. This blocks for the
 * lifetime of the window, and requires a display and a Vulkan-capable device.
 *
 * Returns 1 after the window closes, or 0 when the handle is not live.
 */
CENGINE_API std::int64_t ConcordCvmRunScene(std::int64_t scene, std::int64_t width,
                                            std::int64_t height, const char* title);

/*
 * Imported models.
 *
 * Loading is the one place a CVM program reaches the filesystem, so it is the
 * one place that can fail for a reason outside the program's control. A failed
 * load returns 0; ConcordCvmLastError carries the loader's diagnostic.
 */

/** Decodes a model and returns a handle, or 0 on failure. \p scale is uniform. */
CENGINE_API std::int64_t ConcordCvmModelLoad(const char* path, double scale);

/** Releases a model handle. Entities already spawned keep their reference. */
CENGINE_API std::int64_t ConcordCvmModelFree(std::int64_t model);

/** Returns the diagnostic from the most recent failed load, or an empty string. */
CENGINE_API const char* ConcordCvmLastError(void);

/** Spawns an instance of an imported model. Returns an entity handle, or 0. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnModel(std::int64_t scene, std::int64_t model,
                                                   double positionX, double positionY,
                                                   double positionZ);

/** Returns the number of materials on \p model, or 0 for an invalid handle. */
CENGINE_API std::int64_t ConcordCvmModelMaterialCount(std::int64_t model);

/** Returns the number of animation clips, or 0 for an invalid handle. */
CENGINE_API std::int64_t ConcordCvmModelClipCount(std::int64_t model);

/** Returns the number of skeletons, or 0 for an invalid handle. */
CENGINE_API std::int64_t ConcordCvmModelSkeletonCount(std::int64_t model);

/**
 * Returns the name of clip \p index, or an empty string when it does not exist.
 *
 * The pointer is owned by the model handle and stays valid until that handle
 * is freed. A script that needs to keep the text should copy it with str_concat.
 */
CENGINE_API const char* ConcordCvmModelClipName(std::int64_t model, std::int64_t index);

/** Returns the duration of clip \p index in seconds, or 0 when it does not exist. */
CENGINE_API double ConcordCvmModelClipDuration(std::int64_t model, std::int64_t index);

/** Returns the index of the clip named \p name, or -1 when it is not there. */
CENGINE_API std::int64_t ConcordCvmModelFindClip(std::int64_t model, const char* name);

/**
 * Sets material \p index's base-color texture to \p path.
 *
 * The path is stored on the asset, so every instance already spawned from this
 * handle sees it on the next frame. An out-of-range index returns 0.
 */
CENGINE_API std::int64_t ConcordCvmModelSetAlbedoTexture(std::int64_t model, std::int64_t index,
                                                         const char* path);

/**
 * Starts clip \p clip on a spawned model.
 *
 * The entity must carry a model renderer whose asset has a skeleton and that
 * clip. Speed 1 is authored time; loop 0 plays once. Returns 1 when playback
 * was armed.
 */
CENGINE_API std::int64_t ConcordCvmEntityPlayAnimation(std::int64_t entity, std::int64_t clip,
                                                       double speed, std::int64_t loop);

/** Stops playback without removing the pose. Returns 1 when an animation was there. */
CENGINE_API std::int64_t ConcordCvmEntityStopAnimation(std::int64_t entity);

/** Changes playback speed. Returns 1 when an animation component is present. */
CENGINE_API std::int64_t ConcordCvmEntitySetAnimationSpeed(std::int64_t entity, double speed);

/** Returns the current clip time in seconds, or 0 when there is no animation. */
CENGINE_API double ConcordCvmEntityAnimationTime(std::int64_t entity);

/*
 * Particles.
 *
 * An emitter is spawned with defaults and then configured, rather than taking
 * fifteen positional arguments: a call site with fourteen doubles in a row is
 * one nobody can read, and a setter that fails is easier to notice than a
 * parameter that was passed in the wrong slot.
 *
 * Every setter returns 1 when it applied and 0 when the handle is not live or
 * the entity carries no emitter.
 */

/** Spawns a particle emitter with its colour already set. */
CENGINE_API std::int64_t ConcordCvmSceneSpawnParticles(std::int64_t scene, double positionX,
                                                       double positionY, double positionZ,
                                                       std::int64_t red, std::int64_t green,
                                                       std::int64_t blue);

/** Sets particles emitted per second. */
CENGINE_API std::int64_t ConcordCvmParticlesSetRate(std::int64_t entity, double rate);

/** Sets the launch speed range. */
CENGINE_API std::int64_t ConcordCvmParticlesSetSpeed(std::int64_t entity, double low,
                                                     double high);

/** Sets the lifetime range, in seconds. */
CENGINE_API std::int64_t ConcordCvmParticlesSetLifetime(std::int64_t entity, double low,
                                                        double high);

/** Sets the birth-to-death size ramp, in world units. */
CENGINE_API std::int64_t ConcordCvmParticlesSetSize(std::int64_t entity, double start, double end);

/**
 * Sets the spawn volume.
 *
 * \p shape is a Concord::ParticleShape index: 0 point, 1 sphere, 2 box, 3 cone.
 */
CENGINE_API std::int64_t ConcordCvmParticlesSetShape(std::int64_t entity, std::int64_t shape,
                                                     double extentX, double extentY,
                                                     double extentZ);

/** Sets the emission direction and its cone half-angle, in degrees. */
CENGINE_API std::int64_t ConcordCvmParticlesSetDirection(std::int64_t entity, double x, double y,
                                                         double z, double spreadDegrees);

/** Sets constant acceleration and the velocity damping factor. */
CENGINE_API std::int64_t ConcordCvmParticlesSetGravity(std::int64_t entity, double x, double y,
                                                       double z, double drag);

/** Sets the colour ramp. The end alpha is always 0, so particles fade out. */
CENGINE_API std::int64_t ConcordCvmParticlesSetColor(std::int64_t entity, std::int64_t red,
                                                     std::int64_t green, std::int64_t blue,
                                                     std::int64_t endRed, std::int64_t endGreen,
                                                     std::int64_t endBlue);

/** Sets the blend mode: 0 additive for fire, 1 scatter for smoke. */
CENGINE_API std::int64_t ConcordCvmParticlesSetBlend(std::int64_t entity, std::int64_t blend);

/** Sets the pool size. Takes effect on the next update. */
CENGINE_API std::int64_t ConcordCvmParticlesSetCapacity(std::int64_t entity,
                                                        std::int64_t capacity);

/*
 * Water.
 *
 * Water is a material stamped onto a model's meshes rather than a geometry
 * type, so a body of water is spawned from a model handle -- a plane is the
 * usual one. The asset is shared with the model handle, so two bodies from one
 * handle share whichever surface was stamped last.
 */

/**
 * Spawns a body of water on \p model's meshes. Returns an entity handle, or 0.
 *
 * The whole surface arrives in one call rather than through setters, because
 * the engine stamps the material onto the asset's meshes at spawn time: there
 * is no per-entity copy to edit afterwards. \p scale sizes the body, \p red,
 * \p green and \p blue are the open-water colour, \p opacity is how much of
 * what is behind shows through, \p absorptionDistance is how far light travels
 * before the colour has been absorbed, and the last two are the wave amplitude
 * and its speed.
 */
CENGINE_API std::int64_t ConcordCvmSceneSpawnWater(std::int64_t scene, std::int64_t model,
                                                   double positionX, double positionY,
                                                   double positionZ, double scale,
                                                   std::int64_t red, std::int64_t green,
                                                   std::int64_t blue, double opacity,
                                                   double absorptionDistance, double waveAmplitude,
                                                   double waveSpeed);

/**
 * Spawns a one-shot ripple on the water surface.
 *
 * \p x and \p z are the world-space centre; \p wavelength, \p speed, \p strength
 * and \p reach are the ring's shape; \p lifetime is how long it keeps emitting.
 */
CENGINE_API std::int64_t ConcordCvmSceneSpawnRipple(std::int64_t scene, double x, double z,
                                                    double wavelength, double speed,
                                                    double strength, double reach,
                                                    double lifetime);

/**
 * Spawns an area that keeps scattering ripples.
 *
 * \p rate is rings per second; zero is calm water. \p extentX and \p extentZ
 * are the half-extents of the scatter rectangle, centred on the world origin
 * of the entity (which this call leaves at the origin).
 */
CENGINE_API std::int64_t ConcordCvmSceneSpawnRippleField(std::int64_t scene, double rate,
                                                         double extentX, double extentZ);

/*
 * Engine systems.
 *
 * These accumulate and are registered on the Game when ConcordCvmRunScene
 * starts it, because there is no Game before then. Each returns 1 when it was
 * recorded, so a script can tell a typo'd call from a working one.
 */

/** Advances every particle emitter. Without it, emitters never spawn anything. */
CENGINE_API std::int64_t ConcordCvmAddParticleSystem(void);

/** Advances water ripple fields. Without it, ripples never move. */
CENGINE_API std::int64_t ConcordCvmAddWaterRippleSystem(void);

/** Blends skeletal animation and drives state machines. */
CENGINE_API std::int64_t ConcordCvmAddAnimationSystem(void);

/** Drives the sun and ambient light around a day. */
CENGINE_API std::int64_t ConcordCvmAddDayCycle(double secondsPerDay, double startHour,
                                               double peakSunElevationDegrees,
                                               double sunriseAzimuthDegrees,
                                               double cloudCoverage);

/** Adds the engine's first-person controller. \p flyMode ignores gravity. */
CENGINE_API std::int64_t ConcordCvmAddFirstPerson(std::int64_t flyMode);

/** Steps Jolt: gravity, contacts, character motors. Without it, bodies never move. */
CENGINE_API std::int64_t ConcordCvmAddPhysicsSystem(void);

/**
 * Attaches a collider and rigid body to an existing entity.
 *
 * \p motion is 0 static, 1 kinematic, 2 dynamic. Size comes from the mesh
 * when the entity has one, otherwise one metre on each axis.
 */
CENGINE_API std::int64_t ConcordCvmEntityAddBody(std::int64_t entity, std::int64_t motion,
                                                 double mass);

/** Instant linear velocity. Returns 1 when the entity carries a rigid body. */
CENGINE_API std::int64_t ConcordCvmEntitySetVelocity(std::int64_t entity, double x, double y,
                                                     double z);

/** Linear velocity on one axis (0=x, 1=y, 2=z). */
CENGINE_API double ConcordCvmEntityVelocity(std::int64_t entity, std::int64_t axis);

/** Impulse at the centre of mass. Returns 0 when the body has not been stepped yet. */
CENGINE_API std::int64_t ConcordCvmEntityApplyImpulse(std::int64_t entity, double x, double y,
                                                      double z);

/** 1 when a CharacterMotor reports ground under the capsule. */
CENGINE_API std::int64_t ConcordCvmEntityGrounded(std::int64_t entity);

/**
 * First body whose volume meets a sphere, or 0.
 *
 * The world must have been stepped at least once so Jolt has bodies to test.
 */
CENGINE_API std::int64_t ConcordCvmOverlapSphere(std::int64_t scene, double x, double y, double z,
                                                 double radius);

/**
 * Advances Jolt on \p scene by \p deltaSeconds.
 *
 * Headless scripts use this instead of RunScene. Inside a running game the
 * PhysicsSystem already steps, so this call is a no-op that still returns 1.
 */
CENGINE_API std::int64_t ConcordCvmPhysicsStep(std::int64_t scene, double deltaSeconds);

/** Replaces gravity. Dynamic bodies feel it on the next step. */
CENGINE_API std::int64_t ConcordCvmSetGravity(std::int64_t scene, double x, double y, double z);

/** Gravity on one axis (0=x, 1=y, 2=z). Defaults to (0, -9.81, 0) before a world exists. */
CENGINE_API double ConcordCvmGravity(std::int64_t scene, std::int64_t axis);

/** Attaches a walking capsule. Radius and height are metres. */
CENGINE_API std::int64_t ConcordCvmEntityAddMotor(std::int64_t entity, double radius, double height);

/** Horizontal (and jump) intent the CharacterVirtual integrates. */
CENGINE_API std::int64_t ConcordCvmEntitySetWishVelocity(std::int64_t entity, double x, double y,
                                                       double z);

/** Wish velocity on one axis. */
CENGINE_API double ConcordCvmEntityWishVelocity(std::int64_t entity, std::int64_t axis);

/** Instant angular velocity. Returns 1 when the entity carries a rigid body. */
CENGINE_API std::int64_t ConcordCvmEntitySetAngularVelocity(std::int64_t entity, double x, double y,
                                                        double z);

/** Angular velocity on one axis (0=x, 1=y, 2=z). */
CENGINE_API double ConcordCvmEntityAngularVelocity(std::int64_t entity, std::int64_t axis);

/** Switches the first-person controller between fly and walk. */
CENGINE_API std::int64_t ConcordCvmSetFlyMode(std::int64_t flyMode);

/*
 * Input.
 *
 * Outside a running window every query answers 0 rather than failing, so a
 * script never has to ask whether a window exists before reading it.
 */

/**
 * Returns 1 while the key is held, 0 otherwise.
 *
 * \p key is a Concord::KeyCode value: a dense index, so A is 1, Z is 26,
 * Digit1 is 27, Up is 49, Space is 61 and Escape is 63.
 */
CENGINE_API std::int64_t ConcordCvmKeyDown(std::int64_t key);

/** Returns 1 on the frame the key went down, 0 otherwise. */
CENGINE_API std::int64_t ConcordCvmKeyPressed(std::int64_t key);

/** Returns 1 on the frame the key came up, 0 otherwise. */
CENGINE_API std::int64_t ConcordCvmKeyReleased(std::int64_t key);

/** Returns one axis of the mouse movement since the last frame, in pixels. */
CENGINE_API double ConcordCvmMouseDelta(std::int64_t axis);

/**
 * Returns 1 while the mouse button is held.
 *
 * \p button is 0 left, 1 right, 2 middle, 3 X1, 4 X2.
 */
CENGINE_API std::int64_t ConcordCvmMouseDown(std::int64_t button);

/** Returns 1 on the frame the button went down. */
CENGINE_API std::int64_t ConcordCvmMousePressed(std::int64_t button);

/** Returns 1 on the frame the button came up. */
CENGINE_API std::int64_t ConcordCvmMouseReleased(std::int64_t button);

/** Returns one axis of the cursor position in window-client pixels. */
CENGINE_API double ConcordCvmMousePosition(std::int64_t axis);

/** Returns one axis of the wheel delta since the last frame. Positive y is up. */
CENGINE_API double ConcordCvmMouseWheel(std::int64_t axis);

} // extern "C"

#endif // CONCORD_CVMAPI_H
