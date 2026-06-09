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
#include "Mods.hpp"          // complete type for unique_ptr<Mods> in Framework: dllmain owns g_framework
                             // (std::make_unique<Framework>), so ~Framework -> ~unique_ptr<Mods> instantiates
                             // here and needs Mods defined, not just the Framework.hpp forward declaration.
#include "Fh5CameraCbuffer.hpp"
#include "Fh5MenuNav.hpp"

// --- version.dll proxy export forwarding (-> version_real.dll) --------------------------------------
// PORT NOTE (retail Steam 1.688): we used to proxy dxgi.dll, but retail FH5 ships NVIDIA Streamline
// (sl.interposer.dll) which ITSELF interposes DXGI and calls our dxgi proxy's CreateDXGIFactory1 — the
// resulting double-wrap crashed in our forwarder during init. Proxying a DLL Streamline does NOT touch
// (version.dll, imported by BOTH 1.405 and 1.688) decouples us from Streamline entirely; our actual D3D
// hooking is done by the D3D12 dummy-device vtable hook (Framework), not by these exports — the proxy is
// only a loader vehicle. Forwarders match System32\version.dll ordinals @1..@17.
#pragma comment(linker, "/export:GetFileVersionInfoA=version_real.GetFileVersionInfoA,@1")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=version_real.GetFileVersionInfoByHandle,@2")
#pragma comment(linker, "/export:GetFileVersionInfoExA=version_real.GetFileVersionInfoExA,@3")
#pragma comment(linker, "/export:GetFileVersionInfoExW=version_real.GetFileVersionInfoExW,@4")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=version_real.GetFileVersionInfoSizeA,@5")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=version_real.GetFileVersionInfoSizeExA,@6")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=version_real.GetFileVersionInfoSizeExW,@7")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=version_real.GetFileVersionInfoSizeW,@8")
#pragma comment(linker, "/export:GetFileVersionInfoW=version_real.GetFileVersionInfoW,@9")
#pragma comment(linker, "/export:VerFindFileA=version_real.VerFindFileA,@10")
#pragma comment(linker, "/export:VerFindFileW=version_real.VerFindFileW,@11")
#pragma comment(linker, "/export:VerInstallFileA=version_real.VerInstallFileA,@12")
#pragma comment(linker, "/export:VerInstallFileW=version_real.VerInstallFileW,@13")
#pragma comment(linker, "/export:VerLanguageNameA=version_real.VerLanguageNameA,@14")
#pragma comment(linker, "/export:VerLanguageNameW=version_real.VerLanguageNameW,@15")
#pragma comment(linker, "/export:VerQueryValueA=version_real.VerQueryValueA,@16")
#pragma comment(linker, "/export:VerQueryValueW=version_real.VerQueryValueW,@17")

namespace {
HMODULE g_module{};

DWORD WINAPI bootstrap(void*) {
    // INERT BISECT (FH5VR_DISABLE=1): load + forward version.dll exports but install NOTHING (no framework,
    // no D3D12/present hook, no VR). Isolates whether retail FH5's GDK/anti-tamper exits because a foreign
    // version.dll module is present (would still exit when inert) vs the mod's runtime hooking (would survive
    // inert). The /export forwarders are static, so the proxy still chains to version_real either way.
    {
        char dis[8]{};
        if (::GetEnvironmentVariableA("FH5VR_DISABLE", dis, sizeof(dis)) > 0 && dis[0] == '1') {
            return 0;
        }
    }

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
    // BISECT (FH5VR_NO_CB=1): skip the fh5cb D3D12 device-vtable buffer-tracking hooks (committed/placed/
    // CBV/SRV/UAV/RTV/CopyDescriptors/pso). Isolates whether retail's ~28s exit is tripped by the many DEVICE
    // vtable modifications (would survive without them) vs the framework's single swapchain present hook.
    {
        char nocb[8]{};
        const bool no_cb = ::GetEnvironmentVariableA("FH5VR_NO_CB", nocb, sizeof(nocb)) > 0 && nocb[0] == '1';
        if (!no_cb) {
            fh5cb::install_createdevice_hook(d3d12);
        }
    }

    // Install the XInput detour early so the menu navigator can inject controller input during the
    // title/menu flow — before the engine seam (and Fh5MenuNav::start) come up. Idempotent; start() retries.
    // DISABLED (crash bisect 2026-06-06): testing whether the XInput inline detour contributes to the
    // startup crash (vanilla FH5 is stable; the mod crashes). Re-enable after the bisect.
    // fh5nav::install_xinput_hook();

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
