#pragma once

// PORT FROM: REFramework/src/D3D12Hook.hpp  — already 100% engine-agnostic (pure DXGI/
// D3D12 present/resize hooking). Bodies in src/hooks/D3D12Hook.cpp.
//
// Dropped REFramework's `#pragma comment(lib, ...)` (CMake links d3d12/dxgi) and the
// Streamline FunctionHook member (the verbatim port logs the frame-gen swapchain instead
// of re-hooking through RE's hook-monitor thread, which vrframework's spine doesn't run).

#include <functional>
#include <memory>
#include <mutex>

#include <d3d12.h>
#include <dxgi1_4.h>

// PointerHook/VtableHook/FunctionHook come from the safetyhook-backed shim that replaces
// praydog's `utility` lib. Included (not forward-declared) so any TU holding a D3D12Hook
// can destroy its unique_ptr<PointerHook>/<VtableHook> members (complete-type requirement).
#include "utility/Hooks.hpp"

class D3D12Hook {
public:
    typedef std::function<void(D3D12Hook&)> OnPresentFn;
    typedef std::function<void(D3D12Hook&)> OnResizeBuffersFn;
    typedef std::function<void(D3D12Hook&)> OnResizeTargetFn;

    D3D12Hook() = default;
    virtual ~D3D12Hook();

    bool hook();
    bool unhook();
    bool is_hooked() const { return m_hooked; }

    void on_present(OnPresentFn fn) { m_on_present = fn; }
    void on_post_present(OnPresentFn fn) { m_on_post_present = fn; }
    void on_resize_buffers(OnResizeBuffersFn fn) { m_on_resize_buffers = fn; }
    void on_resize_target(OnResizeTargetFn fn) { m_on_resize_target = fn; }

    ID3D12Device4* get_device() const { return m_device; }
    IDXGISwapChain3* get_swap_chain() const { return m_swap_chain; }
    ID3D12CommandQueue* get_command_queue() const { return m_command_queue; }

    auto get_swapchain_0() const { return m_swapchain_0; }
    auto get_swapchain_1() const { return m_swapchain_1; }

    UINT get_display_width() const { return m_display_width; }
    UINT get_display_height() const { return m_display_height; }
    UINT get_render_width() const { return m_render_width; }
    UINT get_render_height() const { return m_render_height; }

    bool is_inside_present() const { return m_inside_present; }
    bool is_proton_swapchain() const { return m_using_proton_swapchain; }
    bool is_framegen_swapchain() const { return m_using_frame_generation_swapchain; }
    void ignore_next_present() { m_ignore_next_present = true; }

protected:
    void hook_impl();

    ID3D12Device4* m_device{ nullptr };
    IDXGISwapChain3* m_swap_chain{ nullptr };
    IDXGISwapChain3* m_swapchain_0{ nullptr };
    IDXGISwapChain3* m_swapchain_1{ nullptr };
    ID3D12CommandQueue* m_command_queue{ nullptr };
    UINT m_display_width{ 0 }, m_display_height{ 0 };
    UINT m_render_width{ 0 }, m_render_height{ 0 };

    static inline uint32_t s_command_queue_offset{};
    static inline uint32_t s_proton_swapchain_offset{};

    bool m_using_proton_swapchain{ false };
    bool m_using_frame_generation_swapchain{ false };
    bool m_hooked{ false };
    bool m_is_phase_1{ true };
    bool m_inside_present{ false };
    bool m_ignore_next_present{ false };

    std::unique_ptr<PointerHook> m_present_hook{};
    std::unique_ptr<VtableHook> m_swapchain_hook{};

    static inline void** s_factory_vtable{ nullptr };
    static inline void** s_swapchain_vtable{ nullptr };

    OnPresentFn m_on_present{ nullptr };
    OnPresentFn m_on_post_present{ nullptr };
    OnResizeBuffersFn m_on_resize_buffers{ nullptr };
    OnResizeTargetFn m_on_resize_target{ nullptr };

    static HRESULT WINAPI present(IDXGISwapChain3*, uint64_t sync_interval, uint64_t flags, void* r9);
    static HRESULT WINAPI resize_buffers(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    static HRESULT WINAPI resize_target(IDXGISwapChain3*, const DXGI_MODE_DESC*);
};
