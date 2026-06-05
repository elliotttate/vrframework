#pragma once

// PORT FROM: REFramework/src/mods/VR.hpp (+ D3D12Component for submit).
// KEPT: the pose/projection/eye/frame-counter surface the engine adapter consumes, the
// runtime sync (WaitGetPoses == xrWaitFrame), and the on_present/on_post_present submit
// path through D3D12Component. CUT: every RE-Engine camera/scene hook (on_pre_application_
// entry, RE8VR, RE camera controllers) — those are now IEngineAdapter callbacks.
//
// Stereo injection goes out through IEngineAdapter::apply_stereo(const StereoView&): each
// engine frame the core picks the eye from frame parity (AFR), computes that eye's view +
// projection from the runtime pose + IPD + EngineCaps basis, fills a StereoView, and hands
// it to the adapter, which writes it into the engine's camera.

#include <array>
#include <memory>

#include "Mod.hpp"
#include "Math.hpp"
#include "vr/VRRuntime.hpp"
#include "vr/runtimes/OpenXR.hpp"
#include "mods/vr/D3D12Component.hpp"

class VR : public Mod {
public:
    static std::shared_ptr<VR>& get();

    std::string_view get_name() const override { return "VR"; }
    std::optional<std::string> on_initialize() override;
    std::optional<std::string> on_initialize_d3d_thread() override;
    void on_draw_ui() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

    // Present-path hooks (wired from Framework's D3D12Hook on_present / on_post_present).
    void on_frame() override;        // BeginRendering edge — safe-ish, used for imgui pacing
    void on_present() override;      // the actual present — drives the stereo submit
    void on_post_present() override;
    void on_device_reset() override;

    // --- runtime access -------------------------------------------------------
    VRRuntime* get_runtime() const { return m_runtime.get(); }
    bool is_hmd_active() const { return get_runtime() && get_runtime()->ready(); }

    auto get_hmd_width()  const { return get_runtime() ? get_runtime()->get_width()  : 0u; }
    auto get_hmd_height() const { return get_runtime() ? get_runtime()->get_height() : 0u; }

    // --- poses & transforms (consumed by the engine camera adapter) -----------
    Matrix4x4f get_rotation(uint32_t index) const;        // index 0 == HMD
    Matrix4x4f get_transform(uint32_t index) const;
    Matrix4x4f get_current_eye_transform(bool flip = false);
    Matrix4x4f get_transform_offset() const;              // recenter/snap-turn offset
    Quaternion get_rotation_offset() const;
    VRRuntime::Eye get_current_render_eye() const { return m_render_eye; }

    const auto& get_eyes() const { return get_runtime()->eyes; }
    const auto& get_raw_projections() const { return get_runtime()->raw_projections; }

    // --- frame lifecycle (called by the engine adapter via FrameTimeline) -----
    void on_wait_rendering(uint32_t frame);
    void on_begin_rendering(uint32_t frame);
    void update_hmd_state(uint32_t frame);

    // --- frame counters (the ports read/write these directly) -----------------
    uint32_t m_engine_frame_count{ 0 };
    uint32_t m_render_frame_count{ 0 };
    uint32_t m_presenter_frame_count{ 0 };
    bool     m_skip_next_present{ false };

    // AFR parity->eye mapping (mutable so we can resync after a dropped frame — guide 10 §8).
    uint32_t m_left_eye_interval{ 0 };
    uint32_t m_right_eye_interval{ 1 };

    bool m_submitted{ false };

    // The concrete OpenXR runtime (D3D12Component talks to it directly, mirroring REFramework).
    std::shared_ptr<runtimes::OpenXR> m_openxr{};

    // Optional: gamepad passthrough the consumer can specialize (the ports define it).
    void on_xinput_get_state(uint32_t* retval, uint32_t user_index, struct _XINPUT_STATE* state);

private:
    std::optional<std::string> initialize_openxr();
    std::optional<std::string> initialize_openxr_swapchains();

    // Compute this eye's StereoView and hand it to the engine adapter.
    void push_stereo_to_adapter();

    std::shared_ptr<VRRuntime> m_runtime{ std::make_shared<VRRuntime>() };
    VRRuntime::Eye m_render_eye{ VRRuntime::Eye::LEFT };
    Matrix4x4f m_rotation_offset{ 1.0f };

    vrmod::D3D12Component m_d3d12{};

    bool m_init_finished{ false };
    bool m_wants_swapchains{ false };

    // Engine depth range used to build projections (filled from EngineCaps).
    float m_nearz{ 0.01f };
    float m_farz{ 3000.0f };
};
