// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMHOSTREGISTRY_H
#define CONCORDSCRIPT_CVMHOSTREGISTRY_H

/**
 * The registry of C-ABI host functions a @cvm block may call.
 *
 * A CVM module is LLVM IR. IR can call a C ABI and nothing else: no C++ class,
 * no template, no overload, no exception. Every API that should be reachable
 * from .cx therefore has to arrive as a plain C symbol, and this table is the
 * single place that decides which symbols those are and what shape they have.
 *
 * The table is deliberately free of any LLVM type. It describes signatures in
 * the CvmHostType vocabulary and leaves materialising an llvm::FunctionType to
 * the caller, so the registry stays a data table that a future tool (a
 * --cvm-list-hosts dump, a binding generator, a test) can read without
 * dragging in LLVM.
 *
 * Each entry carries two names on purpose:
 *
 * - \c name is the CX-visible identifier, chosen for the language
 *   ("list_push"). It is what a .cx file writes.
 * - \c symbol is the link name owned by the runtime library ("cvm_list_push").
 *
 * Keeping them distinct lets the runtime keep a collision-proof C prefix while
 * the language surface stays readable, and it means renaming what a .cx file
 * writes never has to touch a C file.
 */

#include "CvmTypes.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace ConcordScript {

/** A value class crossing the CVM/host boundary. */
using CvmHostType = CvmType;

/** One registered host function. */
struct CvmHostSignature {
    /** Identifier a .cx file writes, for example "list_push". */
    const char* name;

    /** Link name the runtime library exports, for example "cvm_list_push". */
    const char* symbol;

    /** Result class. */
    CvmHostType result;

    /** Parameter classes in call order; may be null when paramCount is 0. */
    const CvmHostType* params;

    /** Number of entries in \c params. */
    std::size_t paramCount;

    /** Host implementation address, used to define the JIT symbol. */
    void* address;
};

/**
 * A host parameter that receives the address of a CVM function.
 *
 * An address is an integer, so nothing about the call itself reveals what shape
 * the callee must have. When the argument is written as a bare function name the
 * compiler does know which function it is, and this table is what lets it say
 * so: a two-parameter function handed to thread_spawn_arg becomes a compile
 * error instead of a wrong answer at run time.
 */
struct CvmCallbackSlot {
    /** Symbol of the host function that takes the address. */
    const char* symbol;

    /** Zero-based index of the parameter receiving it. */
    std::size_t parameter;

    /** Required parameter types; null when parameterCount is 0. */
    const CvmType* parameters;

    /** Number of required parameters. */
    std::size_t parameterCount;

    /** Required result type. */
    CvmType returnType;
};

/** Returns every declared callback slot. */
std::span<const CvmCallbackSlot> CallbackSlots();

/** Returns the slot for \p parameter of \p symbol, or null when there is none. */
const CvmCallbackSlot* FindCallbackSlot(std::string_view symbol, std::size_t parameter);

/**
 * Returns the functions linked into the compiler, in registration order.
 *
 * These are the built-in standard library: their addresses are known when the
 * compiler is built, so the JIT binds them directly.
 */
std::span<const CvmHostSignature> HostFunctions();

/**
 * Returns the functions an external host module provides.
 *
 * The Concord engine binding lives here rather than in HostFunctions() because
 * the compiler must not link the engine: cc.exe stays a standalone tool, and
 * these symbols are resolved at run time from the module named by --cvm-host.
 * Their addresses are therefore null, and an AOT object simply references the
 * names for the real host to link.
 */
std::span<const CvmHostSignature> HostModuleFunctions();

/** Returns the function whose CX-visible name is \p name, or null. */
const CvmHostSignature* FindHostFunction(std::string_view name);

/** Renders a signature for diagnostics, for example "list_push(i64, i64) -> i64". */
std::string DescribeHostSignature(const CvmHostSignature& signature);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMHOSTREGISTRY_H
