// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_OBJLOADER_H
#define CONCORD_ASSET_OBJLOADER_H

#include "engine/asset/ModelLoader.h"

#include <filesystem>

namespace Concord::AssetObj {

/** Internal OBJ decoder; public callers use ModelLoader. */
ModelLoadResult DecodeObj(std::string_view text, const ModelLoadOptions& options,
                          const std::filesystem::path& baseDirectory = {});

} // namespace Concord::AssetObj

#endif // CONCORD_ASSET_OBJLOADER_H
