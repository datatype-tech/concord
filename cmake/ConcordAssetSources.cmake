# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# CPU-side model, image, and animation importers are kept in a named group so
# the runtime and focused asset tests can share the same source manifest.
set(CONCORD_ASSET_SOURCES
    src/engine/asset/Animation.cpp
    src/engine/asset/AnimationPlayer.cpp
    src/engine/asset/ImageAsset.cpp
    src/engine/asset/ImageAssetCache.cpp
    src/engine/asset/ImageAssetUri.cpp
    src/engine/asset/StbImage.cpp
    src/engine/asset/SkinningPalette.cpp
    src/engine/asset/GltfAccessors.cpp
    src/engine/asset/GltfBuffers.cpp
    src/engine/asset/GltfGeometryMaterials.cpp
    src/engine/asset/GltfGeometryMeshes.cpp
    src/engine/asset/GltfLoader.cpp
    src/engine/asset/GltfSceneAnimations.cpp
    src/engine/asset/GltfSceneNodes.cpp
    src/engine/asset/GltfSceneSkins.cpp
    src/engine/asset/JsonParser.cpp
    src/engine/asset/JsonParserContainers.cpp
    src/engine/asset/JsonParserString.cpp
    src/engine/asset/JsonValue.cpp
    src/engine/asset/ModelAsset.cpp
    src/engine/asset/ModelLoader.cpp
    src/engine/asset/ObjLoader.cpp
    src/engine/asset/ObjLoaderFaces.cpp
    src/engine/asset/ObjLoaderGeometry.cpp
    src/engine/asset/ObjLoaderMtl.cpp
    src/engine/asset/Skeleton.cpp
)
