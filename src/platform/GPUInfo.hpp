#pragma once

#include <common.hpp>

/*
    Returns the number of shader cores Bismuth uses to size its compute
    dispatches, or -1 when it could not be determined (the scheduler then falls
    back to a sensible default).

    - Desktop: OpenCL is used to match the GPU that owns the current GL context.
    - Mobile: there is no OpenCL available, so the renderer string is mapped to
      an estimate. It is only used to pick a work group count and can be
      overridden with the "GPU core count" setting.
*/
isize getGPUCoreCount();

// Rough core count estimate for mobile GPUs, based on the GL_RENDERER string.
isize estimateMobileGPUCoreCount();
