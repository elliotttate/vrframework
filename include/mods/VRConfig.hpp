#pragma once

// PORT FROM: REFramework/src/mods/vr/* config bits (engine-agnostic VR options mod).
// A plain settings mod (IPD scale, world scale, snap-turn, render scale, runtime
// preference). Registered first in Mods::Mods() so VR reads it on init.

#include <memory>
#include "Mod.hpp"

class VRConfig : public Mod {
public:
    static std::shared_ptr<VRConfig>& get();

    std::string_view get_name() const override { return "VR_Config"; }
    void on_draw_ui() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

    float world_scale() const { return *m_world_scale; }
    bool  snap_turn() const { return *m_snap_turn; }

private:
    const ModSlider::Ptr m_world_scale{ ModSlider::create("VR_WorldScale", 0.1f, 10.0f, 1.0f) };
    const ModToggle::Ptr m_snap_turn{ ModToggle::create("VR_SnapTurn", true) };
    ValueList m_options{ *m_world_scale, *m_snap_turn };
};
