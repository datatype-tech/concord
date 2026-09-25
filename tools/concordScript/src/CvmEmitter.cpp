// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmEmitter.h"

#include "CvmHostRegistry.h"
#include "CvmLayout.h"
#include "CvmStrings.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ConcordScript {

namespace {

/** Lexical classes the CVM grammar distinguishes. */
enum class TokenKind { End, Identifier, Integer, Real, Text, Punct };

/** One lexeme, positioned so a diagnostic can name its source line. */
struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    int line = 0;
};

/** An LLVM value together with the CVM type it carries. */
struct TypedValue {
    llvm::Value* value = nullptr;
    CvmValueType type;
};

/** Where a name lives, so it can be read or written through the same path. */
struct Place {
    llvm::Value* address = nullptr;
    CvmValueType type;
};

bool IsIdentifierStart(char character)
{
    return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool IsIdentifierBody(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

/**
 * Splits a @cvm body into tokens.
 *
 * A tokenizer rather than substring scanning: block statements contain
 * semicolons, so a body cannot be split on ';' without first knowing where
 * braces open and close.
 */
bool Tokenize(std::string_view text, int firstLine, std::vector<Token>& tokens, std::string& error)
{
    static constexpr std::string_view kSingleCharacter = "(){};,=<>+-*/%:.!? ";
    static constexpr std::string_view kTwoCharacter[] = {
        "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "%=",
    };

    int line = firstLine;
    std::size_t index = 0;
    while (index < text.size()) {
        const char character = text[index];
        if (character == '\n') {
            ++line;
            ++index;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            ++index;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            while (index < text.size() && text[index] != '\n') ++index;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '*') {
            index += 2;
            while (index + 1 < text.size() && !(text[index] == '*' && text[index + 1] == '/')) {
                if (text[index] == '\n') ++line;
                ++index;
            }
            index = index + 1 < text.size() ? index + 2 : text.size();
            continue;
        }
        if (IsIdentifierStart(character)) {
            const std::size_t start = index;
            while (index < text.size() && IsIdentifierBody(text[index])) ++index;
            tokens.push_back(
                {TokenKind::Identifier, std::string(text.substr(start, index - start)), line});
            continue;
        }
        if (character == '"') {
            const int startLine = line;
            ++index;
            std::string decoded;
            bool terminated = false;
            while (index < text.size()) {
                const char current = text[index];
                if (current == '\n') break;
                if (current == '"') {
                    ++index;
                    terminated = true;
                    break;
                }
                if (current == '\\' && index + 1 < text.size()) {
                    const char escape = text[index + 1];
                    index += 2;
                    switch (escape) {
                        case 'n': decoded += '\n'; break;
                        case 't': decoded += '\t'; break;
                        case 'r': decoded += '\r'; break;
                        case '\\': decoded += '\\'; break;
                        case '"': decoded += '"'; break;
                        default:
                            error = "line " + std::to_string(startLine) + ": unknown escape '\\" +
                                    std::string(1, escape) + "' in a string literal";
                            return false;
                    }
                    continue;
                }
                decoded += current;
                ++index;
            }
            if (!terminated) {
                error = "line " + std::to_string(startLine) + ": unterminated string literal";
                return false;
            }
            tokens.push_back({TokenKind::Text, std::move(decoded), startLine});
            continue;
        }
        // A number is real when it has a fraction or an exponent, and that is
        // the same rule the literal parser in CvmProgram.cpp applies.
        const bool startsWithDigit = std::isdigit(static_cast<unsigned char>(character)) != 0;
        const bool startsWithDot =
            character == '.' && index + 1 < text.size() &&
            std::isdigit(static_cast<unsigned char>(text[index + 1])) != 0;
        if (startsWithDigit || startsWithDot) {
            const std::size_t start = index;
            bool isReal = startsWithDot;
            while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                ++index;
            }
            if (index < text.size() && text[index] == '.' && index + 1 < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[index + 1])) != 0) {
                isReal = true;
                ++index;
                while (index < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                    ++index;
                }
            }
            if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
                const std::size_t mark = index;
                ++index;
                if (index < text.size() && (text[index] == '+' || text[index] == '-')) ++index;
                if (index < text.size() &&
                    std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                    isReal = true;
                    while (index < text.size() &&
                           std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                        ++index;
                    }
                } else {
                    index = mark;
                }
            }
            tokens.push_back({isReal ? TokenKind::Real : TokenKind::Integer,
                              std::string(text.substr(start, index - start)), line});
            continue;
        }
        bool matched = false;
        for (const std::string_view candidate : kTwoCharacter) {
            if (text.compare(index, candidate.size(), candidate) == 0) {
                tokens.push_back({TokenKind::Punct, std::string(candidate), line});
                index += candidate.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;
        if (kSingleCharacter.find(character) != std::string_view::npos) {
            tokens.push_back({TokenKind::Punct, std::string(1, character), line});
            ++index;
            continue;
        }
        error = "line " + std::to_string(line) + ": unexpected character '" +
                std::string(1, character) + "' in @cvm body";
        return false;
    }
    tokens.push_back({TokenKind::End, "", line});
    return true;
}

/** Returns how many single-character edits turn \p left into \p right. */
std::size_t EditDistance(std::string_view left, std::string_view right)
{
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t index = 0; index <= right.size(); ++index) previous[index] = index;
    for (std::size_t row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (std::size_t column = 1; column <= right.size(); ++column) {
            const std::size_t substitution =
                previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);
            current[column] = std::min({previous[column] + 1, current[column - 1] + 1, substitution});
        }
        previous.swap(current);
    }
    return previous[right.size()];
}

/** Suggests a registered host name close to \p name, or returns empty. */
std::string SuggestHostName(std::string_view name)
{
    std::string best;
    std::size_t bestDistance = 3;
    for (const CvmHostSignature& signature : HostFunctions()) {
        const std::string_view candidate = signature.name;
        const std::size_t distance = EditDistance(name, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

/**
 * Lowers one statement list into the current function.
 *
 * The emitter is a direct recursive-descent translator: parsing and IR
 * construction happen in one pass, which is sound here because every construct
 * is emitted in the order it is read.
 */
class Emitter {
public:
    Emitter(llvm::Module& module, llvm::Function& function, std::vector<Token> tokens,
            const std::vector<CvmParameter>& parameters, CvmValueType returnType,
            const CvmEmitScope& scope)
        : module_(module), function_(function), tokens_(std::move(tokens)),
          parameters_(parameters), returnType_(returnType), scope_(scope),
          builder_(module.getContext())
    {
        i64_ = llvm::Type::getInt64Ty(module.getContext());
        f64_ = llvm::Type::getDoubleTy(module.getContext());
        i32_ = llvm::Type::getInt32Ty(module.getContext());
        str_ = llvm::PointerType::get(module.getContext(), 0);
        builder_.SetInsertPoint(&function.getEntryBlock());
    }

    /** Parses the whole body; the function must end on a return. */
    bool Run(std::string& error)
    {
        BindParameters();
        if (!StatementList(false, error)) return false;
        if (!Terminated()) {
            error = "missing return statement";
            return false;
        }
        return true;
    }

private:
    struct Variable {
        llvm::AllocaInst* slot = nullptr;
        CvmValueType type;
    };

    llvm::Type* LlvmType(CvmValueType type) const
    {
        switch (type.kind) {
            case CvmType::F64: return f64_;
            case CvmType::Str: return str_;
            case CvmType::Void: return llvm::Type::getVoidTy(module_.getContext());
            case CvmType::Struct:
                return (*scope_.structTypes)[static_cast<std::size_t>(type.structIndex)];
            case CvmType::I64: break;
        }
        return i64_;
    }

    std::string TypeName(CvmValueType type) const
    {
        switch (type.kind) {
            case CvmType::F64: return "f64";
            case CvmType::Str: return "str";
            case CvmType::Void: return "void";
            case CvmType::Struct:
                return (*scope_.structs)[static_cast<std::size_t>(type.structIndex)].name;
            case CvmType::I64: break;
        }
        return "i64";
    }

    /** Returns the declaration index of \p name, or -1 when it is not a record. */
    int RecordIndex(const std::string& name) const
    {
        if (scope_.structs == nullptr) return -1;
        for (std::size_t index = 0; index < scope_.structs->size(); ++index) {
            if ((*scope_.structs)[index].name == name) return static_cast<int>(index);
        }
        return -1;
    }

    /** Returns the field index of \p name within \p record, or -1. */
    int FieldIndex(int record, const std::string& name) const
    {
        const CvmStruct& declaration = (*scope_.structs)[static_cast<std::size_t>(record)];
        for (std::size_t index = 0; index < declaration.fields.size(); ++index) {
            if (declaration.fields[index].name == name) return static_cast<int>(index);
        }
        return -1;
    }

    /**
     * Returns a pointer to a private global holding \p bytes as a CVM string.
     *
     * The global is laid out exactly like the ones CvmString.c allocates: an
     * ownership word of zero followed by the bytes. That zero is the whole
     * point -- it is what makes cvm_str_free on a literal a no-op instead of a
     * crash. Identical literals are interned so one module has one copy.
     */
    llvm::Value* StringLiteral(const std::string& bytes)
    {
        const auto found = literals_.find(bytes);
        if (found != literals_.end()) return found->second;
        llvm::Value* pointer =
            CreateStringConstant(module_, "cvm.str." + std::to_string(literals_.size()), bytes);
        literals_.emplace(bytes, pointer);
        return pointer;
    }

    const Token& Peek(std::size_t ahead = 0) const
    {
        const std::size_t index = std::min(position_ + ahead, tokens_.size() - 1);
        return tokens_[index];
    }

    bool PeekPunct(std::string_view text, std::size_t ahead = 0) const
    {
        const Token& token = Peek(ahead);
        return token.kind == TokenKind::Punct && token.text == text;
    }

    bool PeekKeyword(std::string_view text, std::size_t ahead = 0) const
    {
        const Token& token = Peek(ahead);
        return token.kind == TokenKind::Identifier && token.text == text;
    }

    bool ConsumePunct(std::string_view text)
    {
        if (!PeekPunct(text)) return false;
        ++position_;
        return true;
    }

    bool ConsumeKeyword(std::string_view text)
    {
        if (!PeekKeyword(text)) return false;
        ++position_;
        return true;
    }

    bool ExpectPunct(std::string_view text, std::string& error)
    {
        if (ConsumePunct(text)) return true;
        error = "line " + std::to_string(Peek().line) + ": expected '" + std::string(text) + "'";
        return false;
    }

    bool ExpectSemicolon(std::string& error)
    {
        if (ConsumePunct(";")) return true;
        error = "line " + std::to_string(Peek().line) + ": expected ';'";
        return false;
    }

    bool Terminated() const
    {
        llvm::BasicBlock* block = builder_.GetInsertBlock();
        return block != nullptr && block->getTerminator() != nullptr;
    }

    /**
     * Allocates a slot at the top of the entry block.
     *
     * The insertion point is recomputed per call rather than held in a
     * persistent builder. A builder anchored at an empty block stores the end
     * sentinel, which does not move as the block fills, so every later slot
     * would land after the entry block's terminator -- and because
     * BasicBlock::getTerminator() only reports the last instruction, the block
     * then verifies as having no terminator at all.
     */
    llvm::AllocaInst* CreateSlot(const std::string& name, CvmValueType type)
    {
        llvm::BasicBlock& entry = function_.getEntryBlock();
        llvm::IRBuilder<> slotBuilder(&entry, entry.getFirstInsertionPt());
        return slotBuilder.CreateAlloca(LlvmType(type), nullptr, name + ".slot");
    }

    bool IsProgramFunction(const std::string& name) const
    {
        return scope_.programFunctions != nullptr && scope_.programFunctions->count(name) != 0;
    }

    const CvmGlobalSlot* GlobalSlot(const std::string& name) const
    {
        if (scope_.globals == nullptr) return nullptr;
        const auto found = scope_.globals->find(name);
        return found == scope_.globals->end() ? nullptr : &found->second;
    }

    /** Gives every parameter an assignable local slot holding its argument. */
    void BindParameters()
    {
        std::size_t index = 0;
        for (llvm::Argument& argument : function_.args()) {
            if (index >= parameters_.size()) break;
            const CvmParameter& parameter = parameters_[index];
            llvm::AllocaInst* slot = CreateSlot(parameter.name, parameter.type);
            builder_.CreateStore(&argument, slot);
            variables_.emplace(parameter.name, Variable{slot, parameter.type});
            ++index;
        }
    }

    bool StatementList(bool untilBrace, std::string& error)
    {
        for (;;) {
            if (untilBrace) {
                if (PeekPunct("}")) return true;
                if (Peek().kind == TokenKind::End) {
                    error = "line " + std::to_string(Peek().line) + ": expected '}'";
                    return false;
                }
            } else if (Peek().kind == TokenKind::End) {
                return true;
            }
            if (Terminated()) {
                error = "line " + std::to_string(Peek().line) + ": unreachable statement";
                return false;
            }
            if (!Statement(error)) return false;
        }
    }

    bool Block(bool& terminated, std::string& error)
    {
        if (!ExpectPunct("{", error)) return false;
        if (!StatementList(true, error)) return false;
        if (!ExpectPunct("}", error)) return false;
        terminated = Terminated();
        return true;
    }

    /** Whether the tokens at the cursor are NAME ('.' NAME)* '=' or a compound assign. */
    bool LooksLikeAssignment() const
    {
        if (Peek().kind != TokenKind::Identifier) return false;
        std::size_t ahead = 1;
        while (PeekPunct(".", ahead) && Peek(ahead + 1).kind == TokenKind::Identifier) ahead += 2;
        return PeekPunct("=", ahead) || PeekPunct("+=", ahead) || PeekPunct("-=", ahead) ||
               PeekPunct("*=", ahead) || PeekPunct("/=", ahead) || PeekPunct("%=", ahead);
    }

    bool Statement(std::string& error)
    {
        if (PeekKeyword("let")) return LetStatement(error);
        if (PeekKeyword("if")) return IfStatement(error);
        if (PeekKeyword("while")) return WhileStatement(error);
        if (PeekKeyword("for")) return ForStatement(error);
        if (PeekKeyword("break")) return BreakStatement(error);
        if (PeekKeyword("continue")) return ContinueStatement(error);
        if (PeekKeyword("return")) return ReturnStatement(error);
        if (LooksLikeAssignment()) return AssignStatement(error);
        return ExpressionStatement(error);
    }

    static bool IsReservedName(std::string_view name)
    {
        return name == "let" || name == "if" || name == "else" || name == "while" ||
               name == "for" || name == "break" || name == "continue" || name == "return" ||
               name == "true" || name == "false";
    }

    bool LetStatement(std::string& error) { return BindLet(true, error); }

    bool BindLet(bool expectSemicolon, std::string& error)
    {
        ++position_;  // let
        if (Peek().kind != TokenKind::Identifier) {
            error = "line " + std::to_string(Peek().line) + ": expected a name after 'let'";
            return false;
        }
        const Token name = Peek();
        ++position_;
        if (IsReservedName(name.text)) {
            error = "line " + std::to_string(name.line) + ": '" + name.text +
                    "' is a reserved word";
            return false;
        }
        if (variables_.count(name.text) != 0) {
            error = "line " + std::to_string(name.line) + ": variable '" + name.text +
                    "' is already declared";
            return false;
        }
        if (GlobalSlot(name.text) != nullptr) {
            error = "line " + std::to_string(name.line) + ": '" + name.text +
                    "' is a global; assign to it instead of declaring it here";
            return false;
        }

        bool annotated = false;
        CvmValueType declared;
        if (ConsumePunct(":")) {
            if (Peek().kind != TokenKind::Identifier) {
                error = "line " + std::to_string(Peek().line) + ": expected a type after ':'";
                return false;
            }
            const std::string text = Peek().text;
            ++position_;
            if (text == "i64") {
                declared = Simple(CvmType::I64);
            } else if (text == "f64") {
                declared = Simple(CvmType::F64);
            } else if (text == "str") {
                declared = Simple(CvmType::Str);
            } else {
                const int record = RecordIndex(text);
                if (record < 0) {
                    error = "line " + std::to_string(name.line) + ": unknown type '" + text +
                            "'; CVM has i64, f64, str and any declared struct";
                    return false;
                }
                declared = Record(record);
            }
            annotated = true;
        }
        if (!ExpectPunct("=", error)) return false;

        TypedValue initial;
        if (!Expression(initial, error)) return false;
        const CvmValueType type = annotated ? declared : initial.type;
        if (!Coerce(initial, type, name.line, "variable '" + name.text + "'", error)) return false;

        llvm::AllocaInst* slot = CreateSlot(name.text, type);
        builder_.CreateStore(initial.value, slot);
        variables_.emplace(name.text, Variable{slot, type});
        return expectSemicolon ? ExpectSemicolon(error) : true;
    }

    /**
     * Resolves NAME ('.' FIELD)* to the address it names.
     *
     * Reading and writing go through the same path, so a field can be assigned
     * through exactly the expression that would read it.
     */
    bool ResolvePlace(Place& place, int& line, std::string& error)
    {
        const Token base = Peek();
        line = base.line;
        ++position_;
        const auto local = variables_.find(base.text);
        if (local != variables_.end()) {
            place.address = local->second.slot;
            place.type = local->second.type;
        } else if (const CvmGlobalSlot* global = GlobalSlot(base.text)) {
            place.address = global->variable;
            place.type = global->type;
        } else {
            error = "line " + std::to_string(base.line) + ": unknown variable '" + base.text +
                    "'; declare it with 'let'";
            return false;
        }

        while (ConsumePunct(".")) {
            if (Peek().kind != TokenKind::Identifier) {
                error = "line " + std::to_string(Peek().line) + ": expected a field name after '.'";
                return false;
            }
            if (place.type.kind != CvmType::Struct) {
                error = "line " + std::to_string(Peek().line) + ": " + TypeName(place.type) +
                        " has no fields";
                return false;
            }
            const std::string field = Peek().text;
            ++position_;
            const int slot = FieldIndex(place.type.structIndex, field);
            if (slot < 0) {
                error = "line " + std::to_string(Peek().line) + ": " + TypeName(place.type) +
                        " has no field '" + field + "'";
                return false;
            }
            auto* layout = llvm::cast<llvm::StructType>(LlvmType(place.type));
            place.address =
                builder_.CreateStructGEP(layout, place.address, static_cast<unsigned>(slot),
                                         field + ".addr");
            place.type = (*scope_.structs)[static_cast<std::size_t>(place.type.structIndex)]
                             .fields[static_cast<std::size_t>(slot)]
                             .type;
        }
        return true;
    }

    bool AssignStatement(std::string& error) { return AssignOp(true, error); }

    bool AssignOp(bool expectSemicolon, std::string& error)
    {
        Place place;
        int line = 0;
        if (!ResolvePlace(place, line, error)) return false;

        std::string_view op;
        if (ConsumePunct("=")) {
            op = "=";
        } else if (ConsumePunct("+=")) {
            op = "+=";
        } else if (ConsumePunct("-=")) {
            op = "-=";
        } else if (ConsumePunct("*=")) {
            op = "*=";
        } else if (ConsumePunct("/=")) {
            op = "/=";
        } else if (ConsumePunct("%=")) {
            op = "%=";
        } else {
            error = "line " + std::to_string(line) + ": expected '=' or a compound assignment";
            return false;
        }

        TypedValue value;
        if (!Expression(value, error)) return false;

        if (op == "=") {
            if (!Coerce(value, place.type, line, "the assignment", error)) return false;
            builder_.CreateStore(value.value, place.address);
            return expectSemicolon ? ExpectSemicolon(error) : true;
        }

        TypedValue left;
        left.value = builder_.CreateLoad(LlvmType(place.type), place.address, "assign.lhs");
        left.type = place.type;

        if (op == "+=" && place.type.kind == CvmType::Str) {
            if (value.type.kind != CvmType::Str) {
                error = "line " + std::to_string(line) + ": cannot append " + TypeName(value.type) +
                        " to a string; use str_concat(...)";
                return false;
            }
            const CvmHostSignature* concat = FindHostFunction("str_concat");
            if (concat == nullptr) {
                error = "line " + std::to_string(line) + ": str_concat is not registered";
                return false;
            }
            value.value =
                builder_.CreateCall(HostFunction(*concat), {left.value, value.value}, "str.add");
            value.type = Simple(CvmType::Str);
            builder_.CreateStore(value.value, place.address);
            return expectSemicolon ? ExpectSemicolon(error) : true;
        }

        if (!RequireNumeric(left, error) || !RequireNumeric(value, error)) return false;
        const CvmValueType common = CommonType(left.type, value.type);
        Widen(left, common);
        Widen(value, common);
        if (common.kind == CvmType::F64) {
            if (op == "+=") {
                value.value = builder_.CreateFAdd(left.value, value.value, "add");
            } else if (op == "-=") {
                value.value = builder_.CreateFSub(left.value, value.value, "sub");
            } else if (op == "*=") {
                value.value = builder_.CreateFMul(left.value, value.value, "mul");
            } else if (op == "/=") {
                value.value = builder_.CreateFDiv(left.value, value.value, "div");
            } else {
                value.value = builder_.CreateFRem(left.value, value.value, "rem");
            }
        } else if (op == "+=") {
            value.value = builder_.CreateAdd(left.value, value.value, "add");
        } else if (op == "-=") {
            value.value = builder_.CreateSub(left.value, value.value, "sub");
        } else if (op == "*=") {
            value.value = builder_.CreateMul(left.value, value.value, "mul");
        } else if (op == "/=") {
            value.value = builder_.CreateSDiv(left.value, value.value, "div");
        } else {
            value.value = builder_.CreateSRem(left.value, value.value, "rem");
        }
        value.type = common;
        if (!Coerce(value, place.type, line, "the assignment", error)) return false;
        builder_.CreateStore(value.value, place.address);
        return expectSemicolon ? ExpectSemicolon(error) : true;
    }

    bool ReturnStatement(std::string& error)
    {
        ++position_;  // return
        TypedValue value;
        if (!Expression(value, error)) return false;
        if (!Coerce(value, returnType_, Peek().line, "the return value", error)) return false;
        if (!ExpectSemicolon(error)) return false;
        builder_.CreateRet(value.value);
        return true;
    }

    bool ExpressionStatement(std::string& error)
    {
        const int line = Peek().line;
        TypedValue value;
        if (!Expression(value, error)) return false;
        if (!ExpectSemicolon(error)) return false;
        if (!lastExpressionCalled_) {
            error = "line " + std::to_string(line) +
                    ": expression statement has no effect; use print(...) or return ...";
            return false;
        }
        return true;
    }

    bool IfStatement(std::string& error)
    {
        ++position_;  // if
        if (!ExpectPunct("(", error)) return false;
        TypedValue condition;
        if (!Expression(condition, error)) return false;
        if (!ExpectPunct(")", error)) return false;

        llvm::Value* test = ToBoolean(condition, error);
        if (test == nullptr) return false;

        llvm::BasicBlock* thenBlock =
            llvm::BasicBlock::Create(module_.getContext(), "if.then", &function_);
        llvm::BasicBlock* elseBlock =
            llvm::BasicBlock::Create(module_.getContext(), "if.else", &function_);
        llvm::BasicBlock* endBlock =
            llvm::BasicBlock::Create(module_.getContext(), "if.end", &function_);
        builder_.CreateCondBr(test, thenBlock, elseBlock);

        builder_.SetInsertPoint(thenBlock);
        bool thenTerminated = false;
        if (!Block(thenTerminated, error)) return false;
        if (!thenTerminated) builder_.CreateBr(endBlock);

        builder_.SetInsertPoint(elseBlock);
        bool elseTerminated = false;
        if (ConsumeKeyword("else")) {
            if (PeekKeyword("if")) {
                if (!IfStatement(error)) return false;
                elseTerminated = Terminated();
            } else if (!Block(elseTerminated, error)) {
                return false;
            }
        }
        if (!elseTerminated) builder_.CreateBr(endBlock);

        if (thenTerminated && elseTerminated) {
            // Both arms return, so the join block is unreachable. Dropping it
            // keeps the IR verifiable, since a block with no terminator is not.
            endBlock->eraseFromParent();
            builder_.SetInsertPoint(elseBlock);
            return true;
        }
        builder_.SetInsertPoint(endBlock);
        return true;
    }

    bool WhileStatement(std::string& error)
    {
        ++position_;  // while
        llvm::BasicBlock* conditionBlock =
            llvm::BasicBlock::Create(module_.getContext(), "while.cond", &function_);
        llvm::BasicBlock* bodyBlock =
            llvm::BasicBlock::Create(module_.getContext(), "while.body", &function_);
        llvm::BasicBlock* endBlock =
            llvm::BasicBlock::Create(module_.getContext(), "while.end", &function_);

        builder_.CreateBr(conditionBlock);
        // The condition is emitted inside its own block so it is re-evaluated
        // on every iteration rather than once before the loop.
        builder_.SetInsertPoint(conditionBlock);
        if (!ExpectPunct("(", error)) return false;
        TypedValue condition;
        if (!Expression(condition, error)) return false;
        if (!ExpectPunct(")", error)) return false;
        llvm::Value* test = ToBoolean(condition, error);
        if (test == nullptr) return false;
        builder_.CreateCondBr(test, bodyBlock, endBlock);

        builder_.SetInsertPoint(bodyBlock);
        loops_.push_back(LoopBlocks{conditionBlock, endBlock});
        bool bodyTerminated = false;
        const bool bodyOk = Block(bodyTerminated, error);
        loops_.pop_back();
        if (!bodyOk) return false;
        if (!bodyTerminated) builder_.CreateBr(conditionBlock);

        builder_.SetInsertPoint(endBlock);
        return true;
    }

    /**
     * C-style `for (init; cond; step) { body }`.
     *
     * init is empty, a `let`, or an assignment. cond empty means always-true.
     * step is empty or an assignment; `continue` jumps to the step, not the
     * condition, so `i += 1` still runs after an early continue.
     */
    bool ForStatement(std::string& error)
    {
        ++position_;  // for
        if (!ExpectPunct("(", error)) return false;

        llvm::BasicBlock* conditionBlock =
            llvm::BasicBlock::Create(module_.getContext(), "for.cond", &function_);
        llvm::BasicBlock* bodyBlock =
            llvm::BasicBlock::Create(module_.getContext(), "for.body", &function_);
        llvm::BasicBlock* stepBlock =
            llvm::BasicBlock::Create(module_.getContext(), "for.step", &function_);
        llvm::BasicBlock* endBlock =
            llvm::BasicBlock::Create(module_.getContext(), "for.end", &function_);

        if (PeekKeyword("let")) {
            if (!BindLet(false, error)) return false;
        } else if (!PeekPunct(";")) {
            if (!LooksLikeAssignment()) {
                error = "line " + std::to_string(Peek().line) +
                        ": for-init must be 'let', an assignment, or empty";
                return false;
            }
            if (!AssignOp(false, error)) return false;
        }
        if (!ExpectPunct(";", error)) return false;

        builder_.CreateBr(conditionBlock);
        builder_.SetInsertPoint(conditionBlock);
        if (PeekPunct(";")) {
            ++position_;
            builder_.CreateCondBr(builder_.getTrue(), bodyBlock, endBlock);
        } else {
            TypedValue condition;
            if (!Expression(condition, error)) return false;
            if (!ExpectPunct(";", error)) return false;
            llvm::Value* test = ToBoolean(condition, error);
            if (test == nullptr) return false;
            builder_.CreateCondBr(test, bodyBlock, endBlock);
        }

        builder_.SetInsertPoint(stepBlock);
        if (!PeekPunct(")")) {
            if (!LooksLikeAssignment()) {
                error = "line " + std::to_string(Peek().line) +
                        ": for-step must be an assignment or empty";
                return false;
            }
            if (!AssignOp(false, error)) return false;
        }
        if (!ExpectPunct(")", error)) return false;
        builder_.CreateBr(conditionBlock);

        builder_.SetInsertPoint(bodyBlock);
        loops_.push_back(LoopBlocks{stepBlock, endBlock});
        bool bodyTerminated = false;
        const bool bodyOk = Block(bodyTerminated, error);
        loops_.pop_back();
        if (!bodyOk) return false;
        if (!bodyTerminated) builder_.CreateBr(stepBlock);

        builder_.SetInsertPoint(endBlock);
        return true;
    }

    bool BreakStatement(std::string& error)
    {
        const int line = Peek().line;
        ++position_;
        if (loops_.empty()) {
            error = "line " + std::to_string(line) + ": 'break' is only valid inside a loop";
            return false;
        }
        if (!ExpectSemicolon(error)) return false;
        builder_.CreateBr(loops_.back().breakTarget);
        return true;
    }

    bool ContinueStatement(std::string& error)
    {
        const int line = Peek().line;
        ++position_;
        if (loops_.empty()) {
            error = "line " + std::to_string(line) + ": 'continue' is only valid inside a loop";
            return false;
        }
        if (!ExpectSemicolon(error)) return false;
        builder_.CreateBr(loops_.back().continueTarget);
        return true;
    }

    /**
     * Short-circuits \p left through \p right into an i64 0/1.
     *
     * \p skipWhenTrue is || (the right side is skipped when the left is true)
     * and otherwise &&.
     */
    bool ShortCircuit(TypedValue& value, bool skipWhenTrue, std::string_view rhsLabel,
                      std::string& error)
    {
        llvm::Value* leftBool = ToBoolean(value, error);
        if (leftBool == nullptr) return false;

        llvm::BasicBlock* rhsBlock =
            llvm::BasicBlock::Create(module_.getContext(), std::string(rhsLabel) + ".rhs",
                                     &function_);
        llvm::BasicBlock* endBlock =
            llvm::BasicBlock::Create(module_.getContext(), std::string(rhsLabel) + ".end",
                                     &function_);
        llvm::BasicBlock* leftBlock = builder_.GetInsertBlock();
        if (skipWhenTrue) {
            builder_.CreateCondBr(leftBool, endBlock, rhsBlock);
        } else {
            builder_.CreateCondBr(leftBool, rhsBlock, endBlock);
        }

        builder_.SetInsertPoint(rhsBlock);
        TypedValue right;
        if (skipWhenTrue) {
            if (!LogicalAnd(right, error)) return false;
        } else {
            if (!Comparison(right, error)) return false;
        }
        llvm::Value* rightBool = ToBoolean(right, error);
        if (rightBool == nullptr) return false;
        llvm::Value* rightExt = builder_.CreateZExt(rightBool, i64_, "logic.rhs");
        llvm::BasicBlock* rhsEnd = builder_.GetInsertBlock();
        builder_.CreateBr(endBlock);

        builder_.SetInsertPoint(endBlock);
        llvm::PHINode* phi = builder_.CreatePHI(i64_, 2, skipWhenTrue ? "or" : "and");
        phi->addIncoming(llvm::ConstantInt::get(i64_, skipWhenTrue ? 1 : 0), leftBlock);
        phi->addIncoming(rightExt, rhsEnd);
        value.value = phi;
        value.type = Simple(CvmType::I64);
        return true;
    }

    /** Converts a value to the i1 a branch needs; NaN counts as false. */
    llvm::Value* ToBoolean(const TypedValue& value, std::string& error)
    {
        switch (value.type.kind) {
            case CvmType::F64:
                return builder_.CreateFCmpONE(value.value, llvm::ConstantFP::get(f64_, 0.0),
                                              "tobool");
            case CvmType::Str:
                return builder_.CreateICmpNE(
                    value.value, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(str_)),
                    "tobool");
            case CvmType::Struct:
                error = "a " + TypeName(value.type) + " cannot be used as a condition";
                return nullptr;
            default:
                break;
        }
        return builder_.CreateICmpNE(value.value, llvm::ConstantInt::get(i64_, 0), "tobool");
    }

    /** Widens an i64 to f64 when \p target is f64; nothing else converts. */
    void Widen(TypedValue& value, CvmValueType target)
    {
        if (value.type == target) return;
        if (target.kind == CvmType::F64 && value.type.kind == CvmType::I64) {
            value.value = builder_.CreateSIToFP(value.value, f64_, "widen");
            value.type = target;
        }
    }

    /**
     * Makes \p value usable where \p target is expected.
     *
     * Widening is implicit because it is lossless and the alternative is a cast
     * at every call site that mixes a literal with a float. Narrowing is an
     * error, because it throws away a fraction without saying so, and so is any
     * mismatch between two records -- they are different types even when their
     * fields look alike.
     */
    bool Coerce(TypedValue& value, CvmValueType target, int line, const std::string& what,
                std::string& error)
    {
        if (value.type == target) return true;
        if (value.type.kind == CvmType::Struct || target.kind == CvmType::Struct ||
            value.type.kind == CvmType::Str || target.kind == CvmType::Str) {
            error = "line " + std::to_string(line) + ": " + what + " is " + TypeName(target) +
                    " but is given a " + TypeName(value.type);
            return false;
        }
        if (target.kind == CvmType::F64) {
            Widen(value, target);
            return true;
        }
        error = "line " + std::to_string(line) + ": " + what + " is i64 but is given an f64; " +
                "convert it with to_i64(...)";
        return false;
    }

    /** Rejects a value that has no arithmetic at all. */
    bool RequireNumeric(const TypedValue& value, std::string& error) const
    {
        if (value.type.kind == CvmType::Str) {
            error = "line " + std::to_string(Peek().line) +
                    ": strings do not support arithmetic; use str_concat(...)";
            return false;
        }
        if (value.type.kind == CvmType::Struct) {
            error = "line " + std::to_string(Peek().line) + ": a " + TypeName(value.type) +
                    " does not support arithmetic; operate on its fields";
            return false;
        }
        return true;
    }

    /** Declares a registered host function in this module on first use. */
    llvm::Function* HostFunction(const CvmHostSignature& signature)
    {
        const std::string symbol = signature.symbol;
        if (llvm::Function* existing = module_.getFunction(symbol)) return existing;
        std::vector<llvm::Type*> parameters;
        parameters.reserve(signature.paramCount);
        for (std::size_t index = 0; index < signature.paramCount; ++index) {
            parameters.push_back(LlvmType(Simple(signature.params[index])));
        }
        llvm::Type* result = signature.result == CvmType::Void
                                 ? llvm::Type::getVoidTy(module_.getContext())
                                 : LlvmType(Simple(signature.result));
        llvm::FunctionType* type = llvm::FunctionType::get(result, parameters, false);
        return llvm::Function::Create(type, llvm::Function::ExternalLinkage, symbol, module_);
    }

    /**
     * Verifies that a function handed to a callback parameter has the shape the
     * host will call it through.
     *
     * Only possible because the argument was written as a bare name: once an
     * address is an integer the shape is gone, and the host would call it
     * anyway with whatever it finds there.
     */
    bool CheckCallbackShape(const char* symbol, std::size_t parameter, const std::string& function,
                            int line, std::string& error)
    {
        const CvmCallbackSlot* slot = FindCallbackSlot(symbol, parameter);
        if (slot == nullptr) return true;
        const CvmFunctionSignature& signature = scope_.programFunctions->find(function)->second;

        std::string wanted = "i64(";
        for (std::size_t index = 0; index < slot->parameterCount; ++index) {
            if (index != 0) wanted += ", ";
            wanted += Simple(slot->parameters[index]).kind == CvmType::F64 ? "f64" : "i64";
        }
        wanted += ")";

        bool matches = signature.parameters.size() == slot->parameterCount &&
                       signature.returnType.kind == slot->returnType &&
                       signature.returnType.kind != CvmType::Struct;
        for (std::size_t index = 0; matches && index < slot->parameterCount; ++index) {
            matches = signature.parameters[index].kind == slot->parameters[index];
        }
        if (matches) return true;

        error = "line " + std::to_string(line) + ": '" + function + "' is passed to " + symbol +
                ", which calls it as " + wanted + "; declare it with that signature";
        return false;
    }

    bool Expression(TypedValue& value, std::string& error)
    {
        lastExpressionCalled_ = false;
        return Conditional(value, error);
    }

    /**
     * Right-associative `cond ? a : b`. Each arm is widened to a common type
     * before the PHI, so `flag ? 1 : 0.5` is f64 rather than a type error.
     */
    bool Conditional(TypedValue& value, std::string& error)
    {
        if (!LogicalOr(value, error)) return false;
        if (!PeekPunct("?")) return true;
        const int line = Peek().line;
        ++position_;

        llvm::Value* test = ToBoolean(value, error);
        if (test == nullptr) return false;

        llvm::BasicBlock* thenBlock =
            llvm::BasicBlock::Create(module_.getContext(), "tern.then", &function_);
        llvm::BasicBlock* elseBlock =
            llvm::BasicBlock::Create(module_.getContext(), "tern.else", &function_);
        llvm::BasicBlock* mergeBlock =
            llvm::BasicBlock::Create(module_.getContext(), "tern.merge", &function_);
        builder_.CreateCondBr(test, thenBlock, elseBlock);

        builder_.SetInsertPoint(thenBlock);
        TypedValue thenValue;
        if (!Expression(thenValue, error)) return false;
        if (!ExpectPunct(":", error)) return false;
        llvm::BasicBlock* thenEnd = builder_.GetInsertBlock();

        builder_.SetInsertPoint(elseBlock);
        TypedValue elseValue;
        if (!Conditional(elseValue, error)) return false;
        llvm::BasicBlock* elseEnd = builder_.GetInsertBlock();

        CvmValueType common;
        if (thenValue.type.kind == CvmType::Struct || elseValue.type.kind == CvmType::Struct ||
            thenValue.type.kind == CvmType::Str || elseValue.type.kind == CvmType::Str) {
            if (!(thenValue.type == elseValue.type)) {
                error = "line " + std::to_string(line) + ": ternary arms are " +
                        TypeName(thenValue.type) + " and " + TypeName(elseValue.type) +
                        "; they must match";
                return false;
            }
            common = thenValue.type;
        } else {
            common = CommonType(thenValue.type, elseValue.type);
        }

        builder_.SetInsertPoint(thenEnd);
        Widen(thenValue, common);
        thenEnd = builder_.GetInsertBlock();
        builder_.CreateBr(mergeBlock);

        builder_.SetInsertPoint(elseEnd);
        Widen(elseValue, common);
        elseEnd = builder_.GetInsertBlock();
        builder_.CreateBr(mergeBlock);

        builder_.SetInsertPoint(mergeBlock);
        llvm::PHINode* phi = builder_.CreatePHI(LlvmType(common), 2, "tern");
        phi->addIncoming(thenValue.value, thenEnd);
        phi->addIncoming(elseValue.value, elseEnd);
        value.value = phi;
        value.type = common;
        lastExpressionCalled_ = false;
        return true;
    }

    bool LogicalOr(TypedValue& value, std::string& error)
    {
        if (!LogicalAnd(value, error)) return false;
        while (PeekPunct("||")) {
            ++position_;
            if (!ShortCircuit(value, true, "or", error)) return false;
        }
        return true;
    }

    bool LogicalAnd(TypedValue& value, std::string& error)
    {
        if (!Comparison(value, error)) return false;
        while (PeekPunct("&&")) {
            ++position_;
            if (!ShortCircuit(value, false, "and", error)) return false;
        }
        return true;
    }

    bool Comparison(TypedValue& value, std::string& error)
    {
        if (!Additive(value, error)) return false;

        // Strings compare by content, not by address, because two literals with
        // the same bytes are what a reader means by equal. Records are not
        // comparable at all: there is no obvious answer for a record holding a
        // string, and comparing field by field is what a reader should write.
        if (value.type.kind == CvmType::Str || value.type.kind == CvmType::Struct) {
            const bool equal = PeekPunct("==");
            const bool notEqual = !equal && PeekPunct("!=");
            const bool ordered = PeekPunct("<") || PeekPunct("<=") || PeekPunct(">") ||
                                 PeekPunct(">=");
            if (value.type.kind == CvmType::Struct) {
                if (equal || notEqual || ordered) {
                    error = "line " + std::to_string(Peek().line) + ": a " + TypeName(value.type) +
                            " cannot be compared; compare its fields";
                    return false;
                }
                return true;
            }
            if (!equal && !notEqual) {
                if (ordered) {
                    error = "line " + std::to_string(Peek().line) +
                            ": strings cannot be ordered; use str_eq(...)";
                    return false;
                }
                return true;
            }
            ++position_;
            TypedValue right;
            if (!Additive(right, error)) return false;
            if (right.type.kind != CvmType::Str) {
                error = "line " + std::to_string(Peek().line) + ": cannot compare a string with " +
                        TypeName(right.type);
                return false;
            }
            const CvmHostSignature* equality = FindHostFunction("str_eq");
            llvm::Value* result =
                builder_.CreateCall(HostFunction(*equality), {value.value, right.value}, "str.eq");
            if (notEqual) {
                result = builder_.CreateXor(result, llvm::ConstantInt::get(i64_, 1), "str.ne");
            }
            value.value = result;
            value.type = Simple(CvmType::I64);
            return true;
        }

        struct Operator {
            std::string_view text;
            llvm::CmpInst::Predicate integer;
            llvm::CmpInst::Predicate real;
        };
        static constexpr Operator kOperators[] = {
            {"<=", llvm::CmpInst::ICMP_SLE, llvm::CmpInst::FCMP_OLE},
            {">=", llvm::CmpInst::ICMP_SGE, llvm::CmpInst::FCMP_OGE},
            {"==", llvm::CmpInst::ICMP_EQ, llvm::CmpInst::FCMP_OEQ},
            // Unordered-not-equal, so a NaN compares unequal to everything,
            // which is what every other language does.
            {"!=", llvm::CmpInst::ICMP_NE, llvm::CmpInst::FCMP_UNE},
            {"<", llvm::CmpInst::ICMP_SLT, llvm::CmpInst::FCMP_OLT},
            {">", llvm::CmpInst::ICMP_SGT, llvm::CmpInst::FCMP_OGT},
        };
        for (const Operator& candidate : kOperators) {
            if (!PeekPunct(candidate.text)) continue;
            ++position_;
            TypedValue right;
            if (!Additive(right, error)) return false;
            const CvmValueType common = CommonType(value.type, right.type);
            Widen(value, common);
            Widen(right, common);
            llvm::Value* compare =
                common.kind == CvmType::F64
                    ? builder_.CreateFCmp(candidate.real, value.value, right.value, "cmp")
                    : builder_.CreateICmp(candidate.integer, value.value, right.value, "cmp");
            value.value = builder_.CreateZExt(compare, i64_, "cmp.ext");
            value.type = Simple(CvmType::I64);
            return true;
        }
        return true;
    }

    static CvmValueType CommonType(CvmValueType left, CvmValueType right)
    {
        if (left.kind == CvmType::F64 || right.kind == CvmType::F64) return Simple(CvmType::F64);
        return Simple(CvmType::I64);
    }

    bool Additive(TypedValue& value, std::string& error)
    {
        if (!Multiplicative(value, error)) return false;
        for (;;) {
            const bool add = PeekPunct("+");
            if (!add && !PeekPunct("-")) return true;
            ++position_;
            TypedValue right;
            if (!Multiplicative(right, error)) return false;
            if (!RequireNumeric(value, error) || !RequireNumeric(right, error)) return false;
            const CvmValueType common = CommonType(value.type, right.type);
            Widen(value, common);
            Widen(right, common);
            if (common.kind == CvmType::F64) {
                value.value = add ? builder_.CreateFAdd(value.value, right.value, "add")
                                  : builder_.CreateFSub(value.value, right.value, "sub");
            } else {
                value.value = add ? builder_.CreateAdd(value.value, right.value, "add")
                                  : builder_.CreateSub(value.value, right.value, "sub");
            }
            value.type = common;
        }
    }

    bool Multiplicative(TypedValue& value, std::string& error)
    {
        if (!Unary(value, error)) return false;
        for (;;) {
            const bool multiply = PeekPunct("*");
            const bool divide = !multiply && PeekPunct("/");
            const bool remainder = !multiply && !divide && PeekPunct("%");
            if (!multiply && !divide && !remainder) return true;
            ++position_;
            TypedValue right;
            if (!Unary(right, error)) return false;
            if (!RequireNumeric(value, error) || !RequireNumeric(right, error)) return false;
            const CvmValueType common = CommonType(value.type, right.type);
            Widen(value, common);
            Widen(right, common);
            if (common.kind == CvmType::F64) {
                if (multiply) {
                    value.value = builder_.CreateFMul(value.value, right.value, "mul");
                } else if (divide) {
                    value.value = builder_.CreateFDiv(value.value, right.value, "div");
                } else {
                    value.value = builder_.CreateFRem(value.value, right.value, "rem");
                }
            } else if (multiply) {
                value.value = builder_.CreateMul(value.value, right.value, "mul");
            } else if (divide) {
                value.value = builder_.CreateSDiv(value.value, right.value, "div");
            } else {
                value.value = builder_.CreateSRem(value.value, right.value, "rem");
            }
            value.type = common;
        }
    }

    bool Unary(TypedValue& value, std::string& error)
    {
        if (ConsumePunct("!")) {
            if (!Unary(value, error)) return false;
            llvm::Value* test = ToBoolean(value, error);
            if (test == nullptr) return false;
            llvm::Value* inverted =
                builder_.CreateXor(test, llvm::ConstantInt::get(test->getType(), 1), "not");
            value.value = builder_.CreateZExt(inverted, i64_, "not.ext");
            value.type = Simple(CvmType::I64);
            return true;
        }
        if (ConsumePunct("-")) {
            if (!Unary(value, error)) return false;
            if (!RequireNumeric(value, error)) return false;
            value.value = value.type.kind == CvmType::F64 ? builder_.CreateFNeg(value.value, "neg")
                                                          : builder_.CreateNeg(value.value, "neg");
            return true;
        }
        return Primary(value, error);
    }

    /** Parses NAME { field = expr, ... } into a record value. */
    bool RecordLiteral(int record, TypedValue& value, std::string& error)
    {
        const CvmStruct& declaration = (*scope_.structs)[static_cast<std::size_t>(record)];
        const int startLine = Peek().line;
        if (!ExpectPunct("{", error)) return false;

        std::vector<llvm::Value*> fields(declaration.fields.size(), nullptr);
        std::vector<bool> seen(declaration.fields.size(), false);
        if (!PeekPunct("}")) {
            for (;;) {
                if (Peek().kind != TokenKind::Identifier) {
                    error = "line " + std::to_string(Peek().line) + ": expected a field name";
                    return false;
                }
                const std::string fieldName = Peek().text;
                const int fieldLine = Peek().line;
                ++position_;
                const int slot = FieldIndex(record, fieldName);
                if (slot < 0) {
                    error = "line " + std::to_string(fieldLine) + ": " + declaration.name +
                            " has no field '" + fieldName + "'";
                    return false;
                }
                if (seen[static_cast<std::size_t>(slot)]) {
                    error = "line " + std::to_string(fieldLine) + ": field '" + fieldName +
                            "' is set twice";
                    return false;
                }
                if (!ExpectPunct("=", error)) return false;
                TypedValue fieldValue;
                if (!Expression(fieldValue, error)) return false;
                if (!Coerce(fieldValue, declaration.fields[static_cast<std::size_t>(slot)].type,
                            fieldLine, "field '" + fieldName + "'", error)) {
                    return false;
                }
                fields[static_cast<std::size_t>(slot)] = fieldValue.value;
                seen[static_cast<std::size_t>(slot)] = true;
                if (ConsumePunct(",")) continue;
                break;
            }
        }
        if (!ExpectPunct("}", error)) return false;

        for (std::size_t index = 0; index < seen.size(); ++index) {
            if (!seen[index]) {
                error = "line " + std::to_string(startLine) + ": " + declaration.name +
                        " does not initialize field '" + declaration.fields[index].name + "'";
                return false;
            }
        }

        llvm::Type* layout = LlvmType(Record(record));
        llvm::Value* aggregate = llvm::UndefValue::get(layout);
        // Inserted in declaration order so the emitted IR is deterministic.
        for (std::size_t index = 0; index < fields.size(); ++index) {
            aggregate = builder_.CreateInsertValue(aggregate, fields[index],
                                                   {static_cast<unsigned>(index)},
                                                   declaration.fields[index].name);
        }
        value.value = aggregate;
        value.type = Record(record);
        return true;
    }

    bool Primary(TypedValue& value, std::string& error)
    {
        const Token token = Peek();
        if (ConsumePunct("(")) {
            if (!Expression(value, error)) return false;
            return ExpectPunct(")", error);
        }
        if (token.kind == TokenKind::Integer) {
            ++position_;
            const long long literal = std::strtoll(token.text.c_str(), nullptr, 10);
            value.value = llvm::ConstantInt::get(i64_, static_cast<std::uint64_t>(literal), true);
            value.type = Simple(CvmType::I64);
            return true;
        }
        if (token.kind == TokenKind::Real) {
            ++position_;
            value.value = llvm::ConstantFP::get(f64_, std::strtod(token.text.c_str(), nullptr));
            value.type = Simple(CvmType::F64);
            return true;
        }
        if (token.kind == TokenKind::Text) {
            ++position_;
            value.value = StringLiteral(token.text);
            value.type = Simple(CvmType::Str);
            return true;
        }
        if (token.kind != TokenKind::Identifier) {
            error = "line " + std::to_string(token.line) + ": expected a value";
            return false;
        }
        if (token.text == "true" || token.text == "false") {
            ++position_;
            value.value = llvm::ConstantInt::get(i64_, token.text == "true" ? 1 : 0, true);
            value.type = Simple(CvmType::I64);
            return true;
        }

        // A record literal: the name is a type, not a value.
        if (PeekPunct("{", 1)) {
            const int record = RecordIndex(token.text);
            if (record >= 0) {
                ++position_;
                return RecordLiteral(record, value, error);
            }
        }

        // A call.
        if (PeekPunct("(", 1)) {
            ++position_;
            ++position_;
            return Call(token, value, error);
        }

        // A name that holds a value, possibly followed by field accesses.
        if (variables_.count(token.text) != 0 || GlobalSlot(token.text) != nullptr) {
            Place place;
            int line = 0;
            if (!ResolvePlace(place, line, error)) return false;
            value.value = builder_.CreateLoad(LlvmType(place.type), place.address, token.text);
            value.type = place.type;
            return true;
        }

        // Otherwise it may be a function taken as a value: that is how a frame
        // callback or a thread entry point is handed over.
        if (IsProgramFunction(token.text)) {
            ++position_;
            value.value = builder_.CreatePtrToInt(module_.getFunction(token.text), i64_, "fn.addr");
            value.type = Simple(CvmType::I64);
            return true;
        }
        if (const CvmHostSignature* host = FindHostFunction(token.text)) {
            ++position_;
            value.value = builder_.CreatePtrToInt(HostFunction(*host), i64_, "fn.addr");
            value.type = Simple(CvmType::I64);
            return true;
        }
        if (RecordIndex(token.text) >= 0) {
            error = "line " + std::to_string(token.line) + ": " + token.text +
                    " is a struct; construct it with " + token.text + " { field = value, ... }";
            return false;
        }

        std::string message =
            "line " + std::to_string(token.line) + ": unknown variable '" + token.text +
            "'; declare it with 'let'";
        const std::string suggestion = SuggestHostName(token.text);
        if (!suggestion.empty()) message += "; did you mean '" + suggestion + "'?";
        error = std::move(message);
        return false;
    }

    /** Emits the \c to_i64 and \c to_f64 conversions, which are language-level. */
    bool Conversion(const Token& name, TypedValue& value, std::string& error)
    {
        TypedValue argument;
        if (!Expression(argument, error)) return false;
        if (!ExpectPunct(")", error)) return false;
        if (argument.type.kind == CvmType::Str) {
            error = "line " + std::to_string(name.line) + ": " + name.text +
                    " does not take a string; use str_to_int(...) or str_to_float(...)";
            return false;
        }
        if (argument.type.kind == CvmType::Struct) {
            error = "line " + std::to_string(name.line) + ": " + name.text + " does not take a " +
                    TypeName(argument.type);
            return false;
        }
        const CvmValueType target = Simple(name.text == "to_f64" ? CvmType::F64 : CvmType::I64);
        if (target.kind == CvmType::F64) {
            Widen(argument, target);
        } else if (argument.type.kind == CvmType::F64) {
            argument.value = builder_.CreateFPToSI(argument.value, i64_, "truncate");
            argument.type = Simple(CvmType::I64);
        }
        value = argument;
        lastExpressionCalled_ = true;
        return true;
    }

    bool Call(const Token& name, TypedValue& value, std::string& error)
    {
        // Conversions are builtins rather than runtime calls: they compile to a
        // single instruction, and the language should own its own coercions.
        if (name.text == "to_i64" || name.text == "to_f64") {
            return Conversion(name, value, error);
        }

        std::vector<TypedValue> arguments;
        // Which arguments were written as a bare function name. An address is
        // an integer, so this is the only moment the compiler knows which
        // function a callback parameter is going to receive.
        std::vector<std::string> addressNames;
        if (!PeekPunct(")")) {
            for (;;) {
                const std::size_t before = position_;
                std::string addressName;
                if (Peek().kind == TokenKind::Identifier && !PeekPunct("(", 1) &&
                    IsProgramFunction(Peek().text)) {
                    addressName = Peek().text;
                }
                TypedValue argument;
                if (!Expression(argument, error)) return false;
                // Exactly one token consumed means the argument *was* that name
                // and not something built out of it.
                if (!addressName.empty() && position_ - before != 1) addressName.clear();
                arguments.push_back(argument);
                addressNames.push_back(std::move(addressName));
                if (ConsumePunct(",")) continue;
                break;
            }
        }
        if (!ExpectPunct(")", error)) return false;

        if (const CvmHostSignature* host = FindHostFunction(name.text)) {
            if (arguments.size() != host->paramCount) {
                error = "line " + std::to_string(name.line) + ": '" + name.text + "' expects " +
                        std::to_string(host->paramCount) + " argument(s), " +
                        std::to_string(arguments.size()) + " given";
                return false;
            }
            std::vector<llvm::Value*> lowered;
            lowered.reserve(arguments.size());
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                const CvmValueType wanted = Simple(host->params[index]);
                if (!Coerce(arguments[index], wanted, name.line,
                            "argument " + std::to_string(index + 1) + " of '" + name.text + "'",
                            error)) {
                    return false;
                }
                if (!addressNames[index].empty() &&
                    !CheckCallbackShape(host->symbol, index, addressNames[index], name.line,
                                        error)) {
                    return false;
                }
                lowered.push_back(arguments[index].value);
            }
            lastExpressionCalled_ = true;
            value.type = host->result == CvmType::Void ? Simple(CvmType::I64)
                                                       : Simple(host->result);
            value.value = builder_.CreateCall(HostFunction(*host), lowered, name.text + ".result");
            return true;
        }
        if (IsProgramFunction(name.text)) {
            const CvmFunctionSignature& signature =
                scope_.programFunctions->find(name.text)->second;
            if (arguments.size() != signature.parameters.size()) {
                error = "line " + std::to_string(name.line) + ": @cvm function '" + name.text +
                        "' expects " + std::to_string(signature.parameters.size()) +
                        " argument(s), " + std::to_string(arguments.size()) + " given";
                return false;
            }
            std::vector<llvm::Value*> lowered;
            lowered.reserve(arguments.size());
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                if (!Coerce(arguments[index], signature.parameters[index], name.line,
                            "argument " + std::to_string(index + 1) + " of '" + name.text + "'",
                            error)) {
                    return false;
                }
                lowered.push_back(arguments[index].value);
            }
            lastExpressionCalled_ = true;
            value.type = signature.returnType;
            value.value =
                builder_.CreateCall(module_.getFunction(name.text), lowered, name.text + ".result");
            return true;
        }
        if (GlobalSlot(name.text) != nullptr) {
            error = "line " + std::to_string(name.line) + ": global '" + name.text +
                    "' holds a value, not a function";
            return false;
        }
        // A record name is a type, and calling one is a common typo for
        // constructing one.
        if (RecordIndex(name.text) >= 0) {
            error = "line " + std::to_string(name.line) + ": " + name.text +
                    " is a struct; construct it with " + name.text + " { field = value, ... }";
            return false;
        }
        std::string message =
            "line " + std::to_string(name.line) + ": unknown function '" + name.text + "'";
        const std::string suggestion = SuggestHostName(name.text);
        if (!suggestion.empty()) message += "; did you mean '" + suggestion + "'?";
        error = std::move(message);
        return false;
    }

    llvm::Module& module_;
    llvm::Function& function_;
    std::vector<Token> tokens_;
    std::vector<CvmParameter> parameters_;
    CvmValueType returnType_;
    CvmEmitScope scope_;
    llvm::IRBuilder<> builder_;
    llvm::Type* i64_ = nullptr;
    llvm::Type* f64_ = nullptr;
    llvm::Type* i32_ = nullptr;
    llvm::Type* str_ = nullptr;
    std::unordered_map<std::string, llvm::Value*> literals_;
    std::unordered_map<std::string, Variable> variables_;
    struct LoopBlocks {
        llvm::BasicBlock* continueTarget = nullptr;
        llvm::BasicBlock* breakTarget = nullptr;
    };
    std::vector<LoopBlocks> loops_;
    std::size_t position_ = 0;
    bool lastExpressionCalled_ = false;
};

} // namespace

bool EmitCvmFunction(llvm::Module& module, llvm::Function& function, std::string_view body,
                     int sourceLine, const std::vector<CvmParameter>& parameters,
                     CvmValueType returnType, const CvmEmitScope& scope, std::string& error)
{
    std::vector<Token> tokens;
    if (!Tokenize(body, sourceLine, tokens, error)) {
        error = "@" + function.getName().str() + ": " + error;
        return false;
    }
    // The entry block must exist before the emitter reads it: declarations are
    // hoisted into it, so it is the emitter's anchor for the whole function.
    if (function.empty()) {
        llvm::BasicBlock::Create(module.getContext(), "entry", &function);
    }
    Emitter emitter(module, function, std::move(tokens), parameters, returnType, scope);
    std::string localError;
    if (!emitter.Run(localError)) {
        error = "@" + function.getName().str() + ": " + localError;
        return false;
    }
    return true;
}

} // namespace ConcordScript
