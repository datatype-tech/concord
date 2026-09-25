#ifndef CONCORDSCRIPT_AST_H
#define CONCORDSCRIPT_AST_H

/**
 * AST for the ConcordScript transpiler.
 *
 * ConcordScript is a transpiler, not a full C++ parser: only its own syntax
 * (use/@entry/@register/generic annotations/var/pub-priv-prot/
 * extends-implements) is modeled here. Annotation payloads, method bodies,
 * expressions, and free functions remain opaque text so Vulkan, shader,
 * and extension syntax can pass through without becoming parser coupling.
 */

#include <string>
#include <vector>

namespace ConcordScript {

/** One shallowly parsed argument in an annotation's parenthesized payload. */
struct AnnotationArg {
    /** Optional identifier on the left side of `=`. */
    std::string key;

    /** Value text, or the complete argument text for positional arguments. */
    std::string value;

    /** Original argument text, retained so extensions never lose information. */
    std::string raw;

    /** Whether this argument contained a top-level `=` separator. */
    bool hasKey = false;

    /** Whether the value is surrounded by a matching quote pair. */
    bool quoted = false;
};

/** A generic `@name(...) { ... }` annotation preserved by the transpiler. */
struct Annotation {
    /** Name without the leading `@`, for example `shader` or `vulkan`. */
    std::string name;

    /** Text inside the optional parentheses, without the delimiters. */
    std::string argsText;

    /** Text inside the optional braces, without the delimiters. */
    std::string bodyText;

    /** Shallow key/value view of argsText; raw text remains authoritative. */
    std::vector<AnnotationArg> args;

    /** Whether an argument list was present (including an empty `()`). */
    bool hasArgs = false;

    /** Whether a body block was present (including an empty `{}`). */
    bool hasBody = false;

    /** One-based source line at which the annotation's `@` was read. */
    int line = 0;

    /** Internal marker for the legacy `@register` class modifier. */
    bool isRegister = false;
};

/** One `use` directive. */
struct UseDirective {
    /** True for an engine public header, including the `Concord.Name` form. */
    bool isEngineHeader = false;

    /** True for a `std.xxx` package that maps onto a C++ standard header. */
    bool isStdlib = false;

    /** The path or dotted name exactly as written, e.g. "Concord.CApplication" or "MyGame.Player". */
    std::string target;

    /** One-based source line of the `use` keyword. */
    int line = 0;

    /** Generic annotations attached to this use directive. */
    std::vector<Annotation> annotations;
};

/** A `class Name extends Base implements IFace, ... { ... } ;` declaration. */
struct ClassDecl {
    enum class Role { Regular, Component, System };

    std::string name;
    int line = 0;
    std::string baseClass;                 // empty when there is no extends clause
    std::vector<std::string> interfaces;   // from implements
    Role role = Role::Regular;

    /**
     * The class body's text, captured verbatim between the braces with
     * `var`/`pub`/`priv`/`prot` already substituted to `auto`/access
     * labels, and an implicit `public:` already prepended (default access
     * is pub — see script/README.md).
     */
    std::string bodyText;

    /** Whether `@register` preceded this class. */
    bool isRegistered = false;

    /** Generic annotations appearing immediately before this declaration. */
    std::vector<Annotation> annotations;
};

/** The project's single `@entry { ... } ;` block. */
struct EntryBlock {
    /** Raw C++ statements (with var already substituted), copied into main(). */
    std::string body;

    /** One-based source line at which the `@entry` marker was read. */
    int line = 0;

    /** Generic annotations appearing immediately before this entry block. */
    std::vector<Annotation> annotations;
};

/** Opaque top-level C++ text outside of any recognized construct. */
struct RawCode {
    std::string text;

    /** Generic annotations attached to this opaque top-level fragment. */
    std::vector<Annotation> annotations;
};

/** One top-level item in a .cx file, in source order. */
struct TopLevelItem {
    enum class Kind { Use, Class, Entry, Raw, Annotation };

    Kind kind = Kind::Raw;
    UseDirective use;
    ClassDecl classDecl;
    EntryBlock entry;
    Annotation annotation;
    RawCode raw;
};

/** The parsed contents of one .cx file. */
struct SourceFile {
    /** File name without extension; also the generated namespace name. */
    std::string moduleName;

    std::vector<TopLevelItem> items;
};

/** Clears the parser's result buffer and sets the module name for the next parse. */
void ResetParserResult(const std::string& moduleName, const std::string& sourcePath = {});

/** The result of the most recent yyparse() call. */
SourceFile& ParserResult();

/** Whether the most recent parse reported a syntax or lexer error. */
[[nodiscard]] bool ParserHadError() noexcept;

} // namespace ConcordScript

#endif // CONCORDSCRIPT_AST_H
