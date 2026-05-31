#pragma once

// PORT FROM: REFramework/src/mods/VR.hpp
// Cleaned: kept the pose/projection/eye/frame-counter surface the ports call; removed
// RE camera/scene hooks and the RE8VR game module. Stereo submission is delegated to
// the engine via IEngineAdapter::apply_stereo. Body: src/mods/VR.cpp (stub).

#include <array>
#include <memory>

#include "Mod.hpp"
#include "Math.hpp"
#include "vr/VRRuntime.hpp"

class VR : public Mod {
public:
    static std::shared_ptr<VR>& get();

    std::string_view get_name() const override { return "VR"; }
    std::optional<std::string> on_initialize() override;
    void on_draw_ui() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

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

    // Optional: gamepad passthrough the consumer can specialize (the ports define it).
    void on_xinput_get_state(uint32_t* retval, uint32_t user_index, struct _XINPUT_STATE* state);

private:
    std::unique_ptr<VRRuntime> m_runtime{};
    VRRuntime::Eye m_render_eye{ VRRuntime::Eye::LEFT };
    Matrix4x4f m_rotation_offset{ 1.0f };
};
