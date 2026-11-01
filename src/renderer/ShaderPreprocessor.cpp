#include "ShaderPreprocessor.hpp"

#include <set>
#include <vector>

namespace shaderpp {

namespace {

constexpr int MAX_INCLUDE_DEPTH = 32;

struct ConditionalFrame {
    bool parentActive = true;
    bool active = true;
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

bool evaluateCondition(Context& ctx, const std::string& expression, bool& result) {
    std::string expr = trim(expression);

    // Only the forms used by the shader headers are supported.
    bool negate = false;
    if (!expr.empty() && expr[0] == '!') {
        negate = true;
        expr = trim(expr.substr(1));
    }

    const std::string definedPrefix = "defined";
    if (expr.rfind(definedPrefix, 0) == 0) {
        std::string name = trim(expr.substr(definedPrefix.size()));
        if (!name.empty() && name.front() == '(' && name.back() == ')')
            name = trim(name.substr(1, name.size() - 2));

        result = ctx.definedMacros.contains(name) != negate;
        return true;
    }

    // Numeric literal (0 / 1)
    if (!expr.empty() && (std::isdigit((unsigned char)expr[0]))) {
        result = (expr != "0") != negate;
        return true;
    }

    // Unknown condition: keep the code rather than silently dropping it.
    result = true;
    return false;
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
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "ifndef", rest)) {
            bool defined = ctx.definedMacros.contains(firstToken(rest));
            ConditionalFrame frame;
            frame.parentActive = isActive();
            frame.active = frame.parentActive && !defined;
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "if", rest)) {
            bool condition = true;
            evaluateCondition(ctx, rest, condition);

            ConditionalFrame frame;
            frame.parentActive = isActive();
            frame.active = frame.parentActive && condition;
            conditionals.push_back(frame);
        } else if (startsWithDirective(line, "elif", rest)) {
            if (!conditionals.empty()) {
                auto& frame = conditionals.back();
                bool condition = true;
                evaluateCondition(ctx, rest, condition);
                frame.active = frame.parentActive && !frame.seenElse && condition;
            }
        } else if (startsWithDirective(line, "else", rest)) {
            if (!conditionals.empty()) {
                auto& frame = conditionals.back();
                frame.seenElse = true;
                frame.active = frame.parentActive && !frame.active;
            }
        } else if (startsWithDirective(line, "endif", rest)) {
            if (!conditionals.empty())
                conditionals.pop_back();
        } else if (!isActive()) {
            // Skip everything in a disabled branch.
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
