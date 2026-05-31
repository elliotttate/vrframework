// Example only — not built into the core. Shows how anvil's EngineRendererModule /
// EngineCameraModule / EngineEntry collapse into one IEngineAdapter, with the shared
// stereo + frame-sync work delegated to the core.

#include "ExampleAnvilAdapter.hpp"

#include <Framework.hpp>
#include <mods/VR.hpp>
#include <memory/memory_mul.h>
#include <glm/gtc/matrix_transform.hpp>

EngineCaps ExampleAnvilAdapter::capabilities() const {
    EngineCaps caps{};
    caps.graphics    = EngineCaps::Graphics::D3D12;
    caps.submission  = EngineCaps::Submission::AFR;     // alternate-frame, like the port
    caps.has_taa     = true;
    caps.left_handed = true;                            // anvil asserts GLM_FORCE_LEFT_HANDED
    // Anvil's Y_UP_TO_Z_UP_BASIS — the core pre-multiplies so apply_stereo gets it ready.
    caps.engine_to_vr_basis = glm::mat4{
        1, 0, 0, 0,
        0, 0, 1, 0,
        0,-1, 0, 0,
        0, 0, 0, 1
    };
    caps.vr_to_engine_basis = glm::inverse(caps.engine_to_vr_basis);
    return caps;
}

bool ExampleAnvilAdapter::install_hooks() {
    // Was: EngineRendererModule::InstallHooks + EngineCameraModule::InstallHooks, each
    // calling memory::*_fn_addr(). Here the addresses come from FuncRelocation/manifest,
    // and the hook bodies just feed the timeline / call apply_stereo.
    //
    //   auto begin = memory::FuncRelocation("begin_render_frame", "40 53 48 83 EC 20 ...", 0x10f4db0);
    //   safetyhook::create_inline(begin, &on_begin_render_frame);
    //   ... where on_begin_render_frame does:
    //       timeline().report(FrameTimeline::Event::WAIT_RENDER, frame);
    //       auto r = original();
    //       timeline().report(FrameTimeline::Event::RENDER_FRAME_BEGIN, frame);
    return true; // pretend success for the example
}

void ExampleAnvilAdapter::apply_stereo(const StereoView& view) {
    // Was: onCalcFinalView writing `*in_viewMatrix = *in_viewMatrix * transform` and
    // onCalcProjection writing the frustum projection. Now the core already composed
    // BASIS * offset * hmd * eye * INV_BASIS into view.view[eye]; we only store it where
    // the engine's GfxContext reads it (engine-specific pointer write).
    const int eye = (int)view.current_render_eye;
    (void)eye;
    // *engine_view_ptr      = view.view[eye];
    // *engine_projection_ptr = view.projection[eye];
}

glm::mat4 ExampleAnvilAdapter::get_world_camera() const {
    // Was: read camera node; used for decoupled pitch + head-aim forward vector.
    return glm::mat4{ 1.0f };
}

void ExampleAnvilAdapter::reproject_hud(float scale_x, float scale_y) {
    // Was: onCalcUIViewportHook adjusting ui_vp->left/right/top/bottom by HUD scale.
}

void ExampleAnvilAdapter::disable_incompatible_effects() {
    // Was: EngineTwicks::DisableBadEffects() (DOF, letterbox, etc.).
}

std::optional<std::string> ExampleAnvilAdapter::on_initialize() {
    return install_hooks() ? std::nullopt
                           : std::optional<std::string>{ "ACValhalla: hook install failed" };
}

void ExampleAnvilAdapter::on_draw_ui() {
    if (!ImGui::CollapsingHeader(get_name().data())) return;
    m_decoupled_pitch->draw("Decoupled Camera Pitch");
    m_hud_scale_x->draw("HUD Scale X");
    m_hud_scale_y->draw("HUD Scale Y");
}
