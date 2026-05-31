#pragma once

// PORT FROM: REFramework/src/D3D12Hook.hpp  — already 100% engine-agnostic (pure DXGI/
// D3D12 present/resize hooking). Copy verbatim; bodies in src/hooks/D3D12Hook.cpp.

#include <functional>
#include <memory>
#include <mutex>

#include <d3d12.h>
#include <dxgi1_4.h>

// PORT: these come from praydog's `utility` lib (PointerHook/VtableHook/FunctionHook).
// Replace with safetyhook equivalents or vendor `utility`.
class PointerHook;
class VtableHook;
class FunctionHook;

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

    UINT get_display_width() const { return m_display_width; }
    UINT get_display_height() const { return m_display_height; }
    UINT get_render_width() const { return m_render_width; }
    UINT get_render_height() const { return m_render_height; }

    bool is_inside_present() const { return m_inside_present; }
    void ignore_next_present() { m_ignore_next_present = true; }

protected:
    ID3D12Device4* m_device{ nullptr };
    IDXGISwapChain3* m_swap_chain{ nullptr };
    ID3D12CommandQueue* m_command_queue{ nullptr };
    UINT m_display_width{ 0 }, m_display_height{ 0 };
    UINT m_render_width{ 0 }, m_render_height{ 0 };

    bool m_hooked{ false };
    bool m_inside_present{ false };
    bool m_ignore_next_present{ false };

    std::unique_ptr<PointerHook> m_present_hook{};
    std::unique_ptr<VtableHook> m_swapchain_hook{};

    OnPresentFn m_on_present{ nullptr };
    OnPresentFn m_on_post_present{ nullptr };
    OnResizeBuffersFn m_on_resize_buffers{ nullptr };
    OnResizeTargetFn m_on_resize_target{ nullptr };

    static HRESULT WINAPI present(IDXGISwapChain3*, uint64_t sync_interval, uint64_t flags, void* r9);
    static HRESULT WINAPI resize_buffers(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    static HRESULT WINAPI resize_target(IDXGISwapChain3*, const DXGI_MODE_DESC*);
};
