#include "GPUInfo.hpp"
#include "GLRuntime.hpp"

#if defined(GEODE_IS_WINDOWS)
#include "../cl.hpp"
#endif

#include <algorithm>
#include <cctype>

using namespace geode::prelude;

isize estimateMobileGPUCoreCount() {
    const auto& caps = bismuth::glCapabilities();
    std::string renderer = caps.renderer;

    std::transform(renderer.begin(), renderer.end(), renderer.begin(),
        [](unsigned char ch) { return (char)std::tolower(ch); });

    auto contains = [&](const char* needle) {
        return renderer.find(needle) != std::string::npos;
    };

    /*
        These are ALU-lane estimates, not "cores" in the desktop sense. They only
        decide how many compute work groups the culling pass is split into.
    */
    if (contains("adreno"))
        return 256;
    if (contains("xclipse"))
        return 256;
    if (contains("mali-g7") || contains("mali-g6"))
        return 256;
    if (contains("mali"))
        return 128;
    if (contains("powervr"))
        return 128;
    if (contains("apple"))
        return 128;

    return 128;
}

isize getGPUCoreCount() {
    int setting = Mod::get()->getSettingValue<int>("gpu_core_count");
    if (setting != 0)
        return setting;

#if defined(GEODE_IS_WINDOWS)
    return getGPUCoreCountWindows();
#else
    return estimateMobileGPUCoreCount();
#endif
}

#if defined(GEODE_IS_WINDOWS)
$on_mod(Loaded) {
    getGPUCoreCount();
}
#endif
