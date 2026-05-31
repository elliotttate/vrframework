#pragma once

// PORT FROM: REFramework/src/REFramework.hpp  (class renamed REFramework -> Framework)
// Cleaned: removed sdk/GameIdentity, ReClass, d3d12/CommandContext, GraphicsMemory and
// every RE-reflection member. This is the engine-agnostic surface the ports call
// through `g_framework`. Bodies live in src/Framework.cpp (stub — see PORTING.md).

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>

#include <wrl/client.h>
#include <imgui.h>

#include "hooks/D3D11Hook.hpp"
#include "hooks/D3D12Hook.hpp"
#include "hooks/WindowsMessageHook.hpp"

class Mods;
class IEngineAdapter;

class Framework {
public:
    enum class RendererType : uint8_t { D3D11, D3D12 };

    explicit Framework(HMODULE framework_module);
    virtual ~Framework();

    // --- lifecycle / readiness ------------------------------------------------
    bool is_valid() const { return m_valid; }
    bool is_ready() const { return m_initialized && m_engine_ready; }
    bool is_dx11() const { return m_renderer_type == RendererType::D3D11; }
    bool is_dx12() const { return m_renderer_type == RendererType::D3D12; }
    auto get_renderer_type() const { return m_renderer_type; }

    // The engine adapter signals when game data is up so mods can fully initialize.
    void enable_engine_thread() { m_engine_ready = true; }

    // --- the per-engine adapter (Layer 2) ------------------------------------
    void set_engine_adapter(std::shared_ptr<IEngineAdapter> adapter) { m_engine_adapter = std::move(adapter); }
    const std::shared_ptr<IEngineAdapter>& get_engine_adapter() const { return m_engine_adapter; }

    // --- imgui / overlay ------------------------------------------------------
    void run_imgui_frame(bool from_present);
    bool is_drawing_ui() const { return m_draw_ui; }
    void set_draw_ui(bool state, bool should_save = true);
    bool is_ui_focused() const { return m_is_ui_focused; }

    // --- input ----------------------------------------------------------------
    const auto& get_mouse_delta() const { return m_mouse_delta; }
    const auto& get_keyboard_state() const { return m_last_keys; }
    bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param);
    void on_direct_input_keys(const std::array<uint8_t, 256>& keys);

    // --- window / hooks -------------------------------------------------------
    auto get_window() const { return m_wnd; }
    auto& get_d3d11_hook() const { return m_d3d11_hook; }
    auto& get_d3d12_hook() const { return m_d3d12_hook; }

    // --- render targets (the ports read these for HUD/overlay sizing) ---------
    auto get_rendertarget_width_d3d11() const { return m_d3d11.rt_width; }
    auto get_rendertarget_height_d3d11() const { return m_d3d11.rt_height; }
    auto get_rendertarget_width_d3d12() const { return m_d3d12.rt_width; }
    auto get_rendertarget_height_d3d12() const { return m_d3d12.rt_height; }

    // --- config persistence ---------------------------------------------------
    void request_save_config() { m_wants_save_config = true; }
    static std::filesystem::path get_persistent_dir();
    static std::filesystem::path get_persistent_dir(const std::string& dir) { return get_persistent_dir() / dir; }

    static auto get_framework_module() { return s_framework_module; }

public:
    bool hook_d3d11();
    bool hook_d3d12();

private:
    bool initialize();
    void save_config();
    void consume_input();
    void draw_ui();

    static inline HMODULE s_framework_module{};

    bool m_valid{ false };
    bool m_initialized{ false };
    std::atomic<bool> m_engine_ready{ false };  // set by the adapter (enable_engine_thread)

    bool m_draw_ui{ true };
    bool m_is_ui_focused{ false };
    bool m_wants_save_config{ false };

    HWND m_wnd{ 0 };
    float m_mouse_delta[2]{};
    std::array<uint8_t, 256> m_last_keys{ 0 };

    RendererType m_renderer_type{ RendererType::D3D12 };

    std::unique_ptr<D3D11Hook> m_d3d11_hook{};
    std::unique_ptr<D3D12Hook> m_d3d12_hook{};
    std::unique_ptr<WindowsMessageHook> m_windows_message_hook{};

    std::shared_ptr<IEngineAdapter> m_engine_adapter{};
    std::unique_ptr<Mods> m_mods{};

    std::recursive_mutex m_config_mtx{};
    std::recursive_mutex m_imgui_mtx{};

    // Minimal per-API render-target bookkeeping (full version: see REFramework.hpp).
    struct D3D11 { uint32_t rt_width{}, rt_height{}; } m_d3d11{};
    struct D3D12 { uint32_t rt_width{}, rt_height{}; } m_d3d12{};
};

extern std::unique_ptr<Framework> g_framework;
