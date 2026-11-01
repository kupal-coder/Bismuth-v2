#include "ShaderPreprocessor.hpp"

#include <set>
#include <vector>

namespace shaderpp {

namespace {

constexpr int MAX_INCLUDE_DEPTH = 32;

struct ConditionalFrame {
    bool parentActive = true;
    bool active = true;
    // True once any branch of this #if/#elif/#else chain was taken, so a later
    // #elif cannot activate a second branch.
    bool anyTaken = false;
    bool seenElse = false;
};

struct Context {
    const Options& options;
    std::set<std::string> alreadyIncluded;
    std::set<std::string> definedMacros;
    std::string error;

    explicit Context(const Options& options) : options(options) {}
};

std::string trim(const std::string& value) {
    size_t begin = value.find_first_not_of(" \t\r");
    if (begin == std::string::npos)
        return "";
    size_t end = value.find_last_not_of(" \t\r");
    return value.substr(begin, end - begin + 1);
}

std::string firstToken(const std::string& value) {
    std::string trimmed = trim(value);
    size_t end = trimmed.find_first_of(" \t(");
    return trimmed.substr(0, end);
}

bool startsWithDirective(const std::string& line, const char* directive, std::string& rest) {
    size_t index = 0;
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t'))
        index++;

    if (index >= line.size() || line[index] != '#')
        return false;

    index++;
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t'))
        index++;

    size_t directiveLength = std::char_traits<char>::length(directive);
    if (line.compare(index, directiveLength, directive) != 0)
        return false;

    // Make sure `#if` does not match `#ifdef`
    if (index + directiveLength < line.size()) {
        char next = line[index + directiveLength];
        bool isWordChar = (next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                          (next >= '0' && next <= '9') || next == '_';
        if (isWordChar)
            return false;
    }

    rest = line.substr(index + directiveLength);
    return true;
}

std::optional<std::string> parseIncludeName(const std::string& rest) {
    size_t begin = rest.find_first_of("\"<");
    if (begin == std::string::npos)
        return std::nullopt;

    char closing = rest[begin] == '"' ? '"' : '>';
    size_t end = rest.find(closing, begin + 1);
    if (end == std::string::npos)
        return std::nullopt;

    return rest.substr(begin + 1, end - begin - 1);
}

// Result of evaluating a `#if` expression. Unknown expressions are not guessed:
// the block is kept as-is instead of being silently dropped.
enum class Eval { Known, Unknown };

std::string stripOuterParens(const std::string& expr) {
    if (expr.size() < 2 || expr.front() != '(' || expr.back() != ')')
        return expr;

    // Only strip when the first parenthesis closes at the very end.
    int depth = 0;
    for (size_t i = 0; i < expr.size(); i++) {
        if (expr[i] == '(') depth++;
        else if (expr[i] == ')') {
            depth--;
            if (depth == 0)
                return i == expr.size() - 1 ? trim(expr.substr(1, expr.size() - 2)) : expr;
        }
    }

    return expr;
}

// Finds a top level operator (not nested in parentheses), or npos.
size_t findTopLevel(const std::string& expr, const char* op) {
    size_t opLength = std::char_traits<char>::length(op);
    int depth = 0;

    for (size_t i = 0; i < expr.size(); i++) {
        char ch = expr[i];
        if (ch == '(') {
            depth++;
        } else if (ch == ')') {
            depth--;
        } else if (depth == 0 && expr.compare(i, opLength, op) == 0) {
            return i;
        }
    }

    return std::string::npos;
}

bool parseInteger(const std::string& text, long long& value) {
    if (text.empty())
        return false;

    size_t consumed = text.size();
    while (consumed > 0 && (text[consumed - 1] == 'u' || text[consumed - 1] == 'U' ||
                           text[consumed - 1] == 'l' || text[consumed - 1] == 'L'))
        consumed--;

    std::string digits = text.substr(0, consumed);
    if (digits.empty())
        return false;

    try {
        size_t used = 0;
        value = std::stoll(digits, &used, 0);
        return used == digits.size();
    } catch (...) {
        return false;
    }
}

Eval evaluateExpression(Context& ctx, const std::string& expression, bool& result) {
    std::string expr = stripOuterParens(trim(expression));
    if (expr.empty())
        return Eval::Unknown;

    // `||` binds looser than `&&`.
    size_t orIndex = findTopLevel(expr, "||");
    if (orIndex != std::string::npos) {
        bool left = false, right = false;
        Eval l = evaluateExpression(ctx, expr.substr(0, orIndex), left);
        Eval r = evaluateExpression(ctx, expr.substr(orIndex + 2), right);

        if (l == Eval::Known && left) { result = true; return Eval::Known; }
        if (r == Eval::Known && right) { result = true; return Eval::Known; }
        if (l == Eval::Known && r == Eval::Known) { result = false; return Eval::Known; }
        return Eval::Unknown;
    }

    size_t andIndex = findTopLevel(expr, "&&");
    if (andIndex != std::string::npos) {
        bool left = false, right = false;
        Eval l = evaluateExpression(ctx, expr.substr(0, andIndex), left);
        Eval r = evaluateExpression(ctx, expr.substr(andIndex + 2), right);

        if (l == Eval::Known && !left) { result = false; return Eval::Known; }
        if (r == Eval::Known && !right) { result = false; return Eval::Known; }
        if (l == Eval::Known && r == Eval::Known) { result = true; return Eval::Known; }
        return Eval::Unknown;
    }

    if (!expr.empty() && expr[0] == '!') {
        bool inner = false;
        Eval nested = evaluateExpression(ctx, expr.substr(1), inner);
        if (nested == Eval::Unknown)
            return Eval::Unknown;

        result = !inner;
        return Eval::Known;
    }

    const std::string definedPrefix = "defined";
    if (expr.rfind(definedPrefix, 0) == 0 && expr.size() > definedPrefix.size()) {
        std::string name = trim(expr.substr(definedPrefix.size()));
        if (!name.empty() && name.front() == '(' && name.back() == ')')
            name = trim(name.substr(1, name.size() - 2));

        if (name.empty())
            return Eval::Unknown;

        result = ctx.definedMacros.contains(name);
        return Eval::Known;
    }

    long long number = 0;
    if (parseInteger(expr, number)) {
        result = number != 0;
        return Eval::Known;
    }

    // Identifiers (e.g. `__VERSION__`) and anything more complex are not guessed.
    return Eval::Unknown;
}

void evaluateCondition(Context& ctx, const std::string& expression, bool& result) {
    bool value = false;
    if (evaluateExpression(ctx, expression, value) == Eval::Known) {
        result = value;
        return;
    }

    // Keep the code rather than silently dropping a branch we do not understand.
    result = true;
}

bool expand(Context& ctx, const std::string& source, const std::string& sourceName, int depth, std::string& out) {
    if (depth > MAX_INCLUDE_DEPTH) {
        ctx.error = "include depth exceeded while expanding " + sourceName;
        return false;
    }

    std::vector<ConditionalFrame> conditionals;

    auto isActive = [&]() {
        for (const auto& frame : conditionals)
            if (!frame.active)
                return false;
        return true;
    };

    size_t lineStart = 0;

    while (lineStart <= source.size()) {
        size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = source.size();

        std::string line = source.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::string rest;

        // Conditional directives have to be tracked even inside inactive blocks.
        if (startsWithDirective(line, "ifdef", rest)) {
            bool defined = ctx.definedMacros.contains(firstToken(rest));
            ConditionalFrame frame;
            frame.parentActive = isActive();
            frame.active = frame.parentActive && defined;
            frame.anyTaken = frame.active;
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "ifndef", rest)) {
            bool defined = ctx.definedMacros.contains(firstToken(rest));
            ConditionalFrame frame;
            frame.parentActive = isActive();
            frame.active = frame.parentActive && !defined;
            frame.anyTaken = frame.active;
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "if", rest)) {
            bool condition = true;
            evaluateCondition(ctx, rest, condition);

            ConditionalFrame frame;
            frame.parentActive = isActive();
            frame.active = frame.parentActive && condition;
            frame.anyTaken = frame.active;
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "elif", rest)) {
            if (!conditionals.empty()) {
                auto& frame = conditionals.back();
                bool condition = true;
                evaluateCondition(ctx, rest, condition);
                // A branch can only activate if none before it was taken.
                frame.active = frame.parentActive && !frame.anyTaken && !frame.seenElse && condition;
                frame.anyTaken = frame.anyTaken || frame.active;
            }
        } else if (startsWithDirective(line, "else", rest)) {
            if (!conditionals.empty()) {
                auto& frame = conditionals.back();
                frame.active = frame.parentActive && !frame.anyTaken;
                frame.anyTaken = true;
                frame.seenElse = true;
            }
        } else if (startsWithDirective(line, "endif", rest)) {
            if (!conditionals.empty())
                conditionals.pop_back();
        } else if (!isActive()) {
            // Skip everything in a disabled branch.
        } else if (startsWithDirective(line, "version", rest)) {
            // The prelude already emitted the #version directive for this stage,
            // so drop anything the shader file declares itself.
        } else if (startsWithDirective(line, "include", rest)) {
            auto name = parseIncludeName(rest);
            if (!name) {
                ctx.error = "malformed #include in " + sourceName;
                return false;
            }

            // #pragma once semantics
            if (ctx.alreadyIncluded.contains(*name)) {
                lineStart = lineEnd + 1;
                continue;
            }
            ctx.alreadyIncluded.insert(*name);

            if (!ctx.options.readFile) {
                ctx.error = "no include resolver for " + *name;
                return false;
            }

            auto contents = ctx.options.readFile(*name);
            if (!contents) {
                ctx.error = "could not find include '" + *name + "' (included from " + sourceName + ")";
                return false;
            }

            if (!expand(ctx, *contents, *name, depth + 1, out))
                return false;
        } else if (startsWithDirective(line, "extension", rest)) {
            // The ARB include extension only exists for glslang's preprocessor;
            // includes are already expanded at this point.
            if (rest.find("GL_ARB_shading_language_include") != std::string::npos) {
                lineStart = lineEnd + 1;
                continue;
            }

            out += line;
            out += '\n';
        } else if (startsWithDirective(line, "pragma", rest)) {
            if (trim(rest) == "once") {
                lineStart = lineEnd + 1;
                continue;
            }

            out += line;
            out += '\n';
        } else if (startsWithDirective(line, "define", rest)) {
            ctx.definedMacros.insert(firstToken(rest));
            out += line;
            out += '\n';
        } else if (startsWithDirective(line, "undef", rest)) {
            ctx.definedMacros.erase(firstToken(rest));
            out += line;
            out += '\n';
        } else {
            out += line;
            out += '\n';
        }

        lineStart = lineEnd + 1;
    }

    if (!conditionals.empty()) {
        ctx.error = "unterminated #if in " + sourceName;
        return false;
    }

    return true;
}

} // namespace

const char* stageName(Stage stage) {
    switch (stage) {
    case Stage::Vertex:   return "vertex";
    case Stage::Fragment: return "fragment";
    case Stage::Compute:  return "compute";
    default:              return "unknown";
    }
}

std::optional<std::string> preprocess(
    const std::string& source,
    const std::string& sourceName,
    Stage stage,
    const Options& options,
    std::string* error
) {
    (void)stage;

    Context ctx(options);

    bool isES = options.version.find("es") != std::string::npos;

    std::string prelude;
    prelude += "#version " + options.version + "\n";

    if (isES && options.addPrecisionQualifiers) {
        // Several ES stages have no default float precision, and `mediump`
        // would break the transforms, so be explicit.
        prelude += "precision highp float;\n";
        prelude += "precision highp int;\n";
    }

    // The shader headers are written for both the C++ side and GLSL.
    prelude += "#define GLSL\n";
    ctx.definedMacros.insert("GLSL");

    if (isES) {
        prelude += "#define BISMUTH_GLSL_ES\n";
        ctx.definedMacros.insert("BISMUTH_GLSL_ES");
    }

    for (const auto& [name, value] : options.defines) {
        prelude += "#define " + name + " " + value + "\n";
        ctx.definedMacros.insert(name);
    }

    std::string body;
    if (!expand(ctx, source, sourceName, 0, body)) {
        if (error)
            *error = ctx.error;
        return std::nullopt;
    }

    return prelude + body;
}

} // namespace shaderpp
