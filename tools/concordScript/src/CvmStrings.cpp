// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmStrings.h"

#include <vector>

namespace ConcordScript {

llvm::Constant* CreateStringConstant(llvm::Module& module, std::string_view name,
                                     const std::string& bytes)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::IntegerType* i64 = llvm::IntegerType::getInt64Ty(context);
    llvm::IntegerType* i32 = llvm::IntegerType::getInt32Ty(context);

    llvm::ArrayType* body = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), bytes.size() + 1);
    llvm::StructType* layout = llvm::StructType::get(context, {i64, body});
    llvm::Constant* aggregate = llvm::ConstantStruct::get(
        layout, {llvm::ConstantInt::get(i64, 0),
                 llvm::ConstantDataArray::getString(context, bytes, /*AddNull=*/true)});
    auto* global = new llvm::GlobalVariable(module, layout, /*isConstant=*/true,
                                            llvm::GlobalValue::PrivateLinkage, aggregate,
                                            std::string(name));

    // Spelled as an explicit array: a brace list here is ambiguous between the
    // two ConstantExpr overloads.
    llvm::Constant* indices[] = {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, 1)};
    return llvm::ConstantExpr::getInBoundsGetElementPtr(layout, global, indices);
}

} // namespace ConcordScript
