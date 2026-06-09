# ---------------------------------------------------------------------------
# fh5vr — the Forza Horizon 5 VR mod DLL (FH5VR.dll).
#
# Layer 2 (engine adapter) + Layer 3 (FH5 offsets) + dllmain live here and in
# ../src/fh5vr. The real adapter + dllmain are provided by the orchestrator; this
# skeleton just configures + links a SHARED DLL against the vrframework core so
# the build stands up before the .cpp ports land.
#
# Produces: FH5VR.dll  (x64, MSVC /MD dynamic CRT)
# Links:    vrframework (which brings glm/imgui/spdlog/safetyhook/json/openvr/
#           openxr-headers/directxtk12) + d3d11 d3d12 dxgi dxguid.
# ---------------------------------------------------------------------------

file(GLOB_RECURSE FH5VR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/fh5vr/*.cpp")

add_library(FH5VR SHARED ${FH5VR_SOURCES})

set_target_properties(FH5VR PROPERTIES
    OUTPUT_NAME "FH5VR"
    PREFIX "")

target_link_libraries(FH5VR PRIVATE
    vrframework
    d3d11 d3d12 dxgi dxguid)

target_compile_definitions(FH5VR PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN)

# Dynamic-load the OpenXR loader and other runtimes -> don't resolve them at
# load time. Matches REFramework's proxy-DLL delay-load set. (Linker only acts
# on these once the corresponding import libs are actually referenced.)
if(MSVC)
    # openxr_loader.dll is NOT in the import table (it is LoadLibrary'd + dispatched at runtime), so it is
    # intentionally absent here. Delay-load the runtimes we DO import so they aren't resolved at load time.
    target_link_options(FH5VR PRIVATE
        "/DELAYLOAD:openvr_api.dll"
        "/DELAYLOAD:d3d11.dll"
        "/DELAYLOAD:d3d12.dll")
    target_link_libraries(FH5VR PRIVATE delayimp)

    # Emit a PDB even in Release (keep /O2 codegen via /OPT:REF,ICF) so crash offsets are symbolizable during
    # the retail (1.688) port. Without this, Application-Error fault offsets in dxgi.dll can't be mapped to source.
    target_compile_options(FH5VR PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(FH5VR PRIVATE
        "$<$<CONFIG:Release>:/DEBUG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>")
endif()
