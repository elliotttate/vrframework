#pragma once

// Minimal stand-in for praydog's utility/ScopeProfiler.hpp. The ports sprinkle
// SCOPE_PROFILER() through their hot hooks. This version is a no-op unless
// VRFRAMEWORK_PROFILE is defined, so it costs nothing in release.

#include <chrono>
#include <string_view>

#ifdef VRFRAMEWORK_PROFILE
#include <spdlog/spdlog.h>

class ScopeProfiler {
public:
    explicit ScopeProfiler(std::string_view name)
        : m_name{ name }, m_start{ std::chrono::steady_clock::now() } {}
    ~ScopeProfiler() {
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_start).count();
        spdlog::trace("[prof] {}: {} us", m_name, us);
    }
private:
    std::string_view m_name;
    std::chrono::steady_clock::time_point m_start;
};

#define SCOPE_PROFILER_CAT2(a, b) a##b
#define SCOPE_PROFILER_CAT(a, b) SCOPE_PROFILER_CAT2(a, b)
#define SCOPE_PROFILER() ScopeProfiler SCOPE_PROFILER_CAT(_scope_prof_, __LINE__){ __FUNCTION__ }
#define SCOPE_PROFILER_NAMED(name) ScopeProfiler SCOPE_PROFILER_CAT(_scope_prof_, __LINE__){ name }
#else
#define SCOPE_PROFILER() ((void)0)
#define SCOPE_PROFILER_NAMED(name) ((void)0)
#endif
