#pragma once

// FH5 downstream camera-constant-buffer stereo hook.
//
// WHY THIS EXISTS: FH5 renders camera-relative. The upstream view producer (sub_140BB1EE0) takes
// a4 = camera-to-world; ROTATING a4 rotates the rendered camera, but TRANSLATING a4.row3 does not move
// the camera because that position is already rebased higher up. Later live testing also showed the
// active camera driver's +0x320 pose is not sufficient for visible/culling-coherent translation. The
// current Empress RE candidate is CCamDriver+0x540, consumed by sub_1407A6300 before derived matrices.
// This downstream cbuffer hook remains a bring-up/diagnostic fallback.
//
// Fallback mechanism (see _agent_reports/fh5_stereo_cbuffer_spec.md): edit the already-derived 6912-byte camera
// constant buffer in place, AFTER camera-relative rebasing, applying a VIEW-SPACE lateral offset to the
// camera-relative view-projection (camRelVP@0x100). Because the geometry is already baked relative to the
// ORIGINAL camera, pre-translating the eye in view space re-projects that fixed geometry from a laterally
// offset viewpoint -> genuine depth-correct horizontal parallax that does NOT cancel.
//
// Mechanism (ported from the proven freecam FH5CameraProbe/src/DxgiProxy.cpp): hook the game device's
// CreateCommittedResource / CreatePlacedResource (to map upload rings) and CreateConstantBufferView (the
// per-frame trigger); when a 6912B CBV resolves to a slot whose content matches the main camera
// (near~0.1@0xA0, far~50000@0xA4, posW~1@0x8C, valid VIEW@0x000), transform VIEW/VP/camRelVP in place.

#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cstdint>

namespace fh5cb {

// Hook D3D12CreateDevice (call from dllmain once d3d12.dll is resident, BEFORE the framework/game create
// their devices) so the buffer-tracking hooks install at device creation and catch the camera ring's
// allocation. Without this the camera upload ring is created before any later install and is never tracked.
void install_createdevice_hook(HMODULE d3d12);

// Fallback installer for the device buffer-tracking + CBV-transform hooks (the PRIMARY install is the
// D3D12CreateDevice hook from install_createdevice_hook). Idempotent. The per-eye transform happens in the
// CreateConstantBufferView hook — no command-queue hook (a full ring scan at ExecuteCommandLists crashes).
void ensure_installed(ID3D12Device* device);

// Publish the per-eye camera offset (FH5 world units) the cbuffer hook applies, expressed in the camera's
// own VIEW-SPACE axes: x = right, y = up, z = forward. This is the per-eye IPD (lateral) plus optional
// 6-DOF head translation. `active` gates whether we transform at all (false = pass cbuffers through).
void set_eye_offset(float view_x, float view_y, float view_z, bool active);

// --- UI-draw redirect (Case B: FH5 draws the HUD/menus DIRECTLY into the backbuffer, after a fullscreen
// world composite; see _agent_reports/fh5_hud_quad_re_plan). Hooks the command-list OMSetRenderTargets to
// send the UI draws to a separate transparent RT instead of the backbuffer, leaving the backbuffer = clean
// world. Gated by ctl_ui_redirect(). Install once (device + swapchain known); reset per present. -------
void ui_redirect_install(ID3D12Device* device, IDXGISwapChain* swapchain);
void ui_redirect_on_present();          // reset the per-frame backbuffer-bind counter (once per present)
ID3D12Resource* ui_redirect_target();   // UI-only RT (alpha) for the quad this frame; nullptr if inactive
bool ui_redirect_active();              // redirect on + installed + UI RT valid

// vf54 UI-pass bracket (engine-level, scene-independent). The UIRenderer (D3D12UIRenderer) per-frame render
// entry = vtable slot 54 (EA 0x14181FCB0); Fh5Adapter inline-hooks it and calls enter/leave around the call.
// While the AVUI pass executes it records the HUD/menu draws on THIS thread, so the command-list draw hooks
// redirect every backbuffer draw made during the bracket to the UI RT — no PSO/draw-size heuristic, covers
// HUD AND menus. Thread-local depth (only the recording thread's draws are bracketed). RE: analysis/rtti_map.json
// (UIRenderer vtable 0x145F8F498 slot 54) + FH3 D3D12UIRenderer::GetRenderTarget.
void enter_ui_pass();                   // UIRenderer render-entry hook: ++depth on the calling thread
void leave_ui_pass();                   // ...--depth on return
bool in_ui_pass();                      // this thread is inside the UIRenderer render pass
void probe_ui_renderer_rt(void* uirenderer);  // vf54: find FH5's HUD RT (*(this+0x40)) vs the known RT set ([FH5UIRT])

// OverlayRenderer12 exact UI texture path. Fh5Adapter hooks the filtered
// ResourceBinding_OverlayRendererPSParameters__vf1 path and resolves textureObject+0x10 through the tracked
// D3D12 SRV map, covering sub_140E15C90 and the inline textured-quad path.
void enter_overlay_renderer12_draw(void* renderer, uint32_t layer);
void leave_overlay_renderer12_draw();
void record_overlay_texture_bind(void* renderer, void* texture_lock, void* render_context);
void record_overlay_ps_binding(void* ps);
void record_overlay_viewport_setup(void* renderer, bool surface_sized);
void record_overlay_native_target_bind(void* render_context, void* target_object, bool bind_mode);
void enter_overlay_immediate_flush(void* state);
void leave_overlay_immediate_flush();

// Register the EYE-SOURCE resource (the exact ID3D12Resource the mod CopyResource's into the OpenXR eye
// each frame = D3D12Component's eye_texture = the game's "final" backbuffer). PIX capture (FH5InGame.wpix)
// proved the game renders the world composite AND the HUD into THIS resource (an intermediate, NOT the DXGI
// swapchain the redirect was tracking). The redirect anchors on it BY RESOURCE: the first draw into an
// eye-source resource each frame = world composite (kept); subsequent draws into it = HUD (redirected to the
// UI RT) -> clean-world eyes + HUD on the quad. Call once per frame with the eye texture (accumulates the
// swapchain's buffer set across frames to handle double-buffering).
void register_eye_source(ID3D12Resource* eye_texture);

// FH5 HUD source candidates. hud_plane(idx) keeps the older display-plane path; ui_lineage_candidate()
// returns the current full-screen composite SRV candidate whose resource ancestry includes known UI PSO draws.
ID3D12Resource* hud_plane(int idx);
ID3D12Resource* ui_lineage_candidate();
ID3D12Resource* ui_atlas_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr, uint32_t* out_fmt = nullptr);
ID3D12Resource* pre_ui_eye_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr, uint32_t* out_fmt = nullptr);
ID3D12Resource* pre_ui_eye_candidate_for(ID3D12Resource* eye_texture,
                                         uint32_t* out_w = nullptr,
                                         uint32_t* out_h = nullptr,
                                         uint32_t* out_fmt = nullptr);
ID3D12Resource* overlay_srv_candidate(D3D12_CPU_DESCRIPTOR_HANDLE* out_srv = nullptr,
                                      uint32_t* out_w = nullptr, uint32_t* out_h = nullptr,
                                      uint32_t* out_fmt = nullptr);
ID3D12Resource* overlay_native_target_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr,
                                                uint32_t* out_fmt = nullptr);
ID3D12Resource* overlay_composite_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr,
                                            uint32_t* out_fmt = nullptr);
ID3D12Resource* ui_mirror_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr, uint32_t* out_fmt = nullptr);
ID3D12Resource* ui_final_mirror_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr, uint32_t* out_fmt = nullptr);
ID3D12Resource* ui_final_sample_candidate(uint32_t* out_w = nullptr, uint32_t* out_h = nullptr, uint32_t* out_fmt = nullptr);
int hud_plane_count();
int ctl_hud_plane();   // hudplane=N control: which display-plane index to show on the quad (-1=auto/last)

// Diagnostics for the FH5VR.log heartbeat.
unsigned long long ring_writes();
unsigned long long buffers_tracked();
unsigned long long cam_hits();
unsigned long long cbv6912_count();

// Live-tuning values from the control file (E:\tmp\fh5vr_ctl.txt): half-IPD in metres and the
// FH5 camera-lane units-per-metre scale. apply_stereo reads these so IPD/scale can be swept without a rebuild.
float ctl_half_ipd();
float ctl_world_scale();
int   ctl_recenter_seq();
int   ctl_rotation_mode();        // 0=off, 1=producer a4, 2=CCamDriver +0x320 matrix, 3=camera +0x90 Euler angles (upstream of view+cull+shadow)
bool  ctl_projection_enabled();   // true = replace producer a7 with per-eye OpenXR projection

enum PosLane : int {
    kPosLaneCcam320 = 0,       // active camera +0x320/+0x360
    kPosLaneCcam320D550 = 1,   // +0x320/+0x360 plus raw f64 +0x550 mirror
    kPosLaneClone0 = 2,        // diagnostic: first matrix clone only
    kPosLaneClone1 = 3,
    kPosLaneClone2 = 4,
    kPosLaneDownstream = 5,    // diagnostic: final 6912B camera cbuffer only
    kPosLaneOff = 6,
    kPosLaneViewTail = 7,      // active +0x320 plus first clone with a valid view-tail
    kPosLaneInput540 = 8,      // CCamDriver +0x540 additive camera-space input lane
    kPosLaneProducerA15 = 9,   // producer sub_140BB1EE0 a15/a16 f64 world cameraPos (moves view+cull; shadows slide)
    kPosLaneCamSrc = 10,       // SHADOW-COHERENT: proda15 producer shift PLUS cam+0x320 row3 shift in the sub_1407A1AC0 hook (the matrix the cascade fit reads live)
};
int ctl_pos_lane();
const char* pos_lane_name(int lane);
int ctl_pokerot();      // runtime probe: active_cam offset to basis-rotate (-1=off)
int ctl_pokerotvs();    // runtime probe: view-source(*(cam+0x48)) offset to basis-rotate (-1=off)
bool ctl_dumpcam();     // runtime probe: dump orthonormal-matrix offsets on cam + view-source
bool ctl_hud_quad();    // submit the UI/HUD as a head-locked OpenXR quad layer (hudquad=on)
bool ctl_hud_opaque();  // hudopaque=on -> opaque quad (no source-alpha blend)
bool ctl_hud_premul();  // hudpremul=on -> quad source is premultiplied alpha (default); off -> straight (sets XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT)
bool ctl_hud_flipv();   // hudflipv=on -> flip the quad source vertically in the HUD blit (for runtimes with inverted quad-layer V; e.g. SimXR preview)
int  ctl_hud_phase();   // hudphase=left/right -> which AER phase refreshes the HUD quad source
float ctl_hud_w();      // quad width (metres); height from texture aspect
float ctl_hud_x();      // quad centre offset in view space (metres): +x right
float ctl_hud_y();      // +y up
float ctl_hud_z();      // -z forward (distance in front of the head)
bool  ctl_ui_redirect();// uiredirect=on -> redirect FH5's backbuffer UI draws to a separate RT (Case B)
int   ctl_ui_redirect_mode();

// UPSTREAM camera-translation test (constant camera-relative offset applied in the producer hook to find
// which argument is the real camera-position lever, with shadows/derived data following). tgt: 0=off,
// 1=a4.row3, 2=a17, 3=a18, 4=all. fwd/strafe/up are camera-relative FH5 units.
float ctl_up_fwd();
float ctl_up_strafe();
float ctl_up_up();
int   ctl_up_tgt();

} // namespace fh5cb
