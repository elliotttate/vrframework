#include "mods/VR.hpp"

// PORT FROM: REFramework/src/mods/VR.cpp (+ D3D11Component/D3D12Component for submit).
// KEEP: pose math, runtime sync (WaitGetPoses == xrWaitFrame), on_present/on_post_present
// submission through D3D12Component. CUT: all RE-Engine camera/scene reads — those are now
// IEngineAdapter callbacks. Stereo injection goes out via IEngineAdapter::apply_stereo.

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "Framework.hpp"
#include "spi/IEngineAdapter.hpp"

std::shared_ptr<VR>& VR::get() {
    static auto instance = std::make_shared<VR>();
    return instance;
}

// ===========================================================================
// init
// ===========================================================================
std::optional<std::string> VR::on_initialize() {
    // Pull engine depth range + caps from the adapter (if registered) so projections match.
    if (auto adapter = g_framework ? g_framework->get_engine_adapter() : nullptr) {
        const auto caps = adapter->capabilities();
        m_nearz = caps.near_plane;
        m_farz = caps.far_plane;
        m_left_eye_interval = 0;
        m_right_eye_interval = 1;
    }

    return std::nullopt;
}

std::optional<std::string> VR::on_initialize_d3d_thread() {
    // The OpenXR session needs the live D3D12 device+queue from the framework's hook, which
    // only exist on the render thread — so we init the runtime here, not in on_initialize().
    if (auto err = initialize_openxr()) {
        spdlog::error("[VR] OpenXR init failed: {}", err.value());
        return err;
    }

    m_init_finished = true;
    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr() {
    if (!g_framework || !g_framework->is_dx12()) {
        return "vrframework OpenXR path requires D3D12";
    }

    auto& hook = g_framework->get_d3d12_hook();
    if (!hook || hook->get_device() == nullptr || hook->get_command_queue() == nullptr) {
        return "D3D12 hook not ready (no device/queue)";
    }

    m_openxr = std::make_shared<runtimes::OpenXR>();

    if (auto err = m_openxr->initialize(hook->get_device(), hook->get_command_queue())) {
        m_openxr->loaded = false;
        return err;
    }

    m_runtime = m_openxr;     // VRRuntime* surface now points at the live OpenXR runtime
    m_wants_swapchains = true; // created lazily on the d3d thread once views are known
    spdlog::info("[VR] OpenXR runtime initialized");
    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr_swapchains() {
    if (m_openxr == nullptr || !m_openxr->loaded) {
        return "OpenXR not loaded";
    }
    return m_d3d12.openxr().create_swapchains(this);
}

// ===========================================================================
// present-path: drive the runtime sync + stereo submit
// ===========================================================================
void VR::on_frame() {
    // BeginRendering edge. Nothing engine-specific here; the adapter's FrameTimeline drives
    // on_begin_rendering()/update_hmd_state() per engine frame.
}

void VR::on_present() {
    if (m_openxr == nullptr || !m_openxr->loaded) {
        return;
    }

    // Pump runtime events (session state machine) so the session reaches READY/FOCUSED.
    m_openxr->consume_events(nullptr);

    // Create swapchains once the runtime has begun and we know the per-eye size.
    if (m_wants_swapchains && m_openxr->session_ready && !m_openxr->view_configs.empty()) {
        if (auto err = initialize_openxr_swapchains(); err) {
            spdlog::error("[VR] swapchain creation failed: {}", err.value());
        } else {
            m_wants_swapchains = false;
        }
    }

    if (!m_openxr->ready()) {
        return;
    }

    if (m_skip_next_present) {
        // Drop this present to re-phase L/R cadence (guide 10 §8).
        m_skip_next_present = false;
        return;
    }

    const bool is_left_eye_present = (m_render_frame_count % 2 == m_left_eye_interval);

    // One xrWaitFrame/xrBeginFrame per AFR *pair*, opened on the left-eye present so BOTH
    // eye copies in D3D12Component land inside a begun frame; the right-eye present closes
    // it with xrEndFrame. This is the validated XrSimTest cadence (wait -> locate -> begin
    // -> copy both -> end) spread across the two engine frames of an AFR pair.
    if (is_left_eye_present) {
        m_openxr->synchronize_frame();
        m_openxr->update_poses();
        m_openxr->update_matrices(m_nearz, m_farz);
        m_openxr->begin_frame();
    }

    // Copy this eye's backbuffer into the runtime swapchain image + (on the 2nd eye) submit.
    m_d3d12.on_frame(this);
}

void VR::on_post_present() {
    if (m_openxr == nullptr) {
        return;
    }
    m_d3d12.on_post_present(this);

    // Advance the AFR frame counters. Under AFR each engine present is one eye.
    ++m_render_frame_count;
    ++m_presenter_frame_count;
}

void VR::on_device_reset() {
    m_d3d12.on_reset(this);
    m_wants_swapchains = true;
}

void VR::on_draw_ui() {
    // PORT: REFramework VR UI panel (runtime status, IPD, recenter). Minimal stub.
}

void VR::on_config_load(const utility::Config&, bool) {}
void VR::on_config_save(utility::Config&) {}

// ===========================================================================
// pose accessors
// ===========================================================================
Matrix4x4f VR::get_rotation(uint32_t index) const {
    if (get_runtime() == nullptr || !get_runtime()->is_openxr() || m_openxr == nullptr) {
        return Matrix4x4f{1.0f};
    }
    std::shared_lock _{get_runtime()->pose_mtx};

    if (index == 0) {
        // HMD rotation (view-space location relative to stage).
        const auto& loc = m_openxr->view_space_location;
        return Matrix4x4f{*(glm::quat*)&loc.pose.orientation};
    }

    return Matrix4x4f{1.0f};
}

Matrix4x4f VR::get_transform(uint32_t index) const {
    if (get_runtime() == nullptr || !get_runtime()->is_openxr() || m_openxr == nullptr) {
        return Matrix4x4f{1.0f};
    }
    std::shared_lock _{get_runtime()->pose_mtx};

    if (index == 0) {
        const auto& loc = m_openxr->view_space_location;
        Matrix4x4f m{*(glm::quat*)&loc.pose.orientation};
        m[3] = Vector4f{*(Vector3f*)&loc.pose.position, 1.0f};
        return m;
    }

    return Matrix4x4f{1.0f};
}

Matrix4x4f VR::get_current_eye_transform(bool flip) {
    if (get_runtime() == nullptr) {
        return Matrix4x4f{1.0f};
    }
    std::shared_lock _{get_runtime()->eyes_mtx};
    const auto e = (m_render_eye == VRRuntime::Eye::LEFT) ? 0 : 1;
    return get_runtime()->eyes[e];
}

Matrix4x4f VR::get_transform_offset() const { return m_rotation_offset; }
Quaternion VR::get_rotation_offset() const { return glm::quat_cast(m_rotation_offset); }

// ===========================================================================
// frame lifecycle (driven by the adapter's FrameTimeline)
// ===========================================================================
void VR::on_wait_rendering(uint32_t frame) {
    // Equivalent of OpenVR WaitGetPoses: ensure a fresh frame sync + pose before the engine
    // renders this eye. For LATE/VERY_LATE stages the actual submit-side sync happens in
    // D3D12Component; here we make sure poses are current for camera injection.
    if (m_openxr == nullptr || !m_openxr->ready()) {
        return;
    }

    if (m_openxr->needs_pose_update) {
        m_openxr->synchronize_frame();
        m_openxr->update_poses();
        m_openxr->update_matrices(m_nearz, m_farz);
    }
}

void VR::on_begin_rendering(uint32_t frame) {
    m_engine_frame_count = frame;
    // AFR: even engine frame -> left eye, odd -> right eye (parity mapping is mutable).
    // Under AFR the render and presenter cadence both track the engine frame (anvil lineage:
    // vr->m_render_frame_count = vr->m_engine_frame_count). on_post_present also bumps these
    // when it's dispatched, but on_begin_rendering is the authoritative parity source.
    m_render_frame_count = frame;
    m_presenter_frame_count = frame;
    m_render_eye = (frame % 2 == m_left_eye_interval) ? VRRuntime::Eye::LEFT : VRRuntime::Eye::RIGHT;
}

void VR::update_hmd_state(uint32_t frame) {
    if (!is_hmd_active()) {
        return;
    }

    // Make sure matrices reflect the latest pose before we compose + push to the adapter.
    m_openxr->update_matrices(m_nearz, m_farz);
    push_stereo_to_adapter();
}

// ===========================================================================
// stereo: compose per-eye view + projection and hand to the engine adapter
// ===========================================================================
void VR::push_stereo_to_adapter() {
    auto adapter = g_framework ? g_framework->get_engine_adapter() : nullptr;
    if (adapter == nullptr || get_runtime() == nullptr) {
        return;
    }

    const auto caps = adapter->capabilities();

    StereoView sv{};
    sv.current_render_eye = (m_render_eye == VRRuntime::Eye::LEFT)
                                ? StereoView::Eye::LEFT : StereoView::Eye::RIGHT;
    sv.presenter_frame = (int)m_presenter_frame_count;
    sv.rotation_offset = m_rotation_offset;

    const auto hmd = get_transform(0);   // takes pose_mtx internally
    sv.hmd_transform = hmd;

    {
        // update_matrices() writes both eyes[] and projections[] while holding eyes_mtx,
        // so we read both under the same lock to stay race-free.
        std::shared_lock le{get_runtime()->eyes_mtx};

        for (int eye = 0; eye < 2; ++eye) {
            // View: BASIS * offset * hmd * eye * INV_BASIS (the same composition anvil's
            // onCalcFinalView builds; the core does it so apply_stereo gets ready matrices).
            const auto& eye_to_head = get_runtime()->eyes[eye];

            sv.view[eye] =
                caps.engine_to_vr_basis * m_rotation_offset * hmd * eye_to_head * caps.vr_to_engine_basis;
            sv.projection[eye] = get_runtime()->projections[eye];
        }
    }

    adapter->apply_stereo(sv);
}

void VR::on_xinput_get_state(uint32_t*, uint32_t, _XINPUT_STATE*) {
    // Default no-op; the consuming repo overrides (as the ports do in ModConfig.cpp).
}
