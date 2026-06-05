#include <spdlog/spdlog.h>

#include "Framework.hpp"
#include "Mods.hpp"

// PORT FROM: REFramework/src/Mods.cpp — ITERATION LOGIC ONLY.
//
// NOTE: Mods::Mods() (which mods exist) is provided by the CONSUMING repo per game.
// The core owns only the iteration/dispatch below. See PORTING.md and the ports'
// ModConfig.cpp for the constructor, e.g.:
//
//   Mods::Mods() {
//       m_mods.emplace_back(VRConfig::get());
//       m_mods.emplace_back(VR::get());
//       m_mods.emplace_back(EngineEntry::Get());   // <- the IEngineAdapter mod
//   }

// The config file name the core loads/saves through. (REFramework keyed this off
// REFrameworkConfig::REFRAMEWORK_CONFIG_NAME; here it is a single engine-neutral name.)
static constexpr const char* VRFRAMEWORK_CONFIG_NAME = "vrframework_config.txt";

std::optional<std::string> Mods::on_initialize() const {
    for (const auto& mod : m_mods) {
        spdlog::info("{:s}::on_initialize()", mod->get_name().data());

        if (auto e = mod->on_initialize(); e != std::nullopt) {
            spdlog::info("{:s}::on_initialize() has failed: {:s}", mod->get_name().data(), *e);
            return e;
        }
    }

    utility::Config cfg{ (Framework::get_persistent_dir() / VRFRAMEWORK_CONFIG_NAME).string() };

    for (const auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg, true);
    }

    return std::nullopt;
}

std::optional<std::string> Mods::on_initialize_d3d_thread() const {
    utility::Config cfg{ (Framework::get_persistent_dir() / VRFRAMEWORK_CONFIG_NAME).string() };

    // Load once up-front so values exist before device-thread init.
    for (const auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg, true);
    }

    for (const auto& mod : m_mods) {
        spdlog::info("{:s}::on_initialize_d3d_thread()", mod->get_name().data());

        if (auto e = mod->on_initialize_d3d_thread(); e != std::nullopt) {
            spdlog::info("{:s}::on_initialize_d3d_thread() has failed: {:s}", mod->get_name().data(), *e);
            return e;
        }
    }

    // Load again so mods that constructed options during init still pick up saved values
    // (do NOT reset to defaults this time — preserve what was read above).
    for (const auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg, false);
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
