#include "GLRuntime.hpp"

#if BISMUTH_GL_ES
#include "GL.hpp"
#else
#include "../gl.hpp"
#endif

#include <cctype>
#include <cstring>

using namespace geode::prelude;

namespace bismuth {

namespace {

std::string readGLString(GLenum name) {
    auto value = (const char*)glGetString(name);
    return value ? std::string(value) : std::string();
}

void parseVersion(const std::string& version, bool& isES, int& major, int& minor) {
    const char* esPrefix = "OpenGL ES";
    isES = version.rfind(esPrefix, 0) == 0;

    size_t index = 0;
    while (index < version.size() && !std::isdigit((unsigned char)version[index]))
        index++;

    int parsedMajor = 0;
    while (index < version.size() && std::isdigit((unsigned char)version[index])) {
        parsedMajor = parsedMajor * 10 + (version[index] - '0');
        index++;
    }

    int parsedMinor = 0;
    if (index < version.size() && version[index] == '.') {
        index++;
        while (index < version.size() && std::isdigit((unsigned char)version[index])) {
            parsedMinor = parsedMinor * 10 + (version[index] - '0');
            index++;
        }
    }

    major = parsedMajor;
    minor = parsedMinor;
}

std::string queryExtensionString() {
#if BISMUTH_GL_ES
    auto getStringi = bgl::gl.GetStringi;
#else
    auto getStringi = (const GLubyte* (*)(GLenum, GLuint))glGetStringi;
#endif

    if (getStringi) {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        std::string result;
        for (GLint i = 0; i < count && i < 8192; i++) {
            auto extension = (const char*)getStringi(GL_EXTENSIONS, (GLuint)i);
            if (!extension)
                break;

            if (!result.empty())
                result += ' ';
            result += extension;
        }

        if (!result.empty())
            return result;
    }

    // ES 2.0 / GL 2.x fall back to the combined string.
    return readGLString(GL_EXTENSIONS);
}

bool hasExtension(const std::string& extensions, const char* name) {
    if (extensions.empty())
        return false;

    std::string padded = " " + extensions + " ";
    std::string needle = " " + std::string(name) + " ";
    return padded.find(needle) != std::string::npos;
}

GLCapabilities capabilities;
bool detected = false;

} // namespace

bool isMobilePlatform() {
#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)
    return true;
#else
    return false;
#endif
}

const GLCapabilities& glCapabilities() {
    if (!detected)
        detectGLCapabilities();

    return capabilities;
}

void detectGLCapabilities() {
    if (detected)
        return;

    /*
        The ES 3.x entry points have to be resolved before anything else, the
        extension list needs glGetStringi.
    */
#if BISMUTH_GL_ES
    bgl::loadGLFunctions();
#else
    if (!hasLoadedNewerOpenGLFunctions())
        loadNewerOpenGLFunctions();
#endif

    GLCapabilities caps;

    caps.version = readGLString(GL_VERSION);
    caps.vendor = readGLString(GL_VENDOR);
    caps.renderer = readGLString(GL_RENDERER);
    caps.shadingLanguageVersion = readGLString(GL_SHADING_LANGUAGE_VERSION);

    parseVersion(caps.version, caps.isES, caps.major, caps.minor);

    std::string extensions = queryExtensionString();

#if BISMUTH_GL_ES
    /*
        These flags cover every entry point the renderer calls unconditionally, so
        a null pointer can never be reached once canRender() is true.
    */
    caps.vertexArrays  = bgl::gl.GenVertexArrays &&
                         bgl::gl.BindVertexArray &&
                         bgl::gl.DeleteVertexArrays &&
                         bgl::gl.VertexAttribIPointer;
    caps.mapBufferRange = bgl::gl.MapBufferRange &&
                          bgl::gl.UnmapBuffer &&
                          bgl::gl.FlushMappedBufferRange;
    caps.uniformBuffers = bgl::gl.GetUniformBlockIndex != nullptr;

    /*
        Only entry points that actually exist in OpenGL ES 3.1. Note that
        glShaderStorageBlockBinding does NOT exist on ES (it is desktop GL 4.3+):
        ES relies on the `layout(binding = N)` qualifier plus glBindBufferBase,
        which is exactly what the shaders and the renderer already do.
    */
    bool computeEntryPoints =
        bgl::gl.DispatchCompute &&
        bgl::gl.MemoryBarrier &&
        bgl::gl.GetProgramResourceIndex;

    // Only worth compiling the probe shader when the context does not already
    // claim ES 3.1 (it costs a shader compile).
    caps.computeProbe = !caps.isAtLeast(3, 1) && computeEntryPoints && probeComputeSupport();

    /*
        The version string is authoritative when the context really is ES 3.1+.
        Otherwise (or when a driver reports something odd) a successfully probed
        ES 3.1 compute shader is accepted too, because the game may hand us an
        ES 2.0 context on hardware that supports more.
    */
    caps.computeShaders = computeEntryPoints &&
                          (caps.isAtLeast(3, 1) || caps.computeProbe);
    // SSBO binding itself needs the buffer base/range entry points.
    caps.storageBuffers = caps.computeShaders &&
                          bgl::gl.BindBufferBase &&
                          bgl::gl.BindBufferRange;
    caps.indirectDraw   = caps.computeShaders && bgl::gl.DrawArraysIndirect;
    caps.multiDrawIndirect = bgl::gl.MultiDrawArraysIndirect != nullptr;
    caps.gpuTimers      = hasExtension(extensions, "GL_EXT_disjoint_timer_query") &&
                          bgl::gl.BeginQueryTimeElapsed &&
                          bgl::gl.EndQueryTimeElapsed &&
                          bgl::gl.GetQueryObjectui64v &&
                          bgl::gl.GenQueries &&
                          bgl::gl.DeleteQueries;
    caps.debugLabels    = bgl::gl.ObjectLabel != nullptr;
#else
    caps.vertexArrays  = caps.isAtLeast(3, 0);
    caps.mapBufferRange = caps.isAtLeast(3, 0);
    caps.uniformBuffers = caps.isAtLeast(3, 1);
    caps.computeShaders = caps.isAtLeast(4, 3) && glad_glDispatchCompute;
    caps.storageBuffers = caps.isAtLeast(4, 3);
    caps.indirectDraw   = caps.isAtLeast(4, 3) && glad_glMultiDrawArraysIndirect;
    caps.multiDrawIndirect = glad_glMultiDrawArraysIndirect != nullptr;
    caps.gpuTimers      = caps.isAtLeast(3, 3);
    caps.debugLabels    = caps.isAtLeast(4, 3);
#endif

    capabilities = caps;

    /*
        glGetString returns NULL when there is no current context. In that case do
        not cache the (empty) result, so the next call - which happens once the
        game's context is current - detects properly instead of permanently
        disabling the renderer.
    */
    detected = !caps.version.empty();

    if (!detected) {
        log::warn("GL capabilities queried without a current context, will retry");
        return;
    }

    log::info("GL runtime: {} | vendor '{}' | renderer '{}'",
        caps.versionString(), caps.vendor, caps.renderer);
    log::info("GL runtime: compute={} (probe={}) ssbo={} indirect={} multiIndirect={} timers={}",
        caps.computeShaders, caps.computeProbe, caps.storageBuffers,
        caps.indirectDraw, caps.multiDrawIndirect, caps.gpuTimers);
}

bool probeComputeSupport() {
#if BISMUTH_GL_ES
    if (!bgl::gl.DispatchCompute || !bgl::gl.MemoryBarrier)
        return false;

    static const char* probeSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer ProbeBuffer { uint counter; };\n"
        "void main() { counter = counter + 1u; }\n";

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    if (!shader)
        return false;

    glShaderSource(shader, 1, &probeSource, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glDeleteShader(shader);

    if (status != GL_TRUE) {
        log::info("GL runtime: compute probe rejected, the context cannot run ES 3.1 shaders");
        return false;
    }

    log::info("GL runtime: compute probe succeeded");
    return true;
#else
    return false;
#endif
}

bool gpuTimingSupported() {
    return glCapabilities().gpuTimers;
}

void gpuTimerBegin(uint32_t query) {
    if (!gpuTimingSupported())
        return;

#if BISMUTH_GL_ES
    bgl::gl.BeginQueryTimeElapsed(GL_TIME_ELAPSED_EXT, query);
#else
    glBeginQuery(GL_TIME_ELAPSED, query);
#endif
}

void gpuTimerEnd() {
    if (!gpuTimingSupported())
        return;

#if BISMUTH_GL_ES
    bgl::gl.EndQueryTimeElapsed(GL_TIME_ELAPSED_EXT);
#else
    glEndQuery(GL_TIME_ELAPSED);
#endif
}

int64_t gpuTimerResult(uint32_t query) {
    if (!gpuTimingSupported())
        return 0;

#if BISMUTH_GL_ES
    GLuint64 value = 0;
    bgl::gl.GetQueryObjectui64v(query, GL_QUERY_RESULT, &value);
    return (int64_t)value;
#else
    int64_t value = 0;
    glGetQueryObjecti64v(query, GL_QUERY_RESULT, &value);
    return value;
#endif
}

void gpuMemoryBarrier(GLbitfield barriers) {
    if (!glCapabilities().computeShaders)
        return;

    glMemoryBarrier(barriers);
}

} // namespace bismuth
