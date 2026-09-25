%{
/*
 * ConcordScript lexer.
 *
 * Only ConcordScript's own syntax is tokenized (use / @entry / @register /
 * generic @annotations / class / extends / implements / structural punctuation). Everything else
 * is accumulated into RAW_CODE using flex's longest-match rule: a keyword
 * pattern only wins over the raw-text fallback when it matches at the
 * current position and is followed by a non-identifier character, so
 * "use", "class", etc. appearing inside a longer identifier are never
 * mistaken for keywords.
 *
 * Class and @entry bodies are captured verbatim (braces balanced, string/
 * char literals and comments skipped via a pushed state so embedded braces
 * inside them don't confuse the depth count) by the BODY_CAPTURE states.
 * var/pub/priv/prot substitution happens later, in CodeGen, not here.
 */

#include "Ast.h"
#include "parser.tab.h"

#include <cctype>
#include <string>
#include <utility>

void yyerror(const char* message);
static void yy_pop_state(void);

static std::string g_rawBuffer;
static int g_braceDepth = 0;
static int g_parenDepth = 0;
static int g_rawBraceDepth = 0;
static int g_nativeBraceDepth = 0;
static std::string g_rawDelimiter;

static size_t SkipTrailingTrivia();
static bool HasTrailingLineBreak();

/** Returns whether a raw literal can begin at the current source boundary. */
static bool CanStartRawString()
{
    if (g_rawBuffer.empty()) return true;
    const char previous = g_rawBuffer.back();
    return !std::isalnum(static_cast<unsigned char>(previous)) && previous != '_';
}

/** Returns whether the preceding raw character is a safe annotation boundary. */
static bool CanStartAnnotation()
{
    if (g_rawBraceDepth != 0) {
        return false;
    }
    const size_t position = SkipTrailingTrivia();
    if (position == 0 || HasTrailingLineBreak()) return true;
    const char previous = g_rawBuffer[position - 1];
    return std::isspace(static_cast<unsigned char>(previous)) || previous == ';' ||
           previous == '{' || previous == '}' || previous == ':' || previous == ')';
}

/** Returns whether a top-level C++ structural keyword starts a statement. */
static bool CanStartStructuralKeyword()
{
    if (g_rawBraceDepth != 0) {
        return false;
    }
    const size_t position = SkipTrailingTrivia();
    if (position == 0 || HasTrailingLineBreak()) return true;
    const char previous = g_rawBuffer[position - 1];
    return previous == ';' || previous == '}';
}

/** Keeps the line-oriented `use` directive distinct from ordinary C++ identifiers. */
static bool CanStartUseKeyword()
{
    if (g_rawBraceDepth != 0) return false;
    const size_t position = SkipTrailingTrivia();
    if (position == 0 || HasTrailingLineBreak()) return true;
    const char previous = g_rawBuffer[position - 1];
    return previous == ';' || previous == '}';
}

/** Detects a pending multiline C++ template declaration before `class`/`struct`. */
static bool HasPendingTemplateHeader()
{
    size_t boundary = g_rawBuffer.find_last_of(";{}");
    boundary = boundary == std::string::npos ? 0 : boundary + 1;
    const std::string suffix = g_rawBuffer.substr(boundary);
    const size_t keyword = suffix.rfind("template");
    if (keyword == std::string::npos ||
        (keyword > 0 && (std::isalnum(static_cast<unsigned char>(suffix[keyword - 1])) ||
                         suffix[keyword - 1] == '_'))) return false;
    const size_t open = suffix.find('<', keyword + 8);
    if (open == std::string::npos) return false;
    int depth = 0;
    for (size_t index = open; index < suffix.size(); ++index) {
        if (suffix[index] == '<') ++depth;
        else if (suffix[index] == '>' && --depth == 0) {
            size_t tail = index + 1;
            while (tail < suffix.size() &&
                   std::isspace(static_cast<unsigned char>(suffix[tail]))) ++tail;
            return tail == suffix.size() ||
                   suffix.compare(tail, 5, "class") == 0 ||
                   suffix.compare(tail, 6, "struct") == 0;
        }
    }
    return true;
}

/** Removes whitespace and comments at the end of raw top-level text for boundary checks. */
static size_t SkipTrailingTrivia()
{
    size_t end = g_rawBuffer.size();
    for (;;) {
        while (end > 0 && std::isspace(static_cast<unsigned char>(g_rawBuffer[end - 1]))) --end;
        if (end >= 2 && g_rawBuffer.compare(end - 2, 2, "*/") == 0) {
            const size_t open = g_rawBuffer.rfind("/*", end - 2);
            if (open == std::string::npos) break;
            end = open;
            continue;
        }
        const size_t line = g_rawBuffer.rfind('\n', end == 0 ? 0 : end - 1);
        const size_t comment = g_rawBuffer.rfind("//", end == 0 ? 0 : end - 1);
        if (comment != std::string::npos && comment < end &&
            (line == std::string::npos || comment > line)) {
            end = comment;
            continue;
        }
        break;
    }
    return end;
}

/** Reports whether trailing whitespace/comments include a physical line break. */
static bool HasTrailingLineBreak()
{
    size_t end = g_rawBuffer.size();
    for (;;) {
        while (end > 0 && std::isspace(static_cast<unsigned char>(g_rawBuffer[end - 1]))) {
            if (g_rawBuffer[end - 1] == '\n') return true;
            --end;
        }
        if (end >= 2 && g_rawBuffer.compare(end - 2, 2, "*/") == 0) {
            const size_t open = g_rawBuffer.rfind("/*", end - 2);
            if (open == std::string::npos) return false;
            const size_t lineBreak = g_rawBuffer.find('\n', open);
            if (lineBreak != std::string::npos && lineBreak < end) return true;
            end = open;
            continue;
        }
        const size_t line = g_rawBuffer.rfind('\n', end == 0 ? 0 : end - 1);
        const size_t comment = g_rawBuffer.rfind("//", end == 0 ? 0 : end - 1);
        if (comment != std::string::npos && (line == std::string::npos || comment > line)) {
            return true;
        }
        return false;
    }
}

/** Applies the statement-boundary check to the class keyword. */
static bool CanStartClassKeyword()
{
    return CanStartStructuralKeyword() && !HasPendingTemplateHeader();
}
static bool g_capturePayload = false;
static int g_annotationLine = 1;
static std::string g_annotationName;

/** Clears annotation-specific context once its declaration has been reduced. */
static void ClearAnnotationContext()
{
    g_annotationLine = yylineno;
    g_annotationName.clear();
}

static void ReportAnnotationError(const char* message)
{
    if (g_annotationName.empty()) {
        yyerror(message);
        return;
    }
    const std::string detail = "@" + g_annotationName + " (started at line " +
                               std::to_string(g_annotationLine) + "): " + message;
    yyerror(detail.c_str());
}

/*
 * Pushes back everything the current rule matched so it is rescanned, and
 * returns the raw buffer accumulated so far as one RAW_CODE token. Used
 * whenever a keyword rule fires but raw text was pending before it.
 */
#define FLUSH_RAW_AND_RETRY() \
    do { \
        yyless(0); \
        yylval.str = new std::string(std::move(g_rawBuffer)); \
        g_rawBuffer.clear(); \
        return RAW_CODE; \
    } while (0)

/* Starts a stateful C++ raw-string capture while retaining the exact opener. */
#define BEGIN_RAW_STRING_CAPTURE() \
    do { \
        const std::string opener(yytext, static_cast<size_t>(yyleng)); \
        const size_t quote = opener.find('"'); \
        const size_t openParen = opener.rfind('('); \
        g_rawDelimiter = opener.substr(quote + 1, openParen - quote - 1); \
        g_rawBuffer += opener; \
        yy_push_state(BC_RAW_STRING); \
    } while (0)

#define EMIT_RAW_BUFFER() \
    do { \
        yylval.str = new std::string(std::move(g_rawBuffer)); \
        g_rawBuffer.clear(); \
        BEGIN(INITIAL); \
        return RAW_CODE; \
    } while (0)

static void AppendRawStringChunk(const char* text, size_t length)
{
    g_rawBuffer.append(text, length);
    const std::string close = ")" + g_rawDelimiter + '"';
    if (g_rawBuffer.size() >= close.size() &&
        g_rawBuffer.compare(g_rawBuffer.size() - close.size(), close.size(), close) == 0) {
        g_rawDelimiter.clear();
        yy_pop_state();
    }
}

%}

%option noyywrap
%option nounput
%option stack
%option yylineno

%x USE_MODE
%x CLASS_HEADER
%x AFTER_CLASS_BODY
%x ENTRY_MODE
%x AFTER_ENTRY_BODY
%x ANNOTATION_AFTER
%x ANNOTATION_ARGS_CAPTURE
%x ANNOTATION_BODY_CAPTURE
%x BODY_CAPTURE_CLASS
%x BODY_CAPTURE_ENTRY
%x NATIVE_CLASS_CAPTURE
%x NATIVE_CLASS_AFTER
%x SKIP_LINECOMMENT
%x SKIP_BLOCKCOMMENT
%x BC_STRING
%x BC_CHAR
%x BC_RAW_STRING
%x BC_LINECOMMENT
%x BC_BLOCKCOMMENT

IDENT [a-zA-Z_][a-zA-Z0-9_]*
ANNOT_NAME {IDENT}((::|[./-]){IDENT})*
RAW_PREFIX (u8R[\"]|uR[\"]|UR[\"]|LR[\"]|R[\"])
RAW_DELIMITER [^()\\ \t\v\f\r\n]*
RAW_OPEN {RAW_PREFIX}{RAW_DELIMITER}\(
NATIVE_CLASS_HEADER class[ \t\r\n]+{IDENT}(::{IDENT})*[ \t\r\n]*(final[ \t\r\n]*|:)
NATIVE_CLASS_FORWARD class[ \t\r\n]+{IDENT}(::{IDENT})*[ \t\r\n]*;

%%

<INITIAL,BODY_CAPTURE_CLASS,BODY_CAPTURE_ENTRY,ANNOTATION_ARGS_CAPTURE,ANNOTATION_BODY_CAPTURE,NATIVE_CLASS_CAPTURE>{RAW_OPEN} { if (CanStartRawString()) BEGIN_RAW_STRING_CAPTURE(); else g_rawBuffer.append(yytext, static_cast<size_t>(yyleng)); }

<INITIAL>{
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    "\""                         { g_rawBuffer += '"'; yy_push_state(BC_STRING); }
    "'"                           { g_rawBuffer += '\''; yy_push_state(BC_CHAR); }
    {NATIVE_CLASS_FORWARD}         { if (!CanStartStructuralKeyword()) { g_rawBuffer.append(yytext, static_cast<size_t>(yyleng)); break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   g_rawBuffer.append(yytext, static_cast<size_t>(yyleng));
                                   EMIT_RAW_BUFFER(); }
    {NATIVE_CLASS_HEADER}          { if (!CanStartStructuralKeyword()) { g_rawBuffer.append(yytext, static_cast<size_t>(yyleng)); break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   g_rawBuffer.append(yytext, static_cast<size_t>(yyleng));
                                   g_nativeBraceDepth = 0; BEGIN(NATIVE_CLASS_CAPTURE); }
    "use"/[^a-zA-Z0-9_]         { if (!CanStartUseKeyword()) { g_rawBuffer += yytext; break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   BEGIN(USE_MODE); return USE_KW; }
    "@entry"/[ \t\r\n{/]         { if (!CanStartAnnotation()) { g_rawBuffer += yytext; break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   yylval.line = yylineno;
                                   BEGIN(ENTRY_MODE); return AT_ENTRY; }
    "@register"/[ \t\r\n({;/]    { if (!CanStartAnnotation()) { g_rawBuffer += yytext; break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   yylval.annotation = new ConcordScript::Annotation{};
                                   yylval.annotation->name = "register";
                                   yylval.annotation->isRegister = true;
                                   yylval.annotation->line = yylineno;
                                   g_annotationLine = yylineno;
                                   g_annotationName = "register";
                                   BEGIN(ANNOTATION_AFTER); return AT_IDENT; }
    "@"{ANNOT_NAME}               { if (!CanStartAnnotation()) { g_rawBuffer += yytext; break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   yylval.annotation = new ConcordScript::Annotation{};
                                   yylval.annotation->name.assign(yytext + 1, yyleng - 1);
                                   yylval.annotation->line = yylineno;
                                   g_annotationLine = yylineno;
                                   g_annotationName = yylval.annotation->name;
                                   BEGIN(ANNOTATION_AFTER); return AT_IDENT; }
    "class"/[^a-zA-Z0-9_]       { if (!CanStartClassKeyword()) { g_rawBuffer += yytext; break; }
                                   if (!g_rawBuffer.empty()) FLUSH_RAW_AND_RETRY();
                                   BEGIN(CLASS_HEADER); return CLASS_KW; }
    \n                           { g_rawBuffer += '\n'; }
    .                             { g_rawBuffer += yytext[0];
                                   if (yytext[0] == '{') ++g_rawBraceDepth;
                                   else if (yytext[0] == '}' && g_rawBraceDepth > 0) {
                                       --g_rawBraceDepth;
                                   } }
    <<EOF>>                       {
        if (!g_rawBuffer.empty()) {
            yylval.str = new std::string(std::move(g_rawBuffer));
            g_rawBuffer.clear();
            return RAW_CODE;
        }
        yyterminate();
    }
}

<USE_MODE>{
    [ \t\r\n]+                   { /* skip */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    [a-zA-Z_][a-zA-Z0-9_/.]*     { yylval.str = new std::string(yytext); return USE_PATH; }
    ";"                           { ClearAnnotationContext(); BEGIN(INITIAL); return SEMI; }
}

<CLASS_HEADER>{
    [ \t\r\n]+                   { /* skip */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    "extends"/[^a-zA-Z0-9_]     { return EXTENDS_KW; }
    "implements"/[^a-zA-Z0-9_]  { return IMPLEMENTS_KW; }
    {IDENT}(::{IDENT})*          { yylval.str = new std::string(yytext); return IDENT_TOK; }
    ","                           { return COMMA; }
    "{"                           { g_braceDepth = 1; g_capturePayload = true;
                                   g_annotationName.clear();
                                   g_rawBuffer.clear(); BEGIN(BODY_CAPTURE_CLASS); }
}

<BODY_CAPTURE_CLASS,BODY_CAPTURE_ENTRY>{
    "{"                           { ++g_braceDepth; g_rawBuffer += '{'; }
    "}"                           {
        --g_braceDepth;
        if (g_braceDepth == 0) {
            yylval.str = new std::string(std::move(g_rawBuffer));
            g_rawBuffer.clear();
            g_capturePayload = false;
            int wasClass = (YY_START == BODY_CAPTURE_CLASS);
            BEGIN(wasClass ? AFTER_CLASS_BODY : AFTER_ENTRY_BODY);
            return CAPTURED_BODY;
        }
        g_rawBuffer += '}';
    }
    "\""                          { g_rawBuffer += '"';  yy_push_state(BC_STRING); }
    "'"                           { g_rawBuffer += '\''; yy_push_state(BC_CHAR); }
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
}

<NATIVE_CLASS_CAPTURE>{
    "{"                           { ++g_nativeBraceDepth; g_rawBuffer += '{'; }
    "}"                           { if (g_nativeBraceDepth > 0) --g_nativeBraceDepth;
                                   g_rawBuffer += '}';
                                   if (g_nativeBraceDepth == 0) BEGIN(NATIVE_CLASS_AFTER); }
    "\""                         { g_rawBuffer += '"'; yy_push_state(BC_STRING); }
    "'"                           { g_rawBuffer += '\''; yy_push_state(BC_CHAR); }
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
    <<EOF>>                       { EMIT_RAW_BUFFER(); }
}

<NATIVE_CLASS_AFTER>{
    [ \t\r\n]+                   { g_rawBuffer.append(yytext, static_cast<size_t>(yyleng)); }
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    ";"                           { g_rawBuffer += ';'; EMIT_RAW_BUFFER(); }
    .                              { yyless(0); EMIT_RAW_BUFFER(); }
    <<EOF>>                       { EMIT_RAW_BUFFER(); }
}

<BC_STRING>{
    \\.                          { g_rawBuffer += yytext[0]; g_rawBuffer += yytext[1]; }
    "\""                          { g_rawBuffer += '"'; yy_pop_state(); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
}

<BC_CHAR>{
    \\.                          { g_rawBuffer += yytext[0]; g_rawBuffer += yytext[1]; }
    "'"                           { g_rawBuffer += '\''; yy_pop_state(); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
}

<BC_RAW_STRING>{
    (.|\n)                         { AppendRawStringChunk(yytext, static_cast<size_t>(yyleng)); }
    <<EOF>>                       { ReportAnnotationError("unterminated raw string literal");
                                   g_rawDelimiter.clear(); yy_pop_state();
                                   if (!g_capturePayload && !g_rawBuffer.empty()) {
                                       yylval.str = new std::string(std::move(g_rawBuffer));
                                       g_rawBuffer.clear(); BEGIN(INITIAL); return RAW_CODE;
                                   }
                                   BEGIN(INITIAL); yyterminate(); }
}

<BC_LINECOMMENT>{
    \n                            { g_rawBuffer += '\n'; yy_pop_state(); }
    .                              { g_rawBuffer += yytext[0]; }
}

<BC_BLOCKCOMMENT>{
    "*/"                          { g_rawBuffer += "*/"; yy_pop_state(); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
}

<BC_STRING,BC_CHAR,BC_LINECOMMENT,BC_BLOCKCOMMENT><<EOF>> {
    if (g_capturePayload || YY_START != BC_LINECOMMENT) {
        ReportAnnotationError("unterminated string, character, or comment in captured block");
    }
    if (!g_rawBuffer.empty() && !g_capturePayload) {
        yylval.str = new std::string(std::move(g_rawBuffer));
        g_rawBuffer.clear();
        BEGIN(INITIAL);
        return RAW_CODE;
    }
    BEGIN(INITIAL);
    yyterminate();
}

<AFTER_CLASS_BODY>{
    [ \t\r\n]+                   { /* skip */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    ";"                           { ClearAnnotationContext(); BEGIN(INITIAL); return SEMI; }
}

<ENTRY_MODE>{
    [ \t\r\n]+                   { /* skip */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    "{"                           { g_braceDepth = 1; g_capturePayload = true;
                                   g_annotationName.clear();
                                   g_rawBuffer.clear(); BEGIN(BODY_CAPTURE_ENTRY); }
}

<ANNOTATION_AFTER>{
    [ \t\r\n]+                   { /* whitespace belongs outside the payload */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    "("                           { g_parenDepth = 1; g_capturePayload = true;
                                   g_rawBuffer.clear();
                                   BEGIN(ANNOTATION_ARGS_CAPTURE); }
    "{"                           { g_braceDepth = 1; g_capturePayload = true;
                                   g_rawBuffer.clear();
                                   BEGIN(ANNOTATION_BODY_CAPTURE); }
    ";"                           { ClearAnnotationContext(); BEGIN(INITIAL); return SEMI; }
    .                             { yyless(0); ClearAnnotationContext(); BEGIN(INITIAL); }
    <<EOF>>                       { ReportAnnotationError("annotation must end with ';' or precede a declaration");
                                   ClearAnnotationContext(); BEGIN(INITIAL); yyterminate(); }
}

<ANNOTATION_ARGS_CAPTURE>{
    "("                           { ++g_parenDepth; g_rawBuffer += '('; }
    ")"                           {
        --g_parenDepth;
        if (g_parenDepth == 0) {
            yylval.str = new std::string(std::move(g_rawBuffer));
            g_rawBuffer.clear();
            g_capturePayload = false;
            BEGIN(ANNOTATION_AFTER);
            return ANNOTATION_ARGS;
        }
        g_rawBuffer += ')';
    }
    "\""                         { g_rawBuffer += '"'; yy_push_state(BC_STRING); }
    "'"                           { g_rawBuffer += '\''; yy_push_state(BC_CHAR); }
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
    <<EOF>>                       { ReportAnnotationError("unterminated annotation argument list");
                                   ClearAnnotationContext(); BEGIN(INITIAL); yyterminate(); }
}

<ANNOTATION_BODY_CAPTURE>{
    "{"                           { ++g_braceDepth; g_rawBuffer += '{'; }
    "}"                           {
        --g_braceDepth;
        if (g_braceDepth == 0) {
            yylval.str = new std::string(std::move(g_rawBuffer));
            g_rawBuffer.clear();
            g_capturePayload = false;
            BEGIN(ANNOTATION_AFTER);
            return ANNOTATION_BODY;
        }
        g_rawBuffer += '}';
    }
    "\""                         { g_rawBuffer += '"'; yy_push_state(BC_STRING); }
    "'"                           { g_rawBuffer += '\''; yy_push_state(BC_CHAR); }
    "//"                          { g_rawBuffer += "//"; yy_push_state(BC_LINECOMMENT); }
    "/*"                          { g_rawBuffer += "/*"; yy_push_state(BC_BLOCKCOMMENT); }
    \n                            { g_rawBuffer += '\n'; }
    .                              { g_rawBuffer += yytext[0]; }
    <<EOF>>                       { ReportAnnotationError("unterminated annotation body");
                                   ClearAnnotationContext(); BEGIN(INITIAL); yyterminate(); }
}

<AFTER_ENTRY_BODY>{
    [ \t\r\n]+                   { /* skip */ }
    "//"                         { yy_push_state(SKIP_LINECOMMENT); }
    "/*"                         { yy_push_state(SKIP_BLOCKCOMMENT); }
    ";"                           { ClearAnnotationContext(); BEGIN(INITIAL); return SEMI; }
}

<SKIP_LINECOMMENT>{
    \n                            { yy_pop_state(); }
    .                              { /* discard comment text */ }
    <<EOF>>                       { yy_pop_state(); yyterminate(); }
}

<SKIP_BLOCKCOMMENT>{
    "*/"                         { yy_pop_state(); }
    \n                            { /* discard comment text */ }
    .                              { /* discard comment text */ }
    <<EOF>>                       { ReportAnnotationError("unterminated comment");
                                   yy_pop_state(); yyterminate(); }
}

%%

/** Resets scanner-owned capture state between independent source files. */
void ResetConcordScriptLexerState() noexcept
{
    g_rawBuffer.clear();
    g_braceDepth = 0;
    g_parenDepth = 0;
    g_rawBraceDepth = 0;
    g_nativeBraceDepth = 0;
    g_rawDelimiter.clear();
    g_capturePayload = false;
    g_annotationLine = 1;
    g_annotationName.clear();
    BEGIN(INITIAL);
}
