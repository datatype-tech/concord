// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CVMLAYOUT_H
#define CONCORDSCRIPT_CVMLAYOUT_H

/**
 * The LLVM layout of the records a program declares.
 *
 * Both the backend, which needs a struct type for a function signature or a
 * global, and the emitter, which needs one for a local, go through here. Two
 * places building their own would eventually disagree, and an LLVM type
 * mismatch between a caller and a callee is a verifier error at best.
 */

#include "CvmProgram.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>

#include <cstddef>
#include <vector>

namespace ConcordScript {

/** Builds the field types of \p record, which must already have its fields resolved. */
[[nodiscard]] inline std::vector<llvm::Type*> RecordFieldTypes(llvm::LLVMContext& context,
                                                               const CvmStruct& record,
                                                               const std::vector<llvm::StructType*>&
                                                                   resolved);

/** Builds one LLVM struct type per declared record, in declaration order. */
[[nodiscard]] inline std::vector<llvm::StructType*> BuildRecordTypes(
    llvm::LLVMContext& context, const std::vector<CvmStruct>& structs)
{
    std::vector<llvm::StructType*> types;
    types.reserve(structs.size());
    for (const CvmStruct& record : structs) {
        types.push_back(llvm::StructType::create(context, record.name));
    }
    for (std::size_t index = 0; index < structs.size(); ++index) {
        types[index]->setBody(RecordFieldTypes(context, structs[index], types), /*isPacked=*/false);
    }
    return types;
}

inline std::vector<llvm::Type*> RecordFieldTypes(llvm::LLVMContext& context,
                                                 const CvmStruct& record,
                                                 const std::vector<llvm::StructType*>& resolved)
{
    std::vector<llvm::Type*> fields;
    fields.reserve(record.fields.size());
    for (const CvmField& field : record.fields) {
        switch (field.type.kind) {
            case CvmType::F64:
                fields.push_back(llvm::Type::getDoubleTy(context));
                break;
            case CvmType::Str:
                fields.push_back(llvm::PointerType::get(context, 0));
                break;
            case CvmType::Struct:
                // A named type can be referenced before its body is set, which
                // is what makes a record able to contain another one declared
                // later in the project.
                fields.push_back(resolved[static_cast<std::size_t>(field.type.structIndex)]);
                break;
            default:
                fields.push_back(llvm::Type::getInt64Ty(context));
                break;
        }
    }
    return fields;
}

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMLAYOUT_H
