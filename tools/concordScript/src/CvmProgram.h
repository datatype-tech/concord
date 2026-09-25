// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMPROGRAM_H
#define CONCORDSCRIPT_CVMPROGRAM_H

/**
 * The project-wide view of every declaration a CVM project makes.
 *
 * Collection is separated from code generation because it is the only part of
 * the CVM path that reads the whole project at once: it decides which function
 * is the entry point, resolves the shared global namespace and the struct table,
 * rejects duplicates, and enforces that a project contains nothing the backend
 * cannot lower.
 */

#include "Ast.h"
#include "CvmTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ConcordScript {

/** One field of a declared record. */
struct CvmField {
    /** Name the body refers to after a dot. */
    std::string name;

    /** Resolved field type. */
    CvmValueType type;
};

/**
 * A record declared with \c struct.
 *
 * Fields are laid out in declaration order and the layout is the compiler's
 * alone: nothing outside CVM sees one of these, because the host ABI has only
 * the three simple kinds.
 */
struct CvmStruct {
    /** Type name, used as the constructor and in annotations. */
    std::string name;

    /** Fields in declaration order. */
    std::vector<CvmField> fields;

    /** One-based line of the declaration, used to locate diagnostics. */
    int line = 0;
};

/** One declared parameter: a name and the type it carries. */
struct CvmParameter {
    /** Name the body refers to. */
    std::string name;

    /** Declared type; i64 when the declaration wrote no annotation. */
    CvmValueType type = Simple(CvmType::I64);
};

/** One @cvm block, exactly as its annotation carried it. */
struct CvmFunction {
    /** Function name; a block without name= is called "main". */
    std::string name;

    /** Parameters from args="a, b: f64", in order. */
    std::vector<CvmParameter> parameters;

    /**
     * Result type, from ret="f64"; i64 when the annotation names none.
     *
     * Declared rather than inferred so the emitted signature is decided by the
     * .cx file alone. It matters beyond tidiness: the engine calls a frame
     * callback as i64(f64), so a callback that quietly returned f64 would be
     * called through the wrong signature.
     */
    CvmValueType returnType = Simple(CvmType::I64);

    /** Statement text between the annotation braces. */
    std::string body;

    /** One-based line of the annotation, used to locate diagnostics. */
    int line = 0;
};

/**
 * One module-level variable, shared by every @cvm function.
 *
 * Globals exist because a frame callback needs somewhere to keep state: a
 * function that runs once per frame cannot return its state to anyone, and
 * CVM has no closures. They are also how a program names the constants it
 * would otherwise repeat, such as key codes.
 */
struct CvmGlobal {
    /** Variable name, visible in every @cvm function of the project. */
    std::string name;

    /** Declared or inferred type. */
    CvmValueType type = Simple(CvmType::I64);

    /** Initial value; meaningful when \c type is i64. */
    std::int64_t integerValue = 0;

    /** Initial value; meaningful when \c type is f64. */
    double floatValue = 0.0;

    /** Initial value; meaningful when \c type is str. */
    std::string textValue;

    /**
     * Initial field values, in declaration order, when the type is a struct.
     *
     * A record global has to be initializable from constants, because a global
     * is built before any function runs and there is nothing for a richer
     * initializer to call.
     */
    std::vector<double> fieldValues;

    /** One-based line of the declaration, used to locate diagnostics. */
    int line = 0;
};

/** Everything a CVM project declares. */
struct CvmProgram {
    std::vector<CvmStruct> structs;
    std::vector<CvmFunction> functions;
    std::vector<CvmGlobal> globals;
    std::string entry;
};

/**
 * Gathers the declarations of every source file into one program.
 *
 * Returns nullopt and fills \p error when a declaration is malformed, two
 * share a name, more than one block claims entry, the entry is never defined,
 * or the project contains anything other than @cvm, @cvm_global, struct and
 * comments.
 */
std::optional<CvmProgram> CollectCvmProgram(const std::vector<SourceFile>& files,
                                            std::string& error);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMPROGRAM_H
