#pragma once

// FH5 (ForzaTech, D3D12) engine adapter for vrframework.
//
// FH5's per-frame view/projection PRODUCER is sub_140BB1EE0 (Empress RVA 0xBB1EE0) — the ForzaTech
// equivalent of Anvil's onCalcFinalView+onCalcProjection. It builds VIEW/VP from matrix inputs:
//   a4  = camera-to-world orientation (orthonormal basis in rows 0-2, camera-relative origin in ROW 3)
//   a7  = projection (reverse-Z; near at [3][2])
//   a8/a9 = near (0.1) / far (50000) — our main-camera identity gate
// We hook it (AOB on the prologue) and, when VR is active, OVERWRITE a4 + a7 with the per-eye matrices
// the core hands us via apply_stereo(). Proven live (flat 6-DOF freecam: pitch/yaw/roll + translate,
// culling-correct). See E:\SteamLibrary\...\FH5CameraProbe (memory: fh5-upstream-hook-pivot).

#include <spi/IEngineAdapter.hpp>
#include <Mod.hpp>
#include <memory>
#include <atomic>
#include <cstdint>

class Fh5Adapter : public IEngineAdapter {
public:
    static std::shared_ptr<Fh5Adapter>& get() {
        static auto instance = std::make_shared<Fh5Adapter>();
        return instance;
    }

    std::string_view get_name() const override { return "ForzaHorizon5"; }

    EngineCaps capabilities() const override;
    bool install_hooks() override;
    FrameTimeline& timeline() override { return m_timeline; }
    void apply_stereo(const StereoView& view) override;
    glm::mat4 get_world_camera() const override;
    void reproject_hud(float scale_x, float scale_y) override;
    void disable_incompatible_effects() override;

    std::optional<std::string> on_initialize() override;
    void on_draw_ui() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

private:
    FrameTimeline m_timeline{};

    const ModSlider::Ptr m_ipd_scale{ ModSlider::create("FH5_IpdScale", 0.1f, 3.0f, 1.0f) };
    // FH5 world units are ~cm (~100 units/metre, from the freecam needing ~120 units to exit a ~1.2m
    // cockpit). FH5_WorldScale = units per metre; converts OpenXR-metre head translation -> FH5 units.
    const ModSlider::Ptr m_world_scale{ ModSlider::create("FH5_WorldScale", 10.0f, 400.0f, 100.0f) };
    const ModToggle::Ptr m_disable_taa{ ModToggle::create("FH5_DisableTAA", true) };
    ValueList m_options{ *m_ipd_scale, *m_world_scale, *m_disable_taa };
};

// Producer-hook diagnostics, exposed for the menu navigator's flow-state publisher (Fh5MenuNav). The
// producer fires many times per frame; main_hits counts ONLY the gameplay-camera passes (near~0.1, far>2000),
// so its rate of change tells the orchestrator whether a 3D gameplay camera is actively rendering. last_near/
// last_far are the most recent gameplay-camera planes (scene classification: world vs showcase).
namespace fh5diag {
uint64_t producer_calls();
uint64_t producer_main_hits();
float    last_near();
float    last_far();
uint64_t last_showcase_ms();   // GetTickCount64 ms of the last far>30000 main frame (intro/showcase recency)
uint64_t last_world_ms();      // GetTickCount64 ms of the last far in (2000,30000] main frame (world recency)
int      applied_eye();
uint64_t applied_eye_ms();
uint64_t engaged_hits();
uint64_t view_writes();
uint64_t projection_writes();
} // namespace fh5diag
