#include "CliOptions.h"

#include "CliReport.h"

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ConcordScript {

CliOptions ParseArgs(int argc, char** argv, bool defaultToCvm)
{
    CliOptions options;
    SetToolName(argc > 0 ? argv[0] : "concordc");
    ConfigureColor(ColorMode::Auto);
    bool modeSelected = false;
    bool runSelected = false;
    std::unordered_set<std::string> unique;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto equals = argument.find('=');
        const std::string flag = argument.substr(0, equals);
        const bool attached = equals != std::string::npos;
        const auto Value = [&]() -> std::string {
            std::string value;
            if (attached) value = argument.substr(equals + 1);
            else if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0 &&
                     std::string(argv[index + 1]) != "-h" && std::string(argv[index + 1]) != "-V") {
                value = argv[++index];
            }
            if (value.empty()) throw std::runtime_error("missing value for " + flag);
            return value;
        };
        const auto Once = [&]() {
            if (!unique.insert(flag).second) throw std::runtime_error("duplicate option " + flag);
        };
        const auto SelectMode = [&]() {
            if (modeSelected) throw std::runtime_error("compiler modes are mutually exclusive");
            modeSelected = true;
        };
        const bool takesValue = flag == "--project" || flag == "--out" || flag == "--color" ||
                                flag == "--cvm-host" || flag == "--cvm" || flag == "--init";
        if (attached && !takesValue) throw std::runtime_error("option does not accept a value: " + flag);
        if (flag == "--init") {
            Once();
            options.initDir = Value();
        } else if (flag == "--project" || flag == "--out") {
            Once();
            (flag == "--project" ? options.projectDir : options.outDir) = Value();
        } else if (flag == "--cpp") {
            SelectMode();
            options.cpp = true;
        } else if (flag == "--cvm" || flag == "--jit" || flag == "--aot") {
            SelectMode();
            std::string mode = flag == "--aot" ? "aot" : "jit";
            if (flag == "--cvm" && (attached ||
                (index + 1 < argc && std::string(argv[index + 1]).rfind("-", 0) != 0))) mode = Value();
            if (mode != "jit" && mode != "aot") throw std::runtime_error("unknown CVM mode '" + mode + "'; use jit or aot");
            options.cvmMode = mode == "aot" ? CvmMode::Aot : CvmMode::Jit;
        } else if (flag == "--cvm-host") {
            options.cvmHosts.emplace_back(Value());
        } else if (flag == "--run" || flag == "--no-run") {
            if (runSelected) throw std::runtime_error("--run and --no-run may only be specified once");
            runSelected = true;
            options.runCvm = flag == "--run";
        } else if (flag == "--check") {
            Once();
            options.check = true;
        } else if (flag == "--color") {
            const std::string value = Value();
            if (!ParseColorMode(value, options.color)) throw std::runtime_error("unknown colour mode '" + value + "'; use auto, always or never");
            ConfigureColor(options.color);
        } else if (flag == "--no-color" || flag == "--no-diagnostics-color") {
            options.color = ColorMode::Never;
            ConfigureColor(options.color);
        } else if (flag == "--help" || flag == "-h") {
            options.help = true;
        } else if (flag == "--version" || flag == "-V") {
            options.version = true;
        } else {
            throw std::runtime_error("unknown argument '" + argument + "'; run --help for usage");
        }
    }
    if (!modeSelected) options.cpp = !defaultToCvm;
    if (options.help || options.version) return options;
    if (!options.initDir.empty()) {
        if (!options.projectDir.empty() || !options.outDir.empty() || options.check ||
            modeSelected || runSelected || !options.cvmHosts.empty()) {
            throw std::runtime_error("--init cannot be combined with compilation options");
        }
        return options;
    }
    if (options.projectDir.empty() || (!options.check && options.outDir.empty())) {
        throw std::runtime_error("missing --project and --out (omit --out only with --cpp --check)");
    }
    if (options.cpp && (!options.cvmHosts.empty() || runSelected)) {
        throw std::runtime_error("--cvm-host and --run/--no-run only apply to the CVM backend");
    }
    if (options.check && !options.cpp) throw std::runtime_error("--check requires the C++ backend; use --cpp --check");
    if (!std::filesystem::is_directory(options.projectDir)) {
        throw std::runtime_error("project directory does not exist: " + options.projectDir.string());
    }
    options.projectDir = std::filesystem::canonical(options.projectDir);
    if (!options.outDir.empty()) options.outDir = std::filesystem::weakly_canonical(options.outDir);
    return options;
}

} // namespace ConcordScript
