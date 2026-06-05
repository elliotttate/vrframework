#include "vr/runtimes/OpenXRDispatch.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <spdlog/spdlog.h>

// Single definition of every dispatch pointer.
PFN_xrGetInstanceProcAddr            xrGetInstanceProcAddr            = nullptr;
PFN_xrCreateInstance                 xrCreateInstance                 = nullptr;
PFN_xrDestroyInstance                xrDestroyInstance                = nullptr;
PFN_xrGetSystem                      xrGetSystem                      = nullptr;
PFN_xrGetSystemProperties            xrGetSystemProperties            = nullptr;
PFN_xrCreateSession                  xrCreateSession                  = nullptr;
PFN_xrDestroySession                 xrDestroySession                 = nullptr;
PFN_xrCreateReferenceSpace           xrCreateReferenceSpace           = nullptr;
PFN_xrEnumerateViewConfigurationViews xrEnumerateViewConfigurationViews = nullptr;
PFN_xrBeginSession                   xrBeginSession                   = nullptr;
PFN_xrEndSession                     xrEndSession                     = nullptr;
PFN_xrWaitFrame                      xrWaitFrame                      = nullptr;
PFN_xrBeginFrame                     xrBeginFrame                     = nullptr;
PFN_xrEndFrame                       xrEndFrame                       = nullptr;
PFN_xrLocateViews                    xrLocateViews                    = nullptr;
PFN_xrLocateSpace                    xrLocateSpace                    = nullptr;
PFN_xrPollEvent                      xrPollEvent                      = nullptr;
PFN_xrResultToString                 xrResultToString                 = nullptr;
PFN_xrStructureTypeToString          xrStructureTypeToString          = nullptr;
PFN_xrCreateSwapchain                xrCreateSwapchain                = nullptr;
PFN_xrDestroySwapchain               xrDestroySwapchain               = nullptr;
PFN_xrEnumerateSwapchainImages       xrEnumerateSwapchainImages       = nullptr;
PFN_xrAcquireSwapchainImage          xrAcquireSwapchainImage          = nullptr;
PFN_xrWaitSwapchainImage             xrWaitSwapchainImage             = nullptr;
PFN_xrReleaseSwapchainImage          xrReleaseSwapchainImage          = nullptr;

namespace xrd {
namespace {
HMODULE g_loader = nullptr;
} // namespace

bool loader_ready() { return xrGetInstanceProcAddr != nullptr && xrCreateInstance != nullptr; }

bool load_loader() {
    if (loader_ready()) {
        return true;
    }
    if (g_loader == nullptr) {
        g_loader = LoadLibraryW(L"openxr_loader.dll");
    }
    if (g_loader == nullptr) {
        spdlog::error("OpenXRDispatch: LoadLibrary(openxr_loader.dll) failed (err={})", GetLastError());
        return false;
    }
    xrGetInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        GetProcAddress(g_loader, "xrGetInstanceProcAddr"));
    if (xrGetInstanceProcAddr == nullptr) {
        spdlog::error("OpenXRDispatch: openxr_loader.dll has no xrGetInstanceProcAddr export");
        return false;
    }
    // Pre-instance entry points resolve against XR_NULL_HANDLE.
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrCreateInstance",
        reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateInstance));
    if (xrCreateInstance == nullptr) {
        spdlog::error("OpenXRDispatch: could not resolve xrCreateInstance");
        return false;
    }
    return true;
}

bool load_instance_fns(XrInstance instance) {
    if (xrGetInstanceProcAddr == nullptr || instance == XR_NULL_HANDLE) {
        return false;
    }
#define XRD_GET(name) xrGetInstanceProcAddr(instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(&name))
    XRD_GET(xrDestroyInstance);
    XRD_GET(xrGetSystem);
    XRD_GET(xrGetSystemProperties);
    XRD_GET(xrCreateSession);
    XRD_GET(xrDestroySession);
    XRD_GET(xrCreateReferenceSpace);
    XRD_GET(xrEnumerateViewConfigurationViews);
    XRD_GET(xrBeginSession);
    XRD_GET(xrEndSession);
    XRD_GET(xrWaitFrame);
    XRD_GET(xrBeginFrame);
    XRD_GET(xrEndFrame);
    XRD_GET(xrLocateViews);
    XRD_GET(xrLocateSpace);
    XRD_GET(xrPollEvent);
    XRD_GET(xrResultToString);
    XRD_GET(xrStructureTypeToString);
    XRD_GET(xrCreateSwapchain);
    XRD_GET(xrDestroySwapchain);
    XRD_GET(xrEnumerateSwapchainImages);
    XRD_GET(xrAcquireSwapchainImage);
    XRD_GET(xrWaitSwapchainImage);
    XRD_GET(xrReleaseSwapchainImage);
#undef XRD_GET
    // Minimal sanity: the per-frame core must be present.
    return xrCreateSession != nullptr && xrWaitFrame != nullptr && xrEndFrame != nullptr;
}
} // namespace xrd
