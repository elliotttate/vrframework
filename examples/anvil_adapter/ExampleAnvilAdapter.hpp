#pragma once

// Example only — illustrates the IEngineAdapter contract using Anvil's real shape.
// Lives under examples/ and is NOT compiled into the core library.

#include <spi/IEngineAdapter.hpp>
#include <Mod.hpp>

class ExampleAnvilAdapter : public IEngineAdapter {
public:
    static std::shared_ptr<ExampleAnvilAdapter>& get() {
        static auto instance = std::make_shared<ExampleAnvilAdapter>();
        return instance;
    }

    std::string_view get_name() const override { return "ACValhalla"; }

    EngineCaps capabilities() const override;
    bool install_hooks() override;
    FrameTimeline& timeline() override { return m_timeline; }
    void apply_stereo(const StereoView& view) override;
    glm::mat4 get_world_camera() const override;
    void reproject_hud(float scale_x, float scale_y) override;
    void disable_incompatible_effects() override;

    // Mod surface — settings UI, same as anvil's EngineEntry.
    std::optional<std::string> on_initialize() override;
    void on_draw_ui() override;

private:
    FrameTimeline m_timeline{};

    const ModSlider::Ptr m_hud_scale_x{ ModSlider::create("ACValhalla_HudScaleX", 0.1f, 1.0f, 0.4f) };
    const ModSlider::Ptr m_hud_scale_y{ ModSlider::create("ACValhalla_HudScaleY", 0.1f, 1.0f, 0.21f) };
    const ModToggle::Ptr m_decoupled_pitch{ ModToggle::create("ACValhalla_DecoupledPitch", false) };
    ValueList m_options{ *m_hud_scale_x, *m_hud_scale_y, *m_decoupled_pitch };
};
