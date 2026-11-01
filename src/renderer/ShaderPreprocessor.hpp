#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

/*
    Minimal GLSL preprocessor used on platforms where glslang is not available
    (i.e. mobile).

    It only has to do what the shaders in resources/shaders actually use:
      - `#include "file.h"` (recursively, with #pragma once semantics)
      - `#ifdef` / `#ifndef` / `#if` / `#elif` / `#else` / `#endif`, including
        `defined(X)`, `!`, `&&`, `||` and integer conditions
      - tracking `#define` / `#undef` so those conditions can be evaluated
      - stripping `#extension GL_ARB_shading_language_include` and any `#version`
      - injecting the `#version` line, precision qualifiers and defines

    Anything it cannot evaluate is left in place rather than dropped, so an
    unsupported condition can never silently delete shader code.

    It deliberately has no dependency on Geode or the GL headers so it can be
    unit tested / validated against a GLSL validator outside of the game.
*/

namespace shaderpp {

enum class Stage {
    Vertex,
    Fragment,
    Compute,
};

struct Options {
    // e.g. "460" on desktop, "310 es" on OpenGL ES 3.1
    std::string version = "310 es";

    // Extra defines, e.g. GROUP_ID_LIMIT=64
    std::map<std::string, std::string> defines;

    // Resolves an include name to its contents. Returning nullopt means the
    // include could not be found.
    std::function<std::optional<std::string>(const std::string& name)> readFile;

    // Adds `precision highp float; precision highp int;` for GLSL ES.
    bool addPrecisionQualifiers = true;
};

/*
    Returns the fully expanded shader source (including the #version line), or
    nullopt when an include is missing / nesting is too deep. `error` receives a
    human readable message when it fails.
*/
/*
    `stage` is currently only used for diagnostics; GLSL ES 3.10 has no
    stage-specific preprocessing in these shaders.
*/
std::optional<std::string> preprocess(
    const std::string& source,
    const std::string& sourceName,
    Stage stage,
    const Options& options,
    std::string* error = nullptr
);

// "vertex" / "fragment" / "compute", used for log messages.
const char* stageName(Stage stage);

} // namespace shaderpp
