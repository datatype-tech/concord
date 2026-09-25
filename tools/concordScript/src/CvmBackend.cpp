// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmBackend.h"

#include "CliReport.h"
#include "CvmEmitter.h"
#include "CvmHostModule.h"
#include "CvmHostRegistry.h"
#include "CvmLayout.h"
#include "CvmProgram.h"
#include "CvmStrings.h"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ConcordScript {

namespace {

/**
 * Owns a built LLVM module and its context. The context is declared first so
 * destruction order runs module first: ~Module() unregisters itself from its
 * still-alive context, which is required for a clean shutdown.
 */
struct CvmModule {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
};

CvmModule BuildModule(const CvmProgram& program, std::string& error)
{
    CvmModule result;
    result.context = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("ConcordVisualMachine", *result.context);
    module->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));

    // Signatures are collected before any body is emitted, so a call site knows
    // the parameter and result types the callee declared.
    std::unordered_map<std::string, CvmFunctionSignature> signatures;
    for (const CvmFunction& function : program.functions) {
        CvmFunctionSignature signature;
        signature.returnType = function.returnType;
        for (const CvmParameter& parameter : function.parameters) {
            signature.parameters.push_back(parameter.type);
        }
        signatures.emplace(function.name, std::move(signature));
    }

    llvm::IntegerType* i64 = llvm::IntegerType::getInt64Ty(*result.context);
    llvm::Type* f64 = llvm::Type::getDoubleTy(*result.context);
    llvm::Type* str = llvm::PointerType::get(*result.context, 0);

    // Records are laid out first: a signature, a global or a local may name one,
    // and the layout has to be the same object everywhere it is used.
    const std::vector<llvm::StructType*> structTypes =
        BuildRecordTypes(*result.context, program.structs);

    /** The LLVM type a CVM value type lowers to. */
    const auto toLlvm = [&](CvmValueType type) -> llvm::Type* {
        switch (type.kind) {
            case CvmType::F64: return f64;
            case CvmType::Str: return str;
            case CvmType::Void: return llvm::Type::getVoidTy(*result.context);
            case CvmType::Struct:
                return structTypes[static_cast<std::size_t>(type.structIndex)];
            case CvmType::I64: break;
        }
        return i64;
    };

    // Globals come first: every body may read or write them, so they have to
    // exist before the first body is emitted.
    std::unordered_map<std::string, CvmGlobalSlot> globals;
    for (const CvmGlobal& global : program.globals) {
        llvm::Constant* initial = nullptr;
        switch (global.type.kind) {
            case CvmType::F64:
                initial = llvm::ConstantFP::get(f64, global.floatValue);
                break;
            case CvmType::Str:
                initial = CreateStringConstant(*module, "cvm.global." + global.name,
                                               global.textValue);
                break;
            case CvmType::Struct: {
                // A record global is a constant aggregate: every field is a
                // literal, because a global is built before any function runs.
                const CvmStruct& record =
                    program.structs[static_cast<std::size_t>(global.type.structIndex)];
                std::vector<llvm::Constant*> fields;
                fields.reserve(record.fields.size());
                for (std::size_t index = 0; index < record.fields.size(); ++index) {
                    const double value = global.fieldValues[index];
                    fields.push_back(record.fields[index].type.kind == CvmType::F64
                                         ? llvm::ConstantFP::get(f64, value)
                                         : llvm::ConstantInt::get(
                                               i64, static_cast<std::uint64_t>(
                                                        static_cast<std::int64_t>(value)),
                                               /*isSigned=*/true));
                }
                initial = llvm::ConstantStruct::get(
                    structTypes[static_cast<std::size_t>(global.type.structIndex)], fields);
                break;
            }
            default:
                initial =
                    llvm::ConstantInt::get(i64, static_cast<std::uint64_t>(global.integerValue),
                                           /*isSigned=*/true);
                break;
        }
        auto* variable = new llvm::GlobalVariable(*module, toLlvm(global.type),
                                                  /*isConstant=*/false,
                                                  llvm::GlobalValue::InternalLinkage, initial,
                                                  global.name);
        globals.emplace(global.name, CvmGlobalSlot{variable, global.type});
    }

    for (const CvmFunction& function : program.functions) {
        std::vector<llvm::Type*> parameters;
        parameters.reserve(function.parameters.size());
        for (const CvmParameter& parameter : function.parameters) {
            parameters.push_back(toLlvm(parameter.type));
        }
        llvm::Type* result = toLlvm(function.returnType);
        llvm::Function* created = llvm::Function::Create(
            llvm::FunctionType::get(result, parameters, false),
            llvm::Function::ExternalLinkage, function.name, *module);
        // Arguments are named after the source parameters so the emitted IR
        // reads like the .cx it came from rather than as %0, %1, %2.
        for (std::size_t index = 0; index < function.parameters.size(); ++index) {
            created->getArg(static_cast<unsigned>(index))
                ->setName(function.parameters[index].name);
        }
    }

    CvmEmitScope scope;
    scope.programFunctions = &signatures;
    scope.globals = &globals;
    scope.structs = &program.structs;
    scope.structTypes = &structTypes;

    // Host functions are declared by the emitter on first use rather than
    // up front, so a module only ever names the host surface it actually calls.
    for (const CvmFunction& function : program.functions) {
        llvm::Function* declared = module->getFunction(function.name);
        if (!EmitCvmFunction(*module, *declared, function.body, function.line, function.parameters,
                             function.returnType, scope, error)) {
            return {};
        }
    }

    if (llvm::verifyModule(*module, &llvm::errs())) {
        error = "LLVM module verification failed";
        return {};
    }
    result.module = std::move(module);
    return result;
}

bool WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
    return static_cast<bool>(output);
}

bool WriteBitcode(const std::filesystem::path& path, llvm::Module& module)
{
    std::error_code error;
    llvm::raw_fd_ostream output(path.string(), error, llvm::sys::fs::OF_None);
    if (error) return false;
    llvm::WriteBitcodeToFile(module, output);
    return true;
}

bool EmitAotObject(llvm::Module& module, const std::filesystem::path& path, std::string& error)
{
    std::string lookupError;
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(module.getTargetTriple(), lookupError);
    if (!target) {
        error = "cannot resolve native target: " + lookupError;
        return false;
    }
    llvm::TargetOptions options;
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
        llvm::Triple(module.getTargetTriple()), "generic", "", options,
        llvm::Reloc::Model::PIC_));
    if (!machine) {
        error = "cannot create LLVM target machine";
        return false;
    }
    std::error_code streamError;
    llvm::raw_fd_ostream output(path.string(), streamError, llvm::sys::fs::OF_None);
    if (streamError) {
        error = "cannot open AOT object: " + streamError.message();
        return false;
    }
    llvm::legacy::PassManager passes;
    if (machine->addPassesToEmitFile(passes, output, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        error = "native target cannot emit object files";
        return false;
    }
    passes.run(module);
    output.flush();
    return true;
}

/** MinGW codegen inserts a call to __main in the entry function; the JIT has no CRT. */
extern "C" void CvmNoopMain() {}

/**
 * Every host module the caller named, searched in the order they were given.
 *
 * A set rather than one module because the engine is two: Runtime.dll exports
 * the C ABI, and Render.dll self-registers a backend when it is loaded. Neither
 * is linked into the compiler, so both arrive through --cvm-host.
 */
struct CvmHostSet {
    std::vector<CvmHostModule> modules;

    bool Empty() const { return modules.empty(); }

    /** Returns the first module that exports \p symbol, or null. */
    void* Resolve(std::string_view symbol) const
    {
        for (const CvmHostModule& module : modules) {
            if (void* address = module.Resolve(symbol)) return address;
        }
        return nullptr;
    }
};

/**
 * Defines every reachable host symbol in the JIT's main library.
 *
 * The built-in standard library is bound whole, not only where this program
 * calls it, because a .cx file can also take a host function's address (for
 * example passing one to thread_spawn). Binding is cheap: it is one map entry
 * per row and no code is generated for a symbol nobody references.
 *
 * The host-module surface is bound only for symbols the module actually
 * resolved, so a missing engine export stays missing and is reported by
 * UnresolvedHostModuleSymbols instead of becoming a null call target.
 */
llvm::orc::SymbolMap HostSymbols(llvm::orc::LLJIT& jit, const CvmHostSet& host)
{
    llvm::orc::SymbolMap symbols;
    llvm::orc::ExecutionSession& session = jit.getExecutionSession();
    for (const CvmHostSignature& signature : HostFunctions()) {
        symbols[session.intern(signature.symbol)] = llvm::orc::ExecutorSymbolDef(
            llvm::orc::ExecutorAddr::fromPtr(signature.address), llvm::JITSymbolFlags::Exported);
    }
    {
        for (const CvmHostSignature& signature : HostModuleFunctions()) {
            void* address = host.Resolve(signature.symbol);
            if (address == nullptr) continue;
            symbols[session.intern(signature.symbol)] = llvm::orc::ExecutorSymbolDef(
                llvm::orc::ExecutorAddr::fromPtr(address), llvm::JITSymbolFlags::Exported);
        }
    }
    symbols[session.intern("__main")] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr::fromPtr(&CvmNoopMain), llvm::JITSymbolFlags::Exported);
    return symbols;
}

/**
 * Names the host-module functions this module calls but cannot resolve.
 *
 * The emitter declares a host function only where it is referenced, so a
 * declaration without a body is exactly "this program needs it". Catching that
 * here turns what would be an opaque JIT linker failure into a diagnostic that
 * names the missing engine entry points and says how to supply them.
 */
std::vector<std::string> UnresolvedHostModuleSymbols(const llvm::Module& module,
                                                     const CvmHostSet& host)
{
    std::vector<std::string> unresolved;
    for (const CvmHostSignature& signature : HostModuleFunctions()) {
        const llvm::Function* declared = module.getFunction(signature.symbol);
        if (declared == nullptr || !declared->isDeclaration()) continue;
        if (host.Resolve(signature.symbol) != nullptr) continue;
        unresolved.emplace_back(signature.symbol);
    }
    return unresolved;
}

int RunJit(std::unique_ptr<llvm::Module> module, std::unique_ptr<llvm::LLVMContext> context,
           const std::string& entry, const CvmHostSet& host, std::string& error)
{
    auto jitExpected = llvm::orc::LLJITBuilder().create();
    if (!jitExpected) {
        std::string message = "cannot create LLVM LLJIT";
        llvm::handleAllErrors(jitExpected.takeError(), [&message](llvm::ErrorInfoBase& info) {
            message += ": ";
            message += info.message();
        });
        error = std::move(message);
        return 1;
    }
    auto jit = std::move(*jitExpected);

    if (auto definitionError =
            jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(HostSymbols(*jit, host)))) {
        error = "cannot register CVM host symbols";
        llvm::consumeError(std::move(definitionError));
        return 1;
    }

    if (auto addError =
            jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context)))) {
        error = "cannot add CVM module to the JIT";
        llvm::consumeError(std::move(addError));
        return 1;
    }

    auto symbol = jit->lookup(entry);
    if (!symbol) {
        error = "CVM entry '" + entry + "' was not found in the JIT";
        llvm::consumeError(symbol.takeError());
        return 1;
    }
    const auto function = symbol->toPtr<std::int64_t (*)()>();
    const std::int64_t result = function();
    // printf rather than std::cout: cvm_print writes through stdio, and one
    // stream keeps the program's output and this summary in execution order.
    std::printf("cvm jit result: %lld\n", static_cast<long long>(result));
    std::fflush(stdout);
    return 0;
}

} // namespace

int CompileCvmProject(const std::vector<SourceFile>& files,
                      const std::filesystem::path& outputDirectory,
                      const CvmOptions& options)
{
    std::string error;
    const auto program = CollectCvmProgram(files, error);
    if (!program) {
        ReportError(error);
        return 1;
    }

    // The host module loads before anything is emitted, so a bad path fails
    // before a single artifact has been written.
    CvmHostSet host;
    for (const std::filesystem::path& path : options.hostModules) {
        std::optional<CvmHostModule> module = CvmHostModule::Load(path, error);
        if (!module) {
            ReportError("CVM host: " + error);
            return 1;
        }
        host.modules.push_back(std::move(*module));
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    CvmModule built = BuildModule(*program, error);
    if (!built.module) {
        ReportError("CVM: " + error);
        return 1;
    }
    llvm::Module& module = *built.module;

    // A JIT run needs every symbol it calls to exist in this process. An AOT
    // object deliberately does not: the names are the host's to link.
    const std::vector<std::string> unresolved = UnresolvedHostModuleSymbols(module, host);
    if (!unresolved.empty() && options.mode == CvmMode::Jit) {
        std::string names;
        for (const std::string& symbol : unresolved) {
            if (!names.empty()) names += ", ";
            names += symbol;
        }
        if (host.Empty()) {
            ReportError("CVM: this program calls the Concord engine (" + names +
                        ") but no host module was given; pass --cvm-host <dll>, for example "
                        "--cvm-host build/ConcordFlashGameEngineRuntime.dll");
        } else {
            std::string modules;
            for (const std::filesystem::path& path : options.hostModules) {
                if (!modules.empty()) modules += ", ";
                modules += path.string();
            }
            ReportError("CVM host: none of [" + modules + "] exports " + names +
                        "; the engine needs both ConcordFlashGameEngineRuntime.dll and "
                        "ConcordFlashGameEngineRender.dll");
        }
        return 1;
    }

    std::filesystem::create_directories(outputDirectory);
    std::string ir;
    llvm::raw_string_ostream irStream(ir);
    module.print(irStream, nullptr);
    irStream.flush();
    if (!WriteText(outputDirectory / "ConcordVisualMachine.ll", ir) ||
        !WriteBitcode(outputDirectory / "ConcordVisualMachine.bc", module)) {
        ReportError("CVM: cannot write LLVM IR/bitcode artifacts");
        return 1;
    }

    if (options.mode == CvmMode::Aot) {
        const std::filesystem::path object = outputDirectory / "ConcordVisualMachine.obj";
        if (!EmitAotObject(module, object, error)) {
            ReportError("CVM AOT: " + error);
            return 1;
        }
        // Naming the engine entry points the object still needs is the whole
        // AOT contract; without it the host finds out at link time.
        for (const std::string& symbol : unresolved) {
            ReportNote("CVM AOT: unresolved host symbol " + symbol +
                       " (link ConcordCvmRuntime and the engine)");
        }
        ReportSuccess("CVM AOT artifacts written to " + outputDirectory.string());
        return 0;
    }

    if (options.run) {
        const int status = RunJit(std::move(built.module), std::move(built.context),
                                  program->entry, host, error);
        if (status != 0) ReportError("CVM JIT: " + error);
        return status;
    }
    ReportSuccess("CVM LLVM artifacts written to " + outputDirectory.string());
    return 0;
}

} // namespace ConcordScript
