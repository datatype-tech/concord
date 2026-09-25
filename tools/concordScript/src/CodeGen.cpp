#include "CodeGen.h"

#include "AnnotationCodeGen.h"
#include "AnnotationValues.h"
#include "KeywordSubstitution.h"
#include "Render.h"
#include "StdlibRegistry.h"

#include <cstdlib>
#include <iterator>
#include <sstream>
#include <string_view>

namespace ConcordScript {

namespace {

void Merge(GeneratedFile& destination, AnnotationOutput&& source)
{
    destination.headerText += source.headerText;
    destination.sourceText += source.sourceText;
    destination.tailSourceText += source.tailSourceText;
    destination.requiresRenderApi = destination.requiresRenderApi || source.requiresRenderApi;
    destination.usesVulkan = destination.usesVulkan || source.usesVulkan;
    destination.sidecars.insert(destination.sidecars.end(),
                                std::make_move_iterator(source.sidecars.begin()),
                                std::make_move_iterator(source.sidecars.end()));
    destination.shaders.insert(destination.shaders.end(),
                               std::make_move_iterator(source.shaders.begin()),
                               std::make_move_iterator(source.shaders.end()));
    destination.annotations.insert(destination.annotations.end(),
                                   std::make_move_iterator(source.records.begin()),
                                   std::make_move_iterator(source.records.end()));
    destination.diagnostics.insert(destination.diagnostics.end(),
                                   std::make_move_iterator(source.diagnostics.begin()),
                                   std::make_move_iterator(source.diagnostics.end()));
}

void RenderItemAnnotations(GeneratedFile& output, const std::vector<Annotation>& annotations,
                           const std::string& module, const std::string& owner,
                           AnnotationPlacement placement, size_t& ordinal)
{
    Merge(output, RenderAnnotations(annotations, module, owner, placement, ordinal));
}

bool IsLifecycleAnnotation(const Annotation& annotation, const char* name)
{
    return AnnotationValues::Lower(annotation.name) == name && annotation.hasBody;
}

void CollectLifecycle(const Annotation& annotation, std::ostringstream& startup,
                     std::ostringstream& updates, std::ostringstream& daily,
                     std::vector<GeneratedDiagnostic>& diagnostics)
{
    const std::string name = AnnotationValues::Lower(annotation.name);
    if (name == "startup" || name == "update") {
        if (!annotation.hasBody) return;
        if (name == "startup") {
            startup << SubstituteKeywords(annotation.bodyText, false) << "\n";
        } else {
            updates << "    game.OnUpdate([&](Concord::f32 deltaTime) {\n"
                     << SubstituteKeywords(annotation.bodyText, false)
                     << "\n    });\n";
        }
        return;
    }
    if (name != "daily") return;
    if (!annotation.hasBody) {
        diagnostics.push_back({"@daily requires a body", annotation.line, true});
        return;
    }
    const std::string seconds = AnnotationValues::ArgValue(annotation, "seconds");
    const std::string interval = seconds.empty() ? "86400.0f" : seconds;
    char* end = nullptr;
    const double value = std::strtod(interval.c_str(), &end);
    if (end == interval.c_str() || *end != '\0' || value <= 0.0) {
        diagnostics.push_back({"@daily seconds must be a positive number", annotation.line, true});
        return;
    }
    daily << "    {\n"
          << "        Concord::f32 concordDailyElapsed = 0.0f;\n"
          << "        game.OnUpdate([concordDailyElapsed](Concord::f32 deltaTime) mutable {\n"
          << "            concordDailyElapsed += deltaTime;\n"
          << "            if (concordDailyElapsed >= " << interval << "f) {\n"
          << SubstituteKeywords(annotation.bodyText, false)
          << "\n                concordDailyElapsed = 0.0f;\n"
          << "            }\n"
          << "        });\n"
          << "    }\n";
}

std::string InjectLifecycle(std::string entry, const std::string& startup,
                            const std::string& updates, const std::string& daily,
                            const std::string& systems)
{
    if (startup.empty() && updates.empty() && daily.empty() && systems.empty()) return entry;
    std::string lifecycle = systems + startup + updates + daily;
    const size_t run = entry.rfind("game.Run();");
    if (run != std::string::npos) {
        entry.insert(run, lifecycle);
    } else {
        entry += "\n" + lifecycle;
    }
    return entry;
}

} // namespace

GeneratedFile TranslateFile(const SourceFile& file, std::vector<std::string>& registeredClasses,
                             std::vector<std::string>& systemClasses)
{
    GeneratedFile output;
    output.relativeStem = file.moduleName;
    std::ostringstream header;
    std::ostringstream source;
    std::ostringstream classes;
    std::ostringstream freeCode;
    std::ostringstream entry;
    std::ostringstream startup;
    std::ostringstream updates;
    std::ostringstream daily;
    size_t shaderOrdinal = 0;

    header << "#ifndef CONCORDSCRIPT_" << file.moduleName << "_GEN_H\n"
           << "#define CONCORDSCRIPT_" << file.moduleName << "_GEN_H\n\n";

    for (const TopLevelItem& item : file.items) {
        switch (item.kind) {
        case TopLevelItem::Kind::Use:
            RenderItemAnnotations(output, item.use.annotations, file.moduleName,
                                  file.moduleName + "::use", AnnotationPlacement::Header, shaderOrdinal);
            if (item.use.isStdlib) {
                const std::string_view stdHeader = StdHeaderForPackage(item.use.target);
                if (stdHeader.empty()) {
                    std::string message = "unknown standard library package '" + item.use.target + "'";
                    const std::string suggestion = SuggestStdPackage(item.use.target);
                    if (!suggestion.empty()) message += "; did you mean '" + suggestion + "'?";
                    output.diagnostics.push_back({std::move(message), item.use.line, true});
                } else {
                    header << "#include <" << stdHeader << ">\n";
                }
            } else {
                header << (item.use.isEngineHeader ? RenderEngineInclude(item.use.target)
                                                    : RenderProjectInclude(item.use.target));
            }
            for (const Annotation& annotation : item.use.annotations) {
                const std::string name = AnnotationValues::Lower(annotation.name);
                if (name == "component" || name == "system") {
                    output.diagnostics.push_back({"@" + name + " must be attached to a class", annotation.line, true});
                }
            }
            break;
        case TopLevelItem::Kind::Class: {
            const ClassDecl& decl = item.classDecl;
            const std::string owner = file.moduleName + "::" + decl.name;
            RenderItemAnnotations(output, decl.annotations, file.moduleName, owner,
                                  AnnotationPlacement::Header, shaderOrdinal);
            classes << "class " << decl.name << RenderBaseClauses(decl) << " {\n"
                    << SubstituteKeywords(decl.bodyText, true) << "\n};\n\n";
            if (decl.isRegistered || decl.role == ClassDecl::Role::Component) {
                registeredClasses.push_back(owner);
            }
            if (decl.role == ClassDecl::Role::System) {
                if (decl.baseClass.empty() ||
                    (decl.baseClass != "ISystem" && decl.baseClass != "Concord::ISystem")) {
                    output.diagnostics.push_back({"@system class must extend Concord::ISystem", decl.line, true});
                } else {
                    systemClasses.push_back(owner);
                }
            }
            break;
        }
        case TopLevelItem::Kind::Entry:
            if (output.hasEntry) {
                output.diagnostics.push_back(GeneratedDiagnostic{
                    .message = "multiple @entry blocks in one file; a project may have exactly one",
                    .line = item.entry.line,
                    .isError = true});
            }
            output.hasEntry = true;
            RenderItemAnnotations(output, item.entry.annotations, file.moduleName,
                                  file.moduleName + "::entry", AnnotationPlacement::Source, shaderOrdinal);
            for (const Annotation& annotation : item.entry.annotations) {
                const std::string name = AnnotationValues::Lower(annotation.name);
                if (name == "component" || name == "system") {
                    output.diagnostics.push_back({"@" + name + " must be attached to a class", annotation.line, true});
                }
            }
            entry << SubstituteKeywords(item.entry.body, false);
            for (const Annotation& annotation : item.entry.annotations) {
                CollectLifecycle(annotation, startup, updates, daily, output.diagnostics);
            }
            break;
        case TopLevelItem::Kind::Annotation: {
            RenderItemAnnotations(output, {item.annotation}, file.moduleName,
                                  file.moduleName + "::top", AnnotationPlacement::Source, shaderOrdinal);
            const std::string annotationName = AnnotationValues::Lower(item.annotation.name);
            if (annotationName == "component" || annotationName == "system") {
                output.diagnostics.push_back({"@" + annotationName + " must be attached to a class", item.annotation.line, true});
            }
            CollectLifecycle(item.annotation, startup, updates, daily, output.diagnostics);
            break;
        }
        case TopLevelItem::Kind::Raw:
            RenderItemAnnotations(output, item.raw.annotations, file.moduleName,
                                  file.moduleName + "::raw", AnnotationPlacement::Source, shaderOrdinal);
            for (const Annotation& annotation : item.raw.annotations) {
                const std::string name = AnnotationValues::Lower(annotation.name);
                if (name == "component" || name == "system") {
                    output.diagnostics.push_back({"@" + name + " must be attached to a class", annotation.line, true});
                }
            }
            freeCode << item.raw.text;
            break;
        }
    }

    header << output.headerText << "namespace " << file.moduleName << " {\n\n"
           << classes.str() << "} // namespace " << file.moduleName << "\n\n"
           << "#endif // CONCORDSCRIPT_" << file.moduleName << "_GEN_H\n";
    source << "#include \"" << file.moduleName << ".gen.h\"\n\n";
    if (output.requiresRenderApi) {
        source << "#include <Concord/CRender.h>\n\n";
    }
    source << output.sourceText << "namespace " << file.moduleName << " {\n\n"
           << freeCode.str() << "\n} // namespace " << file.moduleName << "\n"
           << output.tailSourceText;
    if (output.hasEntry) {
        source << "\n#include \"ConcordScriptRegistry.gen.h\"\n\nint main() {\n"
               << InjectLifecycle(entry.str(), startup.str(), updates.str(), daily.str(),
                                  "    Concord::Script::AddSystems(game);\n")
               << "\n}\n";
    }
    output.headerText = header.str();
    output.sourceText = source.str();
    if (!output.annotations.empty()) {
        output.sidecars.push_back(GeneratedSidecar{
            .relativePath = file.moduleName + ".annotations.json",
            .text = RenderAnnotationManifest(output),
            .kind = "manifest",
        });
    }
    return output;
}

} // namespace ConcordScript
