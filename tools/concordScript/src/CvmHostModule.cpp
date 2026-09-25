// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHostModule.h"

#include "CliReport.h"
#include "CvmHostRegistry.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <system_error>
#include <utility>

namespace ConcordScript {

namespace {

/**
 * The engine C ABI revision this table was written against, matching
 * kCvmApiVersion in concord/src/engine/cvm/CvmRun.cpp. A module reporting a
 * different number is still loaded, but the mismatch is printed, because a
 * changed signature would otherwise show up as scrambled arguments at run time.
 */
constexpr std::int64_t kExpectedHostApiVersion = 12;

/** The name of the engine entry point that reports the ABI revision. */
constexpr const char* kVersionSymbol = "ConcordCvmVersion";

} // namespace

CvmHostModule::~CvmHostModule()
{
    if (library_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(library_));
    }
}

CvmHostModule::CvmHostModule(CvmHostModule&& other) noexcept
    : library_(other.library_), symbols_(std::move(other.symbols_)),
      missing_(std::move(other.missing_)), apiVersion_(other.apiVersion_)
{
    other.library_ = nullptr;
    other.apiVersion_ = -1;
}

CvmHostModule& CvmHostModule::operator=(CvmHostModule&& other) noexcept
{
    if (this == &other) return *this;
    if (library_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(library_));
    }
    library_ = other.library_;
    symbols_ = std::move(other.symbols_);
    missing_ = std::move(other.missing_);
    apiVersion_ = other.apiVersion_;
    other.library_ = nullptr;
    other.apiVersion_ = -1;
    return *this;
}

std::optional<CvmHostModule> CvmHostModule::Load(const std::filesystem::path& path,
                                                 std::string& error)
{
    // The loader needs an absolute path with native separators: a forward-slash
    // path (which is what CMake hands over) makes LoadLibraryExW fail with
    // ERROR_MOD_NOT_FOUND, indistinguishable from a missing dependency.
    std::error_code pathError;
    std::filesystem::path resolved = std::filesystem::absolute(path, pathError);
    if (pathError) resolved = path;
    resolved.make_preferred();

    // LOAD_WITH_ALTERED_SEARCH_PATH resolves the module's own dependencies
    // relative to the module, not to cc.exe. The engine DLL sits beside SDL3.dll
    // and phonon.dll in a build tree that is not the compiler's directory, so a
    // plain LoadLibrary would fail on the first dependency instead of finding it.
    const HMODULE loaded =
        LoadLibraryExW(resolved.wstring().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (loaded == nullptr) {
        error = "cannot load host module '" + resolved.string() + "' (error " +
                std::to_string(GetLastError()) + ")";
        return std::nullopt;
    }

    CvmHostModule module;
    module.library_ = loaded;
    for (const CvmHostSignature& signature : HostModuleFunctions()) {
        const auto* address =
            reinterpret_cast<const void*>(GetProcAddress(loaded, signature.symbol));
        if (address == nullptr) {
            module.missing_.emplace_back(signature.symbol);
            continue;
        }
        module.symbols_.emplace(signature.symbol, const_cast<void*>(address));
    }

    // Only a module that offers a version can disagree about one. Render.dll
    // exports no such entry point and is not supposed to, so its silence must
    // not be reported as a mismatch.
    const auto version =
        reinterpret_cast<const std::int64_t (*)(void)>(GetProcAddress(loaded, kVersionSymbol));
    if (version != nullptr) {
        module.apiVersion_ = version();
        if (module.apiVersion_ != kExpectedHostApiVersion) {
            ReportWarning("CVM host: " + path.string() + " reports ABI version " +
                          std::to_string(module.apiVersion_) + ", this compiler expects " +
                          std::to_string(kExpectedHostApiVersion));
        }
    }
    return module;
}

void* CvmHostModule::Resolve(std::string_view symbol) const
{
    const auto found = symbols_.find(std::string(symbol));
    return found == symbols_.end() ? nullptr : found->second;
}

} // namespace ConcordScript
