// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMSTRINGS_H
#define CONCORDSCRIPT_CVMSTRINGS_H

/**
 * Emission of CVM string constants.
 *
 * A CVM string is a pointer to the bytes, preceded by an ownership word. The
 * compiler has to lay its literals out the same way the runtime allocates its
 * own, or cvm_str_free could not tell one from the other -- and telling them
 * apart is what makes freeing a literal harmless rather than fatal.
 *
 * Shared between the emitter, which interns a literal the first time a body
 * mentions one, and the backend, which materializes the initializer of a
 * module-level string.
 */

#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>

#include <string>
#include <string_view>

namespace ConcordScript {

/**
 * Creates a private global holding \p bytes as a CVM string and returns a
 * constant pointer to its first character.
 *
 * \p name only has to be unique within the module; a literal needing no name
 * at all is still named so a disassembly can be read.
 */
llvm::Constant* CreateStringConstant(llvm::Module& module, std::string_view name,
                                     const std::string& bytes);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMSTRINGS_H
