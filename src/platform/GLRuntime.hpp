#pragma once

#include "GL.hpp"
#include <string>

namespace bismuth {

/*
    Snapshot of what the currently bound GL context can actually do.

    Everything the renderer needs is checked here instead of hard-coding
    "OpenGL 4.3", so the mod can also run on OpenGL ES 3.1 devices (and cleanly
    bail out everywhere else).
*/
struct GLCapabilities {
    bool isES = false;
    int major = 0;
    int minor = 0;

    bool vertexArrays = false;
    bool mapBufferRange = false;
    bool uniformBuffers = false;
    bool storageBuffers = false;
    bool computeShaders = false;
    bool indirectDraw = false;
    bool multiDrawIndirect = false;
    bool gpuTimers = false;
    bool debugLabels = false;

    // True when the context really accepted an ES 3.1 compute shader. Most
    // drivers only do that in an ES 3.1 context, but some Android drivers expose
    // the 3.1 entry points in an ES 2.0 context as well.
    bool computeProbe = false;

    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shadingLanguageVersion;

    bool isAtLeast(int requiredMajor, int requiredMinor) const {
        return major > requiredMajor ||
               (major == requiredMajor && minor >= requiredMinor);
    }

    /*
        True when the whole rendering pipeline (compute culling + SSBO state +
        indirect draws) can run. This is the one and only gate the renderer uses.
    */
    bool canRender() const {
        /*
            `computeShaders` already means "ES 3.1, or the driver accepted an ES
            3.1 compute shader after a probe", so the version is not checked
            again here.
        */
        if (isES)
            return computeShaders && storageBuffers &&
                   indirectDraw && vertexArrays && mapBufferRange;

        return isAtLeast(4, 3) && computeShaders && storageBuffers &&
               indirectDraw && vertexArrays;
    }

    std::string versionString() const {
        return (isES ? "OpenGL ES " : "OpenGL ") +
               std::to_string(major) + "." + std::to_string(minor);
    }
};

// Compile-time; true for Android and iOS builds.
bool isMobilePlatform();

// Idempotent; call with the game's context current.
const GLCapabilities& glCapabilities();
void detectGLCapabilities();

/*
    Compiles a minimal `#version 310 es` compute shader in the current context.

    Android's Geometry Dash build may create an ES 2.0 context even on hardware
    that supports 3.1, so the version string alone is not a reliable answer.
    Returns false on desktop and whenever the probe fails, in which case the
    renderer falls back to the vanilla one.
*/
bool probeComputeSupport();

/*
    GPU timer queries. ES only exposes them through EXT_disjoint_timer_query and
    several mobile drivers do not support them at all, so every call is a no-op
    when `glCapabilities().gpuTimers` is false (always check first).
*/
bool gpuTimingSupported();
void gpuTimerBegin(uint32_t query);
void gpuTimerEnd();
int64_t gpuTimerResult(uint32_t query);

// glMemoryBarrier, but only when the context has compute shaders.
void gpuMemoryBarrier(GLbitfield barriers);

} // namespace bismuth
