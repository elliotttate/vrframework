#pragma once

// PORT FROM: REFramework/src/Mods.hpp / Mods.cpp
// The core owns the iteration logic; the CONSUMER provides the constructor body
// (which mods exist) per game — exactly as the ports do in their ModConfig.cpp:
//
//   Mods::Mods() {
//       m_mods.emplace_back(VRConfig::get());
//       m_mods.emplace_back(VR::get());
//       m_mods.emplace_back(EngineEntry::Get());   // <- the IEngineAdapter mod
//   }

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Mod.hpp"

class Mods {
public:
    Mods();  // defined by the consuming repo
    virtual ~Mods() {}

    std::optional<std::string> on_initialize() const;
    std::optional<std::string> on_initialize_d3d_thread() const;

    void on_pre_imgui_frame() const;
    void on_frame() const;
    void on_present() const;
    void on_post_frame() const;
    void on_draw_ui() const;
    void on_device_reset() const;
    void on_config_load(const utility::Config& cfg, bool set_defaults) const;
    void on_config_save(utility::Config& cfg) const;

    const auto& get_mods() const { return m_mods; }

protected:
    std::vector<std::shared_ptr<Mod>> m_mods;
};
