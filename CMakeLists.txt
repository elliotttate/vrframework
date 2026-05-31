cmake_minimum_required(VERSION 3.27)

# Engine-agnostic VR/modding core, consumed by per-engine adapters as extern/vrframework.
project(vrframework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Sources. Stubs are included so the target links; flesh them out per PORTING.md.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE VRFRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(vrframework STATIC ${VRFRAMEWORK_SOURCES})

# Public include root — keeps the adapters' include style:
#   #include "Framework.hpp"  /  <Mod.hpp>  /  <mods/VR.hpp>  /  "memory/memory_mul.h"
target_include_directories(vrframework PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(vrframework PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    # Define SIGNATURE_SCAN to pattern-scan at runtime instead of trusting manifest
    # fallback offsets (see memory/memory_mul.h).
)

# ---------------------------------------------------------------------------
# Dependencies — provided by the consuming repo. Declared here so the core compiles
# in isolation once these targets/packages exist. Swap find_package for add_subdirectory
# as your superbuild prefers.
# ---------------------------------------------------------------------------
# DEP: glm        (header-only math)            -> target glm::glm
# DEP: imgui      (overlay UI)                  -> target imgui
# DEP: spdlog     (logging)                     -> target spdlog::spdlog
# DEP: safetyhook (inline/mid hooks)            -> target safetyhook
# DEP: nlohmann_json (config serialization)     -> target nlohmann_json::nlohmann_json
# DEP: openvr     (OpenVR runtime)              -> target openvr_api
# DEP: OpenXR     (OpenXR loader)               -> target OpenXR::openxr_loader
# DEP: directxtk12 (d3d12 helpers, GraphicsMemory)

set(VRFRAMEWORK_DEPS glm::glm imgui spdlog::spdlog safetyhook
    nlohmann_json::nlohmann_json)

foreach(dep IN LISTS VRFRAMEWORK_DEPS)
    if(TARGET ${dep})
        target_link_libraries(vrframework PUBLIC ${dep})
    else()
        message(STATUS "vrframework: dependency target '${dep}' not found yet "
                       "(expected to be provided by the superbuild).")
    endif()
endforeach()

target_link_libraries(vrframework PUBLIC d3d11 d3d12 dxgi)
