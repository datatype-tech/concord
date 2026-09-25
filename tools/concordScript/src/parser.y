%{
/*
 * ConcordScript parser.
 *
 * Assembles the tokens lexer.l produces into a ConcordScript::SourceFile.
 * Grammar covers ConcordScript's own constructs and preserves generic
 * annotations as opaque, extensible AST nodes. RAW_CODE and captured bodies
 * remain opaque text handed straight to the AST without further parsing.
 *
 * A class header's extends/implements clause is fully reduced before the
 * class_header action runs, so its pieces are collected into a few
 * temporary globals (g_pendingBase / g_pendingInterfaces) rather than
 * pushed straight onto an AST node that does not exist until the whole
 * header is known.
 */

#include "Ast.h"
#include "AnnotationText.h"
#include "CliReport.h"

#include <cstdio>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace ConcordScript;

extern int yylex();
extern int yylineno;
extern void ResetConcordScriptLexerState() noexcept;
void yyerror(const char* msg);

static SourceFile g_result;
static std::string g_sourcePath;
static bool g_parseFailed = false;
static bool g_pendingRegister = false;
static std::string g_pendingBase;
static std::vector<std::string> g_pendingInterfaces;

static std::string TrimAnnotationText(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

static std::vector<AnnotationArg> ParseAnnotationArgs(const std::string& text)
{
    std::vector<AnnotationArg> result;
    auto append = [&result](const std::string& source) {
        const std::string raw = TrimAnnotationText(source);
        if (raw.empty()) {
            return;
        }
        AnnotationArg arg;
        arg.raw = raw;
        const size_t equals = AnnotationText::FindTopLevelEquals(raw);
        if (equals == std::string::npos) {
            arg.value = raw;
        } else {
            arg.hasKey = true;
            arg.key = TrimAnnotationText(raw.substr(0, equals));
            arg.value = TrimAnnotationText(raw.substr(equals + 1));
        }
        arg.quoted = arg.value.size() >= 2 &&
                     ((arg.value.front() == '\"' && arg.value.back() == '\"') ||
                      (arg.value.front() == '\'' && arg.value.back() == '\''));
        result.push_back(std::move(arg));
    };

    for (const std::string& argument : AnnotationText::SplitTopLevel(text)) append(argument);
    return result;
}

static std::string LowerAnnotationKey(std::string text)
{
    for (char& character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return TrimAnnotationText(text);
}

static void PrepareAnnotation(Annotation& annotation)
{
    if (!annotation.hasArgs) {
        return;
    }
    const std::string trimmedArgs = TrimAnnotationText(annotation.argsText);
    if (!trimmedArgs.empty()) {
        const std::vector<std::string> parts = AnnotationText::SplitTopLevel(annotation.argsText);
        if (parts.size() > 1) {
            for (const std::string& part : parts) {
                if (!TrimAnnotationText(part).empty()) continue;
                g_parseFailed = true;
                ConcordScript::ReportError("ConcordScript annotation error at line " +
                                           std::to_string(annotation.line) + ": empty argument");
                break;
            }
        }
    }
    annotation.args = ParseAnnotationArgs(annotation.argsText);
    std::unordered_set<std::string> keys;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey) {
            continue;
        }
        const std::string key = LowerAnnotationKey(argument.key);
        if (key.empty()) {
            g_parseFailed = true;
            ConcordScript::ReportError("ConcordScript annotation error at line " +
                                       std::to_string(annotation.line) + ": empty argument key");
        } else if (!keys.insert(key).second) {
            g_parseFailed = true;
            ConcordScript::ReportError("ConcordScript annotation error at line " +
                                       std::to_string(annotation.line) + ": duplicate argument '" +
                                       key + "'");
        }
    }
}

static bool ConsumeRegisterMarkers(std::vector<Annotation>& annotations)
{
    bool registered = false;
    std::vector<Annotation> filtered;
    filtered.reserve(annotations.size());
    for (Annotation& annotation : annotations) {
        if (annotation.isRegister) {
            registered = true;
        } else {
            filtered.push_back(std::move(annotation));
        }
    }
    annotations = std::move(filtered);
    return registered;
}

static ClassDecl::Role ClassRole(const std::vector<Annotation>& annotations, int line)
{
    ClassDecl::Role role = ClassDecl::Role::Regular;
    for (const Annotation& annotation : annotations) {
        const std::string name = LowerAnnotationKey(annotation.name);
        ClassDecl::Role next = role;
        if (name == "component") next = ClassDecl::Role::Component;
        else if (name == "system") next = ClassDecl::Role::System;
        else continue;
        if (role != ClassDecl::Role::Regular && role != next) {
            g_parseFailed = true;
            ConcordScript::ReportError("ConcordScript annotation error at line " +
                                       std::to_string(line) +
                                       ": class cannot be both component and system");
        }
        role = next;
    }
    return role;
}

static void FillUse(UseDirective& use, const std::string& path)
{
    use.target = path;
    use.line = yylineno;
    use.isStdlib = path.rfind("std.", 0) == 0;
    use.isEngineHeader = !use.isStdlib &&
                         (path.rfind("Concord.", 0) == 0 || path.find('/') != std::string::npos);
}

%}

%code requires {
#include "Ast.h"
#include <string>
#include <vector>
}

%union {
    std::string* str;
    ConcordScript::Annotation* annotation;
    std::vector<ConcordScript::Annotation>* annotations;
    int line;
}

%token USE_KW CLASS_KW EXTENDS_KW IMPLEMENTS_KW
%token <line> AT_ENTRY
%token SEMI COMMA
%token <annotation> AT_IDENT
%token <str> ANNOTATION_ARGS ANNOTATION_BODY
%token <str> USE_PATH IDENT_TOK RAW_CODE CAPTURED_BODY
%type <annotation> annotation
%type <annotations> annotation_prefix
%type <annotations> class_prefix

%destructor { delete $$; } <str>
%destructor { delete $$; } <annotation>
%destructor { delete $$; } <annotations>

%%

program:
    /* empty */
  | program top_level_item
  ;

top_level_item:
    use_stmt
  | class_stmt
  | entry_stmt
  | annotation_stmt
  | raw_stmt
  ;

raw_stmt:
    RAW_CODE {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Raw;
        item.raw.text = std::move(*$1);
        delete $1;
        g_result.items.push_back(std::move(item));
    }
  | annotation_prefix RAW_CODE {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Raw;
        item.raw.text = std::move(*$2);
        item.raw.annotations = std::move(*$1);
        ConsumeRegisterMarkers(item.raw.annotations);
        delete $1;
        delete $2;
        g_result.items.push_back(std::move(item));
    }
  ;

use_stmt:
    USE_KW USE_PATH SEMI {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Use;
        const std::string& path = *$2;
        FillUse(item.use, path);
        delete $2;
        g_result.items.push_back(std::move(item));
    }
  | annotation_prefix USE_KW USE_PATH SEMI {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Use;
        const std::string& path = *$3;
        FillUse(item.use, path);
        item.use.annotations = std::move(*$1);
        ConsumeRegisterMarkers(item.use.annotations);
        delete $1;
        delete $3;
        g_result.items.push_back(std::move(item));
    }
  ;

class_stmt:
    class_prefix class_header CAPTURED_BODY SEMI {
        TopLevelItem& item = g_result.items.back();
        item.classDecl.bodyText = std::move(*$3);
        item.classDecl.annotations = std::move(*$1);
        item.classDecl.isRegistered = item.classDecl.isRegistered ||
                                      ConsumeRegisterMarkers(item.classDecl.annotations);
        item.classDecl.role = ClassRole(item.classDecl.annotations, item.classDecl.line);
        if (item.classDecl.role == ClassDecl::Role::Component) {
            item.classDecl.isRegistered = true;
        }
        delete $1;
        delete $3;
    }
  ;

class_prefix:
    /* empty */ {
        $$ = new std::vector<Annotation>;
        g_pendingRegister = false;
    }
  | annotation_prefix {
        $$ = $1;
        g_pendingRegister = false;
    }
  ;

class_header:
    CLASS_KW IDENT_TOK base_clause {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Class;
        item.classDecl.name = std::move(*$2);
        item.classDecl.line = yylineno;
        delete $2;
        item.classDecl.baseClass = std::move(g_pendingBase);
        item.classDecl.interfaces = std::move(g_pendingInterfaces);
        item.classDecl.isRegistered = g_pendingRegister;
        g_pendingBase.clear();
        g_pendingInterfaces.clear();
        g_pendingRegister = false;
        g_result.items.push_back(std::move(item));
    }
  ;

base_clause:
    /* empty */
  | EXTENDS_KW IDENT_TOK {
        g_pendingBase = std::move(*$2);
        delete $2;
    }
  | EXTENDS_KW IDENT_TOK IMPLEMENTS_KW ident_list {
        g_pendingBase = std::move(*$2);
        delete $2;
    }
  | IMPLEMENTS_KW ident_list
  ;

ident_list:
    IDENT_TOK {
        g_pendingInterfaces.push_back(std::move(*$1));
        delete $1;
    }
  | ident_list COMMA IDENT_TOK {
        g_pendingInterfaces.push_back(std::move(*$3));
        delete $3;
    }
  ;

entry_stmt:
    AT_ENTRY CAPTURED_BODY SEMI {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Entry;
        item.entry.line = $1;
        item.entry.body = std::move(*$2);
        delete $2;
        g_result.items.push_back(std::move(item));
    }
    | annotation_prefix AT_ENTRY CAPTURED_BODY SEMI {
        TopLevelItem item;
        item.kind = TopLevelItem::Kind::Entry;
        item.entry.line = $2;
        item.entry.body = std::move(*$3);
        item.entry.annotations = std::move(*$1);
        ConsumeRegisterMarkers(item.entry.annotations);
        delete $1;
        delete $3;
        g_result.items.push_back(std::move(item));
    }
  ;

annotation_prefix:
    annotation {
        $$ = new std::vector<Annotation>;
        $$->push_back(std::move(*$1));
        delete $1;
    }
  | annotation_prefix annotation {
        $$ = $1;
        $$->push_back(std::move(*$2));
        delete $2;
    }
  ;

annotation_stmt:
    annotation_prefix SEMI {
        ConsumeRegisterMarkers(*$1);
        for (Annotation& annotation : *$1) {
            TopLevelItem item;
            item.kind = TopLevelItem::Kind::Annotation;
            item.annotation = std::move(annotation);
            g_result.items.push_back(std::move(item));
        }
        delete $1;
    }
  ;

annotation:
    AT_IDENT { $$ = $1; }
  | AT_IDENT ANNOTATION_ARGS {
        $$ = $1;
        $$->hasArgs = true;
        $$->argsText = std::move(*$2);
        PrepareAnnotation(*$$);
        delete $2;
    }
  | AT_IDENT ANNOTATION_BODY {
        $$ = $1;
        $$->hasBody = true;
        $$->bodyText = std::move(*$2);
        delete $2;
    }
  | AT_IDENT ANNOTATION_ARGS ANNOTATION_BODY {
        $$ = $1;
        $$->hasArgs = true;
        $$->argsText = std::move(*$2);
        PrepareAnnotation(*$$);
        $$->hasBody = true;
        $$->bodyText = std::move(*$3);
        delete $2;
        delete $3;
    }
  ;

%%

void yyerror(const char* msg)
{
    g_parseFailed = true;
    ConcordScript::ReportLocated(g_sourcePath, yylineno, true, msg);
}

namespace ConcordScript {

SourceFile& ParserResult() { return g_result; }

bool ParserHadError() noexcept { return g_parseFailed; }

void ResetParserResult(const std::string& moduleName, const std::string& sourcePath)
{
    g_sourcePath = sourcePath;
    g_result = SourceFile{};
    g_result.moduleName = moduleName;
    g_parseFailed = false;
    g_pendingRegister = false;
    g_pendingBase.clear();
    g_pendingInterfaces.clear();
    ResetConcordScriptLexerState();
    yylineno = 1;
}

} // namespace ConcordScript
