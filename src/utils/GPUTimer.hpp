#pragma once

#include <common.hpp>
#include <profiler.hpp>

class GPUTimer {
public:
    inline GPUTimer(const char* name)
        : name(name)
    {
        // Time elapsed queries only exist through EXT_disjoint_timer_query on
        // most mobile GPUs, so only create the query when it can be used.
        if (bismuth::gpuTimingSupported()) {
#if BISMUTH_GL_ES
            if (glGenQueries)
#endif
            glGenQueries(1, &queryObject);
        }

        profiler::addTimer(this);
    }

    inline ~GPUTimer() {
        if (queryObject) {
#if BISMUTH_GL_ES
            if (glDeleteQueries)
#endif
            glDeleteQueries(1, &queryObject);
        }

        profiler::removeTimer(this);
    }

    inline void start() {
        if (profiler::isTakingGPUTime && queryObject)
            bismuth::gpuTimerBegin(queryObject);
    }

    inline void stop() {
        if (profiler::isTakingGPUTime && queryObject) {
            bismuth::gpuTimerEnd();
            time = bismuth::gpuTimerResult(queryObject);
        }
    }

    inline const char* getName() const { return name; }
    inline i64 getTime() const { return time; }

private:
    i64 time = 0;
    const char* name;
    u32 queryObject = 0;
};