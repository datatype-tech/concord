// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmProgram.h"

#include "AnnotationValues.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ConcordScript {

namespace {

std::string Trim(std::string value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

std::string Lower(std::string value)
{
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string Arg(const Annotation& annotation, std::string_view key)
{
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey && Lower(Trim(argument.key)) == Lower(std::string(key))) {
            return AnnotationValues::Unquote(argument.value);
        }
    }
    return {};
}

bool IsCvmAnnotation(const Annotation& annotation)
{
    return Lower(annotation.name) == "cvm";
}

bool IsCvmGlobalAnnotation(const Annotation& annotation)
{
    return Lower(annotation.name) == "cvm_global";
}

bool IsIdentifier(std::string_view text)
{
    if (text.empty()) return false;
    if (std::isalpha(static_cast<unsigned char>(text[0])) == 0 && text[0] != '_') return false;
    for (const char character : text) {
        if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_') {
            return false;
        }
    }
    return true;
}

/** Returns text without comments, keeping string literal contents intact. */
std::string StripComments(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        const char character = text[index];
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            while (index < text.size() && text[index] != '\n') ++index;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '*') {
            index += 2;
            while (index + 1 < text.size() && !(text[index] == '*' && text[index + 1] == '/')) {
                if (text[index] == '\n') result += '\n';
                ++index;
            }
            index = index + 1 < text.size() ? index + 2 : text.size();
            continue;
        }
        if (character == '"' || character == '\'') {
            const char quote = character;
            result += character;
            ++index;
            while (index < text.size()) {
                result += text[index];
                if (text[index] == '\\' && index + 1 < text.size()) {
                    result += text[index + 1];
                    index += 2;
                    continue;
                }
                if (text[index] == quote) {
                    ++index;
                    break;
                }
                ++index;
            }
            continue;
        }
        result += character;
        ++index;
    }
    return result;
}

/** The struct table as it is being built, so a field can name another struct. */
struct StructTable {
    /** Declared names, in declaration order; the index is the struct index. */
    std::vector<std::string> names;

    /** Field lists, filled in as declarations are parsed. */
    std::vector<CvmStruct> structs;

    /** Returns the index of \p name, or -1 when there is no such struct. */
    int Find(const std::string& name) const
    {
        for (std::size_t index = 0; index < names.size(); ++index) {
            if (names[index] == name) return static_cast<int>(index);
        }
        return -1;
    }
};

/** Resolves a written type name against the simple kinds and the struct table. */
bool ParseTypeName(const std::string& text, const std::string& where, const StructTable& table,
                   CvmValueType& type, std::string& error)
{
    const std::string trimmed = Trim(text);
    const std::string lowered = Lower(trimmed);
    if (lowered == "i64") {
        type = Simple(CvmType::I64);
        return true;
    }
    if (lowered == "f64") {
        type = Simple(CvmType::F64);
        return true;
    }
    if (lowered == "str") {
        type = Simple(CvmType::Str);
        return true;
    }
    const int index = table.Find(trimmed);
    if (index >= 0) {
        type = Record(index);
        return true;
    }
    error = where + ": unknown type '" + trimmed + "'; CVM has i64, f64, str and any declared struct";
    return false;
}

/** One field as written, before its type is resolved. */
struct RawField {
    std::string name;
    std::string typeName;
};

/** One struct as written, before its fields are resolved. */
struct RawStruct {
    std::string name;
    std::vector<RawField> fields;
    int line = 0;
};

/**
 * Parses a top-level struct declaration out of raw text.
 *
 * The lexer does not know this construct, so the whole declaration arrives as
 * one opaque fragment; that is enough, because the shape is fixed and small.
 */
bool ParseStructDeclaration(const std::string& body, std::size_t& at, const std::string& where,
                            RawStruct& out, std::string& error)
{
    const auto skipSpace = [&body](std::size_t& index) {
        while (index < body.size() &&
               std::isspace(static_cast<unsigned char>(body[index])) != 0) {
            ++index;
        }
    };
    const auto readIdentifier = [&body](std::size_t& index) {
        const std::size_t start = index;
        while (index < body.size() &&
               (std::isalnum(static_cast<unsigned char>(body[index])) != 0 || body[index] == '_')) {
            ++index;
        }
        return body.substr(start, index - start);
    };

    skipSpace(at);
    if (body.compare(at, 6, "struct") != 0 ||
        (at + 6 < body.size() && std::isspace(static_cast<unsigned char>(body[at + 6])) == 0 &&
         body[at + 6] != '{')) {
        error = where + ": expected a struct declaration";
        return false;
    }
    at += 6;
    skipSpace(at);
    out.name = readIdentifier(at);
    if (!IsIdentifier(out.name)) {
        error = where + ": 'struct' needs a name";
        return false;
    }
    skipSpace(at);
    if (at >= body.size() || body[at] != '{') {
        error = where + ": struct " + out.name + " needs a body";
        return false;
    }
    ++at;

    for (;;) {
        skipSpace(at);
        if (at < body.size() && body[at] == '}') {
            ++at;
            break;
        }
        if (at >= body.size()) {
            error = where + ": struct " + out.name + " is missing its closing brace";
            return false;
        }
        RawField field;
        field.name = readIdentifier(at);
        if (!IsIdentifier(field.name)) {
            error = where + ": struct " + out.name + " has a field without a name";
            return false;
        }
        skipSpace(at);
        if (at >= body.size() || body[at] != ':') {
            error = where + ": field '" + field.name + "' of struct " + out.name +
                    " needs a type after ':'";
            return false;
        }
        ++at;
        skipSpace(at);
        // A type name runs to the terminating semicolon.
        const std::size_t typeStart = at;
        while (at < body.size() && body[at] != ';' && body[at] != '}') ++at;
        field.typeName = Trim(body.substr(typeStart, at - typeStart));
        if (field.typeName.empty()) {
            error = where + ": field '" + field.name + "' of struct " + out.name +
                    " has no type";
            return false;
        }
        skipSpace(at);
        if (at < body.size() && body[at] == ';') ++at;
        for (const RawField& existing : out.fields) {
            if (existing.name == field.name) {
                error = where + ": struct " + out.name + " declares field '" + field.name +
                        "' twice";
                return false;
            }
        }
        out.fields.push_back(std::move(field));
    }

    // A declaration ends with a semicolon, the way every other top-level
    // construct in ConcordScript does.
    skipSpace(at);
    if (at < body.size() && body[at] == ';') ++at;

    if (out.fields.empty()) {
        error = where + ": struct " + out.name + " declares no fields";
        return false;
    }
    return true;
}

/**
 * Reports whether \p index reaches itself by following field types.
 *
 * A record that contains itself has no size, and the compiler would not notice
 * until it tried to lay one out.
 */
bool DetectsCycle(int index, const std::vector<CvmStruct>& structs, std::vector<int>& state)
{
    if (state[static_cast<std::size_t>(index)] == 1) return true;   // on the current path
    if (state[static_cast<std::size_t>(index)] == 2) return false;  // already cleared
    state[static_cast<std::size_t>(index)] = 1;
    for (const CvmField& field : structs[static_cast<std::size_t>(index)].fields) {
        if (field.type.kind != CvmType::Struct) continue;
        if (DetectsCycle(field.type.structIndex, structs, state)) return true;
    }
    state[static_cast<std::size_t>(index)] = 2;
    return false;
}

/**
 * Decodes a numeric literal, reporting which of the two types it is.
 *
 * A literal is a float when it has a fraction or an exponent; everything else
 * is an integer. The tokenizer applies the same rule, kept in one place so a
 * global and a local cannot disagree about what 1.5 means.
 */
bool ParseLiteral(const std::string& text, const std::string& where, CvmType& type,
                  std::int64_t& integerValue, double& floatValue, std::string& error)
{
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        error = where + ": expected a numeric literal";
        return false;
    }
    const bool isFloat = trimmed.find('.') != std::string::npos ||
                         trimmed.find('e') != std::string::npos ||
                         trimmed.find('E') != std::string::npos;
    char* end = nullptr;
    if (isFloat) {
        const double value = std::strtod(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || *end != '\0') {
            error = where + ": '" + trimmed + "' is not a valid number";
            return false;
        }
        type = CvmType::F64;
        floatValue = value;
        return true;
    }
    const long long value = std::strtoll(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || *end != '\0') {
        error = where + ": '" + trimmed + "' is not a valid number";
        return false;
    }
    type = CvmType::I64;
    integerValue = static_cast<std::int64_t>(value);
    return true;
}

/** Decodes a quoted literal, applying the same escapes the tokenizer does. */
bool ParseStringLiteral(const std::string& text, const std::string& where, std::string& decoded,
                        std::string& error)
{
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
        error = where + ": expected a quoted string literal";
        return false;
    }
    for (std::size_t index = 1; index + 1 < text.size(); ++index) {
        const char character = text[index];
        if (character != '\\') {
            decoded += character;
            continue;
        }
        if (index + 2 >= text.size()) {
            error = where + ": a trailing backslash in a string literal";
            return false;
        }
        const char escape = text[++index];
        switch (escape) {
            case 'n': decoded += '\n'; break;
            case 't': decoded += '\t'; break;
            case 'r': decoded += '\r'; break;
            case '\\': decoded += '\\'; break;
            case '"': decoded += '"'; break;
            default:
                error = where + ": unknown escape '\\" + std::string(1, escape) +
                        "' in a string literal";
                return false;
        }
    }
    return true;
}

/** Splits an args="a, b: f64" list, rejecting anything that is not a name. */
bool ParseParameters(const std::string& text, const std::string& where, const StructTable& table,
                     std::vector<CvmParameter>& parameters, std::string& error)
{
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t comma = text.find(',', index);
        const std::string piece =
            Trim(text.substr(index, comma == std::string::npos ? std::string::npos : comma - index));
        index = comma == std::string::npos ? text.size() : comma + 1;
        if (piece.empty()) continue;

        const std::size_t colon = piece.find(':');
        const std::string name = Trim(piece.substr(0, colon));
        if (!IsIdentifier(name)) {
            error = where + ": '" + name + "' is not a valid parameter name";
            return false;
        }
        for (const CvmParameter& existing : parameters) {
            if (existing.name == name) {
                error = where + ": duplicate parameter '" + name + "'";
                return false;
            }
        }
        CvmParameter parameter;
        parameter.name = name;
        if (colon != std::string::npos &&
            !ParseTypeName(piece.substr(colon + 1), where, table, parameter.type, error)) {
            return false;
        }
        parameters.push_back(std::move(parameter));
    }
    return true;
}

/**
 * Parses one @cvm_global body, which is a list of literal declarations.
 *
 * Only literals are accepted: a global is initialized after the module is
 * loaded and before any function runs, so there is nothing meaningful for a
 * richer initializer to call. A record initializer is a brace list of literals
 * for the same reason.
 */
bool ParseGlobals(const Annotation& annotation, const std::string& where, const StructTable& table,
                  std::vector<CvmGlobal>& globals, std::string& error)
{
    const std::string body = StripComments(annotation.bodyText);
    std::size_t index = 0;
    while (index < body.size()) {
        const std::size_t semicolon = body.find(';', index);
        const std::string statement =
            Trim(body.substr(index, semicolon == std::string::npos ? std::string::npos
                                                                   : semicolon - index));
        index = semicolon == std::string::npos ? body.size() : semicolon + 1;
        if (statement.empty()) continue;

        if (statement.rfind("let", 0) != 0 ||
            (statement.size() > 3 &&
             std::isspace(static_cast<unsigned char>(statement[3])) == 0)) {
            error = where + ": @cvm_global accepts only 'let name = literal;' declarations, found '" +
                    statement + "'";
            return false;
        }
        const std::size_t equals = statement.find('=');
        if (equals == std::string::npos) {
            error = where + ": global '" + Trim(statement.substr(3)) + "' needs an initializer";
            return false;
        }
        const std::string declared = Trim(statement.substr(3, equals - 3));
        const std::size_t colon = declared.find(':');
        const std::string name = Trim(declared.substr(0, colon));
        if (!IsIdentifier(name)) {
            error = where + ": '" + name + "' is not a valid global name";
            return false;
        }

        CvmGlobal global;
        global.name = name;
        global.line = annotation.line;
        CvmValueType written = Simple(CvmType::I64);
        const bool annotated = colon != std::string::npos;
        if (annotated && !ParseTypeName(declared.substr(colon + 1), where, table, written, error)) {
            return false;
        }

        const std::string initializer = Trim(statement.substr(equals + 1));
        if (!initializer.empty() && initializer.front() == '"') {
            std::string decoded;
            if (!ParseStringLiteral(initializer, where, decoded, error)) return false;
            if (annotated && written.kind != CvmType::Str) {
                error = where + ": global '" + name + "' is declared a number but initialized with "
                                                      "a string";
                return false;
            }
            global.type = Simple(CvmType::Str);
            global.textValue = std::move(decoded);
            globals.push_back(std::move(global));
            continue;
        }

        // A record initializer: Name { field = literal, ... }.
        if (!initializer.empty() && initializer.front() != '"' &&
            initializer.find('{') != std::string::npos) {
            const std::size_t brace = initializer.find('{');
            const std::string typeName = Trim(initializer.substr(0, brace));
            const int structIndex = table.Find(typeName);
            if (structIndex < 0) {
                error = where + ": global '" + name + "' initializes unknown struct '" + typeName +
                        "'";
                return false;
            }
            if (annotated && (written.kind != CvmType::Struct ||
                              written.structIndex != structIndex)) {
                error = where + ": global '" + name + "' is declared a different type than the "
                                                      "record it is initialized with";
                return false;
            }
            std::string inner = initializer.substr(brace + 1);
            const std::size_t close = inner.rfind('}');
            if (close == std::string::npos) {
                error = where + ": global '" + name + "' has an unterminated record initializer";
                return false;
            }
            inner = inner.substr(0, close);

            const CvmStruct& record = table.structs[static_cast<std::size_t>(structIndex)];
            std::vector<double> values(record.fields.size(), 0.0);
            std::vector<bool> seen(record.fields.size(), false);
            std::size_t at = 0;
            while (at < inner.size()) {
                const std::size_t comma = inner.find(',', at);
                const std::string piece = Trim(inner.substr(
                    at, comma == std::string::npos ? std::string::npos : comma - at));
                at = comma == std::string::npos ? inner.size() : comma + 1;
                if (piece.empty()) continue;
                const std::size_t fieldEquals = piece.find('=');
                if (fieldEquals == std::string::npos) {
                    error = where + ": global '" + name + "' record initializer needs field = value";
                    return false;
                }
                const std::string fieldName = Trim(piece.substr(0, fieldEquals));
                int slot = -1;
                for (std::size_t field = 0; field < record.fields.size(); ++field) {
                    if (record.fields[field].name == fieldName) slot = static_cast<int>(field);
                }
                if (slot < 0) {
                    error = where + ": struct " + typeName + " has no field '" + fieldName + "'";
                    return false;
                }
                if (record.fields[static_cast<std::size_t>(slot)].type.kind == CvmType::Str) {
                    error = where + ": record fields of type str cannot be initialized as constants "
                                    "yet";
                    return false;
                }
                CvmType literalType = CvmType::I64;
                std::int64_t literalInteger = 0;
                double literalFloat = 0.0;
                if (!ParseLiteral(piece.substr(fieldEquals + 1), where, literalType, literalInteger,
                                  literalFloat, error)) {
                    return false;
                }
                values[static_cast<std::size_t>(slot)] =
                    literalType == CvmType::F64 ? literalFloat : static_cast<double>(literalInteger);
                seen[static_cast<std::size_t>(slot)] = true;
            }
            for (std::size_t field = 0; field < seen.size(); ++field) {
                if (!seen[field]) {
                    error = where + ": global '" + name + "' does not initialize field '" +
                            record.fields[field].name + "'";
                    return false;
                }
            }
            global.type = Record(structIndex);
            global.fieldValues = std::move(values);
            globals.push_back(std::move(global));
            continue;
        }

        if (annotated && written.kind == CvmType::Struct) {
            error = where + ": global '" + name +
                    "' is a struct and needs a record initializer";
            return false;
        }
        if (annotated && written.kind == CvmType::Str) {
            error = where + ": global '" + name + "' is declared str but is initialized with a "
                                                  "number";
            return false;
        }

        CvmType literalType = CvmType::I64;
        std::int64_t literalInteger = 0;
        double literalFloat = 0.0;
        if (!ParseLiteral(initializer, where, literalType, literalInteger, literalFloat, error)) {
            return false;
        }
        if (annotated && written.kind == CvmType::F64) {
            global.type = Simple(CvmType::F64);
            global.floatValue = literalType == CvmType::F64 ? literalFloat
                                                            : static_cast<double>(literalInteger);
        } else {
            if (annotated && literalType == CvmType::F64) {
                error = where + ": global '" + name +
                        "' is i64 but is initialized with a float literal; write 'f64' or drop the "
                        "fraction";
                return false;
            }
            global.type = annotated ? Simple(CvmType::I64) : Simple(literalType);
            global.integerValue = literalInteger;
            global.floatValue = literalFloat;
        }
        globals.push_back(std::move(global));
    }
    if (globals.empty()) {
        error = where + ": @cvm_global declares no variables";
        return false;
    }
    return true;
}

} // namespace

std::optional<CvmProgram> CollectCvmProgram(const std::vector<SourceFile>& files, std::string& error)
{
    CvmProgram program;
    StructTable table;

    /** Each struct's fields as written, kept so they can be resolved once every
     *  name in the project is known. */
    std::vector<std::vector<RawField>> rawFieldTypes;

    // The struct table is built first, because a parameter, a global or a
    // field may name a record declared in any file, in any order.
    for (const SourceFile& file : files) {
        for (const TopLevelItem& item : file.items) {
            if (item.kind != TopLevelItem::Kind::Raw) continue;
            // The lexer does not model struct, so a run of declarations arrives
            // as one opaque fragment and is walked declaration by declaration.
            const std::string text = Trim(StripComments(item.raw.text));
            if (text.compare(0, 6, "struct") != 0) continue;
            const std::string where = file.moduleName;
            std::size_t at = 0;
            while (true) {
                while (at < text.size() &&
                       std::isspace(static_cast<unsigned char>(text[at])) != 0) {
                    ++at;
                }
                if (at >= text.size()) break;
                RawStruct raw;
                if (!ParseStructDeclaration(text, at, where, raw, error)) return std::nullopt;
                if (table.Find(raw.name) >= 0) {
                    error = where + ": duplicate struct '" + raw.name + "'";
                    return std::nullopt;
                }
                CvmStruct record;
                record.name = raw.name;
                record.line = 0;
                // Field types are resolved once every name is known, below.
                for (const RawField& field : raw.fields) {
                    record.fields.push_back(CvmField{field.name, Simple(CvmType::I64)});
                }
                table.names.push_back(raw.name);
                table.structs.push_back(std::move(record));
                rawFieldTypes.push_back(std::move(raw.fields));
            }
        }
    }

    // Second pass over the raw fields, now that every name resolves.
    for (std::size_t index = 0; index < table.structs.size(); ++index) {
        const std::string where = "struct " + table.names[index];
        const std::vector<RawField>& fields = rawFieldTypes[index];
        for (std::size_t field = 0; field < fields.size(); ++field) {
            if (!ParseTypeName(fields[field].typeName, where, table,
                               table.structs[index].fields[field].type, error)) {
                return std::nullopt;
            }
        }
    }
    std::vector<int> state(table.structs.size(), 0);
    for (std::size_t index = 0; index < table.structs.size(); ++index) {
        if (DetectsCycle(static_cast<int>(index), table.structs, state)) {
            error = "struct " + table.names[index] + " contains itself";
            return std::nullopt;
        }
    }
    program.structs = table.structs;

    std::unordered_set<std::string> names;
    std::unordered_set<std::string> globalNames;
    for (const SourceFile& file : files) {
        for (const TopLevelItem& item : file.items) {
            if (item.kind == TopLevelItem::Kind::Raw) {
                const std::string text = Trim(StripComments(item.raw.text));
                if (text.empty()) continue;
                if (text.compare(0, 6, "struct") == 0) continue;
                error = file.moduleName + ": CVM accepts only @cvm, @cvm_global and struct "
                                          "declarations, found raw C++ code (use --cpp for the C++ "
                                          "backend)";
                return std::nullopt;
            }
            if (item.kind == TopLevelItem::Kind::Annotation &&
                IsCvmGlobalAnnotation(item.annotation)) {
                const Annotation& annotation = item.annotation;
                const std::string where = file.moduleName + ":" + std::to_string(annotation.line);
                if (!annotation.hasBody) {
                    error = where + ": @cvm_global requires a body";
                    return std::nullopt;
                }
                std::vector<CvmGlobal> declared;
                if (!ParseGlobals(annotation, where, table, declared, error)) return std::nullopt;
                for (const CvmGlobal& global : declared) {
                    if (!globalNames.insert(global.name).second) {
                        error = where + ": duplicate global '" + global.name + "'";
                        return std::nullopt;
                    }
                    if (table.Find(global.name) >= 0) {
                        error = where + ": global '" + global.name + "' has the same name as a struct";
                        return std::nullopt;
                    }
                    program.globals.push_back(global);
                }
                continue;
            }
            if (item.kind == TopLevelItem::Kind::Annotation && IsCvmAnnotation(item.annotation)) {
                const Annotation& annotation = item.annotation;
                const std::string where = file.moduleName + ":" + std::to_string(annotation.line);
                if (!annotation.hasBody) {
                    error = where + ": @cvm requires a body";
                    return std::nullopt;
                }
                if (annotation.hasArgs && !annotation.argsText.empty()) {
                    for (const AnnotationArg& argument : annotation.args) {
                        const std::string key = Lower(Trim(argument.key));
                        if (argument.hasKey && key != "name" && key != "entry" && key != "args" &&
                            key != "ret") {
                            error = where + ": unknown @cvm argument '" + argument.key + "'";
                            return std::nullopt;
                        }
                    }
                }
                std::string name = Arg(annotation, "name");
                if (name.empty()) name = "main";
                if (!names.insert(name).second) {
                    error = where + ": duplicate @cvm function '" + name + "'";
                    return std::nullopt;
                }
                if (table.Find(name) >= 0) {
                    error = where + ": function '" + name + "' has the same name as a struct";
                    return std::nullopt;
                }
                CvmFunction function;
                function.name = name;
                function.body = annotation.bodyText;
                function.line = annotation.line;
                if (!ParseParameters(Arg(annotation, "args"), where + ": @" + name, table,
                                     function.parameters, error)) {
                    return std::nullopt;
                }
                const std::string resultType = Arg(annotation, "ret");
                if (!resultType.empty() &&
                    !ParseTypeName(resultType, where + ": @" + name, table, function.returnType,
                                   error)) {
                    return std::nullopt;
                }
                program.functions.push_back(std::move(function));
                if (Lower(Arg(annotation, "entry")) == "true") {
                    if (!program.entry.empty() && program.entry != name) {
                        error = where + ": multiple @cvm functions marked entry=true";
                        return std::nullopt;
                    }
                    program.entry = name;
                }
                continue;
            }
            std::string what;
            if (item.kind == TopLevelItem::Kind::Class) what = "class " + item.classDecl.name;
            else if (item.kind == TopLevelItem::Kind::Use) what = "use " + item.use.target;
            else if (item.kind == TopLevelItem::Kind::Entry) what = "@entry";
            else if (item.kind == TopLevelItem::Kind::Raw) {
                if (Trim(StripComments(item.raw.text)).empty()) continue;
                what = "raw C++ code";
            } else if (item.kind == TopLevelItem::Kind::Annotation) {
                what = "@" + item.annotation.name;
            }
            if (!what.empty()) {
                error = file.moduleName +
                        ": CVM accepts only @cvm, @cvm_global and struct declarations, found " +
                        what + " (use --cpp for the C++ backend)";
                return std::nullopt;
            }
        }
    }
    if (program.functions.empty()) {
        error = "CVM project contains no @cvm block";
        return std::nullopt;
    }
    for (const CvmFunction& function : program.functions) {
        for (const CvmParameter& parameter : function.parameters) {
            if (globalNames.count(parameter.name) != 0) {
                error = "@" + function.name + ": parameter '" + parameter.name +
                        "' shadows a global of the same name";
                return std::nullopt;
            }
        }
    }
    if (program.entry.empty()) program.entry = "main";
    for (const CvmFunction& function : program.functions) {
        if (function.name != program.entry) continue;
        // The entry is invoked by the JIT with no arguments, so it cannot ask
        // for any; saying so here beats an ABI mismatch at call time.
        if (!function.parameters.empty()) {
            error = "@" + function.name + ": the entry function cannot take parameters";
            return std::nullopt;
        }
        return program;
    }
    error = "CVM entry function '" + program.entry + "' was not defined";
    return std::nullopt;
}

} // namespace ConcordScript
