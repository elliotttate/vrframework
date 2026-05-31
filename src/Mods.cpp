#include "Mods.hpp"

// NOTE: Mods::Mods() (which mods exist) is provided by the CONSUMING repo per game.
// The core owns only the iteration/dispatch below. See PORTING.md and the ports'
// ModConfig.cpp for the constructor.

std::optional<std::string> Mods::on_initialize() const {
    for (const auto& mod : m_mods) {
        if (auto e = mod->on_initialize()) return e;
    }
    return std::nullopt;
}

std::optional<std::string> Mods::on_initialize_d3d_thread() const {
    for (const auto& mod : m_mods) {
        if (auto e = mod->on_initialize_d3d_thread()) return e;
    }
    return std::nullopt;
}

void Mods::on_pre_imgui_frame() const { for (const auto& m : m_mods) m->on_pre_imgui_frame(); }
void Mods::on_frame() const { for (const auto& m : m_mods) m->on_frame(); }
void Mods::on_present() const { for (const auto& m : m_mods) m->on_present(); }
void Mods::on_post_frame() const { for (const auto& m : m_mods) m->on_post_frame(); }
void Mods::on_draw_ui() const { for (const auto& m : m_mods) m->on_draw_ui(); }
void Mods::on_device_reset() const { for (const auto& m : m_mods) m->on_device_reset(); }

void Mods::on_config_load(const utility::Config& cfg, bool set_defaults) const {
    for (const auto& m : m_mods) m->on_config_load(cfg, set_defaults);
}
void Mods::on_config_save(utility::Config& cfg) const {
    for (const auto& m : m_mods) m->on_config_save(cfg);
}
