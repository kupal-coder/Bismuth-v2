#pragma once

#include <common.hpp>

/*
    Windows-only OpenCL core count detection. The public entry point lives in
    platform/GPUInfo.hpp; this is the desktop implementation behind it.

    OpenCL is used because it can tell us exactly which GPU owns the current
    OpenGL context (clGetGLContextInfoKHR). Mobile platforms have no OpenCL, see
    platform/GPUInfo.cpp for the estimate used there.
*/

#if defined(GEODE_IS_WINDOWS)
isize getGPUCoreCountWindows();
#endif
