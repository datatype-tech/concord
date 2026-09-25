// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMTYPES_H
#define CONCORDSCRIPT_CVMTYPES_H

/**
 * The types a CVM value can have.
 *
 * There are four kinds, and the language needs exactly these:
 *
 * - \c I64 for everything that names, counts or indexes something -- an entity
 *   handle, a list, a key code, a comparison result. The host ABI passes those
 *   as 64-bit integers.
 * - \c F64 for everything physical -- a position, an angle, a speed, a delta
 *   time. Rounding a metre to a millimetre at every call site is how a binding
 *   becomes unpleasant to use.
 * - \c Str for text. It is a pointer at the ABI level, but a script never sees
 *   it as one: the only things that can be done with a string are the ones the
 *   stdlib declares.
 * - \c Struct for a record the script declared. Unlike the other three it is
 *   not a single kind of thing, so it carries an index into the program's
 *   struct table as well.
 *
 * \c Void exists only for a host entry point that reports through an effect
 * rather than a result; a variable can never have it.
 *
 * The two enums are separate on purpose. \c CvmType is what the host registry
 * speaks -- a host function's parameter is one of three simple kinds, and a
 * record cannot cross that boundary because the host has no such type. The
 * language speaks \c CvmValueType, which is a simple kind or a struct.
 */

namespace ConcordScript {

/** The kinds a value or a host signature can have. */
enum class CvmType { I64, F64, Str, Struct, Void };

/**
 * A value type: one of the simple kinds, or a record named by index.
 *
 * Deliberately not an enum. A struct is not one type, it is one type per
 * declaration, and a plain enumerator could not tell two of them apart -- which
 * is exactly the mistake that would let a Point be passed where a Color is
 * wanted.
 */
struct CvmValueType {
    /** Which kind this is. */
    CvmType kind = CvmType::I64;

    /** Index into the program's struct table; only meaningful for Struct. */
    int structIndex = -1;

    friend bool operator==(CvmValueType, CvmValueType) = default;
};

/** Wraps a simple kind as a value type. */
[[nodiscard]] constexpr CvmValueType Simple(CvmType kind) noexcept
{
    return CvmValueType{kind, -1};
}

/** Wraps a struct index as a value type. */
[[nodiscard]] constexpr CvmValueType Record(int structIndex) noexcept
{
    return CvmValueType{CvmType::Struct, structIndex};
}

/** Whether \p type is a type a variable, parameter or expression can have. */
[[nodiscard]] constexpr bool IsValueType(CvmValueType type) noexcept
{
    return type.kind != CvmType::Void;
}

/** Whether \p type is one of the three the host ABI understands. */
[[nodiscard]] constexpr bool IsSimpleType(CvmType kind) noexcept
{
    return kind == CvmType::I64 || kind == CvmType::F64 || kind == CvmType::Str;
}

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMTYPES_H
