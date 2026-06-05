// PORT FROM: REFramework/src/REFramework.cpp (class renamed REFramework -> Framework).
//
// This is the engine-agnostic "own the frame" spine (Layer 1). Faithfully ported and
// STRIPPED of all RE-Engine coupling:
//   * sdk/GameIdentity, RE reflection, initialize_game_data(), DInputHook, REFrameworkConfig,
//     d3d12::CommandContext, DirectX::GraphicsMemory, ImNodes/font machinery, the
//     hook-monitor jthread, and the heavy ImGui_ImplDX12 overlay render are removed.
//   * Engine readiness is no longer hard-coded reflection: it is signalled by the adapter
//     via enable_engine_thread() (m_engine_ready), and is_ready() == m_initialized &&
//     m_engine_ready.
//   * The frame state machine (60-frame stall, initialize() -> first_frame_initialize() ->
//     mods->on_present()), the present/resize callback wiring, input/menu-key handling, and
//     deferred config save are kept verbatim in shape.
//
// Cross-references that remain as forward declares / seams owned by other subagents:
//   * VR (mods/VR.hpp) — frame-timeline callbacks + is_hmd_active(); owned by the VR agent.
//   * The actual per-game mod list (Mods::Mods()) — owned by the consuming repo.
//   * ImGui device-object init / per-eye overlay RT — owned by the VR/render agent.

#include "Framework.hpp"

#include "Mods.hpp"

#include <algorithm>
#include <fstream>
#include <thread>

#include <shlobj.h>
#include <spdlog/spdlog.h>

#include <imgui.h>

// Provided by the imgui win32 backend (the render/VR agent links it). Forward-declared so
// the spine can pump win32 input without owning the backend header/location.
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
IMGUI_IMPL_API bool    ImGui_ImplWin32_Init(void* hwnd);
IMGUI_IMPL_API void    ImGui_ImplWin32_NewFrame();

std::unique_ptr<Framework> g_framework{};

namespace fs = std::filesystem;

Framework::Framework(HMODULE framework_module) {
    s_framework_module = framework_module;

    spdlog::info("vrframework entry");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Almost nothing happens here on purpose — real init is a frame-driven state machine
    // (see initialize()), because D3D / the game window may not exist yet at load time.
    m_mods = std::make_unique<Mods>();   // Mods::Mods() comes from the consuming repo

    m_valid = true;
}

Framework::~Framework() {
    ImGui::DestroyContext();
}

// ---------------------------------------------------------------------------
// Hook installation — wire the D3D present/resize callbacks to our frame entry points.
// ---------------------------------------------------------------------------
bool Framework::hook_d3d11() {
    m_d3d11_hook.reset();
    m_d3d11_hook = std::make_unique<D3D11Hook>();
    m_d3d11_hook->on_present([this](D3D11Hook&) { on_frame_d3d11(); });
    m_d3d11_hook->on_post_present([this](D3D11Hook&) { on_post_present_d3d11(); });
    m_d3d11_hook->on_resize_buffers([this](D3D11Hook&) { on_reset(); });

    if (!m_is_d3d12) {
        if (m_d3d11_hook->hook()) {
            spdlog::info("Hooked DirectX 11");
            m_valid = true;
            m_is_d3d11 = true;
            m_renderer_type = RendererType::D3D11;
            return true;
        }

        m_d3d11_hook->unhook();
        m_valid = false;
        m_is_d3d11 = false;
        return false;
    }

    return false;
}

bool Framework::hook_d3d12() {
    // Windows 7 / no-D3D12 fallback.
    if (LoadLibraryA("d3d12.dll") == nullptr) {
        spdlog::info("d3d12.dll not found, falling back to hooking D3D11.");
        m_is_d3d12 = false;
        return hook_d3d11();
    }

    m_d3d12_hook.reset();
    m_d3d12_hook = std::make_unique<D3D12Hook>();
    m_d3d12_hook->on_present([this](D3D12Hook&) { on_frame_d3d12(); });
    m_d3d12_hook->on_post_present([this](D3D12Hook&) { on_post_present_d3d12(); });
    m_d3d12_hook->on_resize_buffers([this](D3D12Hook&) { on_reset(); });
    m_d3d12_hook->on_resize_target([this](D3D12Hook&) { on_reset(); });

    if (!m_is_d3d11) {
        if (m_d3d12_hook->hook()) {
            spdlog::info("Hooked DirectX 12");
            m_valid = true;
            m_is_d3d12 = true;
            m_renderer_type = RendererType::D3D12;
            return true;
        }

        m_d3d12_hook->unhook();
        m_valid = false;
        m_is_d3d12 = false;

        // Try to hook D3D11 instead.
        return hook_d3d11();
    }

    return false;
}

// ---------------------------------------------------------------------------
// Frame heartbeat — the present-hook loop / readiness state machine.
// ---------------------------------------------------------------------------
void Framework::on_frame_d3d12() {
    std::scoped_lock _{ m_imgui_mtx };

    m_renderer_type = RendererType::D3D12;

    auto command_queue = m_d3d12_hook->get_command_queue();

    if (!m_initialized) {
        if (!initialize()) {
            return;
        }

        spdlog::info("vrframework initialized");
        m_initialized = true;
        return;
    }

    if (command_queue == nullptr) {
        spdlog::error("Null Command Queue");
        return;
    }

    if (m_message_hook_requested) {
        initialize_windows_message_hook();
    }

    auto device = m_d3d12_hook->get_device();

    if (device == nullptr) {
        spdlog::error("D3D12 Device was null when it shouldn't be, returning...");
        m_initialized = false;
        return;
    }

    bool is_init_ok = m_error.empty() && m_engine_ready;

    if (is_init_ok) {
        // Write default config once if it doesn't exist (so a frontend can read defaults).
        if (!std::exchange(m_created_default_cfg, true)) {
            if (!fs::exists(get_persistent_dir() / "vrframework_config.txt")) {
                save_config();
            }
        }
    }

    is_init_ok = first_frame_initialize();

    if (!m_has_frame) {
        if (!is_init_ok) {
            // Hooks don't run until after init; just pump imgui while initializing.
            run_imgui_frame(true);
            return;
        }
    }

    if (is_init_ok) {
        m_mods->on_present();
        m_mods->on_post_frame();
    }

    // NOTE: the actual ImGui_ImplDX12 overlay submission (descriptor heaps / per-eye RTV /
    // command-list barriers) is intentionally NOT in the engine-neutral spine — it lives in
    // the render/VR component (owned by another agent), which reads get_d3d12_hook() here.
}

void Framework::on_post_present_d3d12() {
    if (!m_error.empty() || !m_initialized || !m_engine_ready) {
        return;
    }

    for (const auto& mod : m_mods->get_mods()) {
        mod->on_post_present();
    }
}

void Framework::on_frame_d3d11() {
    std::scoped_lock _{ m_imgui_mtx };

    m_renderer_type = RendererType::D3D11;

    if (!m_initialized) {
        if (!initialize()) {
            return;
        }

        spdlog::info("vrframework initialized");
        m_initialized = true;
        return;
    }

    if (m_message_hook_requested) {
        initialize_windows_message_hook();
    }

    bool is_init_ok = first_frame_initialize();

    if (!m_has_frame) {
        if (!is_init_ok) {
            run_imgui_frame(true);
            return;
        }
    }

    if (is_init_ok) {
        m_mods->on_present();
        m_mods->on_post_frame();
    }
}

void Framework::on_post_present_d3d11() {
    if (!m_error.empty() || !m_initialized || !m_engine_ready) {
        return;
    }

    for (const auto& mod : m_mods->get_mods()) {
        mod->on_post_present();
    }
}

void Framework::on_reset() {
    std::scoped_lock _{ m_imgui_mtx };

    spdlog::info("Reset!");

    // Every backbuffer-derived resource is now stale; tell mods to drop them.
    if (m_engine_ready) {
        m_mods->on_device_reset();
    }

    m_has_frame = false;
    m_first_initialize = false;
    m_initialized = false;
}

// ---------------------------------------------------------------------------
// Initialization (frame-driven).
// ---------------------------------------------------------------------------
bool Framework::initialize() {
    if (m_initialized) {
        return true;
    }

    if (m_first_initialize) {
        m_frames_since_init = 0;
        m_first_initialize = false;
    }

    // Let the engine/swapchain settle for ~60 frames before binding device objects.
    if (m_frames_since_init < 60) {
        m_frames_since_init++;
        return false;
    }

    if (m_is_d3d12) {
        if (!m_d3d12_hook->is_hooked()) {
            return false;
        }

        auto device = m_d3d12_hook->get_device();
        auto swap_chain = m_d3d12_hook->get_swap_chain();

        if (device == nullptr || swap_chain == nullptr) {
            m_first_initialize = true;
            spdlog::info("D3D12 device/swapchain null; unhooking and trying D3D11...");

            m_d3d12_hook->unhook();
            m_valid = false;
            m_is_d3d12 = false;
            hook_d3d11();
            return false;
        }

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        swap_chain->GetDesc(&swap_desc);
        m_wnd = swap_desc.OutputWindow;
    } else if (m_is_d3d11) {
        if (!m_d3d11_hook->is_hooked()) {
            return false;
        }

        auto device = m_d3d11_hook->get_device();
        auto swap_chain = m_d3d11_hook->get_swap_chain();

        if (device == nullptr || swap_chain == nullptr) {
            m_first_initialize = true;
            spdlog::info("D3D11 device/swapchain null; unhooking and trying D3D12...");

            m_d3d11_hook->unhook();
            m_valid = false;
            m_is_d3d11 = false;
            hook_d3d12();
            return false;
        }

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        swap_chain->GetDesc(&swap_desc);
        m_wnd = swap_desc.OutputWindow;
    } else {
        return false;
    }

    spdlog::info("Window Handle: {:x}", (uintptr_t)m_wnd);

    if (!ImGui_ImplWin32_Init(m_wnd)) {
        spdlog::error("Failed to initialize ImGui ImplWin32.");
        return false;
    }

    initialize_windows_message_hook();

    if (m_first_frame) {
        m_first_frame = false;

        // Engine-neutral: instead of RE's initialize_game_data() reflection thread, run the
        // mods' on_initialize() now. The engine adapter (a Mod) flips enable_engine_thread()
        // from here once its address library / hooks are up, which gates is_ready().
        if (auto e = m_mods->on_initialize(); e) {
            m_error = *e;
            spdlog::error("Mods on_initialize failed: {}", m_error);
        }
    }

    return true;
}

bool Framework::initialize_windows_message_hook() {
    if (m_wnd == 0) {
        return false;
    }

    m_windows_message_hook.reset();
    m_windows_message_hook = std::make_unique<WindowsMessageHook>(m_wnd);
    m_windows_message_hook->on_message([this](auto wnd, auto msg, auto w, auto l) {
        return on_message(wnd, msg, w, l);
    });

    m_message_hook_requested = false;
    return true;
}

// Runs on the render thread, on the first good present after the engine is ready. This is
// where mods do their D3D-thread setup (VR runtime swapchains, textures, etc.).
bool Framework::first_frame_initialize() {
    const bool is_init_ok = m_error.empty() && m_engine_ready;

    if (!is_init_ok || !m_first_frame_d3d_initialize) {
        return is_init_ok;
    }

    spdlog::info("Running first frame D3D initialization of mods...");

    m_first_frame_d3d_initialize = false;

    if (auto e = m_mods->on_initialize_d3d_thread(); e) {
        m_error = e->empty() ? "An unknown error has occurred." : *e;
        spdlog::error("Initialization of mods failed. Reason: {}", m_error);
        m_engine_ready = false;
        m_mods_fully_initialized = false;
        return false;
    }

    // Save defaults out once for the frontend.
    save_config();
    m_mods_fully_initialized = true;
    m_has_frame = true;
    return true;
}

void Framework::call_on_frame() {
    if (m_error.empty() && m_engine_ready) {
        m_mods->on_frame();
    }
}

// ---------------------------------------------------------------------------
// ImGui frame / overlay.
// ---------------------------------------------------------------------------
void Framework::run_imgui_frame(bool from_present) {
    std::scoped_lock _{ m_imgui_mtx };

    if (!m_initialized) {
        return;
    }

    const bool is_init_ok = m_error.empty() && m_engine_ready;

    consume_input();

    ImGui_ImplWin32_NewFrame();

    // from_present guards against running game/script code on the present thread.
    if (is_init_ok && !from_present) {
        m_mods->on_pre_imgui_frame();
    }

    ImGui::NewFrame();

    if (!from_present) {
        call_on_frame();
    }

    draw_ui();

    ImGui::EndFrame();
    ImGui::Render();

    if (!from_present && m_wants_save_config) {
        save_config();
        m_wants_save_config = false;
    }
}

void Framework::draw_ui() {
    if (!m_draw_ui) {
        return;
    }

    if (m_error.empty() && m_engine_ready) {
        m_mods->on_draw_ui();
    }
}

void Framework::set_draw_ui(bool state, bool should_save) {
    std::scoped_lock _{ m_config_mtx };

    const bool prev_state = m_draw_ui;
    m_draw_ui = state;

    if (state != prev_state && should_save && m_engine_ready) {
        request_save_config();
    }
}

// ---------------------------------------------------------------------------
// Input / windows messages.
// ---------------------------------------------------------------------------
bool Framework::on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    if (!m_initialized) {
        return true;
    }

    // Track mouse-button / key state for ModKey and the menu toggle.
    switch (message) {
    case WM_LBUTTONDOWN: m_last_keys[VK_LBUTTON] = true; break;
    case WM_LBUTTONUP:   m_last_keys[VK_LBUTTON] = false; break;
    case WM_RBUTTONDOWN: m_last_keys[VK_RBUTTON] = true; break;
    case WM_RBUTTONUP:   m_last_keys[VK_RBUTTON] = false; break;
    case WM_MBUTTONDOWN: m_last_keys[VK_MBUTTON] = true; break;
    case WM_MBUTTONUP:   m_last_keys[VK_MBUTTON] = false; break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (w_param < 256) m_last_keys[w_param] = true;
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (w_param < 256) m_last_keys[w_param] = false;
        break;
    case WM_KILLFOCUS:
        // Drop stuck keys when the window loses focus.
        std::fill(std::begin(m_last_keys), std::end(m_last_keys), (uint8_t)0);
        break;
    default:
        break;
    }

    // Let the mods inspect the message (any returning false swallows it from the game).
    bool keep = true;
    if (m_engine_ready) {
        for (const auto& mod : m_mods->get_mods()) {
            keep &= mod->on_message(wnd, message, w_param, l_param);
        }
    }

    // Feed ImGui and arbitrate keyboard/mouse capture when the menu is open.
    if (m_initialized && ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplWin32_WndProcHandler(wnd, message, w_param, l_param);

        const auto& io = ImGui::GetIO();
        m_is_ui_focused = m_draw_ui && (io.WantCaptureKeyboard || io.WantCaptureMouse);

        if (m_draw_ui && (io.WantCaptureKeyboard || io.WantCaptureMouse)) {
            // Swallow input from the game while the user is interacting with the overlay.
            keep = false;
        }
    }

    return keep;
}

void Framework::on_direct_input_keys(const std::array<uint8_t, 256>& keys) {
    m_last_keys = keys;
}

void Framework::consume_input() {
    m_mouse_delta[0] = m_accumulated_mouse_delta[0];
    m_mouse_delta[1] = m_accumulated_mouse_delta[1];

    m_accumulated_mouse_delta[0] = 0.0f;
    m_accumulated_mouse_delta[1] = 0.0f;
}

// ---------------------------------------------------------------------------
// Config persistence (deferred off the present thread).
// ---------------------------------------------------------------------------
void Framework::save_config() {
    std::scoped_lock _{ m_config_mtx };

    if (!m_mods) {
        return;
    }

    spdlog::info("Saving config vrframework_config.txt");

    utility::Config cfg{};
    for (const auto& mod : m_mods->get_mods()) {
        mod->on_config_save(cfg);
    }

    try {
        if (!cfg.save((get_persistent_dir() / "vrframework_config.txt").string())) {
            spdlog::error("Failed to save config");
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
    } catch (...) {
        spdlog::error("Unexpected error while saving config");
    }

    m_wants_save_config = false;
}

std::filesystem::path Framework::get_persistent_dir() {
    // Engine-neutral: prefer next to the game exe; fall back to %APPDATA%/vrframework/<exe>.
    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(GetModuleHandle(nullptr), exe_path, MAX_PATH) != 0) {
        std::error_code ec{};
        const auto exe = std::filesystem::path{ exe_path };
        const auto dir = exe.parent_path();

        // Permission probe: if we can write next to the exe, use that.
        try {
            const auto test_file = dir / "vrframework_test.txt";
            std::ofstream test_stream{ test_file };
            if (test_stream.good()) {
                test_stream << "test";
                test_stream.close();
                std::filesystem::remove(test_file, ec);
                return dir;
            }
        } catch (...) {
            // fall through to appdata
        }

        // Fall back to %APPDATA%/vrframework/<exe stem>.
        wchar_t appdata[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
            const auto out = std::filesystem::path{ appdata } / "vrframework" / exe.stem();
            std::filesystem::create_directories(out, ec);
            return out;
        }
    }

    return std::filesystem::current_path();
}
