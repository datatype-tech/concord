// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMHOSTMODULE_H
#define CONCORDSCRIPT_CVMHOSTMODULE_H

/**
 * A dynamically loaded module supplying the registered host-module surface.
 *
 * The compiler must not link the engine: cc.exe is a standalone tool, and an
 * AOT object is meant to be linked by the real host. So the Concord binding
 * arrives the other way round -- the caller names a module with --cvm-host and
 * the JIT binds whatever that module exports, by name, at run time.
 *
 * Loading resolves every registered host-module symbol up front rather than
 * lazily, so a module that is missing part of the surface is reported once, as
 * a list, instead of failing later at the first unresolved call.
 */

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ConcordScript {

class CvmHostModule {
public:
    CvmHostModule() = default;
    ~CvmHostModule();

    CvmHostModule(const CvmHostModule&) = delete;
    CvmHostModule& operator=(const CvmHostModule&) = delete;
    CvmHostModule(CvmHostModule&& other) noexcept;
    CvmHostModule& operator=(CvmHostModule&& other) noexcept;

    /**
     * Loads \p path and resolves every registered host-module symbol.
     *
     * Returns nullopt and fills \p error when the module cannot be loaded. A
     * module that loads but does not export a registered symbol is not an
     * error here: the caller decides, because only a program that actually
     * calls the missing symbol has a problem.
     */
    static std::optional<CvmHostModule> Load(const std::filesystem::path& path, std::string& error);

    /** Returns the address \p symbol resolved to, or null when it did not. */
    void* Resolve(std::string_view symbol) const;

    /** Registered symbols this module does not export, in registration order. */
    const std::vector<std::string>& Missing() const noexcept { return missing_; }

    /**
     * The ABI revision the module reports, or -1 when it exports no version
     * entry point. ConcordScript declares the engine signatures itself, so this
     * number is how a drift between the two sides is noticed.
     */
    std::int64_t ApiVersion() const noexcept { return apiVersion_; }

private:
    /** The platform library handle, kept opaque so no header includes Windows. */
    void* library_ = nullptr;

    std::unordered_map<std::string, void*> symbols_;
    std::vector<std::string> missing_;
    std::int64_t apiVersion_ = -1;
};

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMHOSTMODULE_H
