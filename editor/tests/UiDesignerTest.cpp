// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/UiDesigner.h"
#include <Concord/CUiDocument.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv)
{
    try {
        const auto parent = std::filesystem::absolute(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path());
        const auto root = parent / ("ui-designer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root / "UI");
        Concord::UiDocument original;
        original.elements.push_back({.id = "Play", .text = "Play game"});
        const auto file = root / "UI" / "Menu.yu";
        original.Save(file);

        Concord::Editor::UiDesigner designer;
        designer.SetProjectRoot(root);
        Require(designer.Load(file) && designer.Loaded() && !designer.Dirty(), "loading a saved UI document failed");
        auto external = original;
        external.elements.front().text = "Changed externally";
        external.Save(file);
        Require(!designer.Save(), "saving overwrote an external UI edit");
        Concord::UiDocument onDisk; onDisk.Load(file);
        Require(onDisk.Serialize() == external.Serialize(), "external changes were not preserved");

        designer.NewDocument();
        Require(designer.Loaded() && designer.Dirty() && designer.Path().empty(), "new UI did not become an unsaved document");
        Require(!designer.Save(), "an untitled UI bypassed the filename dialog");
        Require(!designer.Load(file) && designer.Path().empty() && designer.Dirty(), "opening another file dropped unsaved UI edits");
        const auto recovery = root / ".editor" / "UI.recovery.yu";
        designer.SaveRecovery(recovery);
        Require(std::filesystem::is_regular_file(recovery), "UI recovery file was not created");
        Concord::UiDocument recovered; recovered.Load(recovery);
        Require(recovered.elements.size() == 3, "UI recovery did not preserve the authored document");
        Require(designer.Dirty() && designer.Path().empty(), "writing recovery marked the document saved");

        bool refused = false;
        try { designer.SetProjectRoot(root / "AnotherProject"); }
        catch (const std::exception&) { refused = true; }
        Require(refused && designer.Dirty(), "switching projects dropped unsaved UI content");
        refused = false;
        try { designer.SaveRecovery(root.parent_path() / "outside.yu"); }
        catch (const std::exception&) { refused = true; }
        Require(refused, "UI recovery escaped the project directory");
        std::cout << "UI designer document safeguards passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
