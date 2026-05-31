#pragma once

// PORT FROM: REFramework/src/D3D11Hook.hpp — engine-agnostic (DXGI present/resize on a
// D3D11 device). Copy verbatim. Header trimmed to the surface the core needs.

#include <functional>
#include <memory>

#include <d3d11.h>
#include <dxgi1_4.h>

class PointerHook;

class D3D11Hook {
public:
    typedef std::function<void(D3D11Hook&)> OnPresentFn;
    typedef std::function<void(D3D11Hook&)> OnResizeBuffersFn;

    D3D11Hook() = default;
    virtual ~D3D11Hook();

    bool hook();
    bool unhook();
    bool is_hooked() const { return m_hooked; }

    void on_present(OnPresentFn fn) { m_on_present = fn; }
    void on_post_present(OnPresentFn fn) { m_on_post_present = fn; }
    void on_resize_buffers(OnResizeBuffersFn fn) { m_on_resize_buffers = fn; }

    ID3D11Device* get_device() const { return m_device; }
    IDXGISwapChain* get_swap_chain() const { return m_swap_chain; }
    UINT get_render_width() const { return m_render_width; }
    UINT get_render_height() const { return m_render_height; }
    bool is_inside_present() const { return m_inside_present; }

protected:
    ID3D11Device* m_device{ nullptr };
    IDXGISwapChain* m_swap_chain{ nullptr };
    UINT m_render_width{ 0 }, m_render_height{ 0 };
    bool m_hooked{ false };
    bool m_inside_present{ false };

    std::unique_ptr<PointerHook> m_present_hook{};

    OnPresentFn m_on_present{ nullptr };
    OnPresentFn m_on_post_present{ nullptr };
    OnResizeBuffersFn m_on_resize_buffers{ nullptr };

    static HRESULT WINAPI present(IDXGISwapChain*, UINT sync_interval, UINT flags);
    static HRESULT WINAPI resize_buffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
};
