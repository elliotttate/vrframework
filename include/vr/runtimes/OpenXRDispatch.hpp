#pragma once

// Dynamic OpenXR loader dispatch — the proven XrSimTest.cpp approach.
//
// We do NOT link the static openxr_loader.lib (it is /MT and bundles its own JSON + static CRT, which
// collides with our /MD CRT: 35x LNK2038 + 70x LNK2005). Instead the loader DLL is LoadLibrary'd at
// runtime and every xr* entry point is resolved through xrGetInstanceProcAddr — exactly the sequence
// validated end-to-end against the SimXR runtime.
//
// XR_NO_PROTOTYPES (defined globally by CMake) makes <openxr/openxr.h> emit only the PFN_* typedefs, so
// we are free to declare global function POINTERS with the canonical xr* names. Existing call sites
// (`xrCreateSession(...)`, `xrWaitFrame(...)`, …) then dispatch through these pointers unchanged.

// openxr_platform.h references the platform's own types (ID3D12Device, LUID, LARGE_INTEGER, IUnknown)
// inside the D3D12/Win32 graphics-binding structs, so those headers MUST precede it.
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace xrd {
// Step 1: LoadLibrary("openxr_loader.dll"), resolve xrGetInstanceProcAddr, then the pre-instance
// entry points (xrCreateInstance + the enumerators) that take XR_NULL_HANDLE. Returns false if the
// loader DLL is absent or the core export is missing.
bool load_loader();

// Step 2: after xrCreateInstance succeeds, resolve every remaining entry point against the instance.
bool load_instance_fns(XrInstance instance);

// True once load_loader() has populated xrGetInstanceProcAddr + xrCreateInstance.
bool loader_ready();
} // namespace xrd

// Global dispatch pointers (defined once in OpenXRDispatch.cpp). Names match the spec so call sites
// are source-compatible with the prototype build.
extern PFN_xrGetInstanceProcAddr            xrGetInstanceProcAddr;
extern PFN_xrCreateInstance                 xrCreateInstance;
extern PFN_xrDestroyInstance                xrDestroyInstance;
extern PFN_xrGetSystem                      xrGetSystem;
extern PFN_xrGetSystemProperties            xrGetSystemProperties;
extern PFN_xrCreateSession                  xrCreateSession;
extern PFN_xrDestroySession                 xrDestroySession;
extern PFN_xrCreateReferenceSpace           xrCreateReferenceSpace;
extern PFN_xrEnumerateViewConfigurationViews xrEnumerateViewConfigurationViews;
extern PFN_xrBeginSession                   xrBeginSession;
extern PFN_xrEndSession                     xrEndSession;
extern PFN_xrWaitFrame                      xrWaitFrame;
extern PFN_xrBeginFrame                     xrBeginFrame;
extern PFN_xrEndFrame                       xrEndFrame;
extern PFN_xrLocateViews                    xrLocateViews;
extern PFN_xrLocateSpace                    xrLocateSpace;
extern PFN_xrPollEvent                      xrPollEvent;
extern PFN_xrResultToString                 xrResultToString;
extern PFN_xrStructureTypeToString          xrStructureTypeToString;
extern PFN_xrCreateSwapchain                xrCreateSwapchain;
extern PFN_xrDestroySwapchain               xrDestroySwapchain;
extern PFN_xrEnumerateSwapchainImages       xrEnumerateSwapchainImages;
extern PFN_xrAcquireSwapchainImage          xrAcquireSwapchainImage;
extern PFN_xrWaitSwapchainImage             xrWaitSwapchainImage;
extern PFN_xrReleaseSwapchainImage          xrReleaseSwapchainImage;
