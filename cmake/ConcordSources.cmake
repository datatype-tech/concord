# Keep the source manifests split by responsibility.  Each included manifest
# remains explicit so adding a translation unit is visible in code review.
include("${CMAKE_CURRENT_LIST_DIR}/ConcordAssetSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ConcordRuntimeSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ConcordRenderSources.cmake")
