cmake_minimum_required(VERSION 3.27)

# Engine-agnostic VR/modding core, consumed by per-engine adapters as extern/vrframework.
project(vrframework LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dependency resolution (FH5 VR mod superbuild).
#
# vrframework expects a set of dependency TARGETS (see the `# DEP:` comments
# further down). The consuming repos (REFramework / UEVR) already vendor or
# build all of them; here we point at those checkouts and (re)build / import
# each as the target name vrframework links against. All overridable via cache
# vars so a different machine layout can be pointed at without editing this file.
#
#   glm, imgui, spdlog, nlohmann/json, openvr   <- REFramework/dependencies
#   safetyhook (+ its bundled Zydis), DirectXTK12 <- UEVR build/_deps source trees
#   OpenXR headers                              <- OpenXR-Simulator (runtime is
#                                                  dynamic-loaded; see XrSimTest.cpp)
#
# NOTE: deps are (re)built from source with THIS project's flags (MSVC /MD) so
# they match the mod DLL's dynamic CRT. We deliberately do NOT reuse the
# prebuilt UEVR/REFramework .libs because those are /MT (static CRT) and would
# trip /FAILIFMISMATCH "RuntimeLibrary=MT_StaticRelease" against our /MD objects.
# ---------------------------------------------------------------------------
set(REFRAMEWORK_DIR "E:/Github/REFramework" CACHE PATH "REFramework checkout (glm/imgui/spdlog/nlohmann/openvr)")
set(UEVR_DEPS_DIR   "E:/Github/UEVR/build/_deps" CACHE PATH "UEVR build _deps source trees (safetyhook/zydis/directxtk12)")
set(OPENXR_SIM_INCLUDE_DIR "E:/Github/OpenXR-Simulator/include" CACHE PATH "OpenXR headers (simulator)")

# Match the dynamic CRT the game/other targets use.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

include(FetchContent)

# Some vendored deps (glm, directxtk12) carry cmake_minimum_required below what
# modern CMake accepts. Allow their old policy floor (REFramework does the same).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# --- glm (header-only math, has its own CMake -> glm::glm) ---
if(NOT TARGET glm AND NOT TARGET glm::glm)
    add_subdirectory("${REFRAMEWORK_DIR}/dependencies/glm" "${CMAKE_BINARY_DIR}/_dep_glm" EXCLUDE_FROM_ALL)
endif()

# --- spdlog (compiled lib -> spdlog::spdlog) ---
if(NOT TARGET spdlog AND NOT TARGET spdlog::spdlog)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${REFRAMEWORK_DIR}/dependencies/spdlog" "${CMAKE_BINARY_DIR}/_dep_spdlog" EXCLUDE_FROM_ALL)
endif()

# --- imgui (no upstream CMake in REFramework; built inline there). Define the
#     `imgui` static target ourselves, including the d3d11/d3d12/win32 backends
#     the D3D hooks will use once the ports land. ---
if(NOT TARGET imgui)
    set(_imgui_dir "${REFRAMEWORK_DIR}/dependencies/imgui")
    add_library(imgui STATIC
        "${_imgui_dir}/imgui.cpp"
        "${_imgui_dir}/imgui_draw.cpp"
        "${_imgui_dir}/imgui_tables.cpp"
        "${_imgui_dir}/imgui_widgets.cpp"
        "${_imgui_dir}/imgui_demo.cpp"
        "${_imgui_dir}/backends/imgui_impl_win32.cpp"
        "${_imgui_dir}/backends/imgui_impl_dx11.cpp"
        "${_imgui_dir}/backends/imgui_impl_dx12.cpp")
    target_include_directories(imgui PUBLIC
        "${_imgui_dir}"
        "${_imgui_dir}/backends")
endif()

# --- nlohmann/json (single header -> nlohmann_json::nlohmann_json) ---
if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE "${REFRAMEWORK_DIR}/dependencies/nlohmann")
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# --- safetyhook (+ bundled Zydis). Build Zydis first so safetyhook links it
#     without FetchContent (offline). Zydis bundles Zycore in-tree. ---
if(NOT TARGET safetyhook)
    if(NOT TARGET Zydis)
        set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
        add_subdirectory("${UEVR_DEPS_DIR}/zydis-src" "${CMAKE_BINARY_DIR}/_dep_zydis" EXCLUDE_FROM_ALL)
    endif()
    set(SAFETYHOOK_FETCH_ZYDIS OFF CACHE BOOL "" FORCE)   # use the Zydis target above
    set(SAFETYHOOK_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(SAFETYHOOK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${UEVR_DEPS_DIR}/safetyhook-src" "${CMAKE_BINARY_DIR}/_dep_safetyhook" EXCLUDE_FROM_ALL)
endif()

# --- openvr (interface: headers + import lib) ---
if(NOT TARGET openvr_api)
    add_library(openvr_api INTERFACE)
    target_include_directories(openvr_api INTERFACE "${REFRAMEWORK_DIR}/dependencies/openvr/headers")
    target_link_libraries(openvr_api INTERFACE
        "${REFRAMEWORK_DIR}/dependencies/openvr/lib/win64/openvr_api.lib")
endif()

# --- DirectXTK12 — NOT USED. The VR D3D12 stereo path (src/mods/vr/D3D12Component.cpp) uses a
#     self-contained ResourceCopier (allocator+list+fence) instead of GraphicsMemory/SpriteBatch, and
#     Framework.cpp dropped CommandContext/GraphicsMemory. Building it from source also fails on the
#     shader-compile custom step (MSB8066). Left out entirely; re-add only if a future overlay needs it.

# --- OpenXR. The loader is DYNAMIC-LOADED at runtime (LoadLibrary + xrGetInstanceProcAddr,
#     see FH5CameraProbe/src/XrSimTest.cpp) -> we do NOT statically link a loader.
#     The vrframework OpenXR.cpp calls xr* prototypes directly, so the consuming
#     SHARED DLL must still resolve those symbols at link time. We satisfy that with
#     the openxr_loader IMPORT LIB (link-time stubs only) and /DELAYLOAD the DLL in
#     the mod target, so no loader DLL is needed until the runtime actually calls XR.
#     Headers come from the OpenXR-Simulator (the runtime we validate against). ---
if(NOT TARGET OpenXR::openxr_loader)
    # Headers ONLY. We do NOT link any openxr_loader lib: the prebuilt static loader is /MT and bundles
    # its own JSON + static CRT (35x LNK2038 + 70x LNK2005 against our /MD DLL), and the tapir loader DLL
    # ships no import lib. Instead the loader is LoadLibrary'd at runtime and every xr* entry point is
    # resolved via xrGetInstanceProcAddr (XR_NO_PROTOTYPES + the dispatch pointers in OpenXRDispatch.cpp),
    # exactly the sequence validated in XrSimTest.cpp.
    add_library(openxr_loader_iface INTERFACE)
    target_include_directories(openxr_loader_iface INTERFACE "${OPENXR_SIM_INCLUDE_DIR}")
    add_library(OpenXR::openxr_loader ALIAS openxr_loader_iface)
endif()

# ---------------------------------------------------------------------------
# Sources. Stubs are included so the target links; flesh them out per PORTING.md.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE VRFRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# The per-game adapter + dllmain (src/fh5vr/) belong to the FH5VR DLL, NOT the
# engine-agnostic core. Exclude them so Mods::Mods()/dllmain aren't compiled into
# both the static core and the shared mod (duplicate-symbol link error).
list(FILTER VRFRAMEWORK_SOURCES EXCLUDE REGEX "/src/fh5vr/")

add_library(vrframework STATIC ${VRFRAMEWORK_SOURCES})

# Public include root — keeps the adapters' include style:
#   #include "Framework.hpp"  /  <Mod.hpp>  /  <mods/VR.hpp>  /  "memory/memory_mul.h"
target_include_directories(vrframework PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(vrframework PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    # OpenXR quaternions are {x,y,z,w}; the runtime port reinterpret-casts XrQuaternionf
    # to glm::quat, so glm's storage order MUST match across every TU (ODR). Also needed
    # by adapters using gtx/quaternion. Keep PUBLIC so fh5vr inherits the identical layout.
    GLM_FORCE_QUAT_DATA_XYZW
    GLM_ENABLE_EXPERIMENTAL
    # <openxr/openxr.h> emits PFN typedefs only; the loader is dynamic-loaded and dispatched through
    # the global xr* pointers in OpenXRDispatch.{hpp,cpp}. Must be consistent across every TU.
    XR_NO_PROTOTYPES
    # Pattern-scan the producer AOB at runtime (and warn if it disagrees with the fallback
    # offset) instead of blindly trusting RVA 0xBB1EE0 — robust to ASLR/build drift. The probe
    # always scanned; match that. See memory/memory_mul.h FuncRelocation.
    SIGNATURE_SCAN
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
    nlohmann_json::nlohmann_json openvr_api OpenXR::openxr_loader)

foreach(dep IN LISTS VRFRAMEWORK_DEPS)
    if(TARGET ${dep})
        target_link_libraries(vrframework PUBLIC ${dep})
    else()
        message(STATUS "vrframework: dependency target '${dep}' not found yet "
                       "(expected to be provided by the superbuild).")
    endif()
endforeach()

target_link_libraries(vrframework PUBLIC d3d11 d3d12 dxgi)

# Make the OpenXR (simulator) headers visible to the core's OpenXR runtime port.
target_include_directories(vrframework PUBLIC "${OPENXR_SIM_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# Optional: the FH5 VR mod DLL (Layer 2/3 adapter + dllmain) lives in fh5vr/.
# Configured when present so `cmake -S <vrframework> -B build-fh5` produces FH5VR.dll.
# Reusable submodule consumers that don't want it can set -DVRFRAMEWORK_BUILD_FH5=OFF.
# ---------------------------------------------------------------------------
option(VRFRAMEWORK_BUILD_FH5 "Build the FH5 VR mod DLL (fh5vr/)" ON)
if(VRFRAMEWORK_BUILD_FH5 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fh5vr/CMakeLists.txt")
    add_subdirectory(fh5vr)
endif()
