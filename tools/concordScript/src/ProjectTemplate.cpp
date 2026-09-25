#include "ProjectTemplate.h"

#include "CliReport.h"

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace ConcordScript {

void InitializeProject(const std::filesystem::path& directory)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::absolute(directory).lexically_normal();
    if (fs::exists(root) && (!fs::is_directory(root) || !fs::is_empty(root))) {
        throw std::runtime_error("project directory must be new or empty: " + root.string());
    }
    struct File { const char* name; std::string_view contents; };
    const File files[] = {
        {"Main.cx", R"cx(use Concord.CApplication;
use Concord.CCamera;
use Concord.CLight;
use Concord.CObject;
use Concord.CScene;

@entry {
    Concord::Game game;
    var window = Concord::Window({.title = "My ConcordScript Game", .resolution = {1280, 720}});
    game.AttachWindow(window);
    if (!window.IsOpen()) return 1;
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({.position = {0.0f, 3.0f, 6.0f}, .target = {0.0f, 0.0f, 0.0f}});
    scene.Spawn<Concord::Object::SunLight>({.elevationDegrees = 45.0f});
    scene.Spawn<Concord::Object::Box>({});
    game.LoadScene(scene);
    game.Run();
    return 0;
};
)cx"},
        {"CMakeLists.txt", R"cmake(cmake_minimum_required(VERSION 3.24)
project(ConcordScriptGame LANGUAGES CXX)
find_package(ConcordFlash CONFIG REQUIRED)
find_program(CONCORDSCRIPT_COMPILER NAMES concordc HINTS "${CONCORD_SDK_ROOT}/bin" REQUIRED)
file(GLOB sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cx")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(generated_sources "${generated_dir}/ConcordScriptRegistry.gen.cpp")
set(generated_headers "${generated_dir}/ConcordScriptRegistry.gen.h")
foreach(source IN LISTS sources)
    get_filename_component(stem "${source}" NAME_WE)
    list(APPEND generated_sources "${generated_dir}/${stem}.gen.cpp")
    list(APPEND generated_headers "${generated_dir}/${stem}.gen.h")
endforeach()
add_custom_command(OUTPUT ${generated_sources} ${generated_headers}
    COMMAND "${CONCORDSCRIPT_COMPILER}" --cpp --project "${CMAKE_CURRENT_SOURCE_DIR}" --out "${generated_dir}"
    DEPENDS ${sources} "${CONCORDSCRIPT_COMPILER}"
    COMMENT "Compile ConcordScript sources" VERBATIM)
add_executable(game ${generated_sources} ${generated_headers})
target_include_directories(game PRIVATE "${generated_dir}")
target_link_libraries(game PRIVATE concord::concord)
concord_stage_runtime(game)
)cmake"},
        {".gitignore", "build/\nbuild-cli/\ngenerated/\n*.log\n"},
        {"README.md", R"md(# ConcordScript game

All gameplay lives in .cx files. CMake invokes concordc automatically.
Use Windows x64, the preview UCRT64 MinGW SDK, CMake and Ninja.

```powershell
concordc --project . --check
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/path/to/ConcordFlash-sdk"
cmake --build build
./build/game.exe
```

Set -DCONCORDSCRIPT_COMPILER="C:/path/to/concordc.exe" to select another compiler.
)md"},
    };
    fs::create_directories(root);
    for (const File& file : files) {
        const fs::path path = root / file.name;
        std::ofstream output(path, std::ios::binary);
        output << file.contents;
        output.close();
        if (!output) throw std::runtime_error("cannot create project file: " + path.string());
    }
    ReportSuccess("created project: " + root.string());
}

} // namespace ConcordScript
