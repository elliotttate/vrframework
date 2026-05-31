#include "Framework.hpp"

// PORT FROM: REFramework/src/REFramework.cpp (class renamed REFramework -> Framework).
// STUB. Lift: d3d hook setup, imgui init/overlay, input consume, config save/load,
// and the first-frame init that calls Mods::on_initialize. Drop initialize_game_data()
// (RE reflection) — readiness is signaled by the adapter via enable_engine_thread().

#include "Mods.hpp"
#include "spi/IEngineAdapter.hpp"
#include "spi/FrameTimeline.hpp"
#include "mods/VR.hpp"

#include <shlobj.h>
#include <spdlog/spdlog.h>

std::unique_ptr<Framework> g_framework{};

Framework::Framework(HMODULE framework_module) {
    s_framework_module = framework_module;
    spdlog::info("[vrframework] Framework constructed");

    // PORT: detect renderer, hook d3d, hook window messages, init imgui.
    // Minimal scaffold so consumers can link and iterate:
    m_mods = std::make_unique<Mods>();   // Mods::Mods() comes from the consuming repo

    // Wire the adapter's frame timeline to VR + imgui, if an adapter was set.
    if (m_engine_adapter) {
        FrameTimeline::Callbacks cb{};
        auto vr = VR::get();
        cb.on_wait_rendering   = [vr](uint32_t f) { vr->on_wait_rendering(f); };
        cb.on_begin_rendering  = [vr](uint32_t f) { vr->on_begin_rendering(f); };
        cb.on_update_hmd_state = [vr](uint32_t f) { vr->update_hmd_state(f); };
        cb.on_request_imgui    = [this] { run_imgui_frame(false); };
        m_engine_adapter->timeline().set_callbacks(std::move(cb));
    }

    m_valid = true;
    m_initialized = true;
}

Framework::~Framework() = default;

bool Framework::initialize() { return true; }      // PORT
bool Framework::hook_d3d11() { return false; }     // PORT
bool Framework::hook_d3d12() { return false; }     // PORT

void Framework::run_imgui_frame(bool from_present) {
    // PORT: REFramework::run_imgui_frame — new frame, draw_ui(), mods->on_frame().
    if (!is_ready() || !m_mods) return;
    m_mods->on_pre_imgui_frame();
    m_mods->on_frame();
    if (m_draw_ui) m_mods->on_draw_ui();
}

void Framework::set_draw_ui(bool state, bool should_save) {
    m_draw_ui = state;
    if (should_save) request_save_config();
}

bool Framework::on_message(HWND wnd, UINT message, WPARAM w, LPARAM l) {
    if (!m_mods) return true;
    bool keep = true;
    for (const auto& mod : m_mods->get_mods()) keep &= mod->on_message(wnd, message, w, l);
    return keep;
}

void Framework::on_direct_input_keys(const std::array<uint8_t, 256>& keys) { m_last_keys = keys; }
void Framework::consume_input() {}                 // PORT
void Framework::draw_ui() {}                        // PORT

void Framework::save_config() {
    if (!m_mods) return;
    utility::Config cfg{};
    m_mods->on_config_save(cfg);
    cfg.save((get_persistent_dir() / "vrframework_config.txt").string());
    m_wants_save_config = false;
}

std::filesystem::path Framework::get_persistent_dir() {
    wchar_t path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::filesystem::path{ path } / "vrframework";
    }
    return std::filesystem::current_path();
}
