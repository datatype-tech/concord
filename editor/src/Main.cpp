// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Localization.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int)
{
    int argc=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args;for(int i=0;i<argc;++i)args.emplace_back(arguments[i]);LocalFree(arguments);
    try {
        const bool manager=args.size()<2 || args[1]==L"--manager";
        std::filesystem::path project=manager?std::filesystem::path{}:std::filesystem::absolute(args[1]);
        std::filesystem::path cli=args.size()>2?args[2]:L"";
        std::filesystem::path sdk=args.size()>3?args[3]:L"";
        wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
        auto directory=std::filesystem::path(module).parent_path();
        std::filesystem::current_path(directory);
        for(int level=0;level<5 && !directory.empty();++level,directory=directory.parent_path()) {
            if(cli.empty())for(auto candidate:{directory/L"concord.exe",directory/L"bin/releases/concord.exe"})
                if(std::filesystem::is_regular_file(candidate)){cli=candidate;break;}
            if(sdk.empty()) {
                const auto development=directory/L"bin/editor-sdk";
                if(std::filesystem::exists(development/L"include/Concord/CUiDocument.h")) {sdk=development;continue;}
                const auto releases=directory/L"bin/releases";
                if(std::filesystem::is_directory(releases))for(const auto& item:std::filesystem::directory_iterator(releases))
                    if(item.is_directory() && std::filesystem::exists(item.path()/L"bin/concordc.exe")) {sdk=item.path();break;}
            }
        }
        Concord::Window window({.title=manager?"Concord Flash":Concord::Editor::Utf8Text(project.filename())+" - Concord Flash",
            .resolution=manager?Concord::Resolution{1360,900}:Concord::Resolution{1680,1040},.visible=!std::getenv("CONCORD_EDITOR_SMOKE"),.vsync=true,
            .decorated=false,.minimumResolution={960,600}});
        Concord::Game game({.enableRayTracing=false,.frameRateLimit=90,.enableUiToolkit=true,.uiGlyphText=Concord::Editor::TranslationGlyphs()});
        game.AttachWindow(window);
        if(!window.IsOpen() || !game.Toolkit())return 1;
        {
            Concord::Editor::Workspace workspace(game,window,cli,sdk,manager);
            if(!manager)workspace.OpenFile(project);
            game.LoadScene(workspace.LiveScene());
            game.OnUpdate([&](float) {
                workspace.Tick();
                if(std::getenv("CONCORD_EDITOR_SMOKE") && game.FrameCount()>100)game.Quit();
            });
            game.Run();game.OnUpdate({});
        }
        game.DetachWindow();
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"Concord Editor: %s\n",error.what());
        if(!std::getenv("CONCORD_EDITOR_SMOKE"))MessageBoxA(nullptr,error.what(),"Concord Editor",MB_OK|MB_ICONERROR);
        return 1;
    }
}
