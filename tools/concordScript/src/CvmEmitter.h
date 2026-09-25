// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMEMITTER_H
#define CONCORDSCRIPT_CVMEMITTER_H

/**
 * Lowers the body of one @cvm block to LLVM IR.
 *
 * The emitter owns the CVM language: statements, typed expressions, locals,
 * parameters, records, the shared globals a project declares, and calls into
 * the host registry. It is deliberately separate from CvmBackend.cpp, which
 * owns the module, the artifacts and the JIT, so the language can be read and
 * tested without reading the driver.
 *
 * ## Types
 *
 * An expression is i64, f64, str or a record the project declared. Widening an
 * i64 to an f64 is implicit because it is lossless and the alternative is a
 * cast at every call site; everything else has to match. Use the \c to_i64 and
 * \c to_f64 builtins when a numeric conversion is meant, and the \c str_*
 * functions when text is involved, because neither happens on its own.
 */

#include "CvmProgram.h"
#include "CvmTypes.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ConcordScript {

/** A module-level variable and the type it holds. */
struct CvmGlobalSlot {
    llvm::GlobalVariable* variable = nullptr;
    CvmValueType type;
};

/**
 * The signature of a program function, as a call site needs it.
 *
 * Carried explicitly rather than read back off the emitted llvm::Function: the
 * mapping from a CVM type to an LLVM type is not injective, so reverse
 * engineering one from the other silently mis-types anything a two-way test
 * does not anticipate.
 */
struct CvmFunctionSignature {
    /** Parameter types, in order. */
    std::vector<CvmValueType> parameters;

    /** Declared result type. */
    CvmValueType returnType;
};

/** What a function body can name besides its own locals and parameters. */
struct CvmEmitScope {
    /** Every @cvm function in the project, so a call or an address resolves. */
    const std::unordered_map<std::string, CvmFunctionSignature>* programFunctions = nullptr;

    /** Module-level variables, so a name reads or writes the shared slot. */
    const std::unordered_map<std::string, CvmGlobalSlot>* globals = nullptr;

    /** Declared records, so a type name and its fields resolve. */
    const std::vector<CvmStruct>* structs = nullptr;

    /** The LLVM layout of each record, in the same order as \c structs. */
    const std::vector<llvm::StructType*>* structTypes = nullptr;
};

/**
 * Emits \p body into \p function, which must already be declared with the
 * signature its parameters call for.
 *
 * \p sourceLine is the one-based line of the annotation, used to make
 * diagnostics point at the .cx file. On failure returns false and fills
 * \p error with a message beginning \c "@name".
 */
bool EmitCvmFunction(llvm::Module& module, llvm::Function& function, std::string_view body,
                     int sourceLine, const std::vector<CvmParameter>& parameters,
                     CvmValueType returnType, const CvmEmitScope& scope, std::string& error);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMEMITTER_H
