#include "mods/VR.hpp"

// PORT FROM: REFramework/src/mods/VR.cpp (+ D3D11Component/D3D12Component for submit).
// STUB. The pose math and runtime sync are engine-agnostic; lift them and route stereo
// submission through g_framework->get_engine_adapter()->apply_stereo(StereoView).
// RE-specific camera/scene reads in the original become adapter callbacks instead.

#include "Framework.hpp"
#include "spi/IEngineAdapter.hpp"
#include <spdlog/spdlog.h>

std::shared_ptr<VR>& VR::get() {
    static auto instance = std::make_shared<VR>();
    return instance;
}

std::optional<std::string> VR::on_initialize() {
    // PORT: select + init OpenXR/OpenVR runtime (REFramework VR::on_initialize).
    spdlog::info("[vrframework] VR::on_initialize stub — runtime selection not yet ported");
    return std::nullopt;
}

void VR::on_draw_ui() {
    // PORT: REFramework VR UI panel (runtime status, IPD, recenter button).
}

void VR::on_config_load(const utility::Config&, bool) {}
void VR::on_config_save(utility::Config&) {}

// --- pose accessors (PORT FROM REFramework VR get_rotation/get_transform/...) ----
Matrix4x4f VR::get_rotation(uint32_t index) const { return Matrix4x4f{ 1.0f }; }
Matrix4x4f VR::get_transform(uint32_t index) const { return Matrix4x4f{ 1.0f }; }

Matrix4x4f VR::get_current_eye_transform(bool flip) {
    if (!get_runtime()) return Matrix4x4f{ 1.0f };
    std::shared_lock _{ get_runtime()->eyes_mtx };
    const auto e = (m_render_eye == VRRuntime::Eye::LEFT) ? 0 : 1;
    return get_runtime()->eyes[e];
}

Matrix4x4f VR::get_transform_offset() const { return m_rotation_offset; }
Quaternion VR::get_rotation_offset() const { return glm::quat_cast(m_rotation_offset); }

// --- frame lifecycle (driven by the adapter's FrameTimeline) --------------------
void VR::on_wait_rendering(uint32_t frame) {
    // PORT: WaitGetPoses / xrWaitFrame; update needs_pose_update.
}

void VR::on_begin_rendering(uint32_t frame) {
    m_render_eye = (frame % 2 == 0) ? VRRuntime::Eye::LEFT : VRRuntime::Eye::RIGHT;
}

void VR::update_hmd_state(uint32_t frame) {
    if (!is_hmd_active()) return;
    // PORT: update_poses + update_matrices, then build StereoView and push to the adapter.
    if (auto adapter = g_framework ? g_framework->get_engine_adapter() : nullptr) {
        StereoView sv{};
        sv.current_render_eye = (m_render_eye == VRRuntime::Eye::LEFT)
                                    ? StereoView::Eye::LEFT : StereoView::Eye::RIGHT;
        sv.presenter_frame = (int)m_presenter_frame_count;
        // sv.view/projection: fill from runtime once pose math is ported.
        adapter->apply_stereo(sv);
    }
}

void VR::on_xinput_get_state(uint32_t*, uint32_t, _XINPUT_STATE*) {
    // Default no-op; the consuming repo overrides (as the ports do in ModConfig.cpp).
}
