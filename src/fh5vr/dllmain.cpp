// fh5vr/dllmain.cpp — FH5VR entry point.
//
// Deployment = dxgi.dll PROXY next to ForzaHorizon5.exe (the exact loader the FH5CameraProbe proved
// auto-loads on the Empress build — no EAC/Arxan to fight). Rename the system dxgi.dll -> dxgi_real.dll
// beside the exe and drop this DLL as dxgi.dll; the /export forwards below hand every real dxgi entry
// point straight through to dxgi_real so the renderer is unaffected.
//
// All we do on top of forwarding is spin a worker thread that constructs g_framework once d3d12.dll is
// resident. Framework's ctor installs the D3D12 present hook (REFramework-style dummy-swapchain vtable
// grab) and drives the frame-init state machine; Mods::Mods() (fh5vr/ModConfig.cpp) registers the VR
// mod + Fh5Adapter, and the adapter's install_hooks() detours ForzaTech's view/projection producer.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <memory>

#include "Framework.hpp"
#include "Fh5CameraCbuffer.hpp"
#include "Fh5MenuNav.hpp"

// --- dxgi.dll proxy export forwarding (-> dxgi_real.dll) --------------------------------------------
// Same ordinal-stable set the probe uses; the renderer calls CreateDXGIFactory* which forward through.
#pragma comment(linker, "/export:ApplyCompatResolutionQuirking=dxgi_real.ApplyCompatResolutionQuirking,@1")
#pragma comment(linker, "/export:CompatString=dxgi_real.CompatString,@2")
#pragma comment(linker, "/export:CompatValue=dxgi_real.CompatValue,@3")
#pragma comment(linker, "/export:DXGIDumpJournal=dxgi_real.DXGIDumpJournal,@4")
#pragma comment(linker, "/export:PIXBeginCapture=dxgi_real.PIXBeginCapture,@5")
#pragma comment(linker, "/export:PIXEndCapture=dxgi_real.PIXEndCapture,@6")
#pragma comment(linker, "/export:PIXGetCaptureState=dxgi_real.PIXGetCaptureState,@7")
#pragma comment(linker, "/export:SetAppCompatStringPointer=dxgi_real.SetAppCompatStringPointer,@8")
#pragma comment(linker, "/export:UpdateHMDEmulationStatus=dxgi_real.UpdateHMDEmulationStatus,@9")
#pragma comment(linker, "/export:CreateDXGIFactory=dxgi_real.CreateDXGIFactory,@10")
#pragma comment(linker, "/export:CreateDXGIFactory1=dxgi_real.CreateDXGIFactory1,@11")
#pragma comment(linker, "/export:CreateDXGIFactory2=dxgi_real.CreateDXGIFactory2,@12")
#pragma comment(linker, "/export:DXGID3D10CreateDevice=dxgi_real.DXGID3D10CreateDevice,@13")
#pragma comment(linker, "/export:DXGID3D10CreateLayeredDevice=dxgi_real.DXGID3D10CreateLayeredDevice,@14")
#pragma comment(linker, "/export:DXGID3D10GetLayeredDeviceSize=dxgi_real.DXGID3D10GetLayeredDeviceSize,@15")
#pragma comment(linker, "/export:DXGID3D10RegisterLayers=dxgi_real.DXGID3D10RegisterLayers,@16")
#pragma comment(linker, "/export:DXGIDeclareAdapterRemovalSupport=dxgi_real.DXGIDeclareAdapterRemovalSupport,@17")
#pragma comment(linker, "/export:DXGIDisableVBlankVirtualization=dxgi_real.DXGIDisableVBlankVirtualization,@18")
#pragma comment(linker, "/export:DXGIGetDebugInterface1=dxgi_real.DXGIGetDebugInterface1,@19")
#pragma comment(linker, "/export:DXGIReportAdapterConfiguration=dxgi_real.DXGIReportAdapterConfiguration,@20")

namespace {
HMODULE g_module{};

DWORD WINAPI bootstrap(void*) {
    // Wait for the D3D12 runtime to be resident before standing up the framework. ForzaTech loads it
    // early, but the proxy can attach before it does; poll briefly (no app-dir search — system32 only).
    HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
    for (int i = 0; i < 400 && d3d12 == nullptr; ++i) {
        d3d12 = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (d3d12 == nullptr) {
            Sleep(25);
        }
    }
    if (d3d12 == nullptr) {
        return 1;
    }

    // Hook D3D12CreateDevice BEFORE the framework (and the game) create their devices, so the downstream
    // camera-cbuffer buffer-tracking hooks install at device creation and catch the camera ring's
    // allocation (it is created during render init, before any later producer-triggered install).
    fh5cb::install_createdevice_hook(d3d12);

    // Install the XInput detour early so the menu navigator can inject controller input during the
    // title/menu flow — before the engine seam (and Fh5MenuNav::start) come up. Idempotent; start() retries.
    fh5nav::install_xinput_hook();

    // Construct the framework. Its ctor hooks D3D12 present and runs the frame-init state machine;
    // the first present drives Mods init -> Fh5Adapter::install_hooks() (the producer detour).
    g_framework = std::make_unique<Framework>(g_module);
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE t = CreateThread(nullptr, 0, &bootstrap, nullptr, 0, nullptr)) {
            CloseHandle(t);
        }
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:
        break;
    }
    return TRUE;
}
