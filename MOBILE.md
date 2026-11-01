# Bismuth on mobile

Bismuth was written against desktop OpenGL 4.3. This document describes the
Android port: what changed, what still needs a device to verify, and what the mod
does when a phone cannot run the renderer at all.

iOS is **not** supported: Apple's OpenGL ES implementation stops at 3.0, so
compute shaders and shader storage buffers do not exist there. The code compiles
for iOS (the ES path is shared with Android), but the renderer refuses to start
and the game keeps its vanilla renderer.

## Requirements

| | |
|---|---|
| Platform | Android 32-bit and 64-bit (`Android32`, `Android64` Geode targets) |
| Game | Geometry Dash 2.2081 (Geode build) |
| Driver | OpenGL ES **3.1** with shader storage buffers and compute shaders |
| Optional | `GL_EXT_multi_draw_arrays` / core multi-draw for a single-call batch draw, `GL_EXT_disjoint_timer_query` for GPU timings |

GLES 3.1 means roughly Adreno 4xx/5xx/6xx/7xx, Mali-G series, PowerVR Rogue and
newer. A GLES 3.0 or 2.0 context (most Mali-400 era and older devices) cannot run
the renderer, and the mod falls back automatically (see below).

Geometry Dash's Android build may ask EGL for an **ES 2.0 context even on phones
that support 3.1**. The version string is not reliable in that case, so the port
also compiles a minimal `#version 310 es` compute shader once at start-up
(`bismuth::probeComputeSupport()`): if the driver accepts it, the renderer runs;
if not, the mod falls back exactly like on an old device. The probe is only run
when the context reports less than ES 3.1 (it costs a shader compile).
`Bismuth.log` reports `compute=1 (probe=1)` when the probe is what enabled the
renderer.

`canRender()` only requires entry points that exist in ES 3.1 (verified against
the official `gl31.h` prototypes), so a missing optional function can never be
called through a null pointer.

## Falling back to the vanilla renderer

`bismuth::GLCapabilities::canRender()` is the single gate. On a GLES device it
requires ES 3.1 **and** compute shaders, SSBOs, indirect draws, vertex arrays and
`glMapBufferRange`.

If that fails (or if the `mobile_renderer` setting is turned off, or `enabled` is
off), `Renderer::init` logs the reason, shows a notification and returns `false`.
No hooks then touch the renderer: `PlayLayer::updateVisibility` runs the original
game code, the objects are drawn by cocos2d as usual, and gameplay is unaffected.
The mod never leaves a half-initialized renderer behind.

## What the port changed

### One place for GL

`src/platform/GL.hpp` is the only file that includes a GL header:

* desktop (`GEODE_IS_WINDOWS`, `GEODE_IS_MACOS`) includes Geode's `CCGL.h` and
  calls GL directly;
* Android includes `<GLES3/gl31.h>` and resolves every entry point above GLES 2.0
  at runtime (`src/platform/GL.cpp`) through `eglGetProcAddress`, falling back to
  `dlsym("libGLESv2.so")`. Calls are routed through the `bgl::gl` table with
  `#define`s, so call sites read like normal GL. Geode only links `GLESv2`, so
  nothing else can be linked statically;
* iOS includes `<OpenGLES/ES3/gl.h>` and keeps the same seam; the table stays
  empty.

cocos2d aliases `glGenVertexArrays` to the OES variant on Android, and Geode's
Android `CCGL.h` aliases the OES name again to a function pointer variable:

```c
#define glGenVertexArrays   glGenVertexArraysOES
#define glGenVertexArraysOES glGenVertexArraysOESEXT
```

Those aliases have to be `#undef`ed **before** `<GLES3/gl31.h>` is included,
otherwise they rewrite the declarations inside that header into redeclarations of
cocos's variables and the mod does not compile at all. The core ES 3.0 entry
points resolved by `bgl` are used instead.

### Shaders

Desktop keeps glslang (`#include` handling, specialization, `#version 460`).
Mobile cannot use glslang (it is a desktop-only dependency and its SPIR-V output
is useless to a phone driver), so `src/renderer/ShaderPreprocessor.cpp` does the
`#include`, `#define`, `#if`/`#ifdef` and macro work in C++ and emits GLSL ES
3.10: `#version 310 es` + `precision highp float/int` + `#define GLSL` +
`#define BISMUTH_GLSL_ES`.

Because of that, the shader headers had to become ES-clean:

* `std430` buffer layouts use unsigned types for anything that is compared to a
  literal (`spriteIndex`, `flags`, all color-channel enums), since GLSL ES does
  not convert `int` to `uint` implicitly;
* every literal in a `uint` expression has the `u` suffix (`0u`, `3u`, ...), and
  `gl_VertexID % 3` is cast with `uint(...)` before being used as an index;
* the `SAO_BUFFER` token-pasting macros are replaced by hand-written blocks under
  `BISMUTH_GLSL_ES` (glslang ES rejects `##`).

All eight shaders (vertex/fragment/compute) compile as both GLSL 460 and GLSL ES
3.10; see "Verifying" below.

### Buffers, culling and drawing

| Desktop | Mobile |
|---|---|
| `glMapBuffer` | `glMapBufferRange(0, size, GL_MAP_WRITE_BIT \| GL_MAP_FLUSH_EXPLICIT_BIT)` (+`glFlushMappedBufferRange`), with a CPU shadow buffer when mapping fails |
| `glGetBufferSubData` | not available on ES at all; `Buffer::read()` becomes a no-op (nothing in the renderer reads back object data) |
| one `glMultiDrawArraysIndirect` | `glMultiDrawArraysIndirect` when present, otherwise one `glDrawArraysIndirect` per work group (still GPU-driven) |
| `glDispatchCompute` | same, followed by `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT \| GL_COMMAND_BARRIER_BIT)` |
| `glObjectLabel` | ES 3.2 only; routed through the table and ignored when missing |
| `glShaderStorageBlockBinding` | **does not exist in OpenGL ES at all** (desktop GL 4.3+); not needed, ES uses `layout(binding = N)` + `glBindBufferBase` |
| `GL_TIME_ELAPSED` queries | only when `GL_EXT_disjoint_timer_query` is present; otherwise all timings report 0 |
| overdraw visualizer | disabled on mobile (reads back the stencil buffer, which ES cannot do) |
| difference mode | works (RGBA/RGB FBOs are core ES) |

GPU core count (`gpu_core_count` setting) is used to size the compute dispatches.
Windows uses OpenCL; mobile has no OpenCL, so `GL_RENDERER` is mapped to an
estimate in `src/platform/GPUInfo.cpp` (Adreno/Xclipse/Mali-G7x: 256, older
Mali/PowerVR/Apple: 128, unknown: 128). Setting the value manually overrides it.

## Building

```sh
geode build                 # current platform
# or via CI: .github/workflows/multi-platform.yml builds Windows, Android32 and Android64
```

`CMakeLists.txt` only fetches glslang and the OpenCL headers on desktop; mobile
builds link the NDK's GLES headers and nothing else.

## Verifying

* Shaders: preprocess with `src/renderer/ShaderPreprocessor.cpp` and validate the
  output with `glslangValidator -S vert|frag|comp`. All 8 shaders must pass as
  GLSL ES 3.10 and as GLSL 460.
* Platform layer: `g++ -std=c++20 -fsyntax-only` on `src/platform/*.cpp` against
  the Khronos GLES 3.1 headers catches most typos without the Geode SDK.
* On device: check `Bismuth.log` for `GL runtime: compute=1 (probe=0|1) ssbo=1
  indirect=1 ...` and `Running the renderer on OpenGL ES 3.1`.

## Known limitations / TODO

* Untested on real hardware so far — the numbers above come from static
  validation, not from a phone.
* The overdraw visualizer and the debug hotkeys (F3/F8/F9) are not reachable on
  mobile; they need touch UI or Geode's own overlay.
* `CodegenData.json` object metadata is shared as-is between ABIs; if a device
  disagrees with a packed struct, that is the first place to look.
* Shaders are compiled again every time a level is (re)started. Driver shader
  caches make this cheap on most devices, but a session-wide program cache in
  `Shader::create` is the obvious next step for slower phones.
* 2.2 levels are still not supported, matching the desktop build.
