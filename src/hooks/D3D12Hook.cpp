// PORT FROM: REFramework/src/D3D12Hook.cpp — engine-agnostic (pure DXGI/D3D12).
//
// Faithfully ported. Stripped of RE-Engine coupling:
//   * WindowFilter (RE's per-window skip list) — replaced with "no filtering".
//   * g_framework->get_hook_monitor_mutex() — REFramework runs a background hook-monitor
//     thread that races the present thread; vrframework's spine has no such thread, so the
//     serialization lock becomes a file-local std::recursive_mutex (same guarantee within
//     this TU). g_framework->on_reset() (RE device-object teardown) is dropped from the
//     Streamline re-hook path; we still re-hook via g_framework->hook_d3d12().
//   * utility::{PointerHook,VtableHook,FunctionHook,get_original_bytes,Scan,RTTI} come from
//     include/utility/Hooks.hpp (safetyhook-backed) instead of praydog's `utility` submodule.
//   * `#pragma comment(lib, ...)` dropped — CMake links d3d12/dxgi.
// Kept verbatim: dummy device+swapchain vtable steal, command-queue offset scan, the
// Proton/frame-gen two-level scan, two-phase (pointer→vtable) hooking, reentrancy guards.

#include <thread>
#include <vector>
#include <string_view>
#include <wrl/client.h>

#include <spdlog/spdlog.h>

#include "Framework.hpp"
#include "utility/Hooks.hpp"

#include "hooks/D3D12Hook.hpp"

static D3D12Hook* g_d3d12_hook = nullptr;
thread_local bool g_inside_d3d12_hook = false;

// vrframework: stands in for REFramework's g_framework->get_hook_monitor_mutex().
// Serializes the present/resize detours and (un)hook against each other.
static std::recursive_mutex g_hook_monitor_mutex{};

D3D12Hook::~D3D12Hook() {
    unhook();
}

bool D3D12Hook::hook() {
    spdlog::info("Hooking D3D12");

    g_d3d12_hook = this;
    g_inside_d3d12_hook = true;

    struct ScopeGuard {
        ~ScopeGuard() { g_inside_d3d12_hook = false; }
    } guard{};

    IDXGISwapChain1* swap_chain1{ nullptr };
    IDXGISwapChain3* swap_chain{ nullptr };
    ID3D12Device* device{ nullptr };

    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1;

    ZeroMemory(&swap_chain_desc1, sizeof(swap_chain_desc1));

    swap_chain_desc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desc1.BufferCount = 2;
    swap_chain_desc1.SampleDesc.Count = 1;
    swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swap_chain_desc1.Width = 1;
    swap_chain_desc1.Height = 1;

    // Manually get D3D12CreateDevice export because the user may be running Windows 7
    const auto d3d12_module = LoadLibraryA("d3d12.dll");
    if (d3d12_module == nullptr) {
        spdlog::error("Failed to load d3d12.dll");
        return false;
    }

    auto d3d12_create_device = (decltype(D3D12CreateDevice)*)GetProcAddress(d3d12_module, "D3D12CreateDevice");
    if (d3d12_create_device == nullptr) {
        spdlog::error("Failed to get D3D12CreateDevice export");
        return false;
    }

    spdlog::info("Creating dummy device");

    // Get the original on-disk bytes of the D3D12CreateDevice export
    const auto original_bytes = utility::get_original_bytes(Address{ (void*)d3d12_create_device });

    // Temporarily unhook D3D12CreateDevice
    // it allows compatibility with ReShade and other overlays that hook it
    // this is just a dummy device anyways, we don't want the other overlays to be able to use it
    if (original_bytes) {
        spdlog::info("D3D12CreateDevice appears to be hooked, temporarily unhooking");

        std::vector<uint8_t> hooked_bytes(original_bytes->size());
        memcpy(hooked_bytes.data(), d3d12_create_device, original_bytes->size());

        ProtectionOverride protection_override{ (void*)d3d12_create_device, original_bytes->size(), PAGE_EXECUTE_READWRITE };
        memcpy((void*)d3d12_create_device, original_bytes->data(), original_bytes->size());

        if (FAILED(d3d12_create_device(nullptr, feature_level, IID_PPV_ARGS(&device)))) {
            spdlog::error("Failed to create D3D12 Dummy device");
            memcpy((void*)d3d12_create_device, hooked_bytes.data(), hooked_bytes.size());
            return false;
        }

        spdlog::info("Restoring hooked bytes for D3D12CreateDevice");
        memcpy((void*)d3d12_create_device, hooked_bytes.data(), hooked_bytes.size());
    } else { // D3D12CreateDevice is not hooked
        if (FAILED(d3d12_create_device(nullptr, feature_level, IID_PPV_ARGS(&device)))) {
            spdlog::error("Failed to create D3D12 Dummy device");
            return false;
        }
    }

    spdlog::info("Dummy device: {:x}", (uintptr_t)device);

    // Manually get CreateDXGIFactory export because the user may be running Windows 7
    const auto dxgi_module = LoadLibraryA("dxgi.dll");
    if (dxgi_module == nullptr) {
        spdlog::error("Failed to load dxgi.dll");
        return false;
    }

    auto create_dxgi_factory = (decltype(CreateDXGIFactory)*)GetProcAddress(dxgi_module, "CreateDXGIFactory");

    if (create_dxgi_factory == nullptr) {
        spdlog::error("Failed to get CreateDXGIFactory export");
        return false;
    }

    spdlog::info("Creating dummy DXGI factory");

    IDXGIFactory4* factory{ nullptr };
    if (FAILED(create_dxgi_factory(IID_PPV_ARGS(&factory)))) {
        spdlog::error("Failed to create D3D12 Dummy DXGI Factory");
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = 0;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 0;

    spdlog::info("Creating dummy command queue");

    ID3D12CommandQueue* command_queue{ nullptr };
    if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)))) {
        spdlog::error("Failed to create D3D12 Dummy Command Queue");
        return false;
    }

    spdlog::info("Creating dummy swapchain");

    // used in CreateSwapChainForHwnd fallback
    HWND hwnd = 0;
    WNDCLASSEX wc{};

    auto init_dummy_window = [&]() {
        // fallback to CreateSwapChainForHwnd
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DefWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hIcon = NULL;
        wc.hCursor = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = TEXT("VRFRAMEWORK_DX12_DUMMY");
        wc.hIconSm = NULL;

        ::RegisterClassEx(&wc);

        hwnd = ::CreateWindow(wc.lpszClassName, TEXT("VRF DX Dummy Window"), WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

        swap_chain_desc1.BufferCount = 3;
        swap_chain_desc1.Width = 0;
        swap_chain_desc1.Height = 0;
        swap_chain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc1.SampleDesc.Count = 1;
        swap_chain_desc1.SampleDesc.Quality = 0;
        swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
        swap_chain_desc1.Stereo = FALSE;
    };

    std::vector<std::function<bool ()>> swapchain_attempts{
        // we call CreateSwapChainForComposition instead of CreateSwapChainForHwnd
        // because some overlays will have hooks on CreateSwapChainForHwnd
        // and all we're doing is creating a dummy swapchain
        // we don't want to screw up the overlay
        [&]() {
            return !FAILED(factory->CreateSwapChainForComposition(command_queue, &swap_chain_desc1, nullptr, &swap_chain1));
        },
        [&]() {
            init_dummy_window();

            return !FAILED(factory->CreateSwapChainForHwnd(command_queue, hwnd, &swap_chain_desc1, nullptr, nullptr, &swap_chain1));
        },
        [&]() {
            return !FAILED(factory->CreateSwapChainForHwnd(command_queue, GetDesktopWindow(), &swap_chain_desc1, nullptr, nullptr, &swap_chain1));
        },
    };

    bool any_succeed = false;

    for (size_t i = 0; i < swapchain_attempts.size(); i++) {
        auto& attempt = swapchain_attempts[i];

        try {
            spdlog::info("Trying swapchain attempt {}", i);

            if (attempt()) {
                spdlog::info("Created dummy swapchain on attempt {}", i);
                any_succeed = true;
                break;
            }
        } catch (std::exception& e) {
            spdlog::error("Failed to create dummy swapchain on attempt {}: {}", i, e.what());
        } catch(...) {
            spdlog::error("Failed to create dummy swapchain on attempt {}: unknown exception", i);
        }

        spdlog::error("Attempt {} failed", i);
    }

    if (!any_succeed) {
        spdlog::error("Failed to create D3D12 Dummy Swap Chain");

        if (hwnd) {
            ::DestroyWindow(hwnd);
        }

        if (wc.lpszClassName != nullptr) {
            ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        }

        return false;
    }

    spdlog::info("Querying dummy swapchain");

    if (FAILED(swap_chain1->QueryInterface(IID_PPV_ARGS(&swap_chain)))) {
        spdlog::error("Failed to retrieve D3D12 DXGI SwapChain");
        return false;
    }

    // NOTE: REFramework detects DLSS-FG/FSR3 interposers here via RTTI on the swapchain class name.
    // We INTENTIONALLY skip it: this is our OWN freshly-created dummy swapchain (never a frame-gen
    // interposer), and a raw `typeid(*comptr)` reads vtable[-1] which ACCESS-VIOLATES on proxied/overlaid
    // swapchains (an AV is not a C++ exception, so the try/catch did not save it — it hard-crashed FH5
    // right after "Querying dummy swapchain"). m_using_frame_generation_swapchain stays false.
    spdlog::info("Skipping dummy-swapchain RTTI frame-gen probe (not needed for the throwaway swapchain)");

    spdlog::info("Finding command queue offset");

    s_command_queue_offset = 0;

    // Find the command queue offset in the swapchain
    for (size_t i = 0; i < 512 * sizeof(void*); i += sizeof(void*)) {
        const auto base = (uintptr_t)swap_chain1 + i;

        // reached the end
        if (IsBadReadPtr((void*)base, sizeof(void*))) {
            break;
        }

        auto data = *(ID3D12CommandQueue**)base;

        if (data == command_queue) {
            s_command_queue_offset = (uint32_t)i;
            spdlog::info("Found command queue offset: {:x}", i);
            break;
        }
    }

    auto target_swapchain = swap_chain;

    // Scan throughout the swapchain for a valid pointer to scan through
    // this is usually only necessary for Proton (and doubles as the frame-gen inner-swapchain finder)
    if (s_command_queue_offset == 0) {
        bool should_break = false;

        for (size_t base = 0; base < 512 * sizeof(void*); base += sizeof(void*)) {
            const auto pre_scan_base = (uintptr_t)swap_chain1 + base;

            // reached the end
            if (IsBadReadPtr((void*)pre_scan_base, sizeof(void*))) {
                break;
            }

            const auto scan_base = *(uintptr_t*)pre_scan_base;

            if (scan_base == 0 || IsBadReadPtr((void*)scan_base, sizeof(void*))) {
                continue;
            }

            for (size_t i = 0; i < 512 * sizeof(void*); i += sizeof(void*)) {
                const auto pre_data = scan_base + i;

                if (IsBadReadPtr((void*)pre_data, sizeof(void*))) {
                    break;
                }

                auto data = *(ID3D12CommandQueue**)pre_data;

                if (data == command_queue) {
                    // If we hook Streamline's Swapchain, the menu fails to render correctly/flickers
                    // So we switch out the swapchain with the internal one owned by Streamline
                    if (m_using_frame_generation_swapchain) {
                        target_swapchain = (IDXGISwapChain3*)scan_base;
                    }

                    if (!m_using_frame_generation_swapchain) {
                        m_using_proton_swapchain = true;
                    }

                    s_command_queue_offset = (uint32_t)i;
                    s_proton_swapchain_offset = (uint32_t)base;
                    should_break = true;

                    spdlog::info("Proton potentially detected");
                    spdlog::info("Found command queue offset: {:x}", i);
                    break;
                }
            }

            if (m_using_proton_swapchain || should_break) {
                break;
            }
        }
    }

    if (s_command_queue_offset == 0) {
        spdlog::error("Failed to find command queue offset");
        return false;
    }

    try {
        s_swapchain_vtable = *(void***)target_swapchain;
        s_factory_vtable = *(void***)factory;

        hook_impl();
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize hooks: {}", e.what());
        m_hooked = false;
    }

    command_queue->Release();
    swap_chain1->Release();
    swap_chain->Release();
    device->Release();
    factory->Release();

    if (hwnd) {
        ::DestroyWindow(hwnd);
    }

    if (wc.lpszClassName != nullptr) {
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

    return m_hooked;
}

void D3D12Hook::hook_impl() {
    spdlog::info("Initializing hooks");

    m_present_hook.reset();
    m_swapchain_hook.reset();

    m_is_phase_1 = true;

    auto& present_fn = s_swapchain_vtable[8]; // Present
    m_present_hook = std::make_unique<PointerHook>(&present_fn, (void*)&D3D12Hook::present);

    m_hooked = true;
}

bool D3D12Hook::unhook() {
    std::scoped_lock _{ g_hook_monitor_mutex };

    if (!m_hooked) {
        return true;
    }

    spdlog::info("Unhooking D3D12");

    m_present_hook.reset();
    m_swapchain_hook.reset();

    m_hooked = false;
    m_is_phase_1 = true;

    return true;
}

thread_local int32_t g_present_depth = 0;

HRESULT WINAPI D3D12Hook::present(IDXGISwapChain3* swap_chain, uint64_t sync_interval, uint64_t flags, void* r9) {
    while (g_framework == nullptr) {
        std::this_thread::yield();
    }

    std::scoped_lock _{ g_hook_monitor_mutex };

    auto d3d12 = g_d3d12_hook;

    decltype(D3D12Hook::present)* present_fn{ nullptr };

    if (d3d12->m_is_phase_1) {
        present_fn = d3d12->m_present_hook->get_original<decltype(D3D12Hook::present)*>();
    } else {
        present_fn = d3d12->m_swapchain_hook->get_method<decltype(D3D12Hook::present)*>(8);
    }

    if (!d3d12->m_is_phase_1 && swap_chain != d3d12->m_swapchain_hook->get_instance()) {
        return present_fn(swap_chain, sync_interval, flags, r9);
    }

    if (d3d12->m_is_phase_1) {
        // Remove the global pointer hook and switch to a per-instance vtable hook —
        // least intrusive, avoids conflicts with Streamline / other overlays that touch
        // the shared vtable (a global pointer replacement caused unexplainable crashes).
        d3d12->m_present_hook.reset();

        d3d12->m_swapchain_hook = std::make_unique<VtableHook>(swap_chain);
        d3d12->m_swapchain_hook->hook_method(8, (uintptr_t)&D3D12Hook::present);
        d3d12->m_swapchain_hook->hook_method(13, (uintptr_t)&D3D12Hook::resize_buffers);
        d3d12->m_swapchain_hook->hook_method(14, (uintptr_t)&D3D12Hook::resize_target);
        d3d12->m_is_phase_1 = false;

        present_fn = d3d12->m_swapchain_hook->get_method<decltype(D3D12Hook::present)*>(8);
    }

    d3d12->m_inside_present = true;
    d3d12->m_swap_chain = swap_chain;

    {
        Microsoft::WRL::ComPtr<ID3D12Device4> temp_device{};
        swap_chain->GetDevice(IID_PPV_ARGS(&temp_device));
        d3d12->m_device = temp_device.Get();
    }

    if (d3d12->m_using_proton_swapchain) {
        const auto real_swapchain = *(uintptr_t*)((uintptr_t)swap_chain + d3d12->s_proton_swapchain_offset);
        d3d12->m_command_queue = *(ID3D12CommandQueue**)(real_swapchain + d3d12->s_command_queue_offset);
    } else {
        d3d12->m_command_queue = *(ID3D12CommandQueue**)((uintptr_t)swap_chain + d3d12->s_command_queue_offset);
    }

    if (d3d12->m_swapchain_0 == nullptr) {
        d3d12->m_swapchain_0 = swap_chain;
    } else if (d3d12->m_swapchain_1 == nullptr && swap_chain != d3d12->m_swapchain_0) {
        d3d12->m_swapchain_1 = swap_chain;
    }

    // Restore the original bytes if an infinite loop occurs; prevents a crash while
    // keeping our hook intact (a no-op with the vtable redirect — see Hooks.cpp).
    if (g_present_depth > 0) {
        auto original_bytes = utility::get_original_bytes(Address{ (void*)present_fn });

        if (original_bytes) {
            ProtectionOverride protection_override{ (void*)present_fn, original_bytes->size(), PAGE_EXECUTE_READWRITE };
            memcpy((void*)present_fn, original_bytes->data(), original_bytes->size());
            spdlog::info("Present fixed");
        }

        if ((uintptr_t)present_fn != (uintptr_t)D3D12Hook::present && g_present_depth == 1) {
            spdlog::info("Attempting to call real present function");

            ++g_present_depth;
            const auto result = present_fn(swap_chain, sync_interval, flags, r9);
            --g_present_depth;

            if (result != S_OK) {
                spdlog::error("Present failed: {:x}", result);
            }

            return result;
        }

        spdlog::info("Just returning S_OK");
        return S_OK;
    }

    if (d3d12->m_on_present) {
        d3d12->m_on_present(*d3d12);
    }

    ++g_present_depth;

    auto result = S_OK;

    if (!d3d12->m_ignore_next_present) {
        result = present_fn(swap_chain, sync_interval, flags, r9);

        if (result != S_OK) {
            spdlog::error("Present failed: {:x}", result);
        }
    } else {
        d3d12->m_ignore_next_present = false;
    }

    --g_present_depth;

    if (d3d12->m_on_post_present) {
        d3d12->m_on_post_present(*d3d12);
    }

    d3d12->m_inside_present = false;

    return result;
}

thread_local int32_t g_resize_buffers_depth = 0;

HRESULT WINAPI D3D12Hook::resize_buffers(IDXGISwapChain3* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
    while (g_framework == nullptr) {
        std::this_thread::yield();
    }

    std::scoped_lock _{ g_hook_monitor_mutex };

    spdlog::info("D3D12 resize buffers called");
    spdlog::info(" Parameters: buffer_count {} width {} height {} new_format {} swap_chain_flags {}", buffer_count, width, height, (uint32_t)new_format, swap_chain_flags);

    auto d3d12 = g_d3d12_hook;

    auto resize_buffers_fn = d3d12->m_swapchain_hook->get_method<decltype(D3D12Hook::resize_buffers)*>(13);

    d3d12->m_display_width = width;
    d3d12->m_display_height = height;

    if (g_resize_buffers_depth > 0) {
        auto original_bytes = utility::get_original_bytes(Address{ (void*)resize_buffers_fn });

        if (original_bytes) {
            ProtectionOverride protection_override{ (void*)resize_buffers_fn, original_bytes->size(), PAGE_EXECUTE_READWRITE };
            memcpy((void*)resize_buffers_fn, original_bytes->data(), original_bytes->size());
            spdlog::info("Resize buffers fixed");
        }

        if ((uintptr_t)resize_buffers_fn != (uintptr_t)&D3D12Hook::resize_buffers && g_resize_buffers_depth == 1) {
            spdlog::info("Attempting to call the real resize buffers function");

            ++g_resize_buffers_depth;
            const auto result = resize_buffers_fn(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
            --g_resize_buffers_depth;

            if (result != S_OK) {
                spdlog::error("Resize buffers failed: {:x}", result);
            }

            return result;
        } else {
            spdlog::info("Just returning S_OK");
            return S_OK;
        }
    }

    if (d3d12->m_on_resize_buffers) {
        d3d12->m_on_resize_buffers(*d3d12);
    }

    ++g_resize_buffers_depth;

    const auto result = resize_buffers_fn(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);

    if (result != S_OK) {
        spdlog::error("Resize buffers failed: {:x}", result);
    }

    --g_resize_buffers_depth;

    return result;
}

thread_local int32_t g_resize_target_depth = 0;

HRESULT WINAPI D3D12Hook::resize_target(IDXGISwapChain3* swap_chain, const DXGI_MODE_DESC* new_target_parameters) {
    while (g_framework == nullptr) {
        std::this_thread::yield();
    }

    std::scoped_lock _{ g_hook_monitor_mutex };

    spdlog::info("D3D12 resize target called");
    spdlog::info(" Parameters: new_target_parameters {:x}", (uintptr_t)new_target_parameters);

    auto d3d12 = g_d3d12_hook;

    auto resize_target_fn = d3d12->m_swapchain_hook->get_method<decltype(D3D12Hook::resize_target)*>(14);

    d3d12->m_render_width = new_target_parameters->Width;
    d3d12->m_render_height = new_target_parameters->Height;

    if (g_resize_target_depth > 0) {
        auto original_bytes = utility::get_original_bytes(Address{ (void*)resize_target_fn });

        if (original_bytes) {
            ProtectionOverride protection_override{ (void*)resize_target_fn, original_bytes->size(), PAGE_EXECUTE_READWRITE };
            memcpy((void*)resize_target_fn, original_bytes->data(), original_bytes->size());
            spdlog::info("Resize target fixed");
        }

        if ((uintptr_t)resize_target_fn != (uintptr_t)&D3D12Hook::resize_target && g_resize_target_depth == 1) {
            spdlog::info("Attempting to call the real resize target function");

            ++g_resize_target_depth;
            const auto result = resize_target_fn(swap_chain, new_target_parameters);
            --g_resize_target_depth;

            if (result != S_OK) {
                spdlog::error("Resize target failed: {:x}", result);
            }

            return result;
        } else {
            spdlog::info("Just returning S_OK");
            return S_OK;
        }
    }

    if (d3d12->m_on_resize_target) {
        d3d12->m_on_resize_target(*d3d12);
    }

    ++g_resize_target_depth;

    const auto result = resize_target_fn(swap_chain, new_target_parameters);

    if (result != S_OK) {
        spdlog::error("Resize target failed: {:x}", result);
    }

    --g_resize_target_depth;

    return result;
}
