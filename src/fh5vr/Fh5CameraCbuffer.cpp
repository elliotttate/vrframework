// FH5 downstream camera-constant-buffer stereo hook — see Fh5CameraCbuffer.hpp for the rationale.
//
// Ported from the proven freecam FH5CameraProbe/src/DxgiProxy.cpp (resource-ring tracking + CBV-triggered
// in-place transform), with the freecam's rotation build swapped for the per-eye VIEW-SPACE IPD/translation
// shift specified in _agent_reports/fh5_stereo_cbuffer_spec.md §4. Uses the framework's safetyhook-backed
// FunctionHook to inline-hook the game device's CreateCommittedResource/CreatePlacedResource/
// CreateConstantBufferView (the probe used MinHook; same vtable indices 27/29/17).

#include "Fh5CameraCbuffer.hpp"

#include <utility/Hooks.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fh5cb {
namespace {

// ---------------------------------------------------------------------------
// Per-eye offset published by the adapter (apply_stereo), read by Hook_CBV.
// View-space axes (x=right, y=up, z=forward), FH5 world units. Slowly-varying;
// a torn read for one frame is harmless, so plain atomics (no seqlock needed).
// ---------------------------------------------------------------------------
std::atomic<float> g_off_x{ 0.0f }, g_off_y{ 0.0f }, g_off_z{ 0.0f };
std::atomic<bool>  g_active{ false };

// ---------------------------------------------------------------------------
// Live-tuning control file: E:\tmp\fh5vr_ctl.txt, polled by a worker thread, so
// IPD / world-scale / transform-mode can be swept WITHOUT a rebuild (the audit's
// Phase 0). Lines: "ipd=0.032" (half-IPD metres), "scale=100" (FH5 camera-lane
// units/metre), "mode=off|camrel|viewvp|all", "poslane=input540|ccam320|viewtail|clone0|...".
// ---------------------------------------------------------------------------
std::atomic<float> g_ctl_half_ipd{ 0.0f };
std::atomic<float> g_ctl_world_scale{ 1.0f };
std::atomic<int>   g_ctl_mode{ 3 };   // 0=off 1=camrel_only 2=view_vp_only 3=all
std::atomic<int>   g_ctl_rot_mode{ 2 }; // 0=off 1=producer a4 2=CCamDriver+0x320
std::atomic<bool>  g_ctl_projection{ true };
std::atomic<int>   g_ctl_pos_lane{ kPosLaneProducerA15 };   // Empress RE: producer a15/a16 f64 cameraPos is the lever (+0x540 proved inert live)
std::atomic<bool>  g_ctl_started{ false };
std::atomic<int>   g_ctl_recenter_seq{ 0 };
// --- runtime camera-orientation probe (find/rotate the view-source orientation that shadow cascades read) ---
std::atomic<int>   g_ctl_pokerot{ -1 };     // rotate the orthonormal 4x4 at active_cam + offset by head delta (-1=off)
std::atomic<int>   g_ctl_pokerotvs{ -1 };   // rotate the orthonormal 4x4 at (*(active_cam+0x48)) + offset (-1=off)
std::atomic<bool>  g_ctl_dumpcam{ false };  // scan active_cam + view-source for orthonormal matrices; log offsets + basisScore
std::atomic<bool>  g_ctl_hud_quad{ false }; // submit the UI/HUD as a head-locked OpenXR quad layer (hudquad=on)
std::atomic<bool>  g_ctl_hud_opaque{ true }; // hudopaque=on -> opaque quad (no source-alpha blend); for backbuffer validation
std::atomic<float> g_ctl_hud_w{ 1.6f };      // quad width in metres (height derives from texture aspect)
std::atomic<float> g_ctl_hud_x{ 0.0f };      // quad centre offset (metres) in view space: +x right
std::atomic<float> g_ctl_hud_y{ 0.0f };      // +y up
std::atomic<float> g_ctl_hud_z{ -1.8f };     // -z forward (distance in front of the head)
std::atomic<bool>  g_ctl_hud_premul{ true }; // hudpremul=on -> treat the quad source as PREMULTIPLIED alpha (default: FH5 UI draws onto a cleared-transparent RT => premult). off -> straight/unpremultiplied (sets XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT)
std::atomic<bool>  g_ctl_hud_flipv{ false }; // hudflipv=on -> flip the quad source vertically in the HUD blit. Default off (OpenXR-spec convention). Turn on for runtimes whose quad-layer V convention is inverted vs the projection layer (e.g. the SimXR preview shows the HUD V-flipped).
std::atomic<int>   g_ctl_ui_redirect{ 0 };     // uiredirect: 0=off 1=redirect 2=dry run 3=R10A2 probe 4=skip composite-SRV source 5=skip UI PSOs 6=skip latest UI-lineage origin 7=mirror UI PSOs 8=mirror UI PSOs without replay 9=direct HUD atlas source 10=pre-UI/final delta quad 11=OverlayRenderer12 descriptor source 12=mirror whole atlas+replay 13=mirror atlas+skip UI PSO replay 14=mirror atlas+clear native 15=mirror atlas+skip final native sampler 16=broad lineage capture 17=broad final mirror+skip 18=pre-UI projection + delta quad 19=pre-UI projection + overlay RT quad 20=phase-locked final mirror+skip 21=phase-locked final sample, no native suppression 22=phase-locked HUD atlas, no native suppression 23=phase-locked UI PSO mirror, no native suppression 24=phase-locked pre-UI delta quad 25=phase-locked pre-UI projection + late final mirror+replay 26=phase-locked UI PSO mirror+skip native 27=phase-locked pre-UI projection + overlay RT steal 28=full-size final UI/flush steal 29=OverlayRenderer12 native target 30=engine-seam steal via the vf54 UIRenderer bracket (RECOMMENDED: in_ui_scope full-screen UI draws -> UI RT, no replay -> HUD-only quad + clean eyes)
std::atomic<int>   g_ctl_hud_phase{ 0 };       // hudphase=left/right (0/1): AER phase used to refresh the HUD quad texture in phase-locked HUD modes
std::atomic<int>   g_ctl_hud_plane{ -1 };    // hudplane=N -> which ALLOW_DISPLAY HUD plane to show on the quad (-1=auto/last)
std::atomic<int>   g_ctl_src_root{ 0 };       // srcroot/srcslot: composite SRV descriptor to source-skip in uiredirect=4
std::atomic<int>   g_ctl_src_slot{ -1 };

// UPSTREAM camera-translation test: a constant camera-relative offset applied IN THE PRODUCER to a chosen
// argument, to find which lever actually moves the rendered camera (with shadows/derived data following).
std::atomic<float> g_ctl_fwd{ 0.0f }, g_ctl_strafe{ 0.0f }, g_ctl_up{ 0.0f };
std::atomic<int>   g_ctl_tgt{ 0 };    // 0=off 1=a4.row3 2=a17 3=a18 4=all

void poll_control_file() {
    FILE* f = nullptr;
    if (fopen_s(&f, "E:\\tmp\\fh5vr_ctl.txt", "rb") != 0 || f == nullptr) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        float v; int iv;
        if (sscanf_s(line, "ipd=%f", &v) == 1)          g_ctl_half_ipd.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "scale=%f", &v) == 1)   g_ctl_world_scale.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "recenter=%d", &iv) == 1) g_ctl_recenter_seq.store(iv, std::memory_order_relaxed);
        else if (sscanf_s(line, "fwd=%f", &v) == 1)     g_ctl_fwd.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "strafe=%f", &v) == 1)  g_ctl_strafe.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "up=%f", &v) == 1)      g_ctl_up.store(v, std::memory_order_relaxed);
        else if (strncmp(line, "mode=", 5) == 0) {
            if      (strncmp(line + 5, "off", 3) == 0)    iv = 0;
            else if (strncmp(line + 5, "camrel", 6) == 0) iv = 1;
            else if (strncmp(line + 5, "viewvp", 6) == 0) iv = 2;
            else                                          iv = 3;
            g_ctl_mode.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "rot=", 4) == 0) {
            if      (strncmp(line + 4, "off", 3)    == 0) iv = 0;
            else if (strncmp(line + 4, "a4", 2)     == 0) iv = 1;
            else if (strncmp(line + 4, "driver", 6) == 0) iv = 2;
            else if (strncmp(line + 4, "angle", 5)  == 0) iv = 3;   // inject head Euler at cam+0x90/94/98 (upstream of view+cull+shadow)
            else                                          iv = 2;
            g_ctl_rot_mode.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "proj=", 5) == 0) {
            const bool enabled =
                strncmp(line + 5, "on", 2) == 0 ||
                strncmp(line + 5, "1", 1) == 0 ||
                strncmp(line + 5, "true", 4) == 0;
            g_ctl_projection.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudquad=", 8) == 0) {
            const bool enabled =
                strncmp(line + 8, "on", 2) == 0 ||
                strncmp(line + 8, "1", 1) == 0 ||
                strncmp(line + 8, "true", 4) == 0;
            g_ctl_hud_quad.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudopaque=", 10) == 0) {
            const bool enabled =
                strncmp(line + 10, "on", 2) == 0 ||
                strncmp(line + 10, "1", 1) == 0 ||
                strncmp(line + 10, "true", 4) == 0;
            g_ctl_hud_opaque.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudpremul=", 10) == 0) {
            const bool enabled =
                strncmp(line + 10, "on", 2) == 0 ||
                strncmp(line + 10, "1", 1) == 0 ||
                strncmp(line + 10, "true", 4) == 0;
            g_ctl_hud_premul.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudflipv=", 9) == 0) {
            const bool enabled =
                strncmp(line + 9, "on", 2) == 0 ||
                strncmp(line + 9, "1", 1) == 0 ||
                strncmp(line + 9, "true", 4) == 0;
            g_ctl_hud_flipv.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "uiredirect=", 11) == 0) {
            int m = 0;
            const char* v = line + 11;
            if (sscanf_s(v, "%d", &m) != 1) {
                if      (strncmp(v,"seam",4)==0 || strncmp(v,"vf54",4)==0 || strncmp(v,"bracket",7)==0)             m = 30;
                else if (strncmp(v,"overlaytarget",13)==0 || strncmp(v,"nativetarget",12)==0 ||
                         strncmp(v,"overlaynative",13)==0)                                                        m = 29;
                else if (strncmp(v,"fulluiskip",10)==0 || strncmp(v,"finaluiskip",11)==0 ||
                         strncmp(v,"fullhud",7)==0 || strncmp(v,"finalhud",8)==0)                                m = 28;
                else if (strncmp(v,"overlaysteal",12)==0 || strncmp(v,"stealoverlay",12)==0 ||
                         strncmp(v,"fo2overlay",10)==0 || strncmp(v,"fo2ui",5)==0)                                m = 27;
                else if (strncmp(v,"psoskipphase",12)==0 || strncmp(v,"phasepsoskip",12)==0 ||
                         strncmp(v,"uipsoskip",9)==0 || strncmp(v,"phaseskipui",11)==0)                              m = 26;
                else if (strncmp(v,"deltaphase",10)==0 || strncmp(v,"phasedelta",10)==0 ||
                         strncmp(v,"preuiphase",10)==0 || strncmp(v,"phasepreui",10)==0)                            m = 24;
                else if (strncmp(v,"mirrorphase",11)==0 || strncmp(v,"phasemirror",11)==0 ||
                         strncmp(v,"uiphase",7)==0 || strncmp(v,"phaseui",7)==0)                                    m = 23;
                else if (strncmp(v,"atlasphase",10)==0 || strncmp(v,"phaseatlas",10)==0)                            m = 22;
                else if (strncmp(v,"phasecopy",9)==0 || strncmp(v,"samplephase",11)==0)                             m = 21;
                else if (strncmp(v,"phaselock",9)==0 || strncmp(v,"phase",5)==0)                                    m = 20;
                else if (strncmp(v,"deltaeye",8)==0 || strncmp(v,"worlddelta",10)==0)                               m = 18;
                else if (strncmp(v,"broadskip",9)==0 || strncmp(v,"fullfinal",9)==0)                                m = 17;
                else if (strncmp(v,"broad",5)==0 || strncmp(v,"lineagefinal",12)==0)                                m = 16;
                else if (strncmp(v,"finalskip",9)==0 || strncmp(v,"atlasfinal",10)==0)                              m = 15;
                else if (strncmp(v,"atlasclear",10)==0)                                                           m = 14;
                else if (strncmp(v,"atlasreplay",11)==0)                                                          m = 12;
                else if (strncmp(v,"atlaspeel",9)==0 || strncmp(v,"atlasquad",9)==0)                               m = 13;
                else if (strncmp(v,"overlay",7)==0)                                                               m = 11;
                else if (strncmp(v,"delta",5)==0 || strncmp(v,"preui",5)==0)                                      m = 10;
                else if (strncmp(v,"atlas",5)==0 || strncmp(v,"hudatlas",8)==0)                                   m = 9;
                else if (strncmp(v,"peel",4)==0 || strncmp(v,"quad",4)==0)                                        m = 8;
                else if (strncmp(v,"mirror",6)==0 || strncmp(v,"capture",7)==0)                                   m = 7;
                else if (strncmp(v,"linskip",7)==0 || strncmp(v,"lineage",7)==0)                                  m = 6;
                else if (strncmp(v,"skipui",6)==0 || strncmp(v,"uiskip",6)==0)                                    m = 5;
                else if (strncmp(v,"src",3)==0 || strncmp(v,"skip",4)==0)                                         m = 4;
                else if (strncmp(v,"hud",3)==0)                                                                   m = 3;
                else if (strncmp(v,"dry",3)==0 || strncmp(v,"classify",8)==0)                                     m = 2;
                else if (strncmp(v,"on",2)==0 || strncmp(v,"true",4)==0)                                          m = 1;
            }
            g_ctl_ui_redirect.store(m, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudphase=", 9) == 0) {
            int phase = 0;
            const char* v = line + 9;
            if (sscanf_s(v, "%d", &phase) == 1) {
                g_ctl_hud_phase.store(phase ? 1 : 0, std::memory_order_relaxed);
            }
            else if (strncmp(v, "right", 5) == 0 || strncmp(v, "r", 1) == 0) {
                g_ctl_hud_phase.store(1, std::memory_order_relaxed);
            }
            else if (strncmp(v, "left", 4) == 0 || strncmp(v, "l", 1) == 0) {
                g_ctl_hud_phase.store(0, std::memory_order_relaxed);
            }
        }
        else if (sscanf_s(line, "hudplane=%d", &iv) == 1) g_ctl_hud_plane.store(iv, std::memory_order_relaxed);
        else if (sscanf_s(line, "srcroot=%d", &iv) == 1)  g_ctl_src_root.store(iv, std::memory_order_relaxed);
        else if (sscanf_s(line, "srcslot=%d", &iv) == 1)  g_ctl_src_slot.store(iv, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudw=%f", &v) == 1)    g_ctl_hud_w.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudx=%f", &v) == 1)    g_ctl_hud_x.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudy=%f", &v) == 1)    g_ctl_hud_y.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudz=%f", &v) == 1)    g_ctl_hud_z.store(v, std::memory_order_relaxed);
        else if (strncmp(line, "poslane=", 8) == 0) {
            if      (strncmp(line + 8, "camsrc", 6)         == 0) iv = kPosLaneCamSrc;
            else if (strncmp(line + 8, "proda15", 7)        == 0) iv = kPosLaneProducerA15;
            else if (strncmp(line + 8, "a15", 3)            == 0) iv = kPosLaneProducerA15;
            else if (strncmp(line + 8, "input540", 8)       == 0) iv = kPosLaneInput540;
            else if (strncmp(line + 8, "ccam540", 7)        == 0) iv = kPosLaneInput540;
            else if (strncmp(line + 8, "viewtail", 8)       == 0) iv = kPosLaneViewTail;
            else if (strncmp(line + 8, "ccam320_viewtail", 16) == 0) iv = kPosLaneViewTail;
            else if (strncmp(line + 8, "ccam320_d550", 13) == 0) iv = kPosLaneCcam320D550;
            else if (strncmp(line + 8, "ccam320", 7)       == 0) iv = kPosLaneCcam320;
            else if (strncmp(line + 8, "clone0", 6)        == 0) iv = kPosLaneClone0;
            else if (strncmp(line + 8, "clone1", 6)        == 0) iv = kPosLaneClone1;
            else if (strncmp(line + 8, "clone2", 6)        == 0) iv = kPosLaneClone2;
            else if (strncmp(line + 8, "downstream", 10)   == 0) iv = kPosLaneDownstream;
            else if (strncmp(line + 8, "off", 3)           == 0) iv = kPosLaneOff;
            else                                                   iv = kPosLaneCcam320;
            g_ctl_pos_lane.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "tgt=", 4) == 0) {
            if      (strncmp(line + 4, "off", 3)    == 0) iv = 0;
            else if (strncmp(line + 4, "a4",  2)    == 0) iv = 1;
            else if (strncmp(line + 4, "a17", 3)    == 0) iv = 2;
            else if (strncmp(line + 4, "a18", 3)    == 0) iv = 3;
            else if (strncmp(line + 4, "driver", 6) == 0) iv = 5;   // CCamDriver+0x320 (upstream pose)
            else                                          iv = 4;   // all
            g_ctl_tgt.store(iv, std::memory_order_relaxed);
        }
        else {
            unsigned uv = 0;
            if      (sscanf_s(line, "pokerotvs=%x", &uv) == 1) g_ctl_pokerotvs.store((int)uv, std::memory_order_relaxed);
            else if (sscanf_s(line, "pokerot=%x", &uv) == 1)   g_ctl_pokerot.store((int)uv, std::memory_order_relaxed);
            else if (sscanf_s(line, "dumpcam=%u", &uv) == 1)   g_ctl_dumpcam.store(uv != 0, std::memory_order_relaxed);
        }
    }
    fclose(f);
}

DWORD WINAPI ControlThread(void*) {
    int64_t last_logged = INT64_MIN;
    for (;;) {
        poll_control_file();
        const int m = g_ctl_mode.load(std::memory_order_relaxed);
        const int tgt = g_ctl_tgt.load(std::memory_order_relaxed);
        const int rot = g_ctl_rot_mode.load(std::memory_order_relaxed);
        const bool proj = g_ctl_projection.load(std::memory_order_relaxed);
        const int poslane = g_ctl_pos_lane.load(std::memory_order_relaxed);
        const int uir = g_ctl_ui_redirect.load(std::memory_order_relaxed);
        const bool hud_quad = g_ctl_hud_quad.load(std::memory_order_relaxed);
        const bool hud_opaque = g_ctl_hud_opaque.load(std::memory_order_relaxed);
        const int hud_phase = g_ctl_hud_phase.load(std::memory_order_relaxed);
        const int hud_w_sig = (int)std::lround(g_ctl_hud_w.load(std::memory_order_relaxed) * 100.0f);
        const int hud_x_sig = (int)std::lround(g_ctl_hud_x.load(std::memory_order_relaxed) * 100.0f);
        const int hud_y_sig = (int)std::lround(g_ctl_hud_y.load(std::memory_order_relaxed) * 100.0f);
        const int hud_z_sig = (int)std::lround(g_ctl_hud_z.load(std::memory_order_relaxed) * 100.0f);
        const int64_t sig = (int64_t)m * 100000 + (int64_t)tgt * 10000
            + rot * 1000 + (proj ? 500 : 0) + poslane
            + (int64_t)uir * 10000000 + (hud_quad ? 200000000LL : 0) + (hud_opaque ? 400000000LL : 0)
            + (int64_t)hud_phase * 700000000
            + (int64_t)hud_w_sig * 1000000000
            + (int64_t)(hud_x_sig + 10000) * 100000
            + (int64_t)(hud_y_sig + 10000) * 10
            + (int64_t)(hud_z_sig + 10000)
            + (int)(g_ctl_half_ipd.load(std::memory_order_relaxed) * 100)
            + (int)(g_ctl_fwd.load(std::memory_order_relaxed) + g_ctl_strafe.load(std::memory_order_relaxed));
        if (sig != last_logged) {
            last_logged = sig;
            spdlog::info("[FH5CTL] ipd={:.3f} scale={:.1f} mode={} rot={} proj={} poslane={} hudquad={} hudopaque={} uiredirect={} hudphase={} hud=({:.2f},{:.2f},{:.2f},{:.2f}) | UPSTREAM tgt={} fwd={:.1f} strafe={:.1f} up={:.1f}",
                         g_ctl_half_ipd.load(), g_ctl_world_scale.load(), m, rot, proj ? 1 : 0,
                         pos_lane_name(poslane), hud_quad ? 1 : 0, hud_opaque ? 1 : 0, uir, hud_phase,
                         g_ctl_hud_w.load(), g_ctl_hud_x.load(), g_ctl_hud_y.load(), g_ctl_hud_z.load(), tgt,
                         g_ctl_fwd.load(), g_ctl_strafe.load(), g_ctl_up.load());
        }
        Sleep(300);
    }
}

// ---- math (identical helpers to DxgiProxy.cpp) ----------------------------
struct Mat4 { float m[16]; };
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0; for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}
// Rigid inverse of a column-vector world->view matrix (translation in m[3],m[7],m[11]).
inline Mat4 InvRigid(const Mat4& V) {
    Mat4 r{};
    r.m[0] = V.m[0]; r.m[1] = V.m[4]; r.m[2] = V.m[8];
    r.m[4] = V.m[1]; r.m[5] = V.m[5]; r.m[6] = V.m[9];
    r.m[8] = V.m[2]; r.m[9] = V.m[6]; r.m[10] = V.m[10];
    const float tx = V.m[3], ty = V.m[7], tz = V.m[11];
    r.m[3]  = -(r.m[0] * tx + r.m[1] * ty + r.m[2] * tz);
    r.m[7]  = -(r.m[4] * tx + r.m[5] * ty + r.m[6] * tz);
    r.m[11] = -(r.m[8] * tx + r.m[9] * ty + r.m[10] * tz);
    r.m[15] = 1;
    return r;
}

inline float F32(const uint8_t* p) { float v = 0; std::memcpy(&v, p, 4); return v; }

// Same VIEW validator the proven freecam used: finite, last ROW (m12..15) == (0,0,0,1), orthonormal rows.
inline bool LooksView(const float* m) {
    for (int i = 0; i < 16; ++i) if (!std::isfinite(m[i])) return false;
    if (std::fabs(m[12]) > 0.01f || std::fabs(m[13]) > 0.01f || std::fabs(m[14]) > 0.01f || std::fabs(m[15] - 1) > 0.01f) return false;
    for (int r = 0; r < 3; ++r) {
        float l = std::sqrt(m[r * 4] * m[r * 4] + m[r * 4 + 1] * m[r * 4 + 1] + m[r * 4 + 2] * m[r * 4 + 2]);
        if (l < 0.9f || l > 1.1f) return false;
    }
    return true;
}
inline bool Finite16(const float* f) { for (int i = 0; i < 16; ++i) if (!std::isfinite(f[i])) return false; return true; }

// Main-camera identity gate on the 6912B block: near~0.1@0xA0, posW~1@0x8C, valid VIEW@0. The FAR plane
// varies by scene — 5000 in free-roam, 50000 at the showcase intro (matches the producer gate a9>2000) —
// so gate far on a generous floor, NOT the showcase-only [45000,55000] (which silently rejected free-roam).
inline bool LooksMainCameraCb(const uint8_t* p) {
    const float* view = reinterpret_cast<const float*>(p);
    const float* vp = reinterpret_cast<const float*>(p + 64);
    const float* camRelVp = reinterpret_cast<const float*>(p + 256);
    const float nr = F32(p + 160), fr = F32(p + 164), posW = F32(p + 140);
    return nr > 0.08f && nr < 0.2f && fr > 2000.0f && fr < 60000.0f &&
           std::fabs(posW - 1.0f) < 0.01f && LooksView(view) && Finite16(vp) && Finite16(camRelVp);
}

// ---------------------------------------------------------------------------
// The per-eye transform. VIEW@0x000 (col-vector world->view), VP@0x040 and
// camRelVP@0x100 (row-vector world/camRel->clip). The eye is shifted by the
// published VIEW-SPACE offset; geometry is already camera-relative so this does
// not cancel (spec §4.4). Returns true if applied.
// ---------------------------------------------------------------------------
bool TransformStereo(uint8_t* p) {
    const float dx = g_off_x.load(std::memory_order_relaxed);
    const float dy = g_off_y.load(std::memory_order_relaxed);
    const float dz = g_off_z.load(std::memory_order_relaxed);

    Mat4 V{}, P{}, R{};
    std::memcpy(V.m, p, 64);
    if (!LooksView(V.m)) return false;
    std::memcpy(P.m, p + 64, 64);    // VP @ 0x040
    std::memcpy(R.m, p + 256, 64);   // camRelVP @ 0x100

    // Camera axes in world (rows of V^-1 = view->world rotation): right/up/forward.
    Mat4 Vinv = InvRigid(V);
    const float rx = Vinv.m[0], ry = Vinv.m[1], rz = Vinv.m[2];   // camera +X (right) in world
    const float ux = Vinv.m[4], uy = Vinv.m[5], uz = Vinv.m[6];   // camera +Y (up) in world
    const float fx = Vinv.m[8], fy = Vinv.m[9], fz = Vinv.m[10];  // camera +Z (forward) in world

    // World-space displacement of the eye for the requested view-space offset (camera axes * offset).
    const float wdx = dx * rx + dy * ux + dz * fx;
    const float wdy = dx * ry + dy * uy + dz * fy;
    const float wdz = dx * rz + dy * uz + dz * fz;

    // These matrices are COLUMN-VECTOR (clip/view = M * pos_col; translation in COLUMN 3 = m[3],m[7],m[11];
    // last ROW = (0,0,0,1)). Proven by the freecam's InvRigid reading tx=m[3],ty=m[7],tz=m[11] and LooksView
    // requiring m[12..14]=0. To move the camera by +wd (world), POST-multiply by a column-vector translation
    // that pre-shifts the input position by -wd: Mnew = M * Tcol(-wd). (Pre-multiplying a ROW-vector Trow was
    // the bug -- it had zero render effect because the convention was wrong.) The camera-relative geometry
    // fed to camRelVP is FIXED for the frame, so shifting the eye here does NOT cancel -> real parallax.
    Mat4 Tcol{ {1,0,0,-wdx,  0,1,0,-wdy,  0,0,1,-wdz,  0,0,0,1} };   // column-vector translate by -wd

    // Mode gate (live via the control file): isolate which field moves which surface.
    const int mode = g_ctl_mode.load(std::memory_order_relaxed);
    if (mode == 0) return false;                      // off
    if (mode == 1 || mode == 3) {                     // camrel_only / all -> world parallax lever
        Mat4 camRelVPnew = Mul(R, Tcol);
        std::memcpy(p + 256,   camRelVPnew.m, 64);
        std::memcpy(p + 0xC40, camRelVPnew.m, 64);    // camRelVP exact duplicate @ 0xC40 (spec §1.2)
    }
    if (mode == 2 || mode == 3) {                     // view_vp_only / all -> car/cockpit/world-space path
        Mat4 Vnew  = Mul(V, Tcol);                    // VIEW @ 0x000 (world->view)
        Mat4 VPnew = Mul(P, Tcol);                    // VP   @ 0x040 (world->clip)
        std::memcpy(p,       Vnew.m,  64);
        std::memcpy(p + 64,  VPnew.m, 64);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Upload-ring tracking: map every CPU-visible BUFFER once (persistent), resolve
// a CBV's GPU-VA to a CPU pointer by linear scan (newest first).
// ---------------------------------------------------------------------------
struct BufRec { D3D12_GPU_VIRTUAL_ADDRESS va; UINT64 size; uint8_t* cpu; };
std::mutex            g_buf_mtx;
std::vector<BufRec>   g_buffers;
std::atomic<uint64_t> g_buf_count{ 0 };

void TrackBuffer(ID3D12Resource* res, const D3D12_RESOURCE_DESC* desc, bool cpuVisible) {
    if (!res || !desc || desc->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || !cpuVisible) return;
    D3D12_GPU_VIRTUAL_ADDRESS va = res->GetGPUVirtualAddress();
    if (!va) return;
    void* cpu = nullptr;
    if (FAILED(res->Map(0, nullptr, &cpu)) || !cpu) return;   // persistent map; never Unmap
    std::lock_guard<std::mutex> lk(g_buf_mtx);
    g_buffers.push_back({ va, desc->Width, reinterpret_cast<uint8_t*>(cpu) });
    g_buf_count.store(g_buffers.size(), std::memory_order_relaxed);
}

uint8_t* ResolveVA(D3D12_GPU_VIRTUAL_ADDRESS va, UINT need) {
    if (!va || !need) return nullptr;
    std::lock_guard<std::mutex> lk(g_buf_mtx);
    for (size_t i = g_buffers.size(); i-- > 0;) {
        const BufRec& b = g_buffers[i];
        if (!b.cpu || va < b.va || need > b.size) continue;
        const UINT64 off = static_cast<UINT64>(va - b.va);
        if (off <= b.size - need) return b.cpu + off;
    }
    return nullptr;
}

// Refill guard: transform a physical slot at most once per engine refill (else repeated CBV creations on
// the same slot in one frame re-apply the shift -> doubled disparity / degenerate). Map slot -> our hash.
std::mutex g_slot_mtx;
std::unordered_map<uintptr_t, uint64_t> g_slot_hash;
uint64_t HashCam(const uint8_t* p) { uint64_t h = 14695981039346656037ull; for (int i = 0; i < 320; ++i) h = (h ^ p[i]) * 1099511628211ull; return h; }
bool SlotDone(uintptr_t a, uint64_t h) { std::lock_guard<std::mutex> lk(g_slot_mtx); auto it = g_slot_hash.find(a); return it != g_slot_hash.end() && it->second == h; }
void SlotMark(uintptr_t a, uint64_t h) { std::lock_guard<std::mutex> lk(g_slot_mtx); g_slot_hash[a] = h; }

std::atomic<uint64_t> g_ring_writes{ 0 }, g_cam_hits{ 0 };

// SEH-guarded transform of one resolved slot (the mapped ptr can dangle if its ring was freed). No C++
// objects with destructors live in THIS frame (the helpers it calls own their own frames), so __try is legal.
bool TransformSlotSEH(uint8_t* p) {
    __try {
        if (!LooksMainCameraCb(p)) return false;
        g_cam_hits.fetch_add(1, std::memory_order_relaxed);
        if (!g_active.load(std::memory_order_relaxed)) return false;
        uint64_t h = HashCam(p);
        if (SlotDone(reinterpret_cast<uintptr_t>(p), h)) return false;   // already transformed this refill
        if (TransformStereo(p)) {
            SlotMark(reinterpret_cast<uintptr_t>(p), HashCam(p));
            g_ring_writes.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

// ---------------------------------------------------------------------------
// Device vtable hooks (indices 27/29/17, same as the proven probe).
// ---------------------------------------------------------------------------
using FnCommitted = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_HEAP_PROPERTIES*,
    D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void**);
using FnPlaced = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Heap*, UINT64,
    const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void**);
using FnCBV = void(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
using FnSRV = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
using FnUAV = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, ID3D12Resource*, const D3D12_UNORDERED_ACCESS_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
using FnCopyDescriptors = void(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, D3D12_DESCRIPTOR_HEAP_TYPE);
using FnCopyDescriptorsSimple = void(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE);

std::unique_ptr<FunctionHook> g_hk_committed, g_hk_placed, g_hk_cbv, g_hk_srv, g_hk_uav, g_hk_copydesc, g_hk_copydesc_simple;
std::atomic<uint64_t> g_cbv6912{ 0 };   // count of 6912B CBVs seen by Hook_CBV
std::atomic<uint32_t> g_srv_inc{ 0 };    // CBV/SRV/UAV descriptor increment, set from the live device
void ClearSrvHandle(size_t handle);      // defined after the lock-free SRV descriptor map

// HUD DISPLAY-PLANE: FH5 renders the in-game HUD into a dedicated full-screen RGBA8 surface created with
// D3D12_HEAP_FLAG_ALLOW_DISPLAY (a distinctive signature — the DXGI swapchain backbuffer comes from
// IDXGISwapChain::GetBuffer, NOT CreateCommittedResource; ordinary RTs don't request ALLOW_DISPLAY). PIX
// (FH5InGame.wpix, resource 4099: 1152x864 R8G8B8A8, heap SHARED|ALLOW_DISPLAY) proved this surface is
// HUD-ONLY (the HUD on transparent). We catch these in the CreateCommittedResource hook and expose the set
// so the OpenXR quad can copy the HUD-only plane instead of the world+HUD backbuffer.
std::mutex g_display_mtx;
std::vector<ID3D12Resource*> g_display_planes;    // ALLOW_DISPLAY full-screen RGBA8 surfaces (HUD-plane candidates)

HRESULT STDMETHODCALLTYPE Hook_Committed(ID3D12Device* self, const D3D12_HEAP_PROPERTIES* hp,
    D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state,
    const D3D12_CLEAR_VALUE* clr, REFIID riid, void** ppv) {
    auto orig = g_hk_committed->get_original<FnCommitted>();
    HRESULT hr = orig(self, hp, flags, desc, state, clr, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && hp &&
        (hp->Type == D3D12_HEAP_TYPE_UPLOAD ||
         (hp->Type == D3D12_HEAP_TYPE_CUSTOM && hp->CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)))
        TrackBuffer(reinterpret_cast<ID3D12Resource*>(*ppv), desc, true);
    // Catch FH5's HUD display-plane(s) by the ALLOW_DISPLAY signature.
    if (SUCCEEDED(hr) && ppv && *ppv && desc &&
        (flags & D3D12_HEAP_FLAG_ALLOW_DISPLAY) &&
        desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        (desc->Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc->Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
         desc->Format == DXGI_FORMAT_R8G8B8A8_TYPELESS) &&
        desc->Width >= 800 && desc->Height >= 400) {
        auto* r = reinterpret_cast<ID3D12Resource*>(*ppv);
        std::scoped_lock lk(g_display_mtx);
        bool known = false; for (auto* x : g_display_planes) if (x == r) { known = true; break; }
        if (!known && g_display_planes.size() < 16) {
            g_display_planes.push_back(r);
            spdlog::info("[FH5UIR] HUD display-plane candidate #{}: 0x{:X} {}x{} fmt{} (ALLOW_DISPLAY)",
                         g_display_planes.size(), reinterpret_cast<uintptr_t>(r),
                         (UINT)desc->Width, desc->Height, (int)desc->Format);
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_Placed(ID3D12Device* self, ID3D12Heap* heap, UINT64 off,
    const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clr,
    REFIID riid, void** ppv) {
    auto orig = g_hk_placed->get_original<FnPlaced>();
    HRESULT hr = orig(self, heap, off, desc, state, clr, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && desc && desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        auto* res = reinterpret_cast<ID3D12Resource*>(*ppv);
        D3D12_HEAP_PROPERTIES php{}; D3D12_HEAP_FLAGS hf{};
        bool cpuVisible = SUCCEEDED(res->GetHeapProperties(&php, &hf)) &&
            (php.Type == D3D12_HEAP_TYPE_UPLOAD ||
             (php.Type == D3D12_HEAP_TYPE_CUSTOM && php.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE));
        TrackBuffer(res, desc, cpuVisible);
    }
    return hr;
}

void STDMETHODCALLTYPE Hook_CBV(ID3D12Device* self, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_cbv->get_original<FnCBV>();
    orig(self, desc, handle);
    ClearSrvHandle((size_t)handle.ptr);   // the CBV/SRV/UAV heap is shared; avoid stale SRV identities
    // The main-camera block is 6912 bytes, 256-aligned location. Resolve to CPU and transform in place
    // BEFORE the engine records draws that read it (CBV creation precedes the binding/draw).
    if (desc && desc->SizeInBytes == 6912 && (desc->BufferLocation & 0xFF) == 0) {
        g_cbv6912.fetch_add(1, std::memory_order_relaxed);
        uint8_t* cpu = ResolveVA(desc->BufferLocation, desc->SizeInBytes);
        if (cpu) TransformSlotSEH(cpu);
    }
}

// NOTE: an ExecuteCommandLists ring-scan was tried and REMOVED — scanning all persistently-mapped upload
// buffers crashes FH5 with STATUS_IN_PAGE_ERROR (a tracked resource is freed/decommitted and the dangling
// mapped pointer faults, or a remapped page write corrupts another allocation). The probe hit the same
// wall. The safe, proven path is the bounded per-CBV transform in Hook_CBV above: the camera cbuffer lives
// in a moving upload-ring slot allocated FRESH each frame, so the engine creates a fresh 6912B CBV for it
// every frame -> Hook_CBV resolves the exact LIVE slot (no scan, no dangling) and transforms it in place.

// ---------------------------------------------------------------------------
// UI-draw redirect (Case B): track RTV->resource so we can tell when a command list binds the backbuffer.
// CreateRenderTargetView is hooked EARLY (with the other device hooks) so the game's backbuffer RTVs —
// created at swapchain setup, before ui_redirect_install — are captured. Map updated; backbuffer handles
// are resolved in ui_redirect_install once the swapchain is known.
// ---------------------------------------------------------------------------
using FnRTV = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
std::unique_ptr<FunctionHook> g_hk_rtv;

// ============================ LOCK-FREE HOT PATH ============================
// The per-RTV / per-bind / per-PSO / per-draw hooks fire on FH5's MANY PARALLEL command-recording threads.
// Using global std::mutexes here DEADLOCKED against the game's own render synchronization (x64dbg showed every
// render thread cascade-waiting in WaitForSingleObject; the redirect had classified/redirected nothing that
// frame, so it was the per-draw LOCK contention, not the rebind). So ALL recording-time state is now LOCK-FREE:
//   - per-thread data keyed by a hash of the thread id (CreateRTV -> OMSetRenderTargets -> SetPSO -> Draw for
//     ONE binding all run on ONE recording thread, so per-thread state pairs them without any shared lock);
//   - small read-mostly sets (eye-source resources, composite PSOs) as atomic arrays (lock-free reads).
// A tid-hash collision degrades to an occasional misclassified draw (a cosmetic glitch), never a hang.
constexpr uint32_t kTidSlots = 1024;            // power of two
constexpr uint32_t kTidMask  = kTidSlots - 1;
inline uint32_t tid_slot() { return (uint32_t)(GetCurrentThreadId() & kTidMask); }

std::vector<ID3D12Resource*> g_uir_bb_resources;   // swapchain backbuffer ptrs (set in install; compare-only)

bool IsFullscreenFmtLF(ID3D12Resource* res);                          // fwd: fullscreen display-format verdict (below)
bool GetResInfoSEH(ID3D12Resource* res, UINT64& w, UINT& h, UINT& fmt); // fwd: SEH-isolated GetDesc (below)

// GLOBAL lock-free CreateRTV map: handle.ptr -> {resource, fullscreen-verdict}. A CPU descriptor holds exactly
// ONE view at a time, so "most-recent write per handle, visible to ALL threads" is unambiguous -- the view bound
// in OMSetRenderTargets was created by the latest CreateRTV(handle) on ANY thread. The OLD per-thread ring missed
// cross-thread CreateRTV->bind pairs (FH5 creates RTVs on one thread, binds on many recording threads) and so
// resolved NULL for ~95% of draws -- the world composite + eye-source + HUD draws never reached classification.
// `fs` is the verdict computed in Hook_RTV when the resource is provably alive (the app just created a view of
// it); the hot path reads only this cached int -- it never calls GetDesc on a resolved pointer (which during
// scene-transition resource churn can be a FREED COM object: a wild vtable call past SEH -> hard crash). UAF-safe.
constexpr uint32_t kRtvMapSlots = 4096;        // power of two; open addressing. FH5 uses far fewer distinct RTVs.
constexpr uint32_t kRtvMapMask  = kRtvMapSlots - 1;
// meta packs the resource's dims+format (cached at CreateRTV when alive): (w<<32)|(h<<8)|fmt. Lets the hot path
// classify by size/format WITHOUT a GetDesc on a possibly-freed resolved pointer.
inline uint64_t pack_meta(uint64_t w, uint32_t h, uint32_t fmt) { return (w << 32) | ((uint64_t)(h & 0xFFFFFF) << 8) | (fmt & 0xFF); }
inline void     unpack_meta(uint64_t m, uint32_t& w, uint32_t& h, uint32_t& fmt) { w = (uint32_t)(m >> 32); h = (uint32_t)((m >> 8) & 0xFFFFFF); fmt = (uint32_t)(m & 0xFF); }
struct RtvSlot { std::atomic<size_t> handle{0}; std::atomic<ID3D12Resource*> res{nullptr}; std::atomic<int> fs{0}; std::atomic<uint64_t> meta{0}; };
RtvSlot g_rtv_map[kRtvMapSlots];
inline uint32_t rtv_hash(size_t h) { return (uint32_t)((h >> 5) * 2654435761u) & kRtvMapMask; }  // RTV handles ~32B-aligned
void RecordRtv(size_t handle, ID3D12Resource* res, int fs, uint64_t meta) {
    if (!handle) return;
    uint32_t i = rtv_hash(handle);
    for (uint32_t n = 0; n < kRtvMapSlots; ++n, i = (i + 1) & kRtvMapMask) {
        const size_t h = g_rtv_map[i].handle.load(std::memory_order_relaxed);
        if (h == handle || h == 0) {                                    // update-in-place or claim empty slot
            g_rtv_map[i].res.store(res, std::memory_order_relaxed);     // res + fs + meta first, then handle (publish last)
            g_rtv_map[i].fs.store(fs, std::memory_order_relaxed);
            g_rtv_map[i].meta.store(meta, std::memory_order_relaxed);
            g_rtv_map[i].handle.store(handle, std::memory_order_release);
            return;
        }
    }
    // table full (>4096 distinct live RTV handles -- not seen in practice) -> drop; self-heals next CreateRTV.
}
// REVERSE lookup: is `p` a resource we've seen via CreateRenderTargetView? If so, return its packed dims/format
// (else 0). PURE POINTER-COMPARE (never derefs p) -> safe to call on an arbitrary candidate pointer (e.g. while
// scanning an engine object for its render target). Used by the vf54 UIRenderer probe to locate FH5's HUD RT
// (UIRenderer owns its RT at *(this+0x40); we match that pointer against the known RT set to read its dims/fmt).
uint64_t RtResourceMeta(void* p) {
    if (!p) return 0;
    for (uint32_t i = 0; i < kRtvMapSlots; ++i) {
        if (g_rtv_map[i].handle.load(std::memory_order_relaxed) != 0 &&
            (void*)g_rtv_map[i].res.load(std::memory_order_relaxed) == p)
            return g_rtv_map[i].meta.load(std::memory_order_relaxed);
    }
    return 0;
}

// Returns the resolved resource (a bare pointer -- compare-only, NEVER deref it: it may be freed). Optionally
// writes the cached fullscreen verdict to *out_fs and packed dims/format to *out_meta (safe: captured while alive).
ID3D12Resource* ResolveRtv(size_t handle, int* out_fs = nullptr, uint64_t* out_meta = nullptr) {
    if (out_fs) *out_fs = 0;
    if (out_meta) *out_meta = 0;
    if (!handle) return nullptr;
    uint32_t i = rtv_hash(handle);
    for (uint32_t n = 0; n < kRtvMapSlots; ++n, i = (i + 1) & kRtvMapMask) {
        const size_t h = g_rtv_map[i].handle.load(std::memory_order_acquire);
        if (h == handle) {
            if (out_fs)   *out_fs   = g_rtv_map[i].fs.load(std::memory_order_relaxed);
            if (out_meta) *out_meta = g_rtv_map[i].meta.load(std::memory_order_relaxed);
            return g_rtv_map[i].res.load(std::memory_order_relaxed);
        }
        if (h == 0) return nullptr;                                     // open-addressing: first empty slot = miss
    }
    return nullptr;
}

void STDMETHODCALLTYPE Hook_RTV(ID3D12Device* self, ID3D12Resource* res,
    const D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_rtv->get_original<FnRTV>();
    orig(self, res, desc, handle);
    // GetDesc here is SAFE (res is alive -- the app just created a view of it). Cache the RTV view format when
    // supplied, not just the resource format: typeless resources are commonly rendered via UNORM/SRGB views, and
    // mirror targets must match the PSO-visible RTV format.
    __try {
        UINT64 w = 0; UINT h = 0, f = 0;
        const bool ok = GetResInfoSEH(res, w, h, f);
        const UINT rtv_fmt = (desc && desc->Format != DXGI_FORMAT_UNKNOWN) ? (UINT)desc->Format : f;
        const bool fs = ok && w >= 1000 && h >= 500 &&
            (rtv_fmt == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM      || rtv_fmt == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM ||
             rtv_fmt == (UINT)DXGI_FORMAT_R8G8B8A8_TYPELESS   || rtv_fmt == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
             rtv_fmt == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || rtv_fmt == (UINT)DXGI_FORMAT_R10G10B10A2_UNORM);
        RecordRtv((size_t)handle.ptr, res, fs ? 1 : 0, ok ? pack_meta(w, h, rtv_fmt) : 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}   // lock-free, global
}

// GLOBAL lock-free SRV descriptor map: CPU descriptor handle -> sampled resource + cached dims/format. The
// composite draw binds GPU descriptor tables, so SetGraphicsRootDescriptorTable converts GPU->CPU using the
// currently-bound CBV/SRV/UAV heap, then this map resolves individual SRVs without dereferencing resources in
// the draw hook. Descriptor copies are propagated so shader-visible heaps still resolve when populated from a
// staging heap.
constexpr uint32_t kSrvMapSlots = 1048576;
constexpr uint32_t kSrvMapMask  = kSrvMapSlots - 1;
struct SrvSlot {
    std::atomic<size_t> handle{ 0 };
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<uint64_t> meta{ 0 };
    std::atomic<uint32_t> view{ 0 };
};
SrvSlot g_srv_map[kSrvMapSlots];
constexpr uint32_t kSrvResSlots = 262144;
constexpr uint32_t kSrvResMask  = kSrvResSlots - 1;
struct SrvResSlot {
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<size_t> handle{ 0 };
    std::atomic<uint64_t> meta{ 0 };
    std::atomic<uint32_t> view{ 0 };
};
SrvResSlot g_srv_res_map[kSrvResSlots];
std::atomic<uint64_t> g_dbg_srv_create_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_null_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_copy_range_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_copy_simple_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_prop_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_clear_total{ 0 };
std::atomic<uint64_t> g_dbg_srv_record_full{ 0 };
std::atomic<size_t>   g_dbg_srv_first_handle{ 0 };
std::atomic<size_t>   g_dbg_srv_last_handle{ 0 };
inline uint32_t srv_hash(size_t h) { return (uint32_t)((h >> 5) * 2654435761u) & kSrvMapMask; }
inline uint32_t srv_res_hash(ID3D12Resource* r) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(r);
    return (uint32_t)((p >> 4) * 2654435761u) & kSrvResMask;
}
void NoteSrvHandle(size_t handle) {
    if (!handle) return;
    size_t zero = 0;
    g_dbg_srv_first_handle.compare_exchange_strong(zero, handle, std::memory_order_relaxed);
    g_dbg_srv_last_handle.store(handle, std::memory_order_relaxed);
}
void RecordSrvResource(ID3D12Resource* res, uint64_t meta, uint32_t view, size_t handle) {
    if (!res || !meta) return;
    uint32_t i = srv_res_hash(res);
    for (uint32_t n = 0; n < kSrvResSlots; ++n, i = (i + 1) & kSrvResMask) {
        ID3D12Resource* cur = g_srv_res_map[i].res.load(std::memory_order_acquire);
        if (cur == res) {
            g_srv_res_map[i].handle.store(handle, std::memory_order_relaxed);
            g_srv_res_map[i].meta.store(meta, std::memory_order_relaxed);
            g_srv_res_map[i].view.store(view, std::memory_order_relaxed);
            return;
        }
        if (cur == nullptr) {
            ID3D12Resource* expected = nullptr;
            if (!g_srv_res_map[i].res.compare_exchange_strong(expected, res, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                continue;
            }
            g_srv_res_map[i].handle.store(handle, std::memory_order_relaxed);
            g_srv_res_map[i].meta.store(meta, std::memory_order_relaxed);
            g_srv_res_map[i].view.store(view, std::memory_order_relaxed);
            return;
        }
    }
}
void RecordSrv(size_t handle, ID3D12Resource* res, uint64_t meta, uint32_t view) {
    if (!handle) return;
    NoteSrvHandle(handle);
    uint32_t i = srv_hash(handle);
    for (uint32_t n = 0; n < kSrvMapSlots; ++n, i = (i + 1) & kSrvMapMask) {
        const size_t h = g_srv_map[i].handle.load(std::memory_order_relaxed);
        if (h == handle || h == 0) {
            g_srv_map[i].res.store(res, std::memory_order_relaxed);
            g_srv_map[i].meta.store(meta, std::memory_order_relaxed);
            g_srv_map[i].view.store(view, std::memory_order_relaxed);
            g_srv_map[i].handle.store(handle, std::memory_order_release);
            RecordSrvResource(res, meta, view, handle);
            return;
        }
    }
    g_dbg_srv_record_full.fetch_add(1, std::memory_order_relaxed);
}
void ClearSrvHandle(size_t handle) {
    if (handle) g_dbg_srv_clear_total.fetch_add(1, std::memory_order_relaxed);
    RecordSrv(handle, nullptr, 0, 0);
}
bool TryResolveSrv(size_t handle, ID3D12Resource** out_res, uint64_t* out_meta, uint32_t* out_view) {
    if (out_res) *out_res = nullptr;
    if (out_meta) *out_meta = 0;
    if (out_view) *out_view = 0;
    if (!handle) return false;
    uint32_t i = srv_hash(handle);
    for (uint32_t n = 0; n < kSrvMapSlots; ++n, i = (i + 1) & kSrvMapMask) {
        const size_t h = g_srv_map[i].handle.load(std::memory_order_acquire);
        if (h == handle) {
            if (out_res)  *out_res  = g_srv_map[i].res.load(std::memory_order_relaxed);
            if (out_meta) *out_meta = g_srv_map[i].meta.load(std::memory_order_relaxed);
            if (out_view) *out_view = g_srv_map[i].view.load(std::memory_order_relaxed);
            return true;
        }
        if (h == 0) return false;
    }
    return false;
}
bool TryResolveSrvResource(ID3D12Resource* res, size_t* out_handle, uint64_t* out_meta, uint32_t* out_view) {
    if (out_handle) *out_handle = 0;
    if (out_meta) *out_meta = 0;
    if (out_view) *out_view = 0;
    if (!res) return false;
    uint32_t i = srv_res_hash(res);
    for (uint32_t n = 0; n < kSrvResSlots; ++n, i = (i + 1) & kSrvResMask) {
        ID3D12Resource* cur = g_srv_res_map[i].res.load(std::memory_order_acquire);
        if (cur == res) {
            const uint64_t meta = g_srv_res_map[i].meta.load(std::memory_order_relaxed);
            const size_t handle = g_srv_res_map[i].handle.load(std::memory_order_relaxed);
            if (!meta) return false;
            if (out_handle) *out_handle = handle;
            if (out_meta) *out_meta = meta;
            if (out_view) *out_view = g_srv_res_map[i].view.load(std::memory_order_relaxed);
            return true;
        }
        if (cur == nullptr) return false;
    }
    return false;
}
void CopyOrClearSrv(size_t dst, size_t src) {
    ID3D12Resource* res = nullptr; uint64_t meta = 0; uint32_t view = 0;
    if (TryResolveSrv(src, &res, &meta, &view)) {
        g_dbg_srv_prop_total.fetch_add(1, std::memory_order_relaxed);
        RecordSrv(dst, res, meta, view);
    } else {
        ClearSrvHandle(dst);
    }
}

void STDMETHODCALLTYPE Hook_SRV(ID3D12Device* self, ID3D12Resource* res,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_srv->get_original<FnSRV>();
    orig(self, res, desc, handle);
    g_dbg_srv_create_total.fetch_add(1, std::memory_order_relaxed);
    if (!res) g_dbg_srv_null_total.fetch_add(1, std::memory_order_relaxed);
    __try {
        UINT64 w = 0; UINT h = 0, f = 0;
        const bool ok = GetResInfoSEH(res, w, h, f);
        const uint32_t view = desc ? (uint32_t)desc->ViewDimension : 0xFFFFFFFFu;
        RecordSrv((size_t)handle.ptr, res, ok ? pack_meta(w, h, f) : 0, view);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_UAV(ID3D12Device* self, ID3D12Resource* res, ID3D12Resource* counter,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_uav->get_original<FnUAV>();
    orig(self, res, counter, desc, handle);
    ClearSrvHandle((size_t)handle.ptr);
}

void PropagateCopyDescriptors(UINT nd, const D3D12_CPU_DESCRIPTOR_HANDLE* dsts, const UINT* dsizes,
                              UINT ns, const D3D12_CPU_DESCRIPTOR_HANDLE* srcs, const UINT* ssizes) {
    const uint32_t inc = g_srv_inc.load(std::memory_order_relaxed);
    if (!inc || !nd || !ns || !dsts || !srcs) return;
    UINT di = 0, si = 0, doff = 0, soff = 0, copied = 0;
    while (di < nd && si < ns && copied < 4096) {
        const UINT dsz = dsizes ? dsizes[di] : 1;
        const UINT ssz = ssizes ? ssizes[si] : 1;
        if (doff >= dsz) { ++di; doff = 0; continue; }
        if (soff >= ssz) { ++si; soff = 0; continue; }
        UINT run = dsz - doff;
        const UINT srun = ssz - soff;
        if (run > srun) run = srun;
        if (run > 4096 - copied) run = 4096 - copied;
        for (UINT k = 0; k < run; ++k) {
            const size_t dst = (size_t)dsts[di].ptr + (size_t)(doff + k) * inc;
            const size_t src = (size_t)srcs[si].ptr + (size_t)(soff + k) * inc;
            CopyOrClearSrv(dst, src);
        }
        copied += run;
        doff += run;
        soff += run;
    }
}

void STDMETHODCALLTYPE Hook_CopyDescriptors(ID3D12Device* self, UINT nd, const D3D12_CPU_DESCRIPTOR_HANDLE* dsts,
    const UINT* dsizes, UINT ns, const D3D12_CPU_DESCRIPTOR_HANDLE* srcs, const UINT* ssizes,
    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    auto orig = g_hk_copydesc->get_original<FnCopyDescriptors>();
    orig(self, nd, dsts, dsizes, ns, srcs, ssizes, type);
    if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) g_dbg_srv_copy_range_total.fetch_add(1, std::memory_order_relaxed);
    __try {
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) PropagateCopyDescriptors(nd, dsts, dsizes, ns, srcs, ssizes);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_CopyDescriptorsSimple(ID3D12Device* self, UINT n, D3D12_CPU_DESCRIPTOR_HANDLE dst,
    D3D12_CPU_DESCRIPTOR_HANDLE src, D3D12_DESCRIPTOR_HEAP_TYPE type) {
    auto orig = g_hk_copydesc_simple->get_original<FnCopyDescriptorsSimple>();
    orig(self, n, dst, src, type);
    if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) g_dbg_srv_copy_simple_total.fetch_add(n, std::memory_order_relaxed);
    __try {
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            const uint32_t inc = g_srv_inc.load(std::memory_order_relaxed);
            if (inc) {
                UINT capped = n > 4096 ? 4096 : n;
                for (UINT i = 0; i < capped; ++i) {
                    CopyOrClearSrv((size_t)dst.ptr + (size_t)i * inc, (size_t)src.ptr + (size_t)i * inc);
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---------------------------------------------------------------------------
// UI-draw redirect: classify PSOs by shader bytecode hash. D3D12 DXIL still arrives in a DXBC container whose
// 16-byte checksum lives at byte offset 4; RenderDoc's state-at-event `bytecodeHash` reports the same value.
// The world composite is a fullscreen tonemap draw; known UI PSOs come from fh5_noxr_frame68577.rdc.
// ---------------------------------------------------------------------------
using FnCreateGfxPSO = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using FnCreatePipelineState2 = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device2*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);
std::unique_ptr<FunctionHook> g_hk_gfxpso;
std::unique_ptr<FunctionHook> g_hk_pipestream;
// Composite PSOs as a LOCK-FREE atomic array (read per eye-source draw on the hot path; written rarely at PSO
// creation). ~14 composite PSOs in practice. A benign append race at PSO-create just risks a 1-frame miss.
constexpr int kMaxCompPso = 128;
std::atomic<void*>    g_comp_pso_lf[kMaxCompPso];
std::atomic<int>      g_comp_pso_n{ 0 };
std::atomic<uint64_t> g_pso_create_count{ 0 };       // de-risk: total graphics PSOs created (vt10 coverage)
std::atomic<uint64_t> g_pso_stream_count{ 0 };       // PSOs created via ID3D12Device2::CreatePipelineState
std::atomic<uint64_t> g_composite_pso_count{ 0 };    // de-risk: how many matched the composite hash
constexpr int kMaxUiPso = 256;
std::atomic<void*>    g_ui_pso_lf[kMaxUiPso];
std::atomic<int>      g_ui_pso_n{ 0 };
std::atomic<uint64_t> g_ui_pso_count{ 0 };
std::atomic<uint64_t> g_ui_toggler_pso_count{ 0 };

constexpr uint32_t kPsoMetaSlots = 8192;
constexpr uint32_t kPsoMetaMask = kPsoMetaSlots - 1;
struct PsoMetaSlot {
    std::atomic<void*>    pso{ nullptr };
    std::atomic<uint64_t> vs_lo{ 0 };
    std::atomic<uint64_t> vs_hi{ 0 };
    std::atomic<uint64_t> ps_lo{ 0 };
    std::atomic<uint64_t> ps_hi{ 0 };
    std::atomic<uint32_t> vs_crc{ 0 };
    std::atomic<uint32_t> ps_crc{ 0 };
    std::atomic<uint32_t> flags{ 0 }; // bits: 0-1 VS kind, 2-3 PS kind, 4 comp, 5 ui, 6 stream, 7 ShaderToggler UI
};
PsoMetaSlot g_pso_meta[kPsoMetaSlots];

constexpr uint32_t kPsoDrawSlots = 2048;
constexpr uint32_t kPsoDrawMask = kPsoDrawSlots - 1;
struct PsoDrawSlot {
    std::atomic<void*>    pso{ nullptr };
    std::atomic<uint32_t> total{ 0 };
    std::atomic<uint32_t> fs{ 0 };
    std::atomic<uint32_t> display{ 0 };
    std::atomic<uint32_t> eye{ 0 };
    std::atomic<uint32_t> small_draw{ 0 };
    std::atomic<uint64_t> meta{ 0 };
};
PsoDrawSlot g_pso_draws[kPsoDrawSlots];

// World-composite PS hashes. The flat no-OpenXR capture reports 7783d957..., while the live AFR/SimXR path
// consistently binds 9774ba97... for the eye-source fullscreen composite.
static const unsigned char kCompositePsHashes[][16] = {
    {0x97,0x74,0xBA,0x97, 0x64,0xC2,0x07,0x59, 0x05,0xEC,0x5A,0x33, 0x38,0x05,0x25,0x10},
    {0x77,0x83,0xD9,0x57, 0x2E,0xFF,0xBA,0x0F, 0xDC,0x5C,0xCE,0xEC, 0xDC,0xD4,0x03,0x15},
};

// High-confidence UI PS hashes from fh5_noxr_frame68577.rdc. These cover the recurring free-roam glyph/text
// batches plus the main panel shader; the live AFR test logs the RTs they target before we mutate anything.
static const unsigned char kUiPsHashes[][16] = {
    {0xA9,0x77,0xD5,0xC8, 0xE9,0x71,0x52,0x25, 0x74,0xA8,0x7A,0xCF, 0xDF,0xED,0xCC,0xC7}, // PS 6012 panels
    {0x47,0xC8,0x14,0x49, 0xE7,0xE9,0xE8,0x59, 0x0B,0x90,0xC3,0xC6, 0x4D,0x03,0x27,0x9C}, // PS 7639
    {0xFA,0x1C,0x98,0x8F, 0x87,0x10,0x49,0x32, 0xCE,0xB5,0x7D,0xDD, 0xB8,0xBA,0xC7,0x68}, // PS 7410
    {0xBF,0xAF,0x81,0x09, 0x2A,0xEF,0x7D,0xCB, 0x7A,0xBA,0x73,0xF6, 0x98,0xEF,0xEB,0x14}, // PS 7350
    {0xFA,0x16,0x3D,0x56, 0x20,0xF9,0xBD,0x9E, 0xE0,0x46,0xE6,0x16, 0x6B,0x91,0xEB,0x11}, // PS 7603
    {0x88,0x8C,0x6E,0x5E, 0x21,0x7B,0xCF,0x0C, 0xAE,0x17,0x24,0xC2, 0x62,0xB4,0x08,0x23}, // late UI swapchain composite (EID 52462)
    {0x97,0x0A,0xBF,0x86, 0x4D,0xCE,0x4B,0x5B, 0xDD,0x40,0x3C,0x4C, 0x87,0xC4,0x82,0x99}, // late UI 180x180 offscreen (EID 53149)
    {0xFB,0x90,0x3E,0x61, 0x0B,0x47,0xD8,0x92, 0xD7,0xA5,0x63,0xD4, 0x01,0xD6,0x75,0xFD}, // late UI 20x20 offscreen (EID 53390)
    {0x1A,0xBD,0x47,0x74, 0x0E,0x26,0x40,0x51, 0x8F,0x71,0xB4,0x0B, 0x90,0x70,0x46,0x43}, // live display-eye UI compositor, PS crc B66723DB
};

// UI vertex shader hashes used by the same capture's UI draws. These broaden the diagnostic tally without
// driving redirect decisions by themselves.
static const unsigned char kUiVsHashes[][16] = {
    {0xAC,0x83,0x08,0x4F, 0xA5,0x28,0xBD,0x27, 0xAB,0xE6,0xA5,0x78, 0x2A,0x97,0xEB,0x51}, // VS 7325
    {0x10,0xEC,0x8B,0x20, 0xF9,0x49,0x75,0x82, 0x3B,0xB7,0x33,0x4D, 0xDA,0x4C,0x6C,0xFE}, // VS 6011
};

// Live in-game UI pixel shaders identified with ShaderToggler/ReShade. ShaderToggler stores a standard CRC32
// over the raw shader bytecode blob, which is a separate hash domain from the DXBC container checksum above.
static const uint32_t kLiveUiPsShaderTogglerCrcs[] = {
    0x00353E4Eu, // 3489358
    0x98F76AFAu, // 2566351610
    0xEC35690Du, // 3962923277
    0xBC41B4FEu, // 3158422782
    0xEF11CCABu, // 4010921131
    0x3DD3D4A2u, // 1037292706
    0x8C2958B4u, // 2351519924
    0x41FA90D1u, // 1106940113
};

bool ShaderHashEquals(const D3D12_SHADER_BYTECODE& shader, const unsigned char hash[16]) {
    if (!shader.pShaderBytecode || shader.BytecodeLength < 20) return false;
    const unsigned char* b = reinterpret_cast<const unsigned char*>(shader.pShaderBytecode);
    if (!(b[0] == 'D' && b[1] == 'X' && b[2] == 'B' && b[3] == 'C')) return false;   // DXBC container
    return memcmp(b + 4, hash, 16) == 0;
}
bool ShaderHashIn(const D3D12_SHADER_BYTECODE& shader, const unsigned char (*hashes)[16], size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (ShaderHashEquals(shader, hashes[i])) return true;
    }
    return false;
}
bool PsIsComposite(const D3D12_SHADER_BYTECODE& ps) {
    return ShaderHashIn(ps, kCompositePsHashes, sizeof(kCompositePsHashes) / sizeof(kCompositePsHashes[0]));
}
uint32_t ShaderTogglerCrc32(const D3D12_SHADER_BYTECODE& shader) {
    if (!shader.pShaderBytecode || shader.BytecodeLength == 0) return 0;
    const auto* data = reinterpret_cast<const uint8_t*>(shader.pShaderBytecode);
    uint32_t crc = 0xFFFFFFFFu;
    for (SIZE_T i = 0; i < shader.BytecodeLength; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}
bool ShaderTogglerHashIn(uint32_t crc, const uint32_t* hashes, size_t count) {
    if (!crc) return false;
    for (size_t i = 0; i < count; ++i) {
        if (crc == hashes[i]) return true;
    }
    return false;
}
bool PsoDescLooksUi(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc) {
    if (!desc) return false;
    const uint32_t ps_crc = ShaderTogglerCrc32(desc->PS);
    return ShaderHashIn(desc->PS, kUiPsHashes, sizeof(kUiPsHashes) / sizeof(kUiPsHashes[0])) ||
           ShaderHashIn(desc->VS, kUiVsHashes, sizeof(kUiVsHashes) / sizeof(kUiVsHashes[0])) ||
           ShaderTogglerHashIn(ps_crc, kLiveUiPsShaderTogglerCrcs,
                               sizeof(kLiveUiPsShaderTogglerCrcs) / sizeof(kLiveUiPsShaderTogglerCrcs[0]));
}
void ShaderHashHex(const D3D12_SHADER_BYTECODE& shader, char* out, size_t outsz) {
    if (!out || outsz == 0) return;
    out[0] = 0;
    if (!shader.pShaderBytecode || shader.BytecodeLength < 20) {
        _snprintf_s(out, outsz, _TRUNCATE, "none");
        return;
    }
    const unsigned char* b = reinterpret_cast<const unsigned char*>(shader.pShaderBytecode);
    if (!(b[0] == 'D' && b[1] == 'X' && b[2] == 'B' && b[3] == 'C')) {
        _snprintf_s(out, outsz, _TRUNCATE, "nonDXBC");
        return;
    }
    _snprintf_s(out, outsz, _TRUNCATE,
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11],
                b[12], b[13], b[14], b[15], b[16], b[17], b[18], b[19]);
}
uint32_t ShaderHashParts(const D3D12_SHADER_BYTECODE& shader, uint64_t& lo, uint64_t& hi) {
    lo = 0; hi = 0;
    if (!shader.pShaderBytecode || shader.BytecodeLength < 20) return 0;
    const unsigned char* b = reinterpret_cast<const unsigned char*>(shader.pShaderBytecode);
    if (!(b[0] == 'D' && b[1] == 'X' && b[2] == 'B' && b[3] == 'C')) return 2;
    memcpy(&lo, b + 4, 8);
    memcpy(&hi, b + 12, 8);
    return 1;
}
void HashPartsHex(uint32_t kind, uint64_t lo, uint64_t hi, char* out, size_t outsz) {
    if (!out || outsz == 0) return;
    if (kind == 0) { _snprintf_s(out, outsz, _TRUNCATE, "none"); return; }
    if (kind != 1) { _snprintf_s(out, outsz, _TRUNCATE, "nonDXBC"); return; }
    unsigned char b[16]{};
    memcpy(b, &lo, 8);
    memcpy(b + 8, &hi, 8);
    _snprintf_s(out, outsz, _TRUNCATE,
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}
inline uint32_t pso_hash(void* pso, uint32_t mask) {
    const uintptr_t v = reinterpret_cast<uintptr_t>(pso);
    return (uint32_t)((v >> 6) * 2654435761u) & mask;
}
void RememberPsoMeta(void* pso, const D3D12_SHADER_BYTECODE& vs, const D3D12_SHADER_BYTECODE& ps,
                     bool comp, bool ui, bool ui_toggler, bool stream, uint32_t vs_crc, uint32_t ps_crc) {
    if (!pso) return;
    uint64_t vslo = 0, vshi = 0, pslo = 0, pshi = 0;
    const uint32_t vsk = ShaderHashParts(vs, vslo, vshi);
    const uint32_t psk = ShaderHashParts(ps, pslo, pshi);
    const uint32_t flags = (vsk & 3u) | ((psk & 3u) << 2) | (comp ? 0x10u : 0u) |
                           (ui ? 0x20u : 0u) | (stream ? 0x40u : 0u) | (ui_toggler ? 0x80u : 0u);
    uint32_t i = pso_hash(pso, kPsoMetaMask);
    for (uint32_t n = 0; n < kPsoMetaSlots; ++n, i = (i + 1) & kPsoMetaMask) {
        void* cur = g_pso_meta[i].pso.load(std::memory_order_acquire);
        if (cur == pso || cur == nullptr) {
            g_pso_meta[i].vs_lo.store(vslo, std::memory_order_relaxed);
            g_pso_meta[i].vs_hi.store(vshi, std::memory_order_relaxed);
            g_pso_meta[i].ps_lo.store(pslo, std::memory_order_relaxed);
            g_pso_meta[i].ps_hi.store(pshi, std::memory_order_relaxed);
            g_pso_meta[i].vs_crc.store(vs_crc, std::memory_order_relaxed);
            g_pso_meta[i].ps_crc.store(ps_crc, std::memory_order_relaxed);
            g_pso_meta[i].flags.store(flags, std::memory_order_relaxed);
            g_pso_meta[i].pso.store(pso, std::memory_order_release);
            return;
        }
    }
}
bool LookupPsoMeta(void* pso, uint32_t& flags, uint64_t& vslo, uint64_t& vshi, uint64_t& pslo, uint64_t& pshi,
                   uint32_t& vscrc, uint32_t& pscrc) {
    flags = 0; vslo = vshi = pslo = pshi = 0;
    vscrc = pscrc = 0;
    if (!pso) return false;
    uint32_t i = pso_hash(pso, kPsoMetaMask);
    for (uint32_t n = 0; n < kPsoMetaSlots; ++n, i = (i + 1) & kPsoMetaMask) {
        void* cur = g_pso_meta[i].pso.load(std::memory_order_acquire);
        if (cur == pso) {
            flags = g_pso_meta[i].flags.load(std::memory_order_relaxed);
            vslo = g_pso_meta[i].vs_lo.load(std::memory_order_relaxed);
            vshi = g_pso_meta[i].vs_hi.load(std::memory_order_relaxed);
            pslo = g_pso_meta[i].ps_lo.load(std::memory_order_relaxed);
            pshi = g_pso_meta[i].ps_hi.load(std::memory_order_relaxed);
            vscrc = g_pso_meta[i].vs_crc.load(std::memory_order_relaxed);
            pscrc = g_pso_meta[i].ps_crc.load(std::memory_order_relaxed);
            return true;
        }
        if (cur == nullptr) return false;
    }
    return false;
}
inline bool IsDisplayFmt(uint32_t f) {
    return f == (uint32_t)DXGI_FORMAT_R8G8B8A8_UNORM      || f == (uint32_t)DXGI_FORMAT_B8G8R8A8_UNORM ||
           f == (uint32_t)DXGI_FORMAT_R8G8B8A8_TYPELESS   || f == (uint32_t)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           f == (uint32_t)DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == (uint32_t)DXGI_FORMAT_R10G10B10A2_UNORM;
}
inline bool IsUiFinalTargetFmt(uint32_t f) {
    return IsDisplayFmt(f) || f == (uint32_t)DXGI_FORMAT_R11G11B10_FLOAT;
}
inline bool IsStrictUiFinalSampleMeta(uint64_t meta) {
    uint32_t w = 0, h = 0, f = 0;
    unpack_meta(meta, w, h, f);
    if (w == 288 && h == 216) {
        return true;
    }
    if (w < 1000 || h < 500 || !IsUiFinalTargetFmt(f)) {
        return false;
    }
    return (uint64_t)w * 10ull >= (uint64_t)h * 12ull &&
           (uint64_t)w * 10ull <= (uint64_t)h * 24ull;
}
void TallyPsoDraw(void* pso, uint64_t meta, bool fs_sized, bool display_fmt, bool eye, uint32_t draw_size) {
    if (!pso) return;
    uint32_t i = pso_hash(pso, kPsoDrawMask);
    for (uint32_t n = 0; n < kPsoDrawSlots; ++n, i = (i + 1) & kPsoDrawMask) {
        void* cur = g_pso_draws[i].pso.load(std::memory_order_relaxed);
        if (cur == pso || cur == nullptr) {
            if (cur == nullptr) {
                void* expected = nullptr;
                if (!g_pso_draws[i].pso.compare_exchange_strong(expected, pso, std::memory_order_relaxed) &&
                    g_pso_draws[i].pso.load(std::memory_order_relaxed) != pso) {
                    continue;
                }
            }
            g_pso_draws[i].total.fetch_add(1, std::memory_order_relaxed);
            if (fs_sized) g_pso_draws[i].fs.fetch_add(1, std::memory_order_relaxed);
            if (display_fmt) g_pso_draws[i].display.fetch_add(1, std::memory_order_relaxed);
            if (eye) g_pso_draws[i].eye.fetch_add(1, std::memory_order_relaxed);
            if (draw_size <= 128) g_pso_draws[i].small_draw.fetch_add(1, std::memory_order_relaxed);
            if (meta) g_pso_draws[i].meta.store(meta, std::memory_order_relaxed);
            return;
        }
    }
}
void RememberCompositePso(void* pso) {
    int n = g_comp_pso_n.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) if (g_comp_pso_lf[i].load(std::memory_order_relaxed) == pso) return;
    if (n < kMaxCompPso) { g_comp_pso_lf[n].store(pso, std::memory_order_relaxed); g_comp_pso_n.store(n + 1, std::memory_order_release); }
}
bool IsCompositePso(void* pso) {                                   // LOCK-FREE read (hot path)
    if (!pso) return false;
    const int n = g_comp_pso_n.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) if (g_comp_pso_lf[i].load(std::memory_order_relaxed) == pso) return true;
    return false;
}
void RememberUiPso(void* pso) {
    int n = g_ui_pso_n.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) if (g_ui_pso_lf[i].load(std::memory_order_relaxed) == pso) return;
    if (n < kMaxUiPso) { g_ui_pso_lf[n].store(pso, std::memory_order_relaxed); g_ui_pso_n.store(n + 1, std::memory_order_release); }
}
bool IsUiPso(void* pso) {
    if (!pso) return false;
    const int n = g_ui_pso_n.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) if (g_ui_pso_lf[i].load(std::memory_order_relaxed) == pso) return true;
    return false;
}
void ClassifyCreatedPso(void* pso, const D3D12_SHADER_BYTECODE& vs, const D3D12_SHADER_BYTECODE& ps, const char* source, uint64_t ordinal) {
    const uint32_t vs_crc = ShaderTogglerCrc32(vs);
    const uint32_t ps_crc = ShaderTogglerCrc32(ps);
    const bool comp = PsIsComposite(ps);
    const bool ui_dxbc = ShaderHashIn(ps, kUiPsHashes, sizeof(kUiPsHashes) / sizeof(kUiPsHashes[0])) ||
                         ShaderHashIn(vs, kUiVsHashes, sizeof(kUiVsHashes) / sizeof(kUiVsHashes[0]));
    const bool ui_toggler = ShaderTogglerHashIn(ps_crc, kLiveUiPsShaderTogglerCrcs,
                                                sizeof(kLiveUiPsShaderTogglerCrcs) / sizeof(kLiveUiPsShaderTogglerCrcs[0]));
    const bool ui = ui_dxbc || ui_toggler;
    RememberPsoMeta(pso, vs, ps, comp, ui, ui_toggler, source && strcmp(source, "stream") == 0, vs_crc, ps_crc);
    if (comp) {
        RememberCompositePso(pso);
        g_composite_pso_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (ui) {
        RememberUiPso(pso);
        const uint64_t n = g_ui_pso_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (ui_toggler) g_ui_toggler_pso_count.fetch_add(1, std::memory_order_relaxed);
        if (n <= 12) {
            spdlog::info("[FH5UIPSO] matched UI PSO #{} source={} pso=0x{:X} dxbc={} toggler={} psCrc={:08X} vsCrc={:08X}",
                         n, source, reinterpret_cast<uintptr_t>(pso), ui_dxbc ? 1 : 0, ui_toggler ? 1 : 0,
                         ps_crc, vs_crc);
        }
    }
    if (ordinal <= 64 || comp || ui) {
        char vh[40], ph[40];
        ShaderHashHex(vs, vh, sizeof(vh));
        ShaderHashHex(ps, ph, sizeof(ph));
        spdlog::info("[FH5PSO] {}#{} pso=0x{:X} comp={} ui={} toggler={} VS={}({:08X}) PS={}({:08X})",
                     source, ordinal, reinterpret_cast<uintptr_t>(pso), comp ? 1 : 0, ui ? 1 : 0,
                     ui_toggler ? 1 : 0, vh, vs_crc, ph, ps_crc);
    }
}

inline size_t align_up_sz(size_t v, size_t a) { return (v + (a - 1)) & ~(a - 1); }
struct StreamSubobjectLayout { size_t size; size_t align; };
bool StreamLayout(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, StreamSubobjectLayout& out) {
    switch (type) {
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:        out = { sizeof(ID3D12RootSignature*), alignof(ID3D12RootSignature*) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:                    out = { sizeof(D3D12_SHADER_BYTECODE), alignof(D3D12_SHADER_BYTECODE) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:         out = { sizeof(D3D12_STREAM_OUTPUT_DESC), alignof(D3D12_STREAM_OUTPUT_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:                 out = { sizeof(D3D12_BLEND_DESC), alignof(D3D12_BLEND_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:           out = { sizeof(UINT), alignof(UINT) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:            out = { sizeof(D3D12_RASTERIZER_DESC), alignof(D3D12_RASTERIZER_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:         out = { sizeof(D3D12_DEPTH_STENCIL_DESC), alignof(D3D12_DEPTH_STENCIL_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:          out = { sizeof(D3D12_INPUT_LAYOUT_DESC), alignof(D3D12_INPUT_LAYOUT_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:    out = { sizeof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE), alignof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:    out = { sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE), alignof(D3D12_PRIMITIVE_TOPOLOGY_TYPE) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS: out = { sizeof(D3D12_RT_FORMAT_ARRAY), alignof(D3D12_RT_FORMAT_ARRAY) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:  out = { sizeof(DXGI_FORMAT), alignof(DXGI_FORMAT) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:           out = { sizeof(DXGI_SAMPLE_DESC), alignof(DXGI_SAMPLE_DESC) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:             out = { sizeof(UINT), alignof(UINT) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:            out = { sizeof(D3D12_CACHED_PIPELINE_STATE), alignof(D3D12_CACHED_PIPELINE_STATE) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:                 out = { sizeof(D3D12_PIPELINE_STATE_FLAGS), alignof(D3D12_PIPELINE_STATE_FLAGS) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:        out = { sizeof(D3D12_DEPTH_STENCIL_DESC1), alignof(D3D12_DEPTH_STENCIL_DESC1) }; return true;
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:       out = { sizeof(D3D12_VIEW_INSTANCING_DESC), alignof(D3D12_VIEW_INSTANCING_DESC) }; return true;
    default: return false;
    }
}
bool ExtractStreamShaders(const D3D12_PIPELINE_STATE_STREAM_DESC* desc, D3D12_SHADER_BYTECODE& vs, D3D12_SHADER_BYTECODE& ps) {
    vs = {}; ps = {};
    if (!desc || !desc->pPipelineStateSubobjectStream || desc->SizeInBytes < sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE)) return false;
    const auto* begin = reinterpret_cast<const uint8_t*>(desc->pPipelineStateSubobjectStream);
    const auto* p = begin;
    const auto* end = begin + desc->SizeInBytes;
    while (p + sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) <= end) {
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type{};
        memcpy(&type, p, sizeof(type));
        StreamSubobjectLayout layout{};
        if (!StreamLayout(type, layout)) return false;
        const size_t value_off = align_up_sz(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE), layout.align);
        const size_t total = align_up_sz(value_off + layout.size, sizeof(void*));
        if (p + total > end) return false;
        const auto* value = p + value_off;
        if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS) memcpy(&vs, value, sizeof(vs));
        else if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS) memcpy(&ps, value, sizeof(ps));
        p += total;
    }
    return true;
}

HRESULT STDMETHODCALLTYPE Hook_CreateGfxPSO(ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
    REFIID riid, void** ppv) {
    auto orig = g_hk_gfxpso->get_original<FnCreateGfxPSO>();
    HRESULT hr = orig(self, desc, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && desc) {
        const uint64_t ord = g_pso_create_count.fetch_add(1, std::memory_order_relaxed) + 1;
        __try {
            ClassifyCreatedPso(*ppv, desc->VS, desc->PS, "gfx", ord);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_CreatePipelineState2(ID3D12Device2* self, const D3D12_PIPELINE_STATE_STREAM_DESC* desc,
    REFIID riid, void** ppv) {
    auto orig = g_hk_pipestream->get_original<FnCreatePipelineState2>();
    HRESULT hr = orig(self, desc, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && desc) {
        const uint64_t ord = g_pso_stream_count.fetch_add(1, std::memory_order_relaxed) + 1;
        __try {
            D3D12_SHADER_BYTECODE vs{}, ps{};
            if (ExtractStreamShaders(desc, vs, ps)) {
                ClassifyCreatedPso(*ppv, vs, ps, "stream", ord);
            } else if (ord <= 16) {
                spdlog::info("[FH5PSO] stream#{} pso=0x{:X} parse=0", ord, reinterpret_cast<uintptr_t>(*ppv));
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return hr;
}

std::atomic<bool> g_dev_done{ false };

// Install the device buffer-tracking hooks (committed/placed/cbv). Called as early as possible — from the
// D3D12CreateDevice hook — so the camera upload ring's creation is tracked. Idempotent. The vtable function
// addresses are shared across all ID3D12Device instances of the same runtime, so hooking once catches all.
void install_device_hooks(ID3D12Device* device) {
    if (!device || g_dev_done.exchange(true)) return;
    g_srv_inc.store(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), std::memory_order_relaxed);
    void** vt = *reinterpret_cast<void***>(device);
    g_hk_committed = std::make_unique<FunctionHook>(Address{ vt[27] }, &Hook_Committed);
    g_hk_placed    = std::make_unique<FunctionHook>(Address{ vt[29] }, &Hook_Placed);
    g_hk_cbv       = std::make_unique<FunctionHook>(Address{ vt[17] }, &Hook_CBV);
    g_hk_srv       = std::make_unique<FunctionHook>(Address{ vt[18] }, &Hook_SRV);
    g_hk_uav       = std::make_unique<FunctionHook>(Address{ vt[19] }, &Hook_UAV);
    g_hk_rtv       = std::make_unique<FunctionHook>(Address{ vt[20] }, &Hook_RTV);       // CreateRenderTargetView
    g_hk_copydesc  = std::make_unique<FunctionHook>(Address{ vt[23] }, &Hook_CopyDescriptors);
    g_hk_copydesc_simple = std::make_unique<FunctionHook>(Address{ vt[24] }, &Hook_CopyDescriptorsSimple);
    g_hk_gfxpso    = std::make_unique<FunctionHook>(Address{ vt[10] }, &Hook_CreateGfxPSO); // CreateGraphicsPipelineState
    const bool ok = g_hk_committed->create() && g_hk_placed->create() && g_hk_cbv->create()
                    && g_hk_rtv->create() && g_hk_gfxpso->create();
    const bool srv_ok = g_hk_srv->create();
    const bool uav_ok = g_hk_uav->create();
    const bool copy_ok = g_hk_copydesc->create();
    const bool copy_simple_ok = g_hk_copydesc_simple->create();
    if (!srv_ok) g_hk_srv.reset();
    if (!uav_ok) g_hk_uav.reset();
    if (!copy_ok) g_hk_copydesc.reset();
    if (!copy_simple_ok) g_hk_copydesc_simple.reset();
    ID3D12Device2* dev2 = nullptr;
    bool pipe_stream_ok = false;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dev2))) && dev2) {
        void** vt2 = *reinterpret_cast<void***>(dev2);
        g_hk_pipestream = std::make_unique<FunctionHook>(Address{ vt2[47] }, &Hook_CreatePipelineState2); // ID3D12Device2::CreatePipelineState
        pipe_stream_ok = g_hk_pipestream->create();
        dev2->Release();
        if (!pipe_stream_ok) {
            g_hk_pipestream.reset();
            spdlog::warn("[FH5CB] ID3D12Device2::CreatePipelineState hook FAILED");
        }
    }
    spdlog::info("[FH5CB] device buffer-tracking hooks {} (committed=vt27 placed=vt29 cbv=vt17 srv=vt18/{} uav=vt19/{} rtv=vt20 copydesc=vt23/{} copydescSimple=vt24/{} gfxpso=vt10 pipestream=vt47/{} srvInc={})",
                 ok ? "installed" : "FAILED", srv_ok ? 1 : 0, uav_ok ? 1 : 0, copy_ok ? 1 : 0,
                 copy_simple_ok ? 1 : 0, pipe_stream_ok ? 1 : 0, g_srv_inc.load(std::memory_order_relaxed));
    if (!ok) {
        g_hk_committed.reset(); g_hk_placed.reset(); g_hk_cbv.reset(); g_hk_srv.reset(); g_hk_uav.reset();
        g_hk_rtv.reset(); g_hk_copydesc.reset(); g_hk_copydesc_simple.reset(); g_hk_gfxpso.reset();
        g_dev_done.store(false);
    }
}

// D3D12CreateDevice detour — catches the game device at creation (before the camera ring is allocated) and
// installs the buffer-tracking hooks. Mirrors FH5CameraProbe/src/DxgiProxy.cpp:557.
using FnCreateDevice = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
std::unique_ptr<FunctionHook> g_hk_createdev;

HRESULT WINAPI Hook_CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL fl, REFIID riid, void** ppDevice) {
    auto orig = g_hk_createdev->get_original<FnCreateDevice>();
    HRESULT hr = orig(adapter, fl, riid, ppDevice);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        ID3D12Device* dev = nullptr;
        if (SUCCEEDED(reinterpret_cast<IUnknown*>(*ppDevice)->QueryInterface(IID_PPV_ARGS(&dev))) && dev) {
            install_device_hooks(dev);
            dev->Release();
        }
    }
    return hr;
}

// ---- UI-draw redirect: PSO + frame-phase state machine -----------------------------------------------
// Discriminator = RESOURCE (backbuffer bound) + FRAME PHASE (after the world composite) + PSO (not the
// composite PSO). OMSetRenderTargets records the per-list backbuffer binding; SetPipelineState records the
// per-list current PSO. In the draw hooks (BOTH DrawInstanced and DrawIndexedInstanced): a backbuffer draw
// with the COMPOSITE PSO hits the backbuffer and flips the frame phase to "after composite"; subsequent
// backbuffer draws with any OTHER PSO are UI -> redirected to the UI RT (binding restored after). Draw size
// is NOT used (a UI quad and a fullscreen quad are both 6 verts; only the PSO/shader is a stable separator).
using FnOMSetRT = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT,
    const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*);
using FnDrawIdx  = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);
using FnDrawInst = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, UINT);
using FnSetPSO   = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12PipelineState*);
using FnSetDescriptorHeaps = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, ID3D12DescriptorHeap* const*);
using FnSetGfxRootSig = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12RootSignature*);
using FnSetGfxRootDescriptorTable = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE);
using FnCopyResource  = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*);
using FnCopyTexRegion = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, const D3D12_TEXTURE_COPY_LOCATION*, UINT, UINT, UINT, const D3D12_TEXTURE_COPY_LOCATION*, const D3D12_BOX*);
std::unique_ptr<FunctionHook> g_hk_omsetrt, g_hk_drawidx, g_hk_drawinst, g_hk_setpso, g_hk_setheaps, g_hk_setgrs, g_hk_setgrdt, g_hk_clearrtv, g_hk_copyres, g_hk_copytex;
// DIAGNOSTIC: does the HUD reach the eye-source (swapchain backbuffer) via a COPY (not a draw)? eyeSrcDraws ==
// compositeDraws says ONLY the world composite draws into the eye-source -- yet the HUD is visible in it. So the
// HUD must arrive by CopyResource/CopyTextureRegion from a HUD-bearing intermediate, OR be sampled by the
// composite PS. These hooks count copies whose DESTINATION is a registered eye-source and capture the SOURCE
// (the HUD-bearing buffer). dst/src come straight from the call -- no handle resolution, pointer-compare only.
std::atomic<uint32_t>        g_dbg_copy_to_eye{ 0 };       // copies (any kind) whose dst is a registered eye-source
std::atomic<ID3D12Resource*> g_dbg_last_copysrc{ nullptr };// last such copy's source resource (HUD-bearing candidate)
std::atomic<ID3D12Resource*> g_dbg_last_copydst{ nullptr };
using FnClearRTV = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*);
// CAPTURE-REPLAY (FH5InGame.wpix ev5668): FH5 assembles the in-game HUD into a full-screen RGBA8 RT that it
// CLEARS TO FULLY TRANSPARENT (0,0,0,0) first (a UI compositing layer; the world composite never clears to
// alpha=0). Catch that RT in a ClearRenderTargetView hook -> it's the assembled HUD layer -> copy to the quad.
// Independent of eye-source/swapchain registration, so it works in free-roam where the draw-redirect didn't.
std::atomic<ID3D12Resource*> g_hud_layer{ nullptr };
std::atomic<uint64_t> g_hud_layer_clears{ 0 };   // diagnostic
std::atomic<bool> g_uir_installed{ false };
ID3D12Resource*       g_uir_ui_rt = nullptr;      // UI-only RT (alpha); process-lifetime singleton
ID3D12DescriptorHeap* g_uir_rtv_heap = nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE g_uir_ui_rtv{};
std::atomic<uint32_t> g_uir_rt_w{ 0 };
std::atomic<uint32_t> g_uir_rt_h{ 0 };
std::atomic<uint32_t> g_uir_rt_fmt{ 0 };
std::atomic<bool>     g_uir_rt_valid{ false };
std::atomic<ID3D12Device*> g_uir_device{ nullptr };

constexpr int kUiMirrorSlots = 128;
struct UiMirrorSlot {
    std::atomic<ID3D12Resource*> src{ nullptr }; // original game RT identity; compare-only
    std::atomic<ID3D12Resource*> rt{ nullptr };  // same-sized/same-format mirror target
    std::atomic<uint64_t> meta{ 0 };             // packed dims + RTV view format
    std::atomic<uintptr_t> rtv{ 0 };
    std::atomic<uint64_t> clear_frame{ UINT64_MAX };
    std::atomic<uint32_t> draws{ 0 };
    ID3D12DescriptorHeap* heap{ nullptr };       // released only under g_ui_mirror_mtx
};
UiMirrorSlot g_ui_mirror[kUiMirrorSlots];
std::mutex g_ui_mirror_mtx;
std::mutex g_uir_device_mtx;
std::atomic<uint64_t> g_dbg_uimirror_created{ 0 };
std::atomic<uint64_t> g_dbg_uimirror_failed{ 0 };
std::atomic<uint64_t> g_dbg_uimirror_full{ 0 };
std::atomic<uint64_t> g_dbg_uimirror_hits{ 0 };
std::atomic<uint64_t> g_dbg_uimirror_draws{ 0 };
std::atomic<ID3D12Resource*> g_ui_mirror_latest{ nullptr };
std::atomic<uint64_t>        g_ui_mirror_latest_meta{ 0 };
std::atomic<ID3D12Resource*> g_ui_atlas_latest{ nullptr };
std::atomic<uint64_t>        g_ui_atlas_latest_meta{ 0 };
std::atomic<uint64_t>        g_ui_atlas_latest_ms{ 0 };
std::atomic<uint32_t>        g_ui_atlas_draws{ 0 };

thread_local int             t_overlay_scope_depth = 0;
thread_local uintptr_t       t_overlay_renderer = 0;
thread_local uint32_t        t_overlay_layer = 0;
thread_local int             t_overlay_flush_depth = 0;
std::atomic<ID3D12Resource*> g_overlay_srv_latest{ nullptr };
std::atomic<uint64_t>        g_overlay_srv_latest_meta{ 0 };
std::atomic<size_t>          g_overlay_srv_latest_cpu{ 0 };
std::atomic<uint64_t>        g_overlay_srv_latest_ms{ 0 };
std::atomic<uint64_t>        g_overlay_latest_renderer{ 0 };
std::atomic<uint64_t>        g_overlay_latest_ps{ 0 };
std::atomic<uint64_t>        g_overlay_latest_tex{ 0 };
std::atomic<uint64_t>        g_overlay_latest_desc{ 0 };
std::atomic<uint64_t>        g_overlay_latest_cached{ 0 };
std::atomic<uint64_t>        g_overlay_latest_binding{ 0 };
std::atomic<uint64_t>        g_overlay_latest_lock_retained{ 0 };
std::atomic<uint32_t>        g_overlay_latest_layer{ 0 };
std::atomic<uint32_t>        g_overlay_latest_param_slot{ 0xFFFFFFFFu };
std::atomic<uint32_t>        g_overlay_viewport_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_scope_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_bind_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_valid{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_resolved{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_unresolved{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_ps_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_ps_known{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_field_resolved{ 0 };
std::atomic<size_t>          g_dbg_overlay_unresolved_desc{ 0 };
std::atomic<uint64_t>        g_overlay_latest_field_base{ 0 };
std::atomic<uint32_t>        g_overlay_latest_field_off{ 0 };
std::atomic<uint64_t>        g_overlay_latest_field_qword{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_draw_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_root_scanned{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_root_resolved{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_root_noheap{ 0 };
std::atomic<uint32_t>        g_overlay_latest_root_slot{ 0xFFFFFFFFu };
std::atomic<uint32_t>        g_overlay_latest_srv_slot{ 0xFFFFFFFFu };
std::atomic<uint32_t>        g_dbg_overlay_flush_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_flush_draws{ 0 };
std::atomic<uint64_t>        g_overlay_latest_flush_state{ 0 };

constexpr int kOverlayPsSlots = 64;
std::atomic<uintptr_t>       g_overlay_ps_set[kOverlayPsSlots];

// Mode 10: copy the eye-source immediately after the world composite draw. Later, the OpenXR HUD quad blits
// final-eye minus this pre-UI copy, which avoids replaying/rebinding every UI draw on FH5's command lists.
std::mutex                   g_preui_eye_mtx;
ID3D12Resource*              g_preui_eye_copy{ nullptr };
uint64_t                     g_preui_eye_meta{ 0 };
std::atomic<uint32_t>        g_preui_eye_state{ D3D12_RESOURCE_STATE_COPY_DEST };
std::atomic_flag             g_preui_eye_recording = ATOMIC_FLAG_INIT;
std::atomic<ID3D12Resource*> g_preui_eye_latest{ nullptr };
std::atomic<uint64_t>        g_preui_eye_latest_meta{ 0 };
std::atomic<uint64_t>        g_preui_eye_latest_frame{ 0 };
constexpr int                kPreUiEyeCopySlots = 8;
struct PreUiEyeCopySlot {
    ID3D12Resource* src{ nullptr };
    ID3D12Resource* copy{ nullptr };
    uint64_t meta{ 0 };
    std::atomic<uint32_t> state{ D3D12_RESOURCE_STATE_COPY_DEST };
    std::atomic<uint64_t> seen_ms{ 0 };
};
PreUiEyeCopySlot             g_preui_eye_slots[kPreUiEyeCopySlots];
std::atomic<uint32_t>        g_dbg_preui_captures{ 0 };
std::atomic<uint32_t>        g_dbg_preui_recreated{ 0 };
std::atomic<uint32_t>        g_dbg_preui_failed{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_rt_draws{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_rt_replayed{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_rt_rejected{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_rt_clears{ 0 };
std::atomic<uint64_t>        g_overlay_rt_latest_ms{ 0 };
std::atomic<uint64_t>        g_overlay_rt_latest_meta{ 0 };
std::atomic<ID3D12Resource*> g_overlay_native_target_latest{ nullptr };
std::atomic<uint64_t>        g_overlay_native_target_meta{ 0 };
std::atomic<uint64_t>        g_overlay_native_target_ms{ 0 };
std::atomic<uintptr_t>       g_overlay_native_target_object{ 0 };
std::atomic<uintptr_t>       g_overlay_native_render_context{ 0 };
std::atomic<uintptr_t>       g_overlay_vf14_target_object{ 0 };
std::atomic<uintptr_t>       g_overlay_vf14_render_context{ 0 };
std::atomic<uint64_t>        g_overlay_vf14_target_ms{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_native_target_hits{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_native_target_probes{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_native_target_bind_calls{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_draw_rt0_draws{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_draw_rt0_eye{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_draw_rt0_fs{ 0 };
std::atomic<uint32_t>        g_dbg_overlay_draw_rt0_native_match{ 0 };
std::atomic<ID3D12Resource*> g_overlay_draw_rt0_latest{ nullptr };
std::atomic<uint64_t>        g_overlay_draw_rt0_latest_meta{ 0 };
std::atomic<uint32_t>        g_overlay_draw_rt0_latest_size{ 0 };
std::atomic<uint32_t>        g_overlay_draw_rt0_latest_flags{ 0 };
std::atomic<uintptr_t>       g_overlay_draw_rt0_latest_rtv{ 0 };

DXGI_FORMAT UiMirrorConcreteFormat(uint32_t f) {
    switch ((DXGI_FORMAT)f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:                              return (DXGI_FORMAT)f;
    }
}

void ReleaseUiMirrorSlotLocked(UiMirrorSlot& s) {
    ID3D12Resource* rt = s.rt.exchange(nullptr, std::memory_order_acq_rel);
    if (rt) rt->Release();
    if (s.heap) {
        s.heap->Release();
        s.heap = nullptr;
    }
    s.src.store(nullptr, std::memory_order_release);
    s.meta.store(0, std::memory_order_relaxed);
    s.rtv.store(0, std::memory_order_relaxed);
    s.clear_frame.store(UINT64_MAX, std::memory_order_relaxed);
    s.draws.store(0, std::memory_order_relaxed);
}

void ReleaseUiMirrors() {
    std::scoped_lock lk(g_ui_mirror_mtx);
    for (auto& s : g_ui_mirror) ReleaseUiMirrorSlotLocked(s);
    g_ui_mirror_latest.store(nullptr, std::memory_order_relaxed);
    g_ui_mirror_latest_meta.store(0, std::memory_order_relaxed);
}

void ReleasePreUiEyeCopyLocked() {
    if (g_preui_eye_copy != nullptr) {
        g_preui_eye_copy->Release();
        g_preui_eye_copy = nullptr;
    }
    g_preui_eye_meta = 0;
    g_preui_eye_state.store(D3D12_RESOURCE_STATE_COPY_DEST, std::memory_order_relaxed);
    g_preui_eye_latest.store(nullptr, std::memory_order_relaxed);
    g_preui_eye_latest_meta.store(0, std::memory_order_relaxed);
    g_preui_eye_latest_frame.store(0, std::memory_order_relaxed);
    for (auto& slot : g_preui_eye_slots) {
        if (slot.copy != nullptr) {
            slot.copy->Release();
            slot.copy = nullptr;
        }
        slot.src = nullptr;
        slot.meta = 0;
        slot.state.store(D3D12_RESOURCE_STATE_COPY_DEST, std::memory_order_relaxed);
        slot.seen_ms.store(0, std::memory_order_relaxed);
    }
}

void ReleasePreUiEyeCopy() {
    std::scoped_lock lk(g_preui_eye_mtx);
    ReleasePreUiEyeCopyLocked();
}

void ClearUirFullTargets();

void SetUiRedirectDevice(ID3D12Device* device) {
    ID3D12Device* cur = g_uir_device.load(std::memory_order_acquire);
    if (cur == device) return;
    std::scoped_lock lk(g_uir_device_mtx);
    cur = g_uir_device.load(std::memory_order_relaxed);
    if (cur == device) return;
    if (device) device->AddRef();
    ReleaseUiMirrors();
    ReleasePreUiEyeCopy();
    ClearUirFullTargets();
    g_uir_device.store(device, std::memory_order_release);
    if (cur) cur->Release();
}

UiMirrorSlot* LookupUiMirror(ID3D12Resource* src, uint64_t meta) {
    for (auto& s : g_ui_mirror) {
        if (s.src.load(std::memory_order_acquire) == src &&
            s.meta.load(std::memory_order_relaxed) == meta &&
            s.rt.load(std::memory_order_acquire) != nullptr &&
            s.rtv.load(std::memory_order_relaxed) != 0) {
            return &s;
        }
    }
    return nullptr;
}

UiMirrorSlot* GetOrCreateUiMirror(ID3D12Resource* src, uint64_t meta) {
    if (!src || !meta) return nullptr;
    if (UiMirrorSlot* existing = LookupUiMirror(src, meta)) {
        g_dbg_uimirror_hits.fetch_add(1, std::memory_order_relaxed);
        return existing;
    }

    std::scoped_lock lk(g_ui_mirror_mtx);
    if (UiMirrorSlot* existing = LookupUiMirror(src, meta)) {
        g_dbg_uimirror_hits.fetch_add(1, std::memory_order_relaxed);
        return existing;
    }

    UiMirrorSlot* slot = nullptr;
    for (auto& s : g_ui_mirror) {
        if (s.src.load(std::memory_order_relaxed) == nullptr ||
            s.rt.load(std::memory_order_relaxed) == nullptr) {
            slot = &s;
            break;
        }
    }
    if (!slot) {
        g_dbg_uimirror_full.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    ReleaseUiMirrorSlotLocked(*slot);

    uint32_t w = 0, h = 0, f = 0;
    unpack_meta(meta, w, h, f);
    const DXGI_FORMAT fmt = UiMirrorConcreteFormat(f);
    if (w == 0 || h == 0 || w > 8192 || h > 8192 || fmt == DXGI_FORMAT_UNKNOWN) {
        g_dbg_uimirror_failed.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    ID3D12Device* device = g_uir_device.load(std::memory_order_acquire);
    if (!device) {
        g_dbg_uimirror_failed.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = w;
    rd.Height = h;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = fmt;
    rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    ID3D12Resource* mirror = nullptr;
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&mirror));
    if (FAILED(hr) || !mirror) {
        g_dbg_uimirror_failed.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("[FH5UIMIRROR] RT create failed {}x{} fmt={} hr=0x{:08X}", w, h, (int)fmt, (uint32_t)hr);
        return nullptr;
    }

    ID3D12DescriptorHeap* heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    hr = device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    if (FAILED(hr) || !heap) {
        mirror->Release();
        g_dbg_uimirror_failed.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("[FH5UIMIRROR] RTV heap create failed {}x{} fmt={} hr=0x{:08X}", w, h, (int)fmt, (uint32_t)hr);
        return nullptr;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(mirror, nullptr, rtv);
    slot->heap = heap;
    slot->meta.store(pack_meta(w, h, (uint32_t)fmt), std::memory_order_relaxed);
    slot->rtv.store((uintptr_t)rtv.ptr, std::memory_order_relaxed);
    slot->rt.store(mirror, std::memory_order_release);
    slot->src.store(src, std::memory_order_release);
    g_dbg_uimirror_created.fetch_add(1, std::memory_order_relaxed);
    return slot;
}

std::atomic<bool>     g_uir_cleared{ false };          // UI RT cleared this frame
std::atomic<bool>     g_uir_after_composite{ false };  // frame phase: world composite has run this frame
std::atomic<uint64_t> g_uir_redirects{ 0 };            // total UI draws redirected (diagnostic)
// The EXACT resource the world composite (composite-PSO fullscreen draw) writes to = the real eye-source
// backbuffer. We anchor on this (set when the composite fires) and redirect later draws targeting the SAME
// resource — robust vs RTV-handle tagging (the UI may bind the backbuffer via a different RTV descriptor).
std::atomic<ID3D12Resource*> g_composite_target{ nullptr };

// EYE-SOURCE resources: the exact ID3D12Resource(s) the mod copies into the OpenXR eye each frame (registered
// from D3D12Component). PIX-proven: the game renders world+HUD INTO these (intermediate "final" targets), NOT
// the DXGI swapchain. We accumulate the distinct pointers (swapchain cycles a small buffer set). Compare-only.
// Eye-source resources as a LOCK-FREE atomic array (read per draw on the hot path). ~2-4 swapchain buffers.
constexpr int kMaxEyeSrc = 16;
std::atomic<ID3D12Resource*> g_eye_src_lf[kMaxEyeSrc];
std::atomic<int>             g_eye_src_n{ 0 };
bool IsEyeSource(ID3D12Resource* res) {                            // LOCK-FREE read (hot path)
    if (!res) return false;
    const int n = g_eye_src_n.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) if (g_eye_src_lf[i].load(std::memory_order_relaxed) == res) return true;
    return false;
}
// Per-frame: which eye-source resources have had their FIRST (world-composite) draw seen this frame. Subsequent
// draws into an already-seen eye-source resource are the HUD -> redirect. Thread-safe set keyed by resource.
std::mutex g_eye_seen_mtx;
std::unordered_set<ID3D12Resource*> g_eye_first_seen;
bool MarkEyeFirstSeen(ID3D12Resource* res) {  // returns true if this is the FIRST draw into res this frame
    std::scoped_lock lk(g_eye_seen_mtx);
    return g_eye_first_seen.insert(res).second;
}

// vf54 UI-pass bracket depth (thread-local): set by the UIRenderer render-entry hook (Fh5Adapter) via
// enter/leave_ui_pass; read by BeginDrawRedirect on the SAME (recording) thread. >0 => every backbuffer
// draw on this thread is part of the AVUI pass -> redirect it (engine-level, scene-independent signal).
thread_local int      t_ui_pass_depth = 0;
std::atomic<int>      g_ui_pass_depth{ 0 };     // vf54 can enqueue worker-thread command recording; expose it globally too.

// CASE A (live free-roam): vf54 renders the UI into an OFF-SCREEN RT (the UIRenderer's own *(this+64)),
// NOT the backbuffer (uiDraws=0 with the bb_bound gate). So instead of "redirect backbuffer draws", we
// auto-identify the main UI RT = the off-screen render target receiving the MOST draws while in the vf54
// bracket, and redirect THOSE draws to our own RT (known RENDER_TARGET state -> copyable to the quad). The
// engine's UI RT is then left empty, so its composite onto the backbuffer adds no UI -> clean-world eyes.
std::mutex            g_uir_cap_mtx;
struct UirCapTally {
    uint32_t count{ 0 };
    uint64_t meta{ 0 }; // draw-time RTV metadata; avoids stale reverse-map metadata after transitions.
};
std::unordered_map<ID3D12Resource*, UirCapTally> g_uir_cap_counts; // this-frame draw tally per vf54 RT
ID3D12Resource*       g_uir_target_res = nullptr;                 // last frame's winner = the main UI RT
std::atomic<uint64_t> g_uir_target_count{ 0 };                    // its draw count (diagnostic)
constexpr int         kUirFullTargetSlots = 4;
std::atomic<ID3D12Resource*> g_uir_full_target_res[kUirFullTargetSlots];
std::atomic<uint64_t>        g_uir_full_target_meta[kUirFullTargetSlots];
std::atomic<uint32_t>        g_uir_full_target_count[kUirFullTargetSlots];
// vf54-bracket diagnostics (per-second).
std::atomic<uint32_t> g_dbg_uipass_draws{ 0 };   // draws recorded while in the vf54 bracket
std::atomic<uint32_t> g_dbg_uipass_nobind{ 0 };  // ...with no recorded RT binding (bundle/untracked list)
std::atomic<uint32_t> g_dbg_uipass_off{ 0 };     // ...into an off-screen RT (Case A)
std::atomic<uint32_t> g_dbg_uipass_bb{ 0 };      // ...into the backbuffer (Case B)
std::atomic<uint32_t> g_dbg_total_draws{ 0 };    // all draws through the hook (uiredirect on)
std::atomic<uint32_t> g_dbg_bound_draws{ 0 };    // ...with a recorded RT binding
std::atomic<uint32_t> g_dbg_nullres_draws{ 0 };  // ...whose bound RTV resolved to no tracked resource
std::atomic<DWORD>    g_dbg_draw_tid{ 0 };       // thread id of the last draw (vs the vf54 thread)

void TallyUiRtDraw(ID3D12Resource* res, uint64_t meta) {
    if (!res) return;
    std::scoped_lock lk(g_uir_cap_mtx);
    UirCapTally& t = g_uir_cap_counts[res];
    t.count++;
    if (meta) t.meta = meta;
}
bool IsCapturedUiRt(ID3D12Resource* res) {
    return res != nullptr && res == g_uir_target_res;   // g_uir_target_res only written at on_present
}
bool IsCapturedFullUiRt(ID3D12Resource* res, uint64_t* out_meta = nullptr, uint32_t* out_count = nullptr) {
    if (!res) return false;
    for (int i = 0; i < kUirFullTargetSlots; ++i) {
        if (g_uir_full_target_res[i].load(std::memory_order_acquire) == res) {
            if (out_meta) *out_meta = g_uir_full_target_meta[i].load(std::memory_order_relaxed);
            if (out_count) *out_count = g_uir_full_target_count[i].load(std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}
// SEH-isolated (D3D12_RESOURCE_DESC is POD -> no C++ unwinding here, unlike on_present which holds a lock).
bool GetResDimsSEH(ID3D12Resource* res, UINT64& w, UINT& h) {
    if (!res) return false;
    __try { D3D12_RESOURCE_DESC d = res->GetDesc(); w = d.Width; h = d.Height; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool GetResInfoSEH(ID3D12Resource* res, UINT64& w, UINT& h, UINT& fmt) {
    if (!res) return false;
    __try { D3D12_RESOURCE_DESC d = res->GetDesc(); w = d.Width; h = d.Height; fmt = (UINT)d.Format; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void ReleasePreUiEyeSlotLocked(PreUiEyeCopySlot& slot) {
    if (slot.copy != nullptr) {
        slot.copy->Release();
        slot.copy = nullptr;
    }
    slot.src = nullptr;
    slot.meta = 0;
    slot.state.store(D3D12_RESOURCE_STATE_COPY_DEST, std::memory_order_relaxed);
    slot.seen_ms.store(0, std::memory_order_relaxed);
}

PreUiEyeCopySlot* EnsurePreUiEyeCopySlot(ID3D12Resource* src, uint64_t meta) {
    if (!src || !meta) return nullptr;

    std::scoped_lock lk(g_preui_eye_mtx);
    for (auto& slot : g_preui_eye_slots) {
        if (slot.src == src && slot.copy != nullptr && slot.meta == meta) {
            return &slot;
        }
    }

    ID3D12Device* device = g_uir_device.load(std::memory_order_acquire);
    if (!device) {
        g_dbg_preui_failed.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    D3D12_RESOURCE_DESC rd = src->GetDesc();
    if (rd.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || rd.Width == 0 || rd.Height == 0 ||
        rd.SampleDesc.Count != 1 || rd.Format == DXGI_FORMAT_UNKNOWN) {
        g_dbg_preui_failed.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    PreUiEyeCopySlot* target = nullptr;
    for (auto& slot : g_preui_eye_slots) {
        if (slot.src == src) {
            target = &slot;
            break;
        }
        if (target == nullptr && slot.src == nullptr) {
            target = &slot;
        }
    }
    if (target == nullptr) {
        target = &g_preui_eye_slots[0];
        for (auto& slot : g_preui_eye_slots) {
            if (slot.seen_ms.load(std::memory_order_relaxed) <
                target->seen_ms.load(std::memory_order_relaxed)) {
                target = &slot;
            }
        }
    }

    ReleasePreUiEyeSlotLocked(*target);
    rd.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    ID3D12Resource* copy = nullptr;
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&copy));
    if (FAILED(hr) || copy == nullptr) {
        g_dbg_preui_failed.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("[FH5PREUI] copy texture create failed {}x{} fmt={} hr=0x{:08X}",
                     (uint64_t)rd.Width, (uint32_t)rd.Height, (int)rd.Format, (uint32_t)hr);
        return nullptr;
    }
    copy->SetName(L"FH5VR pre-UI eye copy");
    target->src = src;
    target->copy = copy;
    target->meta = meta;
    target->state.store(D3D12_RESOURCE_STATE_COPY_DEST, std::memory_order_relaxed);
    target->seen_ms.store(0, std::memory_order_relaxed);
    g_dbg_preui_recreated.fetch_add(1, std::memory_order_relaxed);
    spdlog::info("[FH5PREUI] created pre-UI eye copy src=0x{:X} copy=0x{:X} {}x{} fmt={}",
                 reinterpret_cast<uintptr_t>(src), reinterpret_cast<uintptr_t>(copy),
                 (uint64_t)rd.Width, (uint32_t)rd.Height, (int)rd.Format);
    return target;
}

void CapturePreUiEyeAfterComposite(ID3D12GraphicsCommandList* self, ID3D12Resource* src, uint64_t meta) {
    if (!self || !src || !meta) return;
    if (g_preui_eye_recording.test_and_set(std::memory_order_acquire)) {
        return;
    }

    PreUiEyeCopySlot* slot = EnsurePreUiEyeCopySlot(src, meta);
    ID3D12Resource* dst = slot ? slot->copy : nullptr;
    if (dst != nullptr) {
        const auto dst_before = (D3D12_RESOURCE_STATES)slot->state.exchange(
            D3D12_RESOURCE_STATE_COPY_DEST, std::memory_order_acq_rel);

        D3D12_RESOURCE_BARRIER barriers[3]{};
        UINT nb = 0;
        if (dst_before != D3D12_RESOURCE_STATE_COPY_DEST) {
            barriers[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[nb].Transition.pResource = dst;
            barriers[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[nb].Transition.StateBefore = dst_before;
            barriers[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            ++nb;
        }
        barriers[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[nb].Transition.pResource = src;
        barriers[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[nb].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        ++nb;
        self->ResourceBarrier(nb, barriers);

        D3D12_TEXTURE_COPY_LOCATION d{};
        d.pResource = dst;
        d.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION s{};
        s.pResource = src;
        s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        s.SubresourceIndex = 0;
        self->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);

        D3D12_RESOURCE_BARRIER restore[2]{};
        restore[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        restore[0].Transition.pResource = src;
        restore[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        restore[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        restore[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        restore[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        restore[1].Transition.pResource = dst;
        restore[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        restore[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        restore[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        self->ResourceBarrier(2, restore);

        const uint64_t now_ms = GetTickCount64();
        slot->state.store(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, std::memory_order_release);
        slot->seen_ms.store(now_ms, std::memory_order_relaxed);
        g_preui_eye_latest.store(dst, std::memory_order_release);
        g_preui_eye_latest_meta.store(meta, std::memory_order_relaxed);
        g_preui_eye_latest_frame.store(now_ms, std::memory_order_relaxed);
        g_dbg_preui_captures.fetch_add(1, std::memory_order_relaxed);
    }

    g_preui_eye_recording.clear(std::memory_order_release);
}

// DIAGNOSTIC: tally draws per bound RT resource (any thread) so we can identify the engine's UI render
// target = the off-screen full-screen RGBA8 RT that receives the UI-element draws (vf54 is decoupled from
// D3D12 recording, so we discover the RT by its draw signature, not by bracketing).
std::mutex g_rt_mtx;
std::unordered_map<ID3D12Resource*, uint32_t> g_rt_tally;     // this-frame draws per RT
void TallyRtDraw(ID3D12Resource* res) {
    if (!res) return;
    std::scoped_lock lk(g_rt_mtx);
    g_rt_tally[res]++;
}

// LOCK-FREE per-RT draw tally over FULL-SCREEN-SIZED render targets (any format). Now that RTV resolution works
// globally, this reveals every full-screen buffer the engine draws into this frame, with cached dims/format and
// a draw count -- the HUD's off-screen RT is a full-screen buffer with a moderate per-frame draw burst, distinct
// from the eye-source (1 composite draw) and the main scene buffer (thousands). Small fixed array (few distinct
// full-screen RTs); per-draw cost is one atomic increment (no mutex -> no render-sync contention).
constexpr int kFsTally = 48;
struct FsTallyEntry {
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<uint32_t>        count{ 0 };
    std::atomic<uint64_t>        meta{ 0 };
};
FsTallyEntry g_fs_tally[kFsTally];
void TallyFsDraw(ID3D12Resource* res, uint64_t meta) {
    if (!res) return;
    for (int i = 0; i < kFsTally; ++i) {
        ID3D12Resource* e = g_fs_tally[i].res.load(std::memory_order_relaxed);
        if (e == res) { g_fs_tally[i].count.fetch_add(1, std::memory_order_relaxed); return; }
        if (e == nullptr) {
            ID3D12Resource* expected = nullptr;
            if (g_fs_tally[i].res.compare_exchange_strong(expected, res, std::memory_order_relaxed)) {
                g_fs_tally[i].meta.store(meta, std::memory_order_relaxed);
            } else if (g_fs_tally[i].res.load(std::memory_order_relaxed) != res) {
                continue;                                  // someone claimed it for another res -> keep probing
            }
            g_fs_tally[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
}

constexpr int kUiPsoRtTally = 48;
FsTallyEntry g_uipso_rt_tally[kUiPsoRtTally];
void TallyUiPsoRt(ID3D12Resource* res, uint64_t meta) {
    if (!res) return;
    uint32_t w = 0, h = 0, f = 0;
    unpack_meta(meta, w, h, f);
    if (w == 288 && h == 216 && f == (uint32_t)DXGI_FORMAT_R11G11B10_FLOAT) {
        g_ui_atlas_latest.store(res, std::memory_order_relaxed);
        g_ui_atlas_latest_meta.store(meta, std::memory_order_relaxed);
        g_ui_atlas_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
        g_ui_atlas_draws.fetch_add(1, std::memory_order_relaxed);
    }
    for (int i = 0; i < kUiPsoRtTally; ++i) {
        ID3D12Resource* e = g_uipso_rt_tally[i].res.load(std::memory_order_relaxed);
        if (e == res) { g_uipso_rt_tally[i].count.fetch_add(1, std::memory_order_relaxed); return; }
        if (e == nullptr) {
            ID3D12Resource* expected = nullptr;
            if (g_uipso_rt_tally[i].res.compare_exchange_strong(expected, res, std::memory_order_relaxed)) {
                g_uipso_rt_tally[i].meta.store(meta, std::memory_order_relaxed);
            } else if (g_uipso_rt_tally[i].res.load(std::memory_order_relaxed) != res) {
                continue;
            }
            g_uipso_rt_tally[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
}

// UI lineage probe: mark render targets written by known UI PSOs, propagate that mark through D3D12 copies,
// then annotate composite SRVs that sample a marked resource. This ports the useful UEVRJ producer-lineage idea
// in a FH5-sized, lock-free form; display use is opt-in via hudplane=-2.
constexpr int kUiLineageSlots = 2048;
constexpr int kUiLineageMask = kUiLineageSlots - 1;
constexpr int kUiLineageHitTally = 64;
struct UiLineageInfo {
    ID3D12Resource* origin{ nullptr };
    uint64_t meta{ 0 };
    uint64_t frame{ 0 };
    uint32_t depth{ 0 };
};
struct UiLineageSlot {
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<ID3D12Resource*> origin{ nullptr };
    std::atomic<uint64_t> meta{ 0 };
    std::atomic<uint64_t> frame{ 0 };
    std::atomic<uint32_t> depth{ 0 };
};
struct UiLineageHitSlot {
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<ID3D12Resource*> origin{ nullptr };
    std::atomic<uint32_t> count{ 0 };
    std::atomic<uint64_t> meta{ 0 };
    std::atomic<uint32_t> root_slot{ 0 };
    std::atomic<uint32_t> depth{ 0 };
};
UiLineageSlot g_ui_lineage[kUiLineageSlots];
UiLineageHitSlot g_ui_lineage_hits[kUiLineageHitTally];
std::atomic<uint64_t> g_ui_lineage_frame{ 0 };
std::atomic<uint32_t> g_dbg_uil_ui_marks{ 0 };
std::atomic<uint32_t> g_dbg_uil_copy_marks{ 0 };
std::atomic<uint32_t> g_dbg_uil_comp_hits{ 0 };
std::atomic<uint32_t> g_dbg_uil_candidate_hits{ 0 };
std::atomic<ID3D12Resource*> g_uil_last_copy_src{ nullptr };
std::atomic<ID3D12Resource*> g_uil_last_copy_dst{ nullptr };
std::atomic<ID3D12Resource*> g_uil_candidate{ nullptr };
std::atomic<ID3D12Resource*> g_uil_candidate_origin{ nullptr };
std::atomic<uint64_t> g_uil_candidate_meta{ 0 };
std::atomic<uint64_t> g_uil_candidate_frame{ 0 };
std::atomic<uint32_t> g_uil_candidate_root_slot{ 0 };
std::atomic<uint32_t> g_uil_candidate_depth{ 0 };
std::atomic<ID3D12Resource*> g_uil_latest_sample{ nullptr };
std::atomic<ID3D12Resource*> g_uil_latest_origin{ nullptr };
std::atomic<uint64_t> g_uil_latest_meta{ 0 };
std::atomic<uint64_t> g_uil_latest_frame{ 0 };
std::atomic<uint32_t> g_uil_latest_root_slot{ 0 };
std::atomic<uint32_t> g_uil_latest_depth{ 0 };

inline uint32_t ResourcePtrHash(ID3D12Resource* res, uint32_t mask) {
    const uintptr_t v = reinterpret_cast<uintptr_t>(res);
    return (uint32_t)((v >> 6) * 2654435761u) & mask;
}

bool FreshUiLineageFrame(uint64_t frame, uint64_t max_delta) {
    const uint64_t cur = g_ui_lineage_frame.load(std::memory_order_relaxed);
    return frame >= cur || (cur - frame) <= max_delta;
}

uint64_t LiveResourceMeta(ID3D12Resource* res, uint64_t fallback) {
    UINT64 w = 0; UINT h = 0, f = 0;
    return GetResInfoSEH(res, w, h, f) ? pack_meta(w, h, f) : fallback;
}

void RecordUiLineageResource(ID3D12Resource* res, ID3D12Resource* origin, uint64_t meta, uint32_t depth) {
    if (!res) return;
    if (!origin) origin = res;
    const uint64_t frame = g_ui_lineage_frame.load(std::memory_order_relaxed);
    uint32_t i = ResourcePtrHash(res, kUiLineageMask);
    for (int n = 0; n < kUiLineageSlots; ++n, i = (i + 1) & kUiLineageMask) {
        ID3D12Resource* cur = g_ui_lineage[i].res.load(std::memory_order_acquire);
        const uint64_t slot_frame = g_ui_lineage[i].frame.load(std::memory_order_relaxed);
        if (cur == res || cur == nullptr || !FreshUiLineageFrame(slot_frame, 16)) {
            g_ui_lineage[i].origin.store(origin, std::memory_order_relaxed);
            g_ui_lineage[i].meta.store(meta, std::memory_order_relaxed);
            g_ui_lineage[i].depth.store(depth, std::memory_order_relaxed);
            g_ui_lineage[i].frame.store(frame, std::memory_order_relaxed);
            if (cur != res) {
                ID3D12Resource* expected = cur;
                if (!g_ui_lineage[i].res.compare_exchange_strong(expected, res, std::memory_order_release, std::memory_order_relaxed)) {
                    continue;
                }
            }
            return;
        }
    }
}

bool LookupUiLineage(ID3D12Resource* res, UiLineageInfo& out) {
    out = UiLineageInfo{};
    if (!res) return false;
    uint32_t i = ResourcePtrHash(res, kUiLineageMask);
    for (int n = 0; n < kUiLineageSlots; ++n, i = (i + 1) & kUiLineageMask) {
        ID3D12Resource* cur = g_ui_lineage[i].res.load(std::memory_order_acquire);
        if (cur == res) {
            const uint64_t frame = g_ui_lineage[i].frame.load(std::memory_order_relaxed);
            if (!FreshUiLineageFrame(frame, 2)) return false;
            out.origin = g_ui_lineage[i].origin.load(std::memory_order_relaxed);
            out.meta = g_ui_lineage[i].meta.load(std::memory_order_relaxed);
            out.frame = frame;
            out.depth = g_ui_lineage[i].depth.load(std::memory_order_relaxed);
            return out.origin != nullptr;
        }
        if (cur == nullptr) return false;
    }
    return false;
}

void NoteUiPsoWrite(ID3D12Resource* res, uint64_t meta) {
    if (!res) return;
    RecordUiLineageResource(res, res, meta, 0);
    g_dbg_uil_ui_marks.fetch_add(1, std::memory_order_relaxed);
}

void PropagateUiLineageCopy(ID3D12Resource* dst, ID3D12Resource* src) {
    if (!dst || !src || dst == src) return;
    UiLineageInfo lin{};
    if (!LookupUiLineage(src, lin)) return;
    const uint64_t dst_meta = LiveResourceMeta(dst, lin.meta);
    RecordUiLineageResource(dst, lin.origin, dst_meta, lin.depth + 1);
    g_dbg_uil_copy_marks.fetch_add(1, std::memory_order_relaxed);
    g_uil_last_copy_src.store(src, std::memory_order_relaxed);
    g_uil_last_copy_dst.store(dst, std::memory_order_relaxed);
}

void TallyUiLineageHit(ID3D12Resource* res, ID3D12Resource* origin, uint64_t meta, uint32_t root, uint32_t slot, uint32_t depth) {
    if (!res) return;
    uint32_t i = ResourcePtrHash(res, kUiLineageHitTally - 1);
    for (int n = 0; n < kUiLineageHitTally; ++n, i = (i + 1) & (kUiLineageHitTally - 1)) {
        ID3D12Resource* cur = g_ui_lineage_hits[i].res.load(std::memory_order_relaxed);
        if (cur == res) {
            g_ui_lineage_hits[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (cur == nullptr) {
            ID3D12Resource* expected = nullptr;
            if (g_ui_lineage_hits[i].res.compare_exchange_strong(expected, res, std::memory_order_relaxed)) {
                g_ui_lineage_hits[i].origin.store(origin, std::memory_order_relaxed);
                g_ui_lineage_hits[i].meta.store(meta, std::memory_order_relaxed);
                g_ui_lineage_hits[i].root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
                g_ui_lineage_hits[i].depth.store(depth, std::memory_order_relaxed);
            } else if (g_ui_lineage_hits[i].res.load(std::memory_order_relaxed) != res) {
                continue;
            }
            g_ui_lineage_hits[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
}

void TallyUiLineageComposite(ID3D12Resource* res, uint64_t meta, uint32_t root, uint32_t slot) {
    UiLineageInfo lin{};
    if (!LookupUiLineage(res, lin)) return;
    const uint64_t use_meta = meta ? meta : lin.meta;
    TallyUiLineageHit(res, lin.origin, use_meta, root, slot, lin.depth);
    g_dbg_uil_comp_hits.fetch_add(1, std::memory_order_relaxed);
    g_uil_latest_sample.store(res, std::memory_order_relaxed);
    g_uil_latest_origin.store(lin.origin, std::memory_order_relaxed);
    g_uil_latest_meta.store(use_meta, std::memory_order_relaxed);
    g_uil_latest_frame.store(g_ui_lineage_frame.load(std::memory_order_relaxed), std::memory_order_relaxed);
    g_uil_latest_root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
    g_uil_latest_depth.store(lin.depth, std::memory_order_relaxed);
    uint32_t w = 0, h = 0, f = 0;
    unpack_meta(use_meta, w, h, f);
    if (w >= 1000 && h >= 500 && IsDisplayFmt(f)) {
        g_uil_candidate.store(res, std::memory_order_relaxed);
        g_uil_candidate_origin.store(lin.origin, std::memory_order_relaxed);
        g_uil_candidate_meta.store(use_meta, std::memory_order_relaxed);
        g_uil_candidate_frame.store(g_ui_lineage_frame.load(std::memory_order_relaxed), std::memory_order_relaxed);
        g_uil_candidate_root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
        g_uil_candidate_depth.store(lin.depth, std::memory_order_relaxed);
        g_dbg_uil_candidate_hits.fetch_add(1, std::memory_order_relaxed);
    }
}

// Per-thread current binding (RT[0] resource + PSO + DSV + graphics SRV tables), set by OMSetRenderTargets /
// SetPipelineState / SetDescriptorHeaps / SetGraphicsRootDescriptorTable and read by the draw hook -- all on ONE
// recording thread. LOCK-FREE tid-slot array (no shared mutex).
constexpr int kMaxGfxRootTables = 64;
struct BindLF {
    std::atomic<ID3D12Resource*> bb_res{ nullptr };
    std::atomic<void*>           pso{ nullptr };
    std::atomic<size_t>          bb_rtv{ 0 };     // RT[0] CPU handle.ptr (to restore after a redirect)
    std::atomic<size_t>          dsv{ 0 };        // DSV CPU handle.ptr
    std::atomic<int>             has_dsv{ 0 };
    std::atomic<int>             bb_fs{ 0 };      // 1 = RT[0] is a full-screen display-format (RGBA8/R10A2) RT
    std::atomic<uint64_t>        meta{ 0 };       // RT[0] packed dims+format (cached at CreateRTV; UAF-safe)
    std::atomic<size_t>          srv_cpu_base{ 0 };
    std::atomic<uint64_t>        srv_gpu_base{ 0 };
    std::atomic<uint32_t>        srv_count{ 0 };
    std::atomic<uint64_t>        gfx_root_table[kMaxGfxRootTables];
};
BindLF g_bind_lf[kTidSlots];
void ClearGfxRootTables(BindLF& b) {
    for (int i = 0; i < kMaxGfxRootTables; ++i) b.gfx_root_table[i].store(0, std::memory_order_relaxed);
}

constexpr int kCompSrvTally = 128;
struct CompSrvTallyEntry {
    std::atomic<ID3D12Resource*> res{ nullptr };
    std::atomic<uint32_t>        count{ 0 };
    std::atomic<uint64_t>        meta{ 0 };
    std::atomic<uint32_t>        root_slot{ 0 };
};
CompSrvTallyEntry g_comp_srv_tally[kCompSrvTally];
std::atomic<uint32_t> g_dbg_comp_srv_noheap{ 0 };
std::atomic<uint32_t> g_dbg_comp_srv_roots{ 0 };
std::atomic<uint32_t> g_dbg_comp_srv_scanned{ 0 };
std::atomic<uint32_t> g_dbg_comp_srv_resolved{ 0 };
std::atomic<uint32_t> g_dbg_comp_srv_nomap{ 0 };
std::atomic<size_t>   g_dbg_comp_srv_heap_cpu{ 0 };
std::atomic<uint64_t> g_dbg_comp_srv_heap_gpu{ 0 };
std::atomic<uint32_t> g_dbg_comp_srv_heap_count{ 0 };
std::atomic<size_t>   g_dbg_comp_srv_first_miss_cpu{ 0 };
std::atomic<uint64_t> g_dbg_comp_srv_first_miss_gpu{ 0 };
std::atomic<ID3D12Resource*> g_srcskip_target{ nullptr };
std::atomic<uint64_t>        g_srcskip_meta{ 0 };
std::atomic<uint32_t>        g_srcskip_key{ 0xFFFFFFFFu };
std::atomic<uint32_t>        g_dbg_srcskip_samples{ 0 };
std::atomic<uint32_t>        g_dbg_srcskip_draws{ 0 };
std::atomic<uint32_t>        g_dbg_uifinal_scans{ 0 };
std::atomic<uint32_t>        g_dbg_uifinal_hits{ 0 };
std::atomic<uint32_t>        g_dbg_uifinal_skips{ 0 };
std::atomic<uint32_t>        g_dbg_uifinal_noheap{ 0 };
std::atomic<ID3D12Resource*> g_uifinal_latest_sample{ nullptr };
std::atomic<ID3D12Resource*> g_uifinal_latest_origin{ nullptr };
std::atomic<ID3D12Resource*> g_uifinal_latest_rt{ nullptr };
std::atomic<uint64_t>        g_uifinal_latest_sample_meta{ 0 };
std::atomic<uint64_t>        g_uifinal_latest_rt_meta{ 0 };
std::atomic<uint64_t>        g_uifinal_latest_ms{ 0 };
std::atomic<uint32_t>        g_uifinal_latest_root_slot{ 0xFFFFFFFFu };
std::atomic<uint32_t>        g_uifinal_latest_depth{ 0 };
std::atomic<uint32_t>        g_dbg_uifinal_props{ 0 };
std::atomic<ID3D12Resource*> g_uifinal_mirror_latest{ nullptr };
std::atomic<uint64_t>        g_uifinal_mirror_latest_meta{ 0 };
std::atomic<uint64_t>        g_uifinal_mirror_latest_ms{ 0 };

void ClearUirFullTargets() {
    g_uir_target_res = nullptr;
    g_uir_target_count.store(0, std::memory_order_relaxed);
    for (int i = 0; i < kUirFullTargetSlots; ++i) {
        g_uir_full_target_res[i].store(nullptr, std::memory_order_release);
        g_uir_full_target_meta[i].store(0, std::memory_order_relaxed);
        g_uir_full_target_count[i].store(0, std::memory_order_relaxed);
    }
    {
        std::scoped_lock lk(g_uir_cap_mtx);
        g_uir_cap_counts.clear();
    }
    g_uifinal_mirror_latest.store(nullptr, std::memory_order_relaxed);
    g_uifinal_mirror_latest_meta.store(0, std::memory_order_relaxed);
    g_uifinal_mirror_latest_ms.store(0, std::memory_order_relaxed);
}

uint32_t SrcskipRequestKey() {
    const int slot = g_ctl_src_slot.load(std::memory_order_relaxed);
    if (slot < 0) return 0xFFFFFFFFu;
    return ((uint32_t)(g_ctl_src_root.load(std::memory_order_relaxed) & 0xFFFF) << 16) | (uint32_t)(slot & 0xFFFF);
}
void RefreshSrcskipRequest() {
    const uint32_t want = SrcskipRequestKey();
    uint32_t cur = g_srcskip_key.load(std::memory_order_relaxed);
    if (cur == want) return;
    if (g_srcskip_key.compare_exchange_strong(cur, want, std::memory_order_relaxed)) {
        g_srcskip_target.store(nullptr, std::memory_order_relaxed);
        g_srcskip_meta.store(0, std::memory_order_relaxed);
    }
}

void TallyCompSrv(ID3D12Resource* res, uint64_t meta, uint32_t root, uint32_t slot) {
    if (!res) return;
    const uintptr_t v = reinterpret_cast<uintptr_t>(res);
    uint32_t i = (uint32_t)((v >> 6) * 2654435761u) & (kCompSrvTally - 1);
    for (int n = 0; n < kCompSrvTally; ++n, i = (i + 1) & (kCompSrvTally - 1)) {
        ID3D12Resource* cur = g_comp_srv_tally[i].res.load(std::memory_order_relaxed);
        if (cur == res) {
            g_comp_srv_tally[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (cur == nullptr) {
            ID3D12Resource* expected = nullptr;
            if (g_comp_srv_tally[i].res.compare_exchange_strong(expected, res, std::memory_order_relaxed)) {
                g_comp_srv_tally[i].meta.store(meta, std::memory_order_relaxed);
                g_comp_srv_tally[i].root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
            } else if (g_comp_srv_tally[i].res.load(std::memory_order_relaxed) != res) {
                continue;
            }
            g_comp_srv_tally[i].count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
}

void TallyCompositeSrvsForThread() {
    RefreshSrcskipRequest();
    BindLF& b = g_bind_lf[tid_slot()];
    const size_t cpu_base = b.srv_cpu_base.load(std::memory_order_relaxed);
    const uint64_t gpu_base = b.srv_gpu_base.load(std::memory_order_relaxed);
    const uint32_t count = b.srv_count.load(std::memory_order_relaxed);
    const uint32_t inc = g_srv_inc.load(std::memory_order_relaxed);
    g_dbg_comp_srv_heap_cpu.store(cpu_base, std::memory_order_relaxed);
    g_dbg_comp_srv_heap_gpu.store(gpu_base, std::memory_order_relaxed);
    g_dbg_comp_srv_heap_count.store(count, std::memory_order_relaxed);
    if (!cpu_base || !gpu_base || !count || !inc) {
        g_dbg_comp_srv_noheap.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const uint64_t heap_bytes = (uint64_t)count * (uint64_t)inc;
    for (uint32_t root = 0; root < (uint32_t)kMaxGfxRootTables; ++root) {
        const uint64_t table = b.gfx_root_table[root].load(std::memory_order_relaxed);
        if (!table) continue;
        g_dbg_comp_srv_roots.fetch_add(1, std::memory_order_relaxed);
        if (table < gpu_base || table >= gpu_base + heap_bytes) continue;
        const uint64_t first = (table - gpu_base) / inc;
        for (uint32_t slot = 0; slot < 32 && first + slot < count; ++slot) {
            const size_t cpu = cpu_base + (size_t)(first + slot) * inc;
            ID3D12Resource* res = nullptr; uint64_t meta = 0; uint32_t view = 0;
            g_dbg_comp_srv_scanned.fetch_add(1, std::memory_order_relaxed);
            if (TryResolveSrv(cpu, &res, &meta, &view)) {
                if (res && meta) {
                    g_dbg_comp_srv_resolved.fetch_add(1, std::memory_order_relaxed);
                    TallyCompSrv(res, meta, root, slot);
                    TallyUiLineageComposite(res, meta, root, slot);
                    if ((((root & 0xFFFFu) << 16) | (slot & 0xFFFFu)) == g_srcskip_key.load(std::memory_order_relaxed)) {
                        g_srcskip_target.store(res, std::memory_order_relaxed);
                        g_srcskip_meta.store(meta, std::memory_order_relaxed);
                        g_dbg_srcskip_samples.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            } else {
                size_t zero_cpu = 0;
                if (g_dbg_comp_srv_first_miss_cpu.compare_exchange_strong(zero_cpu, cpu, std::memory_order_relaxed)) {
                    g_dbg_comp_srv_first_miss_gpu.store(table + (uint64_t)slot * inc, std::memory_order_relaxed);
                }
                g_dbg_comp_srv_nomap.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

bool UiFinalSampleMatch(ID3D12Resource* res, uint64_t meta, ID3D12Resource* primary_atlas, bool broad_lineage,
                        ID3D12Resource** out_origin, uint32_t* out_depth) {
    if (out_origin) *out_origin = res;
    if (out_depth) *out_depth = 0;
    if (!res) return false;

    if (primary_atlas && res == primary_atlas) {
        return true;
    }

    UiLineageInfo lin{};
    if (!LookupUiLineage(res, lin)) {
        return false;
    }

    if (out_origin) *out_origin = lin.origin ? lin.origin : res;
    if (out_depth) *out_depth = lin.depth;
    if (broad_lineage) {
        return true;
    }
    if (primary_atlas && lin.origin == primary_atlas) {
        return true;
    }

    uint32_t sw = 0, sh = 0, sf = 0;
    unpack_meta(meta ? meta : lin.meta, sw, sh, sf);
    if (sw == 288 && sh == 216) {
        return true;
    }

    const uint64_t origin_meta = RtResourceMeta(lin.origin);
    uint32_t ow = 0, oh = 0, of = 0;
    unpack_meta(origin_meta, ow, oh, of);
    return ow == 288 && oh == 216;
}

bool BoundSrvsContainUiFinalSampleForThread(ID3D12Resource** out_sample, uint64_t* out_meta,
                                            ID3D12Resource** out_origin, uint32_t* out_root,
                                            uint32_t* out_slot, uint32_t* out_depth,
                                            bool broad_lineage = false,
                                            bool strict_final_sample = false) {
    if (out_sample) *out_sample = nullptr;
    if (out_meta) *out_meta = 0;
    if (out_origin) *out_origin = nullptr;
    if (out_root) *out_root = 0xFFFFFFFFu;
    if (out_slot) *out_slot = 0xFFFFFFFFu;
    if (out_depth) *out_depth = 0;

    BindLF& b = g_bind_lf[tid_slot()];
    const size_t cpu_base = b.srv_cpu_base.load(std::memory_order_relaxed);
    const uint64_t gpu_base = b.srv_gpu_base.load(std::memory_order_relaxed);
    const uint32_t count = b.srv_count.load(std::memory_order_relaxed);
    const uint32_t inc = g_srv_inc.load(std::memory_order_relaxed);
    if (!cpu_base || !gpu_base || !count || !inc) {
        g_dbg_uifinal_noheap.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    ID3D12Resource* primary_atlas = g_ui_atlas_latest.load(std::memory_order_relaxed);
    const uint64_t atlas_seen_ms = g_ui_atlas_latest_ms.load(std::memory_order_relaxed);
    if (primary_atlas && GetTickCount64() - atlas_seen_ms > 2000) {
        primary_atlas = nullptr;
    }

    const uint64_t heap_bytes = (uint64_t)count * (uint64_t)inc;
    for (uint32_t root = 0; root < (uint32_t)kMaxGfxRootTables; ++root) {
        const uint64_t table = b.gfx_root_table[root].load(std::memory_order_relaxed);
        if (!table || table < gpu_base || table >= gpu_base + heap_bytes) continue;
        const uint64_t first = (table - gpu_base) / inc;
        for (uint32_t slot = 0; slot < 32 && first + slot < count; ++slot) {
            const size_t cpu = cpu_base + (size_t)(first + slot) * inc;
            ID3D12Resource* res = nullptr;
            uint64_t meta = 0;
            uint32_t view = 0;
            g_dbg_uifinal_scans.fetch_add(1, std::memory_order_relaxed);
            if (!TryResolveSrv(cpu, &res, &meta, &view) || !res) {
                continue;
            }
            ID3D12Resource* origin = nullptr;
            uint32_t depth = 0;
            if (!UiFinalSampleMatch(res, meta, primary_atlas, broad_lineage, &origin, &depth)) {
                continue;
            }
            if (strict_final_sample && !(primary_atlas && res == primary_atlas)) {
                const uint64_t sample_meta = meta ? meta : RtResourceMeta(res);
                if (!IsStrictUiFinalSampleMeta(sample_meta)) {
                    continue;
                }
            }
            if (out_sample) *out_sample = res;
            if (out_meta) *out_meta = meta;
            if (out_origin) *out_origin = origin;
            if (out_root) *out_root = root;
            if (out_slot) *out_slot = slot;
            if (out_depth) *out_depth = depth;
            return true;
        }
    }
    return false;
}

bool PropagateBoundUiLineageToTargetForThread(ID3D12Resource* target, uint64_t target_meta,
                                              bool target_is_final_like,
                                              bool strict_final_sample = false) {
    if (!target || !target_meta) return false;

    ID3D12Resource* sample = nullptr;
    ID3D12Resource* origin = nullptr;
    uint64_t sample_meta = 0;
    uint32_t root = 0xFFFFFFFFu, slot = 0xFFFFFFFFu, depth = 0;
    if (!BoundSrvsContainUiFinalSampleForThread(&sample, &sample_meta, &origin, &root, &slot, &depth,
                                                true, strict_final_sample)) {
        return false;
    }
    if (!sample || sample == target) {
        return false;
    }

    RecordUiLineageResource(target, origin ? origin : sample, target_meta, depth + 1);
    g_dbg_uifinal_props.fetch_add(1, std::memory_order_relaxed);

    if (target_is_final_like) {
        g_uil_candidate.store(target, std::memory_order_relaxed);
        g_uil_candidate_origin.store(origin ? origin : sample, std::memory_order_relaxed);
        g_uil_candidate_meta.store(target_meta, std::memory_order_relaxed);
        g_uil_candidate_frame.store(g_ui_lineage_frame.load(std::memory_order_relaxed), std::memory_order_relaxed);
        g_uil_candidate_root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
        g_uil_candidate_depth.store(depth + 1, std::memory_order_relaxed);
        g_dbg_uil_candidate_hits.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

void CaptureOverlayBoundSrvsForThread(BindLF& b) {
    if (t_overlay_scope_depth <= 0 && t_overlay_flush_depth <= 0) return;
    g_dbg_overlay_draw_calls.fetch_add(1, std::memory_order_relaxed);

    const size_t cpu_base = b.srv_cpu_base.load(std::memory_order_relaxed);
    const uint64_t gpu_base = b.srv_gpu_base.load(std::memory_order_relaxed);
    const uint32_t count = b.srv_count.load(std::memory_order_relaxed);
    const uint32_t inc = g_srv_inc.load(std::memory_order_relaxed);
    if (!cpu_base || !gpu_base || !count || !inc) {
        g_dbg_overlay_root_noheap.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t heap_bytes = (uint64_t)count * (uint64_t)inc;
    const uint32_t preferred_root = g_overlay_latest_param_slot.load(std::memory_order_relaxed);
    ID3D12Resource* best_res = nullptr;
    uint64_t best_meta = 0;
    size_t best_cpu = 0;
    uint32_t best_root = 0xFFFFFFFFu;
    uint32_t best_slot = 0xFFFFFFFFu;
    uint64_t best_score = 0;

    auto consider = [&](uint32_t root, uint32_t slot) {
        if (root >= (uint32_t)kMaxGfxRootTables) return;
        const uint64_t table = b.gfx_root_table[root].load(std::memory_order_relaxed);
        if (!table || table < gpu_base || table >= gpu_base + heap_bytes) return;
        const uint64_t first = (table - gpu_base) / inc;
        if (first + slot >= count) return;
        const size_t cpu = cpu_base + (size_t)(first + slot) * inc;
        ID3D12Resource* res = nullptr;
        uint64_t meta = 0;
        uint32_t view = 0;
        g_dbg_overlay_root_scanned.fetch_add(1, std::memory_order_relaxed);
        if (!TryResolveSrv(cpu, &res, &meta, &view) || !res || !meta) return;

        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(meta, w, h, f);
        if (w < 2 || h < 2) return; // ignore default/white 1x1 descriptors.

        uint64_t score = (uint64_t)w * (uint64_t)h;
        if (root == preferred_root) score += (1ull << 62);
        if (slot == 1) score += (1ull << 61); // OverlayRendererPSParameters builds {slot0, texture slot1}.
        if (!IsEyeSource(res)) score += (1ull << 60);
        if (score <= best_score) return;

        best_score = score;
        best_res = res;
        best_meta = meta;
        best_cpu = cpu;
        best_root = root;
        best_slot = slot;
    };

    auto scan_root = [&](uint32_t root) {
        if (root >= (uint32_t)kMaxGfxRootTables) return;
        consider(root, 1);
        consider(root, 0);
        for (uint32_t slot = 2; slot < 16; ++slot) {
            consider(root, slot);
        }
    };

    if (preferred_root < (uint32_t)kMaxGfxRootTables) {
        scan_root(preferred_root);
    }
    if (!best_res) {
        for (uint32_t root = 0; root < (uint32_t)kMaxGfxRootTables; ++root) {
            if (root == preferred_root) continue;
            scan_root(root);
            if (best_res && best_slot == 1) break;
        }
    }

    if (!best_res) return;
    g_overlay_srv_latest.store(best_res, std::memory_order_release);
    g_overlay_srv_latest_meta.store(best_meta, std::memory_order_relaxed);
    g_overlay_srv_latest_cpu.store(best_cpu, std::memory_order_relaxed);
    g_overlay_srv_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
    g_overlay_latest_root_slot.store(best_root, std::memory_order_relaxed);
    g_overlay_latest_srv_slot.store(best_slot, std::memory_order_relaxed);
    g_dbg_overlay_root_resolved.fetch_add(1, std::memory_order_relaxed);
    g_dbg_overlay_resolved.fetch_add(1, std::memory_order_relaxed);
}

// Full-screen display-format check, computed AT CREATE-RTV TIME (in Hook_RTV) on the resource the app just
// handed to CreateRenderTargetView (provably alive), and cached in the RTV ring. The bind/draw hot path reads
// only that cached int -- it never calls GetDesc on a resolved pointer, which during scene-transition resource
// churn can be a freed COM object (a wild vtable call past SEH). The HUD is drawn into a full-screen
// display-format INTERMEDIATE (e.g. PIX resource 4099, RGBA8) that is then CopyResource'd to the swapchain the
// eye reads -- so gating on this (not just the registered eye-source) catches the HUD draws. Excludes the HDR
// scene buffer (R11G11B10 = format 26), which is NOT a display format.
bool IsFullscreenFmtLF(ID3D12Resource* res) {
    if (!res) return false;
    UINT64 w = 0; UINT h = 0, f = 0;
    if (!GetResInfoSEH(res, w, h, f)) return false;
    return w >= 1000 && h >= 500 &&
        (f == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM      || f == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM ||
         f == (UINT)DXGI_FORMAT_R8G8B8A8_TYPELESS   || f == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
         f == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == (UINT)DXGI_FORMAT_R10G10B10A2_UNORM);
}

void STDMETHODCALLTYPE Hook_OMSetRT(ID3D12GraphicsCommandList* self, UINT num,
    const D3D12_CPU_DESCRIPTOR_HANDLE* rts, BOOL single, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    g_hk_omsetrt->get_original<FnOMSetRT>()(self, num, rts, single, dsv);   // bind normally; we only RECORD
    __try {
        if (g_uir_rt_valid.load(std::memory_order_relaxed)) {
            const size_t rtvp = (num >= 1 && rts) ? (size_t)rts[0].ptr : 0;
            int fs = 0; uint64_t meta = 0;
            ID3D12Resource* res = rtvp ? ResolveRtv(rtvp, &fs, &meta) : nullptr;   // global lock-free handle->res map
            BindLF& b = g_bind_lf[tid_slot()];
            b.bb_res.store(res, std::memory_order_relaxed);
            b.bb_rtv.store(rtvp, std::memory_order_relaxed);
            b.dsv.store(dsv ? (size_t)dsv->ptr : 0, std::memory_order_relaxed);
            b.has_dsv.store(dsv != nullptr ? 1 : 0, std::memory_order_relaxed);
            b.bb_fs.store(fs, std::memory_order_relaxed);   // cached at CreateRTV time -- NO GetDesc on cached ptr
            b.meta.store(meta, std::memory_order_relaxed);  // cached dims/format (UAF-safe)
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_SetPSO(ID3D12GraphicsCommandList* self, ID3D12PipelineState* pso) {
    g_hk_setpso->get_original<FnSetPSO>()(self, pso);
    __try { if (g_uir_rt_valid.load(std::memory_order_relaxed)) g_bind_lf[tid_slot()].pso.store(pso, std::memory_order_relaxed); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_SetDescriptorHeaps(ID3D12GraphicsCommandList* self, UINT num,
    ID3D12DescriptorHeap* const* heaps) {
    g_hk_setheaps->get_original<FnSetDescriptorHeaps>()(self, num, heaps);
    __try {
        BindLF& b = g_bind_lf[tid_slot()];
        size_t cpu = 0; uint64_t gpu = 0; uint32_t count = 0;
        for (UINT i = 0; i < num && heaps; ++i) {
            ID3D12DescriptorHeap* heap = heaps[i];
            if (!heap) continue;
            D3D12_DESCRIPTOR_HEAP_DESC d = heap->GetDesc();
            if (d.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
                cpu = (size_t)heap->GetCPUDescriptorHandleForHeapStart().ptr;
                gpu = (uint64_t)heap->GetGPUDescriptorHandleForHeapStart().ptr;
                count = d.NumDescriptors;
                break;
            }
        }
        b.srv_cpu_base.store(cpu, std::memory_order_relaxed);
        b.srv_gpu_base.store(gpu, std::memory_order_relaxed);
        b.srv_count.store(count, std::memory_order_relaxed);
        ClearGfxRootTables(b);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_SetGfxRootSig(ID3D12GraphicsCommandList* self, ID3D12RootSignature* sig) {
    g_hk_setgrs->get_original<FnSetGfxRootSig>()(self, sig);
    __try { ClearGfxRootTables(g_bind_lf[tid_slot()]); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void STDMETHODCALLTYPE Hook_SetGfxRootDescriptorTable(ID3D12GraphicsCommandList* self, UINT root,
    D3D12_GPU_DESCRIPTOR_HANDLE base) {
    g_hk_setgrdt->get_original<FnSetGfxRootDescriptorTable>()(self, root, base);
    __try {
        if (root < (UINT)kMaxGfxRootTables) {
            g_bind_lf[tid_slot()].gfx_root_table[root].store((uint64_t)base.ptr, std::memory_order_relaxed);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Detect the assembled HUD layer: a full-screen RGBA8 RT cleared to FULLY TRANSPARENT (alpha=0). No C++
// unwinding objects in the __try body (ResolveRtvResource/GetResInfoSEH lock/__try internally) -> C2712-safe.
void STDMETHODCALLTYPE Hook_ClearRTV(ID3D12GraphicsCommandList* self, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    const FLOAT* color, UINT n, const D3D12_RECT* rects) {
    g_hk_clearrtv->get_original<FnClearRTV>()(self, rtv, color, n, rects);
    // (HUD-layer transparent-clear detection removed: it was a dead-end mechanism AND took a global lock +
    //  GetDesc per clear. The redirect's own UI-RT clear in BeginDrawRedirect calls this trampoline directly.)
}

// COPY hooks: detect the HUD-delivery mechanism into the eye-source. dst/src are passed by the caller (alive at
// call time) -- IsEyeSource is a pointer-compare (no deref). When dst is a registered eye-source, the SOURCE is
// the HUD-bearing buffer we want on the quad (and whose draws we'd peel). Diagnostic-only for now (no mutation).
void STDMETHODCALLTYPE Hook_CopyResource(ID3D12GraphicsCommandList* self, ID3D12Resource* dst, ID3D12Resource* src) {
    g_hk_copyres->get_original<FnCopyResource>()(self, dst, src);
    __try {
        PropagateUiLineageCopy(dst, src);
        if (g_uir_rt_valid.load(std::memory_order_relaxed) && IsEyeSource(dst)) {
            g_dbg_copy_to_eye.fetch_add(1, std::memory_order_relaxed);
            g_dbg_last_copysrc.store(src, std::memory_order_relaxed);
            g_dbg_last_copydst.store(dst, std::memory_order_relaxed);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
void STDMETHODCALLTYPE Hook_CopyTexRegion(ID3D12GraphicsCommandList* self, const D3D12_TEXTURE_COPY_LOCATION* dst,
    UINT x, UINT y, UINT z, const D3D12_TEXTURE_COPY_LOCATION* src, const D3D12_BOX* box) {
    g_hk_copytex->get_original<FnCopyTexRegion>()(self, dst, x, y, z, src, box);
    __try {
        ID3D12Resource* d = dst ? dst->pResource : nullptr;
        ID3D12Resource* s = src ? src->pResource : nullptr;
        PropagateUiLineageCopy(d, s);
        if (g_uir_rt_valid.load(std::memory_order_relaxed) && IsEyeSource(d)) {
            g_dbg_copy_to_eye.fetch_add(1, std::memory_order_relaxed);
            g_dbg_last_copysrc.store(s, std::memory_order_relaxed);
            g_dbg_last_copydst.store(d, std::memory_order_relaxed);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// POD so it is legal alongside __try in the draw hooks (no unwinding).
// skip=true -> the draw hook SKIPS the original draw entirely (drops the HUD draw from the eye-source so the
// eyes stay world-only). No command-list mutation, no RTV rebind, no format risk: the safest peel.
// replay_original=true -> the hook first draws into the rebound target, restores the game RT, then executes the
// same original draw again. This is diagnostic capture: keep the eye image unchanged while filling a mirror RT.
struct RedirectScope {
    bool active{false};
    bool skip{false};
    bool replay_original{false};
    bool clear_original_after_replay{false};
    bool capture_pre_ui{false};
    ID3D12Resource* capture_res{nullptr};
    uint64_t capture_meta{0};
    D3D12_CPU_DESCRIPTOR_HANDLE bb_rtv{};
    bool has_dsv{false};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
};

bool BeginOverlayRtMirror(ID3D12GraphicsCommandList* self, const BindLF& b, ID3D12Resource* bound_res,
                          uint64_t bound_meta, RedirectScope& sc, bool replay_original) {
    if (!self || !bound_res || !bound_meta || t_overlay_scope_depth <= 0 ||
        !g_hk_omsetrt || !g_hk_clearrtv || !g_uir_ui_rt || g_uir_ui_rtv.ptr == 0) {
        return false;
    }

    uint32_t bw = 0, bh = 0, bf = 0;
    unpack_meta(bound_meta, bw, bh, bf);
    const uint32_t target_w = g_uir_rt_w.load(std::memory_order_relaxed);
    const uint32_t target_h = g_uir_rt_h.load(std::memory_order_relaxed);
    const uint32_t target_f = g_uir_rt_fmt.load(std::memory_order_relaxed);
    if (bw != target_w || bh != target_h || bf != target_f || !IsDisplayFmt(bf) ||
        bound_res == g_uir_ui_rt) {
        g_dbg_overlay_rt_rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    if (!g_uir_cleared.exchange(true, std::memory_order_relaxed)) {
        const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_hk_clearrtv->get_original<FnClearRTV>()(self, g_uir_ui_rtv, zero, 0, nullptr);
        g_dbg_overlay_rt_clears.fetch_add(1, std::memory_order_relaxed);
    }

    g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &g_uir_ui_rtv, FALSE, nullptr);
    sc.active = true;
    sc.replay_original = replay_original; // mode 19 mirrors+replays; mode 27 steals into the quad target.
    sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed);
    sc.has_dsv = b.has_dsv.load(std::memory_order_relaxed) != 0;
    sc.dsv.ptr = b.dsv.load(std::memory_order_relaxed);
    g_dbg_overlay_rt_draws.fetch_add(1, std::memory_order_relaxed);
    if (replay_original) {
        g_dbg_overlay_rt_replayed.fetch_add(1, std::memory_order_relaxed);
    }
    g_overlay_rt_latest_meta.store(pack_meta(target_w, target_h, target_f), std::memory_order_relaxed);
    g_overlay_rt_latest_ms.store(GetTickCount64(), std::memory_order_release);
    return true;
}

std::atomic<uint32_t> g_dbg_bb_draws{ 0 };          // draws while the backbuffer is bound
std::atomic<uint32_t> g_dbg_composite_draws{ 0 };   // ...of which use the composite PSO
std::atomic<uint32_t> g_dbg_ui_draws{ 0 };          // ...after composite, non-composite PSO (=UI, redirected)
// PINPOINT diagnostics: where does the composite-detection chain break? (counted per draw, pre-gate)
std::atomic<uint32_t> g_dbg_compbound{ 0 };  // bound draws whose PSO is a composite PSO (REGARDLESS of RT gate)
std::atomic<uint32_t> g_dbg_bbfs{ 0 };       // bound draws whose RT[0] is a full-screen display-format buffer
std::atomic<uint32_t> g_dbg_eyesrc{ 0 };     // bound draws whose RT[0] is a registered eye-source
std::atomic<uint32_t> g_dbg_gatepass{ 0 };   // bound draws passing the (bb_fs || eye-source) gate
std::atomic<uint32_t> g_dbg_uipso_draws{ 0 }; // draws using known UI PSOs (capture-hash tagged)
std::atomic<uint32_t> g_dbg_uipso_null{ 0 };  // ...with no resolved RT binding
std::atomic<uint32_t> g_dbg_uipso_fs{ 0 };    // ...into a full-screen display-format RT
std::atomic<uint32_t> g_dbg_uipso_eye{ 0 };   // ...into a registered eye-source
std::atomic<uint32_t> g_dbg_uipso_skips{ 0 }; // ...dropped by uiredirect=5
std::atomic<uint32_t> g_dbg_uimirror_native_clears{ 0 };

// DIAGNOSTIC: per-frame ordered sequence of backbuffer-bound draws (bit31=composite PSO, low bits=draw size).
// Reveals WHERE the HUD draws sit relative to the world composite on the eye-source backbuffer.
std::mutex g_bbseq_mtx;
std::vector<uint32_t> g_bbseq;
void RecordBbSeq(bool composite, uint32_t size) {
    std::scoped_lock lk(g_bbseq_mtx);
    if (g_bbseq.size() < 256) g_bbseq.push_back((composite ? 0x80000000u : 0u) | (size & 0x7FFFFFFFu));
}

void RecordOverlayDrawRt0ForThread(const BindLF& b, UINT drawSize) {
    if (t_overlay_scope_depth <= 0 && t_overlay_flush_depth <= 0) return;

    ID3D12Resource* rt0 = b.bb_res.load(std::memory_order_relaxed);
    if (!rt0) return;

    const uint64_t meta = b.meta.load(std::memory_order_relaxed);
    uint32_t w = 0, h = 0, f = 0;
    unpack_meta(meta, w, h, f);
    const bool fs = w >= 1000 && h >= 500;
    const bool eye = IsEyeSource(rt0);
    ID3D12Resource* native = g_overlay_native_target_latest.load(std::memory_order_relaxed);
    const bool native_match = native != nullptr && native == rt0;

    uint32_t flags = 0;
    if (t_overlay_scope_depth > 0) flags |= 1u;
    if (t_overlay_flush_depth > 0) flags |= 2u;
    if (fs) flags |= 4u;
    if (eye) flags |= 8u;
    if (native_match) flags |= 16u;

    g_dbg_overlay_draw_rt0_draws.fetch_add(1, std::memory_order_relaxed);
    if (eye) g_dbg_overlay_draw_rt0_eye.fetch_add(1, std::memory_order_relaxed);
    if (fs) g_dbg_overlay_draw_rt0_fs.fetch_add(1, std::memory_order_relaxed);
    if (native_match) g_dbg_overlay_draw_rt0_native_match.fetch_add(1, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest.store(rt0, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_meta.store(meta, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_size.store(drawSize, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_flags.store(flags, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_rtv.store(b.bb_rtv.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

// RenderDoc cross-reference (rdc_lookright): the world composite AND the HUD draws all target a full-screen
// RGBA8 SWAPCHAIN-FORMAT RT (45167); the composite is first (world tonemap, samples world-only), the HUD draws
// follow. So redirect by RT SIGNATURE (any full-screen RGBA8 target), not by a specific registered buffer ---
// robust across the modded per-eye double/triple-buffer dance. Cache the per-resource verdict.
std::mutex g_fs_mtx;
std::unordered_map<ID3D12Resource*, int> g_fs_cache;   // -> 1 = full-screen swapchain-format RGBA8, 0 = no
bool IsFullscreenSwapFormat(ID3D12Resource* res) {
    if (!res) return false;
    { std::scoped_lock lk(g_fs_mtx); auto it = g_fs_cache.find(res); if (it != g_fs_cache.end()) return it->second == 1; }
    UINT64 w = 0; UINT h = 0, f = 0; const bool ok = GetResInfoSEH(res, w, h, f);
    const bool fs = ok && w >= 1000 && h >= 500 &&
        (f == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM || f == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM ||
         f == (UINT)DXGI_FORMAT_R8G8B8A8_TYPELESS || f == (UINT)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
         f == (UINT)DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == (UINT)DXGI_FORMAT_R10G10B10A2_UNORM);
    std::scoped_lock lk(g_fs_mtx); g_fs_cache[res] = fs ? 1 : 0; return fs;
}

RedirectScope BeginDrawRedirect(ID3D12GraphicsCommandList* self, UINT drawSize) {
    RedirectScope sc;
    const int mode = g_ctl_ui_redirect.load(std::memory_order_relaxed);
    if (mode == 0 || !g_uir_rt_valid.load(std::memory_order_relaxed)) return sc;
    g_dbg_total_draws.fetch_add(1, std::memory_order_relaxed);
    BindLF& b = g_bind_lf[tid_slot()];                  // LOCK-FREE per-thread current binding
    CaptureOverlayBoundSrvsForThread(b);
    RecordOverlayDrawRt0ForThread(b, drawSize);
    ID3D12Resource* res = b.bb_res.load(std::memory_order_relaxed);
    void* cur_pso = b.pso.load(std::memory_order_relaxed);
    const bool is_ui_pso = IsUiPso(cur_pso);
    const bool in_ui_scope = t_ui_pass_depth > 0 ||
        g_ui_pass_depth.load(std::memory_order_acquire) > 0 ||
        t_overlay_flush_depth > 0;
    if (is_ui_pso) g_dbg_uipso_draws.fetch_add(1, std::memory_order_relaxed);
    if (mode == 5 && is_ui_pso) {
        g_dbg_uipso_skips.fetch_add(1, std::memory_order_relaxed);
        sc.skip = true;
        return sc;
    }
    if (!res) {
        if (in_ui_scope) {
            g_dbg_uipass_draws.fetch_add(1, std::memory_order_relaxed);
            g_dbg_uipass_nobind.fetch_add(1, std::memory_order_relaxed);
        }
        if (is_ui_pso) g_dbg_uipso_null.fetch_add(1, std::memory_order_relaxed);
        g_dbg_nullres_draws.fetch_add(1, std::memory_order_relaxed);
        return sc;
    }
    g_dbg_bound_draws.fetch_add(1, std::memory_order_relaxed);

    // DISCOVERY: tally this draw against its RT if the RT is full-screen-sized (any format). Reveals the HUD's
    // off-screen buffer (full-screen RT, moderate draw burst) that the world composite samples into the eye.
    const uint64_t bmeta = b.meta.load(std::memory_order_relaxed);
    const bool is_eyesrc_bound = IsEyeSource(res);
    const bool is_comp = IsCompositePso(cur_pso);
    uint32_t bound_w = 0, bound_h = 0, bound_f = 0;
    unpack_meta(bmeta, bound_w, bound_h, bound_f);
    const bool fs_sized = bound_w >= 1000 && bound_h >= 500;
    const bool display_fmt = fs_sized && IsDisplayFmt(bound_f);
    if (in_ui_scope) {
        g_dbg_uipass_draws.fetch_add(1, std::memory_order_relaxed);
        if (fs_sized || is_eyesrc_bound) {
            g_dbg_uipass_bb.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_dbg_uipass_off.fetch_add(1, std::memory_order_relaxed);
        }
        TallyUiRtDraw(res, bmeta);
    }
    {
        TallyPsoDraw(cur_pso, bmeta, fs_sized, display_fmt, is_eyesrc_bound, drawSize);
        if (fs_sized) TallyFsDraw(res, bmeta);
        if (is_ui_pso) {
            TallyUiPsoRt(res, bmeta);
            NoteUiPsoWrite(res, bmeta);
            if (fs_sized) g_dbg_uipso_fs.fetch_add(1, std::memory_order_relaxed);
            if (is_eyesrc_bound) g_dbg_uipso_eye.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // MODE 30 (engine-seam steal): the clean D3D12 translation of FO2-VR's CameraView::DrawHUD RT-swap and
    // fox3's "route the UI pass to a dedicated RT". Live RE (FH5VR.log) showed FH5 draws its HUD as geometry
    // (quads/text, drawSize>=4) STRAIGHT INTO the registered eye-source (the f28 buffer the mod copies to the VR
    // eye), AFTER the world->eye tonemap. So:
    //   CASE C (gameplay + in-game menus): for full-screen draws into an eye-source, KEEP the first draw (world
    //   tonemap) and every fullscreen-triangle post-fx pass (drawSize<=3), and STEAL each subsequent HUD-geometry
    //   draw (drawSize>=4) into a FORMAT-MATCHED mirror with NO replay -> the eye keeps world+post-fx (clean),
    //   the mirror accumulates the laid-out HUD for the quad.
    //   CASE A (front-end menus that draw UI into a NON-eye display buffer matching our UI RT): rebind to the
    //   single UI RT, no replay.
    // Format-matched mirrors guarantee the rebind never violates the PSO's RTV format (mismatch = device removal).
    if (mode == 30) {
        // Redirect HUD draws into the single FORMAT-MATCHED UI RT (no on-the-fly mirror creation -> no resource
        // churn during scene transitions, which is what was destabilising the My-Cars->free-roam hand-off).
        // The UI RT is sized/formatted to the live backbuffer, which equals the eye-source format in gameplay,
        // so the rebind is always format-legal (a mismatch on OMSetRenderTargets = device removal).
        const bool fmt_ok = g_hk_omsetrt && fs_sized && IsDisplayFmt(bound_f) &&
                            g_uir_ui_rt && g_uir_ui_rtv.ptr != 0 &&
                            bound_f == g_uir_rt_fmt.load(std::memory_order_relaxed);
        bool steal = false;
        if (fmt_ok) {
            if (is_eyesrc_bound) {
                // Gameplay/in-game HUD: FH5 draws the HUD straight into the eye-source AFTER the world->eye
                // tonemap. Keep the first full-screen draw (tonemap) + fullscreen-triangle post-fx (drawSize<=3);
                // steal the HUD = the FH5 UI PSO (FH5PSOHOT tags it [UI], PS hash B66723DB) or HUD-geometry quads
                // (drawSize>=4) after the first draw. World stays intact; the UI RT collects the laid-out HUD.
                const bool first_eye_draw = MarkEyeFirstSeen(res);
                steal = is_ui_pso || (!first_eye_draw && drawSize >= 4);
            } else if (in_ui_scope && !is_comp) {
                // Front-end menus/showroom: UI drawn into a non-eye display buffer during the UIRenderer pass.
                steal = true;
            }
        }
        if (steal) {
            if (!g_uir_cleared.exchange(true, std::memory_order_relaxed) && g_hk_clearrtv) {
                const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                g_hk_clearrtv->get_original<FnClearRTV>()(self, g_uir_ui_rtv, zero, 0, nullptr);
            }
            g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &g_uir_ui_rtv, FALSE, nullptr);
            sc.active = true;
            sc.replay_original = false;                              // clean eyes: do NOT draw into the native target
            sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed); // restore the game's RT after our draw
            sc.has_dsv    = b.has_dsv.load(std::memory_order_relaxed) != 0;
            sc.dsv.ptr    = b.dsv.load(std::memory_order_relaxed);
            g_uir_redirects.fetch_add(1, std::memory_order_relaxed);
            g_dbg_ui_draws.fetch_add(1, std::memory_order_relaxed);
        }
        return sc;
    }

    // MODE 28: final full-size UI steal. ShaderToggler/RenderDoc PSO mirrors target tiny atlas/intermediate RTs,
    // which can be black or incomplete. This path steals only full-size final UI/flush quads into a
    // same-format mirror RT, then submits that mirror as the OpenXR HUD layer without replaying native UI.
    if (mode == 28) {
        uint64_t captured_full_meta = 0;
        uint32_t captured_full_count = 0;
        const bool target_is_vf54_full_ui = in_ui_scope &&
            IsCapturedFullUiRt(res, &captured_full_meta, &captured_full_count) &&
            captured_full_count >= 8;
        const uint64_t final_meta = bmeta ? bmeta : captured_full_meta;
        uint32_t final_w = bound_w, final_h = bound_h, final_f = bound_f;
        if ((!final_w || !final_h || !final_f) && final_meta) {
            unpack_meta(final_meta, final_w, final_h, final_f);
        }
        const bool final_target_fmt = final_w >= 1000 && final_h >= 500 && IsUiFinalTargetFmt(final_f);
        const bool final_ui_draw = final_target_fmt && !is_comp && target_is_vf54_full_ui;
        if (final_ui_draw && g_hk_omsetrt) {
            if (UiMirrorSlot* mirror_slot = GetOrCreateUiMirror(res, final_meta)) {
                ID3D12Resource* mirror_rt = mirror_slot->rt.load(std::memory_order_acquire);
                const uintptr_t mirror_rtv_ptr = mirror_slot->rtv.load(std::memory_order_relaxed);
                if (mirror_rt && mirror_rtv_ptr) {
                    D3D12_CPU_DESCRIPTOR_HANDLE mirror_rtv{};
                    mirror_rtv.ptr = (SIZE_T)mirror_rtv_ptr;
                    const uint64_t frame = g_ui_lineage_frame.load(std::memory_order_relaxed);
                    if (g_hk_clearrtv && mirror_slot->clear_frame.exchange(frame, std::memory_order_relaxed) != frame) {
                        const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                        g_hk_clearrtv->get_original<FnClearRTV>()(self, mirror_rtv, zero, 0, nullptr);
                    }
                    sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed);
                    sc.has_dsv = b.has_dsv.load(std::memory_order_relaxed) != 0;
                    sc.dsv.ptr = b.dsv.load(std::memory_order_relaxed);
                    g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &mirror_rtv, FALSE, nullptr);
                    sc.active = true;
                    sc.replay_original = false;
                    mirror_slot->draws.fetch_add(1, std::memory_order_relaxed);
                    g_dbg_uimirror_draws.fetch_add(1, std::memory_order_relaxed);
                    g_dbg_uifinal_hits.fetch_add(1, std::memory_order_relaxed);
                    g_dbg_uifinal_skips.fetch_add(1, std::memory_order_relaxed);
                    g_uifinal_mirror_latest.store(mirror_rt, std::memory_order_relaxed);
                    g_uifinal_mirror_latest_meta.store(mirror_slot->meta.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    g_uifinal_mirror_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
                    return sc;
                }
            }
        }
        return sc;
    }

    // MODE 19/27: capture the composed OverlayRenderer12 output into the transparent HUD RT. Mode 19 mirrors
    // and replays the original native draw. Mode 27 is the FO2-style steal path: draw only into the quad RT,
    // then restore FH5's target without replaying, so the native eye target should stay pre-UI clean.
    if (mode == 19 || mode == 27) {
        if (BeginOverlayRtMirror(self, b, res, bmeta, sc, mode == 19)) {
            return sc;
        }
        // Keep falling through so pre-UI capture at the composite draw still runs below.
    }

    // MODE 7/8/23/26 (UI PSO mirror): mirror only the ShaderToggler-matched UI PSOs. Mode 23 submits the selected
    // mirror phase to both eyes through the OpenXR quad without suppressing FH5's native HUD. Mode 26 is the
    // phase-locked native-suppression variant: draw the matched UI PSOs into the mirror only.
    // MODE 12/13/14/15/16/17/20 (atlas stream mirror): mirror every draw into FH5's stable 288x216 R11G11B10 UI atlas. Mode 12
    // replays every original draw for diagnostics. Mode 13 replays the non-UI setup draws but suppresses only the
    // ShaderToggler-matched UI PSO draws in the native atlas, so the mirror gets a complete HUD while FH5's native
    // atlas keeps its dependencies. Mode 14 replays every original draw but clears the native atlas afterward; this
    // preserves FH5's draw-call sequence while leaving the sampled native atlas blank. Mode 15 keeps the native
    // atlas untouched here and suppresses only the later fullscreen/eye draw that samples the UI payload. Modes 16/17/20
    // additionally propagate UI lineage through shader-sampled blits so full-screen HDR UI intermediates can be used.
    // MODE 9/22 are intentionally direct-read only and return below; they must not mutate FH5's command list.
    {
        uint32_t mw = 0, mh = 0, mf = 0;
        unpack_meta(bmeta, mw, mh, mf);
        const bool is_primary_atlas = (mw == 288 && mh == 216 && mf == (uint32_t)DXGI_FORMAT_R11G11B10_FLOAT);
        const bool mirror_ui_pso = (mode == 7 || mode == 8 || mode == 23 || mode == 26) && is_ui_pso;
        const bool mirror_whole_atlas = (mode == 12 || mode == 13 || mode == 14 || mode == 15 || mode == 16 || mode == 17 || mode == 20) && is_primary_atlas;
        if (mode == 8 && is_ui_pso && !is_primary_atlas) {
            g_dbg_uipso_skips.fetch_add(1, std::memory_order_relaxed);
            sc.skip = true;
            return sc;
        }
        if (!g_hk_omsetrt || (!mirror_ui_pso && !mirror_whole_atlas)) {
            // Fall through to the older redirect classifiers below.
        } else {
        UiMirrorSlot* mirror_slot = GetOrCreateUiMirror(res, bmeta);
        if (mirror_slot) {
            ID3D12Resource* mirror_rt = mirror_slot->rt.load(std::memory_order_acquire);
            const uintptr_t mirror_rtv_ptr = mirror_slot->rtv.load(std::memory_order_relaxed);
            if (mirror_rt && mirror_rtv_ptr) {
                D3D12_CPU_DESCRIPTOR_HANDLE mirror_rtv{};
                mirror_rtv.ptr = (SIZE_T)mirror_rtv_ptr;
                const uint64_t frame = g_ui_lineage_frame.load(std::memory_order_relaxed);
                if (g_hk_clearrtv && mirror_slot->clear_frame.exchange(frame, std::memory_order_relaxed) != frame) {
                    const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    g_hk_clearrtv->get_original<FnClearRTV>()(self, mirror_rtv, zero, 0, nullptr);
                }
                sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed);
                sc.has_dsv = b.has_dsv.load(std::memory_order_relaxed) != 0;
                sc.dsv.ptr = b.dsv.load(std::memory_order_relaxed);
                g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &mirror_rtv, FALSE, nullptr);
                sc.active = true;
                sc.replay_original = (mode == 7 || mode == 23 || mode == 12 || mode == 14 || mode == 15 || mode == 16 || mode == 17 || mode == 20 || (mode == 13 && !is_ui_pso));
                sc.clear_original_after_replay = (mode == 14);
                mirror_slot->draws.fetch_add(1, std::memory_order_relaxed);
                g_dbg_uimirror_draws.fetch_add(1, std::memory_order_relaxed);
                g_ui_mirror_latest.store(mirror_rt, std::memory_order_relaxed);
                g_ui_mirror_latest_meta.store(mirror_slot->meta.load(std::memory_order_relaxed), std::memory_order_relaxed);
                return sc;
            }
        }
            return sc;
        }
    }

    if (mode == 9 || mode == 22) return sc;

    if ((mode == 12 || mode == 15 || mode == 16 || mode == 17 || mode == 20 || mode == 21 || mode == 25) && drawSize <= 128) {
        uint32_t rw = 0, rh = 0, rf = 0;
        unpack_meta(bmeta, rw, rh, rf);
        const bool broad_final = (mode == 16 || mode == 17 || mode == 20 || mode == 21 || mode == 25);
        const bool strict_final_sample = (mode == 20 || mode == 21);
        const bool target_is_display = rw >= 1000 && rh >= 500 &&
            (broad_final ? IsUiFinalTargetFmt(rf) : IsDisplayFmt(rf));
        if (broad_final && !is_comp) {
            PropagateBoundUiLineageToTargetForThread(res, bmeta, target_is_display && !is_eyesrc_bound,
                                                     strict_final_sample);
        }
        if (!is_comp && (target_is_display || is_eyesrc_bound)) {
            ID3D12Resource* sample = nullptr;
            ID3D12Resource* origin = nullptr;
            uint64_t sample_meta = 0;
            uint32_t root = 0xFFFFFFFFu, slot = 0xFFFFFFFFu, depth = 0;
            const bool found_final_sample = BoundSrvsContainUiFinalSampleForThread(&sample, &sample_meta,
                &origin, &root, &slot, &depth, broad_final, strict_final_sample);
            const bool direct_late_final_mode = (mode == 25);
            const bool direct_late_ui_pso = direct_late_final_mode && target_is_display && is_ui_pso;
            const bool direct_overlay_flush = direct_late_final_mode && target_is_display && t_overlay_flush_depth > 0;
            if (direct_overlay_flush) {
                g_dbg_overlay_flush_draws.fetch_add(1, std::memory_order_relaxed);
            }
            if (found_final_sample || direct_late_ui_pso || direct_overlay_flush) {
                g_dbg_uifinal_hits.fetch_add(1, std::memory_order_relaxed);
                g_uifinal_latest_sample.store(sample, std::memory_order_relaxed);
                g_uifinal_latest_origin.store(origin, std::memory_order_relaxed);
                g_uifinal_latest_rt.store(res, std::memory_order_relaxed);
                g_uifinal_latest_sample_meta.store(sample_meta, std::memory_order_relaxed);
                g_uifinal_latest_rt_meta.store(bmeta, std::memory_order_relaxed);
                g_uifinal_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
                g_uifinal_latest_root_slot.store((root << 16) | (slot & 0xFFFFu), std::memory_order_relaxed);
                g_uifinal_latest_depth.store(depth, std::memory_order_relaxed);
                if ((mode == 15 || mode == 17 || mode == 20 || mode == 25) && g_hk_omsetrt) {
                    UiMirrorSlot* discard_slot = GetOrCreateUiMirror(res, bmeta);
                    ID3D12Resource* discard_rt = discard_slot ? discard_slot->rt.load(std::memory_order_acquire) : nullptr;
                    const uintptr_t discard_rtv_ptr = discard_slot ? discard_slot->rtv.load(std::memory_order_relaxed) : 0;
                    if (discard_rt && discard_rtv_ptr) {
                        D3D12_CPU_DESCRIPTOR_HANDLE discard_rtv{};
                        discard_rtv.ptr = (SIZE_T)discard_rtv_ptr;
                        const uint64_t frame = g_ui_lineage_frame.load(std::memory_order_relaxed);
                        if (g_hk_clearrtv && discard_slot->clear_frame.exchange(frame, std::memory_order_relaxed) != frame) {
                            const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                            g_hk_clearrtv->get_original<FnClearRTV>()(self, discard_rtv, zero, 0, nullptr);
                        }
                        sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed);
                        sc.has_dsv = b.has_dsv.load(std::memory_order_relaxed) != 0;
                        sc.dsv.ptr = b.dsv.load(std::memory_order_relaxed);
                        g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &discard_rtv, FALSE, nullptr);
                        sc.active = true;
                        sc.replay_original = (mode == 25);
                        discard_slot->draws.fetch_add(1, std::memory_order_relaxed);
                        g_uifinal_mirror_latest.store(discard_rt, std::memory_order_relaxed);
                        g_uifinal_mirror_latest_meta.store(discard_slot->meta.load(std::memory_order_relaxed), std::memory_order_relaxed);
                        g_uifinal_mirror_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
                    }
                    g_dbg_uifinal_skips.fetch_add(1, std::memory_order_relaxed);
                    return sc;
                }
            }
        }
    }

    // MODE 3 (HUD-LAYER HYPOTHESIS): the fs-RT-tally showed the HUD is drawn PRE-composite into a full-screen
    // R10G10B10A2 (f24) buffer pair -- NOT the f28 eye-source -- which the world composite then samples into the
    // eye (so the post-composite phase machine never sees it -> uiDrawsRedir=0). Redirect EVERY draw into such a
    // buffer to our UI RT: that buffer is left empty -> the composite samples an empty HUD layer -> CLEAN EYES;
    // and the UI RT collects the HUD -> the quad shows HUD-only. Format-safe (UI RT is R10A2 too). A/B test: if
    // the WORLD breaks instead, the f24 buffer was post-fx, not the HUD -> revert to mode 1/2.
    if (mode == 3) {
        uint32_t mw = 0, mh = 0, mf = 0; unpack_meta(bmeta, mw, mh, mf);
        if (mf == (uint32_t)DXGI_FORMAT_R10G10B10A2_UNORM && mw >= 1000 && mh >= 500 && !IsEyeSource(res)) {
            g_dbg_ui_draws.fetch_add(1, std::memory_order_relaxed);
            if (!g_uir_cleared.exchange(true, std::memory_order_relaxed) && g_hk_clearrtv) {
                const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                g_hk_clearrtv->get_original<FnClearRTV>()(self, g_uir_ui_rtv, zero, 0, nullptr);
            }
            g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &g_uir_ui_rtv, FALSE, nullptr);
            sc.active = true;
            sc.bb_rtv.ptr = b.bb_rtv.load(std::memory_order_relaxed);   // restore the game's RT after our draw
            sc.has_dsv    = b.has_dsv.load(std::memory_order_relaxed) != 0;
            sc.dsv.ptr    = b.dsv.load(std::memory_order_relaxed);
            g_uir_redirects.fetch_add(1, std::memory_order_relaxed);
        }
        return sc;
    }

    // MODE 4 (composite-SRV source skip): after the composite SRV scanner identifies root+slot N, skip draws
    // into that source RT on the next frame. This is an A/B discriminator for sampled sources whose format
    // cannot be safely redirected to the HUD RT. It intentionally does not mutate descriptor heaps or resources.
    if (mode == 4 && !is_comp) {
        RefreshSrcskipRequest();
        ID3D12Resource* target = g_srcskip_target.load(std::memory_order_relaxed);
        if (target && res == target && !is_eyesrc_bound) {
            g_dbg_srcskip_draws.fetch_add(1, std::memory_order_relaxed);
            sc.skip = true;
            return sc;
        }
    }

    // MODE 6 (UI-lineage source skip): skip draws into the original RT behind the latest composite SRV whose
    // ancestry includes a known UI PSO. Unlike mode 4, this follows the resource pointer, not a moving root slot.
    if (mode == 6 && !is_comp) {
        ID3D12Resource* target = g_uil_latest_origin.load(std::memory_order_relaxed);
        const uint64_t frame = g_uil_latest_frame.load(std::memory_order_relaxed);
        if (target && FreshUiLineageFrame(frame, 3) && res == target && !is_eyesrc_bound) {
            g_dbg_srcskip_draws.fetch_add(1, std::memory_order_relaxed);
            sc.skip = true;
            return sc;
        }
    }

    // PINPOINT: tally the sub-conditions independently (pre-gate) so the log shows WHERE the chain breaks --
    // composite-PSO-bound draws, full-screen-format-RT draws, eye-source-RT draws -- vs the gate that drops them.
    const bool is_bbfs   = b.bb_fs.load(std::memory_order_relaxed) != 0;
    const bool is_eyesrc = is_eyesrc_bound;
    if (is_comp)   g_dbg_compbound.fetch_add(1, std::memory_order_relaxed);
    if (is_bbfs)   g_dbg_bbfs.fetch_add(1, std::memory_order_relaxed);
    if (is_eyesrc) g_dbg_eyesrc.fetch_add(1, std::memory_order_relaxed);

    // GATE: a draw matters if its RT is a full-screen display-format buffer (the HUD's intermediate, computed
    // lock-free at bind time) OR the registered eye-source. All hot-path lookups here are LOCK-FREE -- the
    // global std::mutexes that used to guard these were what deadlocked against the game's render sync.
    if (!(is_bbfs || is_eyesrc)) return sc;
    g_dbg_gatepass.fetch_add(1, std::memory_order_relaxed);

    // PSO-gated phase machine (per frame; reset in ui_redirect_on_present): the world composite PSO (PS hash
    // 7783d957) finalises the world into the eye-source; the HUD draws follow it. Keep everything up to and
    // including the composite; rebind non-composite draws into the eye-source AFTER it (= the HUD) to our RT.
    if (is_comp) {
        g_uir_after_composite.store(true, std::memory_order_relaxed);
        g_composite_target.store(res, std::memory_order_relaxed);
        g_dbg_composite_draws.fetch_add(1, std::memory_order_relaxed);
        TallyCompositeSrvsForThread();
        if ((mode == 10 || mode == 18 || mode == 19 || mode == 24 || mode == 25 || mode == 27) && is_eyesrc) {
            sc.capture_pre_ui = true;
            sc.capture_res = res;
            sc.capture_meta = bmeta;
        }
        return sc;                                      // world composite -> keep
    }
    if (!g_uir_after_composite.load(std::memory_order_relaxed)) return sc;   // pre-composite world build -> keep
    // Post-composite, non-composite draw into the eye-source = HUD.
    g_dbg_bb_draws.fetch_add(1, std::memory_order_relaxed);
    g_dbg_ui_draws.fetch_add(1, std::memory_order_relaxed);
    if (mode == 2 || mode == 4 || mode == 6 || mode == 10 || mode == 18 || mode == 19 || mode == 21 || mode == 27) return sc;   // dry-run/source-skip/delta/overlay/sample capture: no post-composite rebind
    // mode 1: REBIND this HUD draw to our FORMAT-MATCHED UI RT. Only trampoline calls below (no mod mutex), so
    // this is lock-free; the draw still executes (into our RT) -> eye-source stays world-only -> clean eyes.
    if (!g_uir_cleared.exchange(true, std::memory_order_relaxed) && g_hk_clearrtv) {
        const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_hk_clearrtv->get_original<FnClearRTV>()(self, g_uir_ui_rtv, zero, 0, nullptr);
    }
    g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &g_uir_ui_rtv, FALSE, nullptr);
    sc.active = true;
    sc.bb_rtv.ptr  = b.bb_rtv.load(std::memory_order_relaxed);   // restore the game's RT (+DSV) after our draw
    sc.has_dsv     = b.has_dsv.load(std::memory_order_relaxed) != 0;
    sc.dsv.ptr     = b.dsv.load(std::memory_order_relaxed);
    g_uir_redirects.fetch_add(1, std::memory_order_relaxed);
    return sc;
}
void EndDrawRedirect(ID3D12GraphicsCommandList* self, RedirectScope sc) {
    if (sc.capture_pre_ui) {
        CapturePreUiEyeAfterComposite(self, sc.capture_res, sc.capture_meta);
    }
    if (sc.active) {
        g_hk_omsetrt->get_original<FnOMSetRT>()(self, 1, &sc.bb_rtv, FALSE, sc.has_dsv ? &sc.dsv : nullptr);
    }
}

void ClearOriginalAfterReplay(ID3D12GraphicsCommandList* self, const RedirectScope& sc) {
    if (!sc.clear_original_after_replay || !g_hk_clearrtv || sc.bb_rtv.ptr == 0) {
        return;
    }
    const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_hk_clearrtv->get_original<FnClearRTV>()(self, sc.bb_rtv, zero, 0, nullptr);
    g_dbg_uimirror_native_clears.fetch_add(1, std::memory_order_relaxed);
}

void STDMETHODCALLTYPE Hook_DrawIdx(ID3D12GraphicsCommandList* self, UINT idxCount, UINT instCount,
    UINT startIdx, INT baseVtx, UINT startInst) {
    auto orig = g_hk_drawidx->get_original<FnDrawIdx>();
    RedirectScope sc;
    __try { sc = BeginDrawRedirect(self, idxCount); } __except (EXCEPTION_EXECUTE_HANDLER) { sc = RedirectScope{}; }
    if (!sc.skip) orig(self, idxCount, instCount, startIdx, baseVtx, startInst);   // skip -> drop HUD draw
    __try { EndDrawRedirect(self, sc); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (sc.replay_original && !sc.skip) {
        orig(self, idxCount, instCount, startIdx, baseVtx, startInst);
        __try { ClearOriginalAfterReplay(self, sc); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

void STDMETHODCALLTYPE Hook_DrawInst(ID3D12GraphicsCommandList* self, UINT vtxCount, UINT instCount,
    UINT startVtx, UINT startInst) {
    auto orig = g_hk_drawinst->get_original<FnDrawInst>();
    RedirectScope sc;
    __try { sc = BeginDrawRedirect(self, vtxCount); } __except (EXCEPTION_EXECUTE_HANDLER) { sc = RedirectScope{}; }
    if (!sc.skip) orig(self, vtxCount, instCount, startVtx, startInst);   // skip -> drop HUD draw
    __try { EndDrawRedirect(self, sc); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (sc.replay_original && !sc.skip) {
        orig(self, vtxCount, instCount, startVtx, startInst);
        __try { ClearOriginalAfterReplay(self, sc); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void install_createdevice_hook(HMODULE d3d12) {
    static std::atomic<bool> done{ false };
    if (!d3d12 || done.exchange(true)) return;
    auto* p = reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    if (!p) { spdlog::warn("[FH5CB] D3D12CreateDevice export not found"); done.store(false); return; }
    g_hk_createdev = std::make_unique<FunctionHook>(Address{ p }, &Hook_CreateDevice);
    if (g_hk_createdev->create()) spdlog::info("[FH5CB] D3D12CreateDevice hooked @0x{:X}", reinterpret_cast<uintptr_t>(p));
    else { g_hk_createdev.reset(); done.store(false); spdlog::warn("[FH5CB] D3D12CreateDevice hook FAILED"); }
}

void ensure_installed(ID3D12Device* device) {
    // Fallback: if the CreateDevice hook missed (e.g. installed late), still install the device hooks now.
    // The transform happens in Hook_CBV (no command-queue hook needed).
    install_device_hooks(device);
}

void ReleaseUiRedirectRt() {
    g_uir_rt_valid.store(false, std::memory_order_release);
    g_uir_cleared.store(false, std::memory_order_relaxed);
    g_uir_redirects.store(0, std::memory_order_relaxed);
    if (g_uir_ui_rt) {
        g_uir_ui_rt->Release();
        g_uir_ui_rt = nullptr;
    }
    if (g_uir_rtv_heap) {
        g_uir_rtv_heap->Release();
        g_uir_rtv_heap = nullptr;
    }
    g_uir_ui_rtv.ptr = 0;
    g_uir_rt_w.store(0, std::memory_order_relaxed);
    g_uir_rt_h.store(0, std::memory_order_relaxed);
    g_uir_rt_fmt.store(0, std::memory_order_relaxed);
    g_overlay_rt_latest_ms.store(0, std::memory_order_relaxed);
    g_overlay_rt_latest_meta.store(0, std::memory_order_relaxed);
    g_overlay_native_target_latest.store(nullptr, std::memory_order_relaxed);
    g_overlay_native_target_meta.store(0, std::memory_order_relaxed);
    g_overlay_native_target_ms.store(0, std::memory_order_relaxed);
    g_overlay_native_target_object.store(0, std::memory_order_relaxed);
    g_overlay_native_render_context.store(0, std::memory_order_relaxed);
    g_overlay_vf14_target_object.store(0, std::memory_order_relaxed);
    g_overlay_vf14_render_context.store(0, std::memory_order_relaxed);
    g_overlay_vf14_target_ms.store(0, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest.store(nullptr, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_meta.store(0, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_size.store(0, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_flags.store(0, std::memory_order_relaxed);
    g_overlay_draw_rt0_latest_rtv.store(0, std::memory_order_relaxed);
    ClearUirFullTargets();
}

void ui_redirect_install(ID3D12Device* device, IDXGISwapChain* swapchain) {
    if (!device || !swapchain) return;
    SetUiRedirectDevice(device);

    DXGI_SWAP_CHAIN_DESC scd{};
    if (FAILED(swapchain->GetDesc(&scd))) return;
    // (No backbuffer-RTV detection needed: the lock-free redirect gates on eye-source RESOURCE identity --
    //  registered via register_eye_source -- not on which RTV handle is a swapchain backbuffer.)

    const UINT w = scd.BufferDesc.Width  ? scd.BufferDesc.Width  : 1152;
    const UINT h = scd.BufferDesc.Height ? scd.BufferDesc.Height : 864;
    const DXGI_FORMAT fmt = scd.BufferDesc.Format ? scd.BufferDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
    const bool hooks_ok_now = g_hk_omsetrt && g_hk_drawinst && g_hk_drawidx && g_hk_setpso;
    const bool same_rt = g_uir_ui_rt &&
        g_uir_rt_w.load(std::memory_order_relaxed) == w &&
        g_uir_rt_h.load(std::memory_order_relaxed) == h &&
        g_uir_rt_fmt.load(std::memory_order_relaxed) == (uint32_t)fmt;
    if (same_rt && g_uir_installed.load(std::memory_order_relaxed)) {
        g_uir_rt_valid.store(hooks_ok_now, std::memory_order_release);
        return;
    }

    // Create/recreate the UI-only render target (backbuffer-sized, backbuffer format) + its RTV. FH5 recreates
    // the swapchain during boot/menu transitions; keeping the first RT makes later CopyResource calls invalid.
    ReleaseUiRedirectRt();
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = w; rd.Height = h;
    rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    // CRITICAL: match the swapchain/eye-source format (FH5 live = R10G10B10A2, format 24) so a redirected HUD
    // draw's PSO (which outputs the eye-source format) matches our RTV format. A mismatch (we previously
    // hardcoded R8G8B8A8) = PSO/RTV format violation -> GPU device-removal -> crash within frames.
    rd.Format = fmt;
    rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE cv{}; cv.Format = rd.Format;   // transparent black
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, IID_PPV_ARGS(&g_uir_ui_rt))) || !g_uir_ui_rt) {
        spdlog::warn("[FH5UIR] UI RT create failed"); return;
    }
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_uir_rtv_heap))) || !g_uir_rtv_heap) {
        spdlog::warn("[FH5UIR] RTV heap create failed");
        ReleaseUiRedirectRt();
        return;
    }
    g_uir_ui_rtv = g_uir_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(g_uir_ui_rt, nullptr, g_uir_ui_rtv);
    g_uir_rt_w.store(w, std::memory_order_relaxed);
    g_uir_rt_h.store(h, std::memory_order_relaxed);
    g_uir_rt_fmt.store((uint32_t)rd.Format, std::memory_order_relaxed);

    // Hook OMSetRenderTargets (ID3D12GraphicsCommandList vtable index 46) via a throwaway list's vtable
    // (shared across all lists from this device, so this catches FH5's UI-recording list too).
    ID3D12CommandAllocator* alloc = nullptr; ID3D12GraphicsCommandList* cl = nullptr;
    const bool install_hooks = !g_uir_installed.exchange(true);
    if (install_hooks &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))) && alloc &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cl))) && cl) {
        void** cvt = *reinterpret_cast<void***>(cl);
        g_hk_omsetrt  = std::make_unique<FunctionHook>(Address{ cvt[46] }, &Hook_OMSetRT);   // OMSetRenderTargets
        g_hk_drawinst = std::make_unique<FunctionHook>(Address{ cvt[12] }, &Hook_DrawInst);  // DrawInstanced
        g_hk_drawidx  = std::make_unique<FunctionHook>(Address{ cvt[13] }, &Hook_DrawIdx);   // DrawIndexedInstanced
        g_hk_setpso   = std::make_unique<FunctionHook>(Address{ cvt[25] }, &Hook_SetPSO);    // SetPipelineState
        g_hk_setheaps = std::make_unique<FunctionHook>(Address{ cvt[28] }, &Hook_SetDescriptorHeaps); // SetDescriptorHeaps
        g_hk_setgrs   = std::make_unique<FunctionHook>(Address{ cvt[30] }, &Hook_SetGfxRootSig); // SetGraphicsRootSignature
        g_hk_setgrdt  = std::make_unique<FunctionHook>(Address{ cvt[32] }, &Hook_SetGfxRootDescriptorTable); // SetGraphicsRootDescriptorTable
        g_hk_clearrtv = std::make_unique<FunctionHook>(Address{ cvt[48] }, &Hook_ClearRTV);  // ClearRenderTargetView
        g_hk_copyres  = std::make_unique<FunctionHook>(Address{ cvt[17] }, &Hook_CopyResource); // CopyResource
        g_hk_copytex  = std::make_unique<FunctionHook>(Address{ cvt[16] }, &Hook_CopyTexRegion);// CopyTextureRegion
        if (!g_hk_omsetrt->create())  { g_hk_omsetrt.reset();  spdlog::warn("[FH5UIR] OMSetRenderTargets hook FAILED"); }
        if (!g_hk_drawinst->create()) { g_hk_drawinst.reset(); spdlog::warn("[FH5UIR] DrawInstanced hook FAILED"); }
        if (!g_hk_drawidx->create())  { g_hk_drawidx.reset();  spdlog::warn("[FH5UIR] DrawIndexedInstanced hook FAILED"); }
        if (!g_hk_setpso->create())   { g_hk_setpso.reset();   spdlog::warn("[FH5UIR] SetPipelineState hook FAILED"); }
        if (!g_hk_setheaps->create()) { g_hk_setheaps.reset(); spdlog::warn("[FH5UIR] SetDescriptorHeaps hook FAILED"); }
        if (!g_hk_setgrs->create())   { g_hk_setgrs.reset();   spdlog::warn("[FH5UIR] SetGraphicsRootSignature hook FAILED"); }
        if (!g_hk_setgrdt->create())  { g_hk_setgrdt.reset();  spdlog::warn("[FH5UIR] SetGraphicsRootDescriptorTable hook FAILED"); }
        if (!g_hk_clearrtv->create()) { g_hk_clearrtv.reset(); spdlog::warn("[FH5UIR] ClearRenderTargetView hook FAILED"); }
        if (!g_hk_copyres->create())  { g_hk_copyres.reset();  spdlog::warn("[FH5UIR] CopyResource hook FAILED"); }
        if (!g_hk_copytex->create())  { g_hk_copytex.reset();  spdlog::warn("[FH5UIR] CopyTextureRegion hook FAILED"); }
    }
    if (cl) cl->Release();
    if (alloc) alloc->Release();

    g_uir_rt_valid.store(g_hk_omsetrt && g_hk_drawinst && g_hk_drawidx && g_hk_setpso, std::memory_order_release);
    spdlog::info("[FH5UIR] {} (LOCK-FREE): UI RT {}x{} fmt={} hooks(omset/drawinst/drawidx/setpso/setheaps/setgrs/setgrdt/clearrtv/copyres/copytex)={}/{}/{}/{}/{}/{}/{}/{}/{}/{}",
                 install_hooks ? "installed" : "reconfigured", w, h, (int)rd.Format,
                 g_hk_omsetrt ? 1 : 0, g_hk_drawinst ? 1 : 0,
                 g_hk_drawidx ? 1 : 0, g_hk_setpso ? 1 : 0, g_hk_setheaps ? 1 : 0,
                 g_hk_setgrs ? 1 : 0, g_hk_setgrdt ? 1 : 0, g_hk_clearrtv ? 1 : 0,
                 g_hk_copyres ? 1 : 0, g_hk_copytex ? 1 : 0);
}

void ui_redirect_on_present() {
    static uint64_t s_last_log = 0;
    static uint64_t s_last_redirects = 0;
    g_ui_lineage_frame.fetch_add(1, std::memory_order_relaxed);
    g_uir_cleared.store(false, std::memory_order_relaxed);          // re-clear the UI RT next frame
    g_uir_after_composite.store(false, std::memory_order_relaxed);  // reset the frame phase
    { std::scoped_lock lk(g_eye_seen_mtx); g_eye_first_seen.clear(); }  // re-arm the eye-source first-draw anchor

    // Pick the main UI RT = the off-screen RT that received the most draws inside the vf54 bracket this
    // frame; next frame's BeginDrawRedirect redirects exactly that RT (one-frame lag, fine for steady UI).
    // Also keep the best full-size display-format vf54 targets separately: in-game HUD composition can have
    // fewer draws than the tiny atlas, so using only the global winner misses the actual final UI target.
    struct FullTargetCandidate { ID3D12Resource* res; uint32_t count; uint64_t meta; };
    ID3D12Resource* winner = nullptr; uint32_t best = 0; size_t distinct = 0;
    std::vector<FullTargetCandidate> full_targets;
    {
        std::scoped_lock lk(g_uir_cap_mtx);
        distinct = g_uir_cap_counts.size();
        for (auto& kv : g_uir_cap_counts) {
            if (kv.second.count > best) {
                best = kv.second.count;
                winner = kv.first;
            }

            const uint64_t meta = kv.second.meta ? kv.second.meta : RtResourceMeta(kv.first);
            uint32_t rw = 0, rh = 0, rf = 0;
            unpack_meta(meta, rw, rh, rf);
            if (rw >= 1000 && rh >= 500 && IsUiFinalTargetFmt(rf)) {
                full_targets.push_back({ kv.first, kv.second.count, meta });
            }
        }
        g_uir_cap_counts.clear();
    }
    std::sort(full_targets.begin(), full_targets.end(), [](const FullTargetCandidate& a, const FullTargetCandidate& b) {
        if (a.count != b.count) return a.count > b.count;
        uint32_t aw = 0, ah = 0, af = 0, bw = 0, bh = 0, bf = 0;
        unpack_meta(a.meta, aw, ah, af);
        unpack_meta(b.meta, bw, bh, bf);
        return (uint64_t)aw * (uint64_t)ah > (uint64_t)bw * (uint64_t)bh;
    });
    for (int i = 0; i < kUirFullTargetSlots; ++i) {
        if (i < (int)full_targets.size()) {
            g_uir_full_target_meta[i].store(full_targets[i].meta, std::memory_order_relaxed);
            g_uir_full_target_count[i].store(full_targets[i].count, std::memory_order_relaxed);
            g_uir_full_target_res[i].store(full_targets[i].res, std::memory_order_release);
        } else {
            g_uir_full_target_res[i].store(nullptr, std::memory_order_release);
            g_uir_full_target_meta[i].store(0, std::memory_order_relaxed);
            g_uir_full_target_count[i].store(0, std::memory_order_relaxed);
        }
    }
    g_uir_target_res = winner;                                   // compare-only; read next frame
    g_uir_target_count.store(best, std::memory_order_relaxed);

    // DIAGNOSTIC: snapshot + clear the per-RT draw tally + bb-draw sequence (dump below, once/sec).
    std::vector<std::pair<ID3D12Resource*, uint32_t>> rtdump;
    {
        std::scoped_lock lk(g_rt_mtx);
        rtdump.assign(g_rt_tally.begin(), g_rt_tally.end());
        g_rt_tally.clear();
    }
    std::vector<uint32_t> seq;
    {
        std::scoped_lock lk(g_bbseq_mtx);
        seq = g_bbseq;
        g_bbseq.clear();
    }

    const uint64_t now = GetTickCount64();
    if (g_ctl_ui_redirect.load(std::memory_order_relaxed) && now - s_last_log >= 1000) {
        s_last_log = now;
        const uint64_t tot = g_uir_redirects.load(std::memory_order_relaxed);
        // Winner dims (GetDesc once/sec on the picked RT — confirms it's the full-screen UI RT, not a sub-RT).
        UINT64 ww = 0; UINT hh = 0;
        GetResDimsSEH(winner, ww, hh);
        std::string full_top;
        for (int i = 0; i < kUirFullTargetSlots; ++i) {
            ID3D12Resource* r = g_uir_full_target_res[i].load(std::memory_order_acquire);
            if (!r) continue;
            uint32_t rw = 0, rh = 0, rf = 0;
            unpack_meta(g_uir_full_target_meta[i].load(std::memory_order_relaxed), rw, rh, rf);
            char buf[112];
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "0x%llX[%ux%u f%u]=%u ",
                        (unsigned long long)reinterpret_cast<uintptr_t>(r), rw, rh, rf,
                        g_uir_full_target_count[i].load(std::memory_order_relaxed));
            full_top += buf;
        }
        // Dump the top RTs by draw count this frame, with dims/format -> reveals the UI RT (off-screen RGBA8,
        // full-screen, drawn after the world). Sort desc by count, take top 10.
        std::sort(rtdump.begin(), rtdump.end(), [](auto& a, auto& b){ return a.second > b.second; });
        std::string top;
        for (size_t i = 0; i < rtdump.size() && i < 10; ++i) {
            UINT64 rw = 0; UINT rh = 0, rf = 0;
            GetResInfoSEH(rtdump[i].first, rw, rh, rf);
            char buf[96];
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "0x%llX[%llux%u f%u]=%u ",
                        (unsigned long long)reinterpret_cast<uintptr_t>(rtdump[i].first),
                        (unsigned long long)rw, rh, rf, rtdump[i].second);
            top += buf;
        }
        spdlog::info("[FH5UIR] RT-tally(top): {}", top);

        // Dump the ordered backbuffer-draw sequence (eye source): C=composite-PSO draw, u=non-composite.
        // Shows where the HUD draws (non-composite, after the world composite) sit -> fixes the boundary.
        std::string sq; int comp_idx = -1;
        for (size_t i = 0; i < seq.size() && i < 60; ++i) {
            const bool comp = (seq[i] & 0x80000000u) != 0;
            if (comp && comp_idx < 0) comp_idx = (int)i;
            char b[24]; _snprintf_s(b, sizeof(b), _TRUNCATE, "%s%u ", comp ? "C" : "u", seq[i] & 0x7FFFFFFFu);
            sq += b;
        }
        spdlog::info("[FH5UIR] bb-seq(n={} firstComposite@{}): {}", seq.size(), comp_idx, sq);
        spdlog::info("[FH5UIR] frame: uipass(draws={} nobind={} off={} bb={}) uiDrawsRedir={} redirected~{}/frame(total {}) | uiRT=0x{:X} {}x{} draws={} distinctRTs={} | uiFullRTs={} | bbDraws(non-vf54)={} compositePSO={}",
                     g_dbg_uipass_draws.exchange(0, std::memory_order_relaxed),
                     g_dbg_uipass_nobind.exchange(0, std::memory_order_relaxed),
                     g_dbg_uipass_off.exchange(0, std::memory_order_relaxed),
                     g_dbg_uipass_bb.exchange(0, std::memory_order_relaxed),
                     g_dbg_ui_draws.exchange(0, std::memory_order_relaxed),
                     tot - s_last_redirects, tot,
                     reinterpret_cast<uintptr_t>(winner), (uint64_t)ww, hh, best, distinct,
                     full_top,
                     g_dbg_bb_draws.exchange(0, std::memory_order_relaxed),
                     g_composite_pso_count.load(std::memory_order_relaxed));
        spdlog::info("[FH5UIR] classify(LOCK-FREE): totalDraws={} boundDraws={} nullResDraws={} compositeDraws={} eyeSrcN={} afterComposite={}",
                     g_dbg_total_draws.exchange(0, std::memory_order_relaxed),
                     g_dbg_bound_draws.exchange(0, std::memory_order_relaxed),
                     g_dbg_nullres_draws.exchange(0, std::memory_order_relaxed),
                     g_dbg_composite_draws.exchange(0, std::memory_order_relaxed),
                     g_eye_src_n.load(std::memory_order_relaxed),
                     g_uir_after_composite.load(std::memory_order_relaxed) ? 1 : 0);
        spdlog::info("[FH5UIR] pinpoint: compPsoBound={} bbfsDraws={} eyeSrcDraws={} gatePass={} (of boundDraws)",
                     g_dbg_compbound.exchange(0, std::memory_order_relaxed),
                     g_dbg_bbfs.exchange(0, std::memory_order_relaxed),
                     g_dbg_eyesrc.exchange(0, std::memory_order_relaxed),
                     g_dbg_gatepass.exchange(0, std::memory_order_relaxed));
        {
            struct Row { ID3D12Resource* r; uint32_t c; uint64_t m; };
            std::vector<Row> rows;
            for (int i = 0; i < kUiPsoRtTally; ++i) {
                ID3D12Resource* r = g_uipso_rt_tally[i].res.load(std::memory_order_relaxed);
                if (!r) continue;
                rows.push_back({ r, g_uipso_rt_tally[i].count.load(std::memory_order_relaxed),
                                    g_uipso_rt_tally[i].meta.load(std::memory_order_relaxed) });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.c > b.c; });
            std::string targets;
            for (size_t i = 0; i < rows.size() && i < 12; ++i) {
                uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(rows[i].m, rw, rh, rf);
                char buf[112];
                _snprintf_s(buf, sizeof(buf), _TRUNCATE, "0x%llX[%ux%u f%u]=%u%s ",
                            (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].r), rw, rh, rf, rows[i].c,
                            IsEyeSource(rows[i].r) ? "[EYE]" : "");
                targets += buf;
            }
        spdlog::info("[FH5UIPSO] draws={} skipped={} nullRT={} fsRT={} eyeRT={} uiPsoCreated={} uiTogglerPso={} gfxPso={} streamPso={} targets(distinct={}): {}",
                         g_dbg_uipso_draws.exchange(0, std::memory_order_relaxed),
                         g_dbg_uipso_skips.exchange(0, std::memory_order_relaxed),
                         g_dbg_uipso_null.exchange(0, std::memory_order_relaxed),
                         g_dbg_uipso_fs.exchange(0, std::memory_order_relaxed),
                         g_dbg_uipso_eye.exchange(0, std::memory_order_relaxed),
                         g_ui_pso_count.load(std::memory_order_relaxed),
                         g_ui_toggler_pso_count.load(std::memory_order_relaxed),
                         g_pso_create_count.load(std::memory_order_relaxed),
                         g_pso_stream_count.load(std::memory_order_relaxed),
                         rows.size(), targets);
            for (int i = 0; i < kUiPsoRtTally; ++i) {
                g_uipso_rt_tally[i].res.store(nullptr, std::memory_order_relaxed);
                g_uipso_rt_tally[i].count.store(0, std::memory_order_relaxed);
                g_uipso_rt_tally[i].meta.store(0, std::memory_order_relaxed);
            }
        }
        {
            struct Row { ID3D12Resource* src; ID3D12Resource* rt; uint32_t c; uint64_t m; };
            std::vector<Row> rows;
            for (auto& s : g_ui_mirror) {
                const uint32_t draws = s.draws.exchange(0, std::memory_order_relaxed);
                ID3D12Resource* src = s.src.load(std::memory_order_relaxed);
                ID3D12Resource* rt = s.rt.load(std::memory_order_relaxed);
                if (!draws || !src || !rt) continue;
                rows.push_back({ src, rt, draws, s.meta.load(std::memory_order_relaxed) });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.c > b.c; });
            std::string mirrors;
            for (size_t i = 0; i < rows.size() && i < 12; ++i) {
                uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(rows[i].m, rw, rh, rf);
                char buf[176];
                _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                    "src=0x%llX->rt=0x%llX[%ux%u f%u]=%u%s ",
                    (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].src),
                    (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].rt),
                    rw, rh, rf, rows[i].c, IsEyeSource(rows[i].src) ? "[EYE]" : "");
                mirrors += buf;
            }
            ID3D12Resource* latest = g_ui_mirror_latest.load(std::memory_order_relaxed);
            uint32_t lw = 0, lh = 0, lf = 0; unpack_meta(g_ui_mirror_latest_meta.load(std::memory_order_relaxed), lw, lh, lf);
            spdlog::info("[FH5UIMIRROR] draws={} hits={} created={} failed={} full={} clears={} rows={} latest=0x{:X}[{}x{} f{}] {}",
                         g_dbg_uimirror_draws.exchange(0, std::memory_order_relaxed),
                         g_dbg_uimirror_hits.exchange(0, std::memory_order_relaxed),
                         g_dbg_uimirror_created.exchange(0, std::memory_order_relaxed),
                         g_dbg_uimirror_failed.exchange(0, std::memory_order_relaxed),
                         g_dbg_uimirror_full.exchange(0, std::memory_order_relaxed),
                         g_dbg_uimirror_native_clears.exchange(0, std::memory_order_relaxed),
                         rows.size(), reinterpret_cast<uintptr_t>(latest), lw, lh, lf, mirrors);
        }
        {
            uint32_t ow = 0, oh = 0, of = 0;
            unpack_meta(g_overlay_rt_latest_meta.load(std::memory_order_relaxed), ow, oh, of);
            spdlog::info("[FH5OVERLAYRT] draws={} replayed={} rejected={} clears={} latest=0x{:X}[{}x{} f{}] ms={}",
                         g_dbg_overlay_rt_draws.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_rt_replayed.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_rt_rejected.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_rt_clears.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(g_uir_ui_rt), ow, oh, of,
                         g_overlay_rt_latest_ms.load(std::memory_order_relaxed));
        }
        {
            ID3D12Resource* sample = g_uifinal_latest_sample.load(std::memory_order_relaxed);
            ID3D12Resource* origin = g_uifinal_latest_origin.load(std::memory_order_relaxed);
            ID3D12Resource* rt = g_uifinal_latest_rt.load(std::memory_order_relaxed);
            ID3D12Resource* mirror = g_uifinal_mirror_latest.load(std::memory_order_relaxed);
            uint32_t sw = 0, sh = 0, sf = 0;
            uint32_t rw = 0, rh = 0, rf = 0;
            uint32_t mw = 0, mh = 0, mf = 0;
            unpack_meta(g_uifinal_latest_sample_meta.load(std::memory_order_relaxed), sw, sh, sf);
            unpack_meta(g_uifinal_latest_rt_meta.load(std::memory_order_relaxed), rw, rh, rf);
            unpack_meta(g_uifinal_mirror_latest_meta.load(std::memory_order_relaxed), mw, mh, mf);
            const uint32_t rs = g_uifinal_latest_root_slot.load(std::memory_order_relaxed);
            spdlog::info("[FH5UIFINAL] scans={} hits={} skips={} props={} noHeap={} sample=0x{:X}[{}x{} f{}] origin=0x{:X} rt=0x{:X}[{}x{} f{}] mirror=0x{:X}[{}x{} f{}] r{}+s{} d{}",
                         g_dbg_uifinal_scans.exchange(0, std::memory_order_relaxed),
                         g_dbg_uifinal_hits.exchange(0, std::memory_order_relaxed),
                         g_dbg_uifinal_skips.exchange(0, std::memory_order_relaxed),
                         g_dbg_uifinal_props.exchange(0, std::memory_order_relaxed),
                         g_dbg_uifinal_noheap.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(sample), sw, sh, sf,
                         reinterpret_cast<uintptr_t>(origin),
                         reinterpret_cast<uintptr_t>(rt), rw, rh, rf,
                         reinterpret_cast<uintptr_t>(mirror), mw, mh, mf,
                         rs >> 16, rs & 0xFFFFu,
                         g_uifinal_latest_depth.load(std::memory_order_relaxed));
        }
        {
            struct Row { void* pso; uint32_t total, fs, display, eye, small_draw; uint64_t meta; };
            std::vector<Row> rows;
            for (int i = 0; i < kPsoDrawSlots; ++i) {
                void* pso = g_pso_draws[i].pso.load(std::memory_order_relaxed);
                if (!pso) continue;
                rows.push_back({ pso,
                    g_pso_draws[i].total.load(std::memory_order_relaxed),
                    g_pso_draws[i].fs.load(std::memory_order_relaxed),
                    g_pso_draws[i].display.load(std::memory_order_relaxed),
                    g_pso_draws[i].eye.load(std::memory_order_relaxed),
                    g_pso_draws[i].small_draw.load(std::memory_order_relaxed),
                    g_pso_draws[i].meta.load(std::memory_order_relaxed) });
            }
            auto dump_hot = [&](const char* label, bool small_mode) {
                std::vector<Row> ranked;
                for (const Row& r : rows) {
                    const uint32_t score = small_mode ? r.small_draw : (r.display + r.eye);
                    if (score != 0) ranked.push_back(r);
                }
                std::sort(ranked.begin(), ranked.end(), [small_mode](const Row& a, const Row& b) {
                    const uint32_t as = small_mode ? a.small_draw : (a.display + a.eye);
                    const uint32_t bs = small_mode ? b.small_draw : (b.display + b.eye);
                    return as == bs ? a.total > b.total : as > bs;
                });
                std::string out;
                for (size_t i = 0; i < ranked.size() && i < 12; ++i) {
                    uint32_t flags = 0; uint32_t vscrc = 0, pscrc = 0;
                    uint64_t vslo = 0, vshi = 0, pslo = 0, pshi = 0;
                    const bool known = LookupPsoMeta(ranked[i].pso, flags, vslo, vshi, pslo, pshi, vscrc, pscrc);
                    char vh[40], ph[40];
                    HashPartsHex(known ? (flags & 3u) : 0u, vslo, vshi, vh, sizeof(vh));
                    HashPartsHex(known ? ((flags >> 2) & 3u) : 0u, pslo, pshi, ph, sizeof(ph));
                    uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(ranked[i].meta, rw, rh, rf);
                    char buf[320];
                    _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                        "0x%llX c=%u fs=%u disp=%u eye=%u sm=%u rt[%ux%u f%u]%s%s%s%s PS=%s(%08X) VS=%s(%08X) ",
                        (unsigned long long)reinterpret_cast<uintptr_t>(ranked[i].pso),
                        ranked[i].total, ranked[i].fs, ranked[i].display, ranked[i].eye, ranked[i].small_draw,
                        rw, rh, rf,
                        (flags & 0x10u) ? "[COMP]" : "",
                        (flags & 0x20u) ? "[UI]" : "",
                        (flags & 0x40u) ? "[STREAM]" : "",
                        (flags & 0x80u) ? "[UITOG]" : "",
                        ph, pscrc, vh, vscrc);
                    out += buf;
                }
                spdlog::info("[FH5PSOHOT] {} rows={} {}", label, ranked.size(), out);
            };
            dump_hot("display_eye", false);
            dump_hot("small_draw", true);
            for (int i = 0; i < kPsoDrawSlots; ++i) {
                g_pso_draws[i].total.store(0, std::memory_order_relaxed);
                g_pso_draws[i].fs.store(0, std::memory_order_relaxed);
                g_pso_draws[i].display.store(0, std::memory_order_relaxed);
                g_pso_draws[i].eye.store(0, std::memory_order_relaxed);
                g_pso_draws[i].small_draw.store(0, std::memory_order_relaxed);
                g_pso_draws[i].meta.store(0, std::memory_order_relaxed);
                g_pso_draws[i].pso.store(nullptr, std::memory_order_relaxed);
            }
        }
        {
            struct Row { ID3D12Resource* r; uint32_t c; uint64_t m; uint32_t rs; };
            std::vector<Row> rows;
            for (int i = 0; i < kCompSrvTally; ++i) {
                ID3D12Resource* r = g_comp_srv_tally[i].res.load(std::memory_order_relaxed);
                if (!r) continue;
                rows.push_back({ r,
                    g_comp_srv_tally[i].count.load(std::memory_order_relaxed),
                    g_comp_srv_tally[i].meta.load(std::memory_order_relaxed),
                    g_comp_srv_tally[i].root_slot.load(std::memory_order_relaxed) });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.c > b.c; });
            std::string out;
            for (size_t i = 0; i < rows.size() && i < 16; ++i) {
                uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(rows[i].m, rw, rh, rf);
                const uint32_t root = rows[i].rs >> 16;
                const uint32_t slot = rows[i].rs & 0xFFFFu;
                const uint64_t rtmeta = RtResourceMeta(rows[i].r);
                char buf[160];
                _snprintf_s(buf, sizeof(buf), _TRUNCATE, "0x%llX[%ux%u f%u]=%u r%u+s%u%s%s ",
                            (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].r), rw, rh, rf, rows[i].c,
                            root, slot, IsEyeSource(rows[i].r) ? "[EYE]" : "", rtmeta ? "[RT]" : "");
                out += buf;
            }
            uint32_t sw = 0, sh = 0, sf = 0; unpack_meta(g_srcskip_meta.load(std::memory_order_relaxed), sw, sh, sf);
            spdlog::info("[FH5COMPSRV] roots={} scanned={} resolved={} noHeap={} noMap={} rows={} srv(create={} null={} copyRange={} copySimple={} prop={} clear={} full={} first=0x{:X} last=0x{:X} heapCpu=0x{:X} heapGpu=0x{:X} heapN={} missCpu=0x{:X} missGpu=0x{:X}) srcskip(root={} slot={} target=0x{:X} {}x{} f{} samples={} skipped={}) {}",
                         g_dbg_comp_srv_roots.exchange(0, std::memory_order_relaxed),
                         g_dbg_comp_srv_scanned.exchange(0, std::memory_order_relaxed),
                         g_dbg_comp_srv_resolved.exchange(0, std::memory_order_relaxed),
                         g_dbg_comp_srv_noheap.exchange(0, std::memory_order_relaxed),
                         g_dbg_comp_srv_nomap.exchange(0, std::memory_order_relaxed),
                         rows.size(),
                         g_dbg_srv_create_total.load(std::memory_order_relaxed),
                         g_dbg_srv_null_total.load(std::memory_order_relaxed),
                         g_dbg_srv_copy_range_total.load(std::memory_order_relaxed),
                         g_dbg_srv_copy_simple_total.load(std::memory_order_relaxed),
                         g_dbg_srv_prop_total.load(std::memory_order_relaxed),
                         g_dbg_srv_clear_total.load(std::memory_order_relaxed),
                         g_dbg_srv_record_full.load(std::memory_order_relaxed),
                         (uintptr_t)g_dbg_srv_first_handle.load(std::memory_order_relaxed),
                         (uintptr_t)g_dbg_srv_last_handle.load(std::memory_order_relaxed),
                         (uintptr_t)g_dbg_comp_srv_heap_cpu.load(std::memory_order_relaxed),
                         g_dbg_comp_srv_heap_gpu.load(std::memory_order_relaxed),
                         g_dbg_comp_srv_heap_count.load(std::memory_order_relaxed),
                         (uintptr_t)g_dbg_comp_srv_first_miss_cpu.exchange(0, std::memory_order_relaxed),
                         g_dbg_comp_srv_first_miss_gpu.exchange(0, std::memory_order_relaxed),
                         g_ctl_src_root.load(std::memory_order_relaxed),
                         g_ctl_src_slot.load(std::memory_order_relaxed),
                         (uintptr_t)g_srcskip_target.load(std::memory_order_relaxed), sw, sh, sf,
                         g_dbg_srcskip_samples.exchange(0, std::memory_order_relaxed),
                         g_dbg_srcskip_draws.exchange(0, std::memory_order_relaxed),
                         out);
            for (int i = 0; i < kCompSrvTally; ++i) {
                g_comp_srv_tally[i].res.store(nullptr, std::memory_order_relaxed);
                g_comp_srv_tally[i].count.store(0, std::memory_order_relaxed);
                g_comp_srv_tally[i].meta.store(0, std::memory_order_relaxed);
                g_comp_srv_tally[i].root_slot.store(0, std::memory_order_relaxed);
            }
        }
        {
            struct Row { ID3D12Resource* r; ID3D12Resource* origin; uint32_t c; uint64_t m; uint32_t rs; uint32_t depth; };
            std::vector<Row> rows;
            for (int i = 0; i < kUiLineageHitTally; ++i) {
                ID3D12Resource* r = g_ui_lineage_hits[i].res.load(std::memory_order_relaxed);
                if (!r) continue;
                rows.push_back({ r,
                    g_ui_lineage_hits[i].origin.load(std::memory_order_relaxed),
                    g_ui_lineage_hits[i].count.load(std::memory_order_relaxed),
                    g_ui_lineage_hits[i].meta.load(std::memory_order_relaxed),
                    g_ui_lineage_hits[i].root_slot.load(std::memory_order_relaxed),
                    g_ui_lineage_hits[i].depth.load(std::memory_order_relaxed) });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.c > b.c; });
            std::string out;
            for (size_t i = 0; i < rows.size() && i < 10; ++i) {
                uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(rows[i].m, rw, rh, rf);
                const uint32_t root = rows[i].rs >> 16;
                const uint32_t slot = rows[i].rs & 0xFFFFu;
                char buf[192];
                _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                    "0x%llX[%ux%u f%u]=%u r%u+s%u origin=0x%llX d%u%s ",
                    (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].r), rw, rh, rf, rows[i].c,
                    root, slot, (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].origin), rows[i].depth,
                    IsEyeSource(rows[i].r) ? "[EYE]" : "");
                out += buf;
            }
            ID3D12Resource* cand = g_uil_candidate.load(std::memory_order_relaxed);
            ID3D12Resource* cand_origin = g_uil_candidate_origin.load(std::memory_order_relaxed);
            uint32_t cw = 0, ch = 0, cf = 0; unpack_meta(g_uil_candidate_meta.load(std::memory_order_relaxed), cw, ch, cf);
            const uint32_t crs = g_uil_candidate_root_slot.load(std::memory_order_relaxed);
            ID3D12Resource* latest_sample = g_uil_latest_sample.load(std::memory_order_relaxed);
            ID3D12Resource* latest_origin = g_uil_latest_origin.load(std::memory_order_relaxed);
            uint32_t lw = 0, lh = 0, lf = 0; unpack_meta(g_uil_latest_meta.load(std::memory_order_relaxed), lw, lh, lf);
            const uint32_t lrs = g_uil_latest_root_slot.load(std::memory_order_relaxed);
            spdlog::info("[FH5UILIN] uiMarks={} copyMarks={} compHits={} candHits={} cand=0x{:X}[{}x{} f{}] origin=0x{:X} r{}+s{} d{} autoSample=0x{:X}[{}x{} f{}] autoOrigin=0x{:X} r{}+s{} d{} lastCopy=0x{:X}->0x{:X} rows={} {}",
                         g_dbg_uil_ui_marks.exchange(0, std::memory_order_relaxed),
                         g_dbg_uil_copy_marks.exchange(0, std::memory_order_relaxed),
                         g_dbg_uil_comp_hits.exchange(0, std::memory_order_relaxed),
                         g_dbg_uil_candidate_hits.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(cand), cw, ch, cf,
                         reinterpret_cast<uintptr_t>(cand_origin), crs >> 16, crs & 0xFFFFu,
                         g_uil_candidate_depth.load(std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(latest_sample), lw, lh, lf,
                         reinterpret_cast<uintptr_t>(latest_origin), lrs >> 16, lrs & 0xFFFFu,
                         g_uil_latest_depth.load(std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(g_uil_last_copy_src.load(std::memory_order_relaxed)),
                         reinterpret_cast<uintptr_t>(g_uil_last_copy_dst.load(std::memory_order_relaxed)),
                         rows.size(), out);
            for (int i = 0; i < kUiLineageHitTally; ++i) {
                g_ui_lineage_hits[i].res.store(nullptr, std::memory_order_relaxed);
                g_ui_lineage_hits[i].origin.store(nullptr, std::memory_order_relaxed);
                g_ui_lineage_hits[i].count.store(0, std::memory_order_relaxed);
                g_ui_lineage_hits[i].meta.store(0, std::memory_order_relaxed);
                g_ui_lineage_hits[i].root_slot.store(0, std::memory_order_relaxed);
                g_ui_lineage_hits[i].depth.store(0, std::memory_order_relaxed);
            }
        }
        // HUD-delivery probe: copies whose DESTINATION is a registered eye-source (swapchain backbuffer). If >0,
        // the HUD reaches the eye via a CopyResource/CopyTextureRegion from `copySrc` (the HUD-bearing buffer).
        spdlog::info("[FH5UIR] copyToEye={} copySrc=0x{:X} copyDst=0x{:X}",
                     g_dbg_copy_to_eye.exchange(0, std::memory_order_relaxed),
                     reinterpret_cast<uintptr_t>(g_dbg_last_copysrc.load(std::memory_order_relaxed)),
                     reinterpret_cast<uintptr_t>(g_dbg_last_copydst.load(std::memory_order_relaxed)));
        {
            ID3D12Resource* latest = g_preui_eye_latest.load(std::memory_order_relaxed);
            uint32_t pw = 0, ph = 0, pf = 0;
            unpack_meta(g_preui_eye_latest_meta.load(std::memory_order_relaxed), pw, ph, pf);
            spdlog::info("[FH5PREUI] captures={} recreated={} failed={} latest=0x{:X}[{}x{} f{}] frame={}",
                         g_dbg_preui_captures.exchange(0, std::memory_order_relaxed),
                         g_dbg_preui_recreated.exchange(0, std::memory_order_relaxed),
                         g_dbg_preui_failed.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(latest), pw, ph, pf,
                         g_preui_eye_latest_frame.load(std::memory_order_relaxed));
        }
        {
            ID3D12Resource* latest = g_overlay_srv_latest.load(std::memory_order_relaxed);
            uint32_t ow = 0, oh = 0, of = 0;
            unpack_meta(g_overlay_srv_latest_meta.load(std::memory_order_relaxed), ow, oh, of);
            ID3D12Resource* native_rt = g_overlay_native_target_latest.load(std::memory_order_relaxed);
            uint32_t nw = 0, nh = 0, nf = 0;
            unpack_meta(g_overlay_native_target_meta.load(std::memory_order_relaxed), nw, nh, nf);
            ID3D12Resource* draw_rt0 = g_overlay_draw_rt0_latest.load(std::memory_order_relaxed);
            uint32_t dw = 0, dh = 0, df = 0;
            unpack_meta(g_overlay_draw_rt0_latest_meta.load(std::memory_order_relaxed), dw, dh, df);
            spdlog::info("[FH5OVERLAY] scopes={} vf14={} binds={} psCalls={} psKnown={} valid={} resolved={} fieldResolved={} rootResolved={} unresolved={} overlayDraws={} flushes={} flushDraws={} rootScan={} rootNoHeap={} targetProbes={} targetBinds={} targetHits={} native=0x{:X}[{}x{} f{}] targetObj=0x{:X} ctx=0x{:X} vf14Obj=0x{:X} vf14Ctx=0x{:X} drawRT0={} eye={} fs={} nativeMatch={} latestDraw=0x{:X}[{}x{} f{}] drawSize={} flags=0x{:X} rtv=0x{:X} latest=0x{:X}[{}x{} f{}] srv=0x{:X} paramSlot={} root={} srvSlot={} layer={} renderer=0x{:X} ps=0x{:X} tex=0x{:X} desc=0x{:X} cached=0x{:X} binding=0x{:X} retained=0x{:X} unresolvedDesc=0x{:X} field=0x{:X}+0x{:X}->0x{:X}",
                         g_dbg_overlay_scope_calls.exchange(0, std::memory_order_relaxed),
                         g_overlay_viewport_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_bind_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_ps_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_ps_known.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_valid.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_resolved.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_field_resolved.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_root_resolved.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_unresolved.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_draw_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_flush_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_flush_draws.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_root_scanned.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_root_noheap.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_native_target_probes.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_native_target_bind_calls.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_native_target_hits.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(native_rt), nw, nh, nf,
                         (uintptr_t)g_overlay_native_target_object.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_native_render_context.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_vf14_target_object.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_vf14_render_context.load(std::memory_order_relaxed),
                         g_dbg_overlay_draw_rt0_draws.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_draw_rt0_eye.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_draw_rt0_fs.exchange(0, std::memory_order_relaxed),
                         g_dbg_overlay_draw_rt0_native_match.exchange(0, std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(draw_rt0), dw, dh, df,
                         g_overlay_draw_rt0_latest_size.load(std::memory_order_relaxed),
                         g_overlay_draw_rt0_latest_flags.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_draw_rt0_latest_rtv.load(std::memory_order_relaxed),
                         reinterpret_cast<uintptr_t>(latest), ow, oh, of,
                         (uintptr_t)g_overlay_srv_latest_cpu.load(std::memory_order_relaxed),
                         g_overlay_latest_param_slot.load(std::memory_order_relaxed),
                         g_overlay_latest_root_slot.load(std::memory_order_relaxed),
                         g_overlay_latest_srv_slot.load(std::memory_order_relaxed),
                         g_overlay_latest_layer.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_renderer.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_ps.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_tex.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_desc.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_cached.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_binding.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_lock_retained.load(std::memory_order_relaxed),
                         (uintptr_t)g_dbg_overlay_unresolved_desc.exchange(0, std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_field_base.load(std::memory_order_relaxed),
                         g_overlay_latest_field_off.load(std::memory_order_relaxed),
                         (uintptr_t)g_overlay_latest_field_qword.load(std::memory_order_relaxed));
        }
        // FULL-SCREEN RT draw tally (since last dump): every full-screen buffer + its draw count + dims/format.
        // The eye-source carries an [EYE] tag; the HUD RT = a full-screen RT with a moderate count, not [EYE].
        {
            struct Row { ID3D12Resource* r; uint32_t c; uint64_t m; };
            std::vector<Row> rows;
            for (int i = 0; i < kFsTally; ++i) {
                ID3D12Resource* r = g_fs_tally[i].res.load(std::memory_order_relaxed);
                if (!r) continue;
                rows.push_back({ r, g_fs_tally[i].count.load(std::memory_order_relaxed),
                                    g_fs_tally[i].meta.load(std::memory_order_relaxed) });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.c > b.c; });
            std::string fst;
            for (size_t i = 0; i < rows.size() && i < 16; ++i) {
                uint32_t rw = 0, rh = 0, rf = 0; unpack_meta(rows[i].m, rw, rh, rf);
                char buf[112];
                _snprintf_s(buf, sizeof(buf), _TRUNCATE, "0x%llX[%ux%u f%u]=%u%s ",
                            (unsigned long long)reinterpret_cast<uintptr_t>(rows[i].r), rw, rh, rf, rows[i].c,
                            IsEyeSource(rows[i].r) ? "[EYE]" : "");
                fst += buf;
            }
            spdlog::info("[FH5UIR] fs-RT-tally(distinct={}): {}", rows.size(), fst);
            for (int i = 0; i < kFsTally; ++i) {           // reset for the next 1s window (benign race w/ draws)
                g_fs_tally[i].res.store(nullptr, std::memory_order_relaxed);
                g_fs_tally[i].count.store(0, std::memory_order_relaxed);
                g_fs_tally[i].meta.store(0, std::memory_order_relaxed);
            }
        }
        s_last_redirects = tot;
    }
}

ID3D12Resource* ui_redirect_target() {
    const int mode = g_ctl_ui_redirect.load(std::memory_order_relaxed);
    return ((mode == 1 || mode == 3 || mode == 30) && g_uir_rt_valid.load(std::memory_order_relaxed)
            && g_uir_redirects.load(std::memory_order_relaxed) > 0) ? g_uir_ui_rt : nullptr;
}

bool ui_redirect_active() { return ui_redirect_target() != nullptr; }

ID3D12Resource* ui_lineage_candidate() {
    ID3D12Resource* candidate = g_uil_candidate.load(std::memory_order_relaxed);
    const uint64_t frame = g_uil_candidate_frame.load(std::memory_order_relaxed);
    return (candidate && FreshUiLineageFrame(frame, 3)) ? candidate : nullptr;
}

ID3D12Resource* ui_atlas_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    const uint64_t meta = g_ui_atlas_latest_meta.load(std::memory_order_relaxed);
    const uint64_t seen_ms = g_ui_atlas_latest_ms.load(std::memory_order_relaxed);
    ID3D12Resource* candidate = g_ui_atlas_latest.load(std::memory_order_relaxed);
    if (GetTickCount64() - seen_ms > 2000) candidate = nullptr;
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* pre_ui_eye_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_preui_eye_latest.load(std::memory_order_acquire);
    const uint64_t seen_ms = g_preui_eye_latest_frame.load(std::memory_order_relaxed);
    const uint64_t meta = g_preui_eye_latest_meta.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* pre_ui_eye_candidate_for(ID3D12Resource* eye_texture,
                                         uint32_t* out_w,
                                         uint32_t* out_h,
                                         uint32_t* out_fmt) {
    ID3D12Resource* candidate = nullptr;
    uint64_t meta = 0;
    const uint64_t now_ms = GetTickCount64();
    {
        std::scoped_lock lk(g_preui_eye_mtx);
        for (auto& slot : g_preui_eye_slots) {
            if (slot.src != eye_texture || slot.copy == nullptr) continue;
            const uint64_t seen_ms = slot.seen_ms.load(std::memory_order_relaxed);
            if (seen_ms != 0 && now_ms - seen_ms <= 2000) {
                candidate = slot.copy;
                meta = slot.meta;
            }
            break;
        }
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* overlay_srv_candidate(D3D12_CPU_DESCRIPTOR_HANDLE* out_srv,
                                      uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_overlay_srv_latest.load(std::memory_order_acquire);
    const uint64_t seen_ms = g_overlay_srv_latest_ms.load(std::memory_order_relaxed);
    const uint64_t meta = g_overlay_srv_latest_meta.load(std::memory_order_relaxed);
    const size_t srv_cpu = g_overlay_srv_latest_cpu.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_srv) {
        out_srv->ptr = candidate ? (SIZE_T)srv_cpu : 0;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* overlay_native_target_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_overlay_native_target_latest.load(std::memory_order_acquire);
    const uint64_t seen_ms = g_overlay_native_target_ms.load(std::memory_order_relaxed);
    const uint64_t meta = g_overlay_native_target_meta.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* overlay_composite_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_uir_ui_rt;
    const uint64_t seen_ms = g_overlay_rt_latest_ms.load(std::memory_order_acquire);
    const uint64_t meta = g_overlay_rt_latest_meta.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* ui_final_mirror_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_uifinal_mirror_latest.load(std::memory_order_acquire);
    const uint64_t seen_ms = g_uifinal_mirror_latest_ms.load(std::memory_order_relaxed);
    const uint64_t meta = g_uifinal_mirror_latest_meta.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* ui_final_sample_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* candidate = g_uifinal_latest_sample.load(std::memory_order_acquire);
    const uint64_t seen_ms = g_uifinal_latest_ms.load(std::memory_order_relaxed);
    const uint64_t meta = g_uifinal_latest_sample_meta.load(std::memory_order_relaxed);
    if (!candidate || seen_ms == 0 || GetTickCount64() - seen_ms > 2000) {
        candidate = nullptr;
    }
    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(candidate ? meta : 0, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return candidate;
}

ID3D12Resource* ui_mirror_candidate(uint32_t* out_w, uint32_t* out_h, uint32_t* out_fmt) {
    ID3D12Resource* exact = nullptr;
    uint64_t exact_meta = 0;
    uint32_t exact_draws = 0;

    ID3D12Resource* best = nullptr;
    uint64_t best_meta = 0;
    uint64_t best_score = 0;

    for (auto& s : g_ui_mirror) {
        ID3D12Resource* rt = s.rt.load(std::memory_order_acquire);
        const uint64_t meta = s.meta.load(std::memory_order_relaxed);
        if (!rt || !meta) continue;

        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(meta, w, h, f);
        if (w == 0 || h == 0 || w > 8192 || h > 8192) continue;

        const uint32_t draws = s.draws.load(std::memory_order_relaxed);
        if (w == 288 && h == 216 && f == (uint32_t)DXGI_FORMAT_R11G11B10_FLOAT && (!exact || draws >= exact_draws)) {
            exact = rt;
            exact_meta = meta;
            exact_draws = draws;
        }

        const uint64_t area = (uint64_t)w * (uint64_t)h;
        const bool useful_size = w >= 64 && h >= 64;

        // Draw count finds the active UI pass; area avoids the "latest small glyph RT" trap.
        uint64_t score = (uint64_t)draws * 100000000ull + area;
        if (useful_size) score += 1000000000ull;
        if (w == 288 && h == 216) score += 500000000ull; // FH5's stable in-game HUD atlas from uiredirect=7.

        if (score > best_score) {
            best = rt;
            best_meta = meta;
            best_score = score;
        }
    }

    if (exact) {
        best = exact;
        best_meta = exact_meta;
    }

    if (!best) {
        best = g_ui_mirror_latest.load(std::memory_order_relaxed);
        best_meta = g_ui_mirror_latest_meta.load(std::memory_order_relaxed);
    }

    if (out_w || out_h || out_fmt) {
        uint32_t w = 0, h = 0, f = 0;
        unpack_meta(best_meta, w, h, f);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (out_fmt) *out_fmt = f;
    }
    return best;
}

// FH5's dedicated HUD display-plane (ALLOW_DISPLAY surface, HUD-only). Returns the plane at index `idx`
// (idx<0 -> the last/most-recently-created). The OpenXR quad copies this for a HUD-only quad. State is
// COMMON (display/shared surfaces sit in COMMON between frames).
ID3D12Resource* hud_plane(int idx) {
    // PREFERRED: the assembled HUD layer found via the transparent (alpha=0) full-screen clear — this is the
    // actual HUD-only compositing layer in free-roam (capture-replay finding), regardless of swapchain regs.
    if (ID3D12Resource* hl = g_hud_layer.load(std::memory_order_relaxed)) return hl;
    std::scoped_lock lk(g_display_mtx);
    if (g_display_planes.empty()) return nullptr;
    if (idx < 0 || idx >= (int)g_display_planes.size()) idx = (int)g_display_planes.size() - 1;
    return g_display_planes[idx];
}
int hud_plane_count() { std::scoped_lock lk(g_display_mtx); return (int)g_display_planes.size(); }

// vf54 UI-pass bracket — called from the UIRenderer render-entry hook (Fh5Adapter::Hook_UIRendererRender)
// on the render thread. Thread-local depth so only that thread's recorded draws are treated as UI.
void enter_ui_pass() {
    ++t_ui_pass_depth;
    g_ui_pass_depth.fetch_add(1, std::memory_order_acq_rel);
}
void leave_ui_pass() {
    if (t_ui_pass_depth > 0) {
        --t_ui_pass_depth;
    }
    int depth = g_ui_pass_depth.load(std::memory_order_acquire);
    while (depth > 0 && !g_ui_pass_depth.compare_exchange_weak(depth, depth - 1, std::memory_order_acq_rel)) {
    }
}
bool in_ui_pass()    { return t_ui_pass_depth > 0 || g_ui_pass_depth.load(std::memory_order_acquire) > 0; }

static bool likely_readable_qword(uintptr_t addr) {
    if (addr <= 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    if (addr < region_base || addr + sizeof(uintptr_t) < addr) return false;
    return addr + sizeof(uintptr_t) <= region_base + mbi.RegionSize;
}

// SEH-isolated single qword read (probing an engine object's memory; no C++ unwinding here -> __try is legal).
static bool seh_rd_qword(uintptr_t addr, uintptr_t& out) {
    if (!likely_readable_qword(addr)) return false;
    __try { out = *reinterpret_cast<const uintptr_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void RememberOverlayNativeTarget(void* render_context, void* target_object, bool bind_mode, const char* source) {
    const uintptr_t ctx = reinterpret_cast<uintptr_t>(render_context);
    const uintptr_t target = reinterpret_cast<uintptr_t>(target_object);
    g_dbg_overlay_native_target_probes.fetch_add(1, std::memory_order_relaxed);
    if (!target) return;

    ID3D12Resource* target_res = nullptr;
    uint64_t target_meta = 0;
    uint32_t target_off = 0xFFFFFFFFu;
    auto consider_known_rt = [&](uintptr_t q, uint32_t off) {
        if (!q || target_res != nullptr) return;
        const uint64_t meta = RtResourceMeta(reinterpret_cast<void*>(q));
        if (!meta) return;
        target_res = reinterpret_cast<ID3D12Resource*>(q);
        target_meta = meta;
        target_off = off;
    };

    consider_known_rt(target, 0xFFFFFFFFu);
    for (uint32_t off = 0; target_res == nullptr && off <= 0x200; off += 8) {
        uintptr_t q = 0;
        if (seh_rd_qword(target + off, q)) {
            consider_known_rt(q, off);
        }
    }

    if (target_res == nullptr || target_meta == 0) return;

    g_overlay_native_target_latest.store(target_res, std::memory_order_release);
    g_overlay_native_target_meta.store(target_meta, std::memory_order_relaxed);
    g_overlay_native_target_ms.store(GetTickCount64(), std::memory_order_relaxed);
    g_overlay_native_target_object.store(target, std::memory_order_relaxed);
    g_overlay_native_render_context.store(ctx, std::memory_order_relaxed);
    g_dbg_overlay_native_target_hits.fetch_add(1, std::memory_order_relaxed);

    static std::atomic<uint64_t> s_native_target_log_ms{0};
    const uint64_t now_ms = GetTickCount64();
    uint64_t last_ms = s_native_target_log_ms.load(std::memory_order_relaxed);
    if (now_ms - last_ms >= 2000 &&
        s_native_target_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
        uint32_t tw = 0, th = 0, tf = 0;
        unpack_meta(target_meta, tw, th, tf);
        spdlog::info("[FH5OVERLAYTARGET] {} ctx=0x{:X} targetObj=0x{:X} field=0x{:X} res=0x{:X}[{}x{} f{}] bindMode={}",
            source ? source : "target", ctx, target, target_off,
            reinterpret_cast<uintptr_t>(target_res), tw, th, tf, bind_mode ? 1 : 0);
    }
}

bool RegisterOverlayPsFromRenderer(uintptr_t renderer) {
    if (!renderer) return false;
    uintptr_t ps = 0;
    if (!seh_rd_qword(renderer + 0x58, ps) || ps == 0) return false;

    for (int i = 0; i < kOverlayPsSlots; ++i) {
        const uintptr_t cur = g_overlay_ps_set[i].load(std::memory_order_relaxed);
        if (cur == ps) return true;
        if (cur == 0) {
            uintptr_t expected = 0;
            if (g_overlay_ps_set[i].compare_exchange_strong(expected, ps, std::memory_order_release, std::memory_order_relaxed)) {
                return true;
            }
        }
    }
    return true;
}

bool IsKnownOverlayPs(uintptr_t ps) {
    if (!ps) return false;
    for (int i = 0; i < kOverlayPsSlots; ++i) {
        const uintptr_t cur = g_overlay_ps_set[i].load(std::memory_order_acquire);
        if (cur == ps) return true;
        if (cur == 0) return false;
    }
    return false;
}

bool IsOverlayPsParameters(uintptr_t ps) {
    uintptr_t vtbl = 0;
    if (!seh_rd_qword(ps, vtbl) || vtbl == 0) return false;
    const uintptr_t exe_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    return vtbl == exe_base + 0x5EBCE70; // vftbl_ResourceBinding_OverlayRendererPSParameters
}

void enter_overlay_renderer12_draw(void* renderer, uint32_t layer) {
    ++t_overlay_scope_depth;
    t_overlay_renderer = reinterpret_cast<uintptr_t>(renderer);
    t_overlay_layer = layer;
    RegisterOverlayPsFromRenderer(t_overlay_renderer);
    g_dbg_overlay_scope_calls.fetch_add(1, std::memory_order_relaxed);
    g_overlay_latest_renderer.store(t_overlay_renderer, std::memory_order_relaxed);
    g_overlay_latest_layer.store(layer, std::memory_order_relaxed);
}

void leave_overlay_renderer12_draw() {
    if (t_overlay_scope_depth > 0) {
        --t_overlay_scope_depth;
    }
    if (t_overlay_scope_depth == 0) {
        t_overlay_renderer = 0;
        t_overlay_layer = 0;
    }
}

void enter_overlay_immediate_flush(void* state) {
    ++t_overlay_flush_depth;
    g_dbg_overlay_flush_calls.fetch_add(1, std::memory_order_relaxed);
    g_overlay_latest_flush_state.store(reinterpret_cast<uintptr_t>(state), std::memory_order_relaxed);
}

void leave_overlay_immediate_flush() {
    if (t_overlay_flush_depth > 0) {
        --t_overlay_flush_depth;
    }
}

void record_overlay_viewport_setup(void* renderer, bool surface_sized) {
    if (!renderer) return;
    const uintptr_t r = reinterpret_cast<uintptr_t>(renderer);
    RegisterOverlayPsFromRenderer(r);
    g_overlay_viewport_calls.fetch_add(1, std::memory_order_relaxed);
    g_overlay_latest_renderer.store(r, std::memory_order_relaxed);

    uintptr_t render_context = 0;
    uintptr_t target_object = 0;
    seh_rd_qword(r + 0x10, render_context);
    seh_rd_qword(r + 0x98, target_object);
    if (target_object != 0) {
        g_overlay_vf14_target_object.store(target_object, std::memory_order_relaxed);
        g_overlay_vf14_render_context.store(render_context, std::memory_order_relaxed);
        g_overlay_vf14_target_ms.store(GetTickCount64(), std::memory_order_relaxed);
    }
    RememberOverlayNativeTarget(reinterpret_cast<void*>(render_context), reinterpret_cast<void*>(target_object),
                                surface_sized, "vf14");
    (void)surface_sized;
}

void record_overlay_native_target_bind(void* render_context, void* target_object, bool bind_mode) {
    g_dbg_overlay_native_target_bind_calls.fetch_add(1, std::memory_order_relaxed);
    const uintptr_t ctx = reinterpret_cast<uintptr_t>(render_context);
    const uintptr_t target = reinterpret_cast<uintptr_t>(target_object);
    const uintptr_t vf14_target = g_overlay_vf14_target_object.load(std::memory_order_relaxed);
    const uintptr_t vf14_ctx = g_overlay_vf14_render_context.load(std::memory_order_relaxed);
    const uint64_t vf14_ms = g_overlay_vf14_target_ms.load(std::memory_order_relaxed);
    if (!target || !vf14_target || target != vf14_target || (vf14_ctx != 0 && ctx != vf14_ctx) ||
        vf14_ms == 0 || GetTickCount64() - vf14_ms > 1000) {
        return;
    }
    RememberOverlayNativeTarget(render_context, target_object, bind_mode, "sub_1409AD3E0");
}

bool TryResolveOverlaySrvHandle(size_t handle, ID3D12Resource** out_res, uint64_t* out_meta, size_t* out_handle) {
    ID3D12Resource* res = nullptr;
    uint64_t meta = 0;
    uint32_t view = 0;
    if (!TryResolveSrv(handle, &res, &meta, &view) || !res || !meta) {
        return false;
    }
    if (out_res) *out_res = res;
    if (out_meta) *out_meta = meta;
    if (out_handle) *out_handle = handle;
    return true;
}

bool TryResolveOverlayDescriptor(uintptr_t descriptor, ID3D12Resource** out_res, uint64_t* out_meta, size_t* out_handle) {
    if (!descriptor) return false;

    if (TryResolveOverlaySrvHandle((size_t)descriptor, out_res, out_meta, out_handle)) {
        return true;
    }

    // textureObject+0x10 is a Forza descriptor object in the decompile, not guaranteed to be the CPU handle
    // itself. Probe its first qwords for a copied D3D12 CPU descriptor handle, resolving only through our SRV map.
    for (uint32_t off = 0; off <= 0x100; off += 8) {
        uintptr_t q = 0;
        if (!seh_rd_qword(descriptor + off, q) || q == 0) continue;
        if (TryResolveOverlaySrvHandle((size_t)q, out_res, out_meta, out_handle)) {
            return true;
        }
    }
    return false;
}

bool TryResolveOverlayKnownResource(uintptr_t candidate, ID3D12Resource** out_res, uint64_t* out_meta, size_t* out_handle) {
    if (candidate <= 0x10000) return false;

    ID3D12Resource* res = reinterpret_cast<ID3D12Resource*>(candidate);
    uint64_t meta = 0;
    size_t handle = 0;
    uint32_t view = 0;
    if (TryResolveSrvResource(res, &handle, &meta, &view) && meta) {
        if (out_res) *out_res = res;
        if (out_meta) *out_meta = meta;
        if (out_handle) *out_handle = handle;
        return true;
    }

    meta = RtResourceMeta(res);
    if (meta) {
        if (out_res) *out_res = res;
        if (out_meta) *out_meta = meta;
        if (out_handle) *out_handle = 0;
        return true;
    }
    return false;
}

bool TryResolveOverlayObjectFields(uintptr_t object, ID3D12Resource** out_res, uint64_t* out_meta, size_t* out_handle) {
    if (object <= 0x10000) return false;

    // Live FH5 can store a small engine token such as 0xC001 at textureObject+0x10.
    // Only compare fields against resources/descriptors we have already observed; do not COM-deref arbitrary qwords.
    for (uint32_t off = 0; off <= 0x200; off += 8) {
        uintptr_t q = 0;
        if (!seh_rd_qword(object + off, q) || q == 0) continue;

        if (TryResolveOverlaySrvHandle((size_t)q, out_res, out_meta, out_handle) ||
            TryResolveOverlayKnownResource(q, out_res, out_meta, out_handle)) {
            g_overlay_latest_field_base.store(object, std::memory_order_relaxed);
            g_overlay_latest_field_off.store(off, std::memory_order_relaxed);
            g_overlay_latest_field_qword.store(q, std::memory_order_relaxed);
            g_dbg_overlay_field_resolved.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void RecordOverlayDescriptorResolved(uintptr_t ps, uintptr_t texture_object, uintptr_t srv_or_descriptor,
                                     uintptr_t descriptor_binding, uintptr_t cached_slot1,
                                     uintptr_t retained, uintptr_t renderer, uint32_t layer) {
    if (!ps || !texture_object) return;

    g_dbg_overlay_valid.fetch_add(1, std::memory_order_relaxed);
    g_overlay_latest_renderer.store(renderer, std::memory_order_relaxed);
    g_overlay_latest_ps.store(ps, std::memory_order_relaxed);
    g_overlay_latest_tex.store(texture_object, std::memory_order_relaxed);
    g_overlay_latest_desc.store(srv_or_descriptor, std::memory_order_relaxed);
    g_overlay_latest_binding.store(descriptor_binding, std::memory_order_relaxed);
    g_overlay_latest_cached.store(cached_slot1, std::memory_order_relaxed);
    g_overlay_latest_lock_retained.store(retained, std::memory_order_relaxed);
    g_overlay_latest_layer.store(layer, std::memory_order_relaxed);
    uintptr_t param_slot_q = 0;
    if (seh_rd_qword(ps + 8, param_slot_q)) {
        g_overlay_latest_param_slot.store((uint32_t)(param_slot_q & 0xFFFFu), std::memory_order_relaxed);
    }

    ID3D12Resource* res = nullptr;
    uint64_t meta = 0;
    size_t srv_cpu = 0;
    const bool resolved =
        (srv_or_descriptor && TryResolveOverlayDescriptor(srv_or_descriptor, &res, &meta, &srv_cpu)) ||
        TryResolveOverlayObjectFields(texture_object, &res, &meta, &srv_cpu) ||
        TryResolveOverlayObjectFields(retained, &res, &meta, &srv_cpu);
    if (resolved) {
        g_overlay_srv_latest.store(res, std::memory_order_release);
        g_overlay_srv_latest_meta.store(meta, std::memory_order_relaxed);
        g_overlay_srv_latest_cpu.store(srv_cpu, std::memory_order_relaxed);
        g_overlay_srv_latest_ms.store(GetTickCount64(), std::memory_order_relaxed);
        g_dbg_overlay_resolved.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_dbg_overlay_unresolved.fetch_add(1, std::memory_order_relaxed);
        g_dbg_overlay_unresolved_desc.store((size_t)srv_or_descriptor, std::memory_order_relaxed);
    }
}

void record_overlay_ps_binding(void* ps_ptr) {
    const uintptr_t ps = reinterpret_cast<uintptr_t>(ps_ptr);
    g_dbg_overlay_ps_calls.fetch_add(1, std::memory_order_relaxed);
    if (!IsOverlayPsParameters(ps)) {
        return;
    }
    g_dbg_overlay_ps_known.fetch_add(1, std::memory_order_relaxed);

    uintptr_t texture_object = 0;
    uintptr_t srv_or_descriptor = 0;
    uintptr_t descriptor_binding = 0;
    uintptr_t cached_slot1 = 0;
    if (!seh_rd_qword(ps + 0x70, texture_object) || texture_object == 0) return;
    if (!seh_rd_qword(texture_object + 0x10, srv_or_descriptor)) return;
    seh_rd_qword(ps + 0x10, descriptor_binding);
    seh_rd_qword(ps + 0xA8, cached_slot1);

    RecordOverlayDescriptorResolved(ps, texture_object, srv_or_descriptor, descriptor_binding, cached_slot1,
                                    g_overlay_latest_lock_retained.load(std::memory_order_relaxed),
                                    g_overlay_latest_renderer.load(std::memory_order_relaxed),
                                    g_overlay_latest_layer.load(std::memory_order_relaxed));
}

void record_overlay_texture_bind(void* renderer, void* texture_lock, void* render_context) {
    g_dbg_overlay_bind_calls.fetch_add(1, std::memory_order_relaxed);
    const uintptr_t r = reinterpret_cast<uintptr_t>(renderer);
    if (!r || t_overlay_scope_depth <= 0 || t_overlay_renderer != r || texture_lock == nullptr) {
        return;
    }

    uintptr_t retained = 0;
    if (!seh_rd_qword(reinterpret_cast<uintptr_t>(texture_lock) + 8, retained) || retained == 0) {
        return;
    }

    uintptr_t ps = 0;
    uintptr_t texture_object = 0;
    uintptr_t srv_or_descriptor = 0;
    uintptr_t descriptor_binding = 0;
    uintptr_t cached_slot1 = 0;
    if (!seh_rd_qword(r + 0x58, ps) || ps == 0) return;
    if (!seh_rd_qword(ps + 0x70, texture_object) || texture_object == 0) return;
    if (!seh_rd_qword(texture_object + 0x10, srv_or_descriptor)) return;
    seh_rd_qword(ps + 0x10, descriptor_binding);
    seh_rd_qword(ps + 0xA8, cached_slot1);

    g_dbg_overlay_valid.fetch_add(1, std::memory_order_relaxed);
    g_overlay_latest_renderer.store(r, std::memory_order_relaxed);
    g_overlay_latest_ps.store(ps, std::memory_order_relaxed);
    g_overlay_latest_tex.store(texture_object, std::memory_order_relaxed);
    g_overlay_latest_desc.store(srv_or_descriptor, std::memory_order_relaxed);
    g_overlay_latest_binding.store(descriptor_binding, std::memory_order_relaxed);
    g_overlay_latest_cached.store(cached_slot1, std::memory_order_relaxed);
    g_overlay_latest_lock_retained.store(retained, std::memory_order_relaxed);
    g_overlay_latest_layer.store(t_overlay_layer, std::memory_order_relaxed);
    (void)render_context;

    RecordOverlayDescriptorResolved(ps, texture_object, srv_or_descriptor, descriptor_binding, cached_slot1,
                                    retained, r, t_overlay_layer);
}

// vf54 UIRenderer RT probe (the engine-seam approach all 4 surveyed VR mods use). The UIRenderer (a1) OWNS its
// render target at *(this+0x40) (FH3 UIRenderer::GetRenderTarget; FH5 decompile vf54). Scan the UIRenderer object
// + that wrapper for a pointer matching a KNOWN RT from our CreateRTV map (PURE POINTER-COMPARE via RtResourceMeta
// -- never derefs the candidate, so AV-safe), and report dims/format. The full-screen display-format match = the
// assembled HUD layer (FH5's "ui_target"). Stores the best candidate in g_hud_layer for the quad source. The
// CALLER rate-limits (once/sec). Diagnostic-first: confirms WHICH resource is the HUD RT before we copy it.
void probe_ui_renderer_rt(void* uirenderer) {
    if (!uirenderer) return;
    const uintptr_t base = reinterpret_cast<uintptr_t>(uirenderer);
    // Sanity: how many RTs do we know about (is the map even populated)? + how many are full-screen.
    uint32_t known_rt = 0, known_fs = 0;
    for (uint32_t i = 0; i < kRtvMapSlots; ++i) {
        if (g_rtv_map[i].handle.load(std::memory_order_relaxed) == 0) continue;
        known_rt++;
        uint32_t w = 0, h = 0, f = 0; unpack_meta(g_rtv_map[i].meta.load(std::memory_order_relaxed), w, h, f);
        if (w >= 1000 && h >= 500) known_fs++;
    }
    uintptr_t best = 0; uint32_t bw = 0, bh = 0, bf = 0; bool found = false; int hits = 0;
    char buf[1200]; buf[0] = 0; int bp = 0;
    auto disp_fmt = [](uint32_t f) { return f == 24 || f == 28 || f == 29 || f == 87 || f == 88 || f == 91; };
    auto consider = [&](uintptr_t q, const char* tag, uint32_t o1, int o2) {
        const uint64_t m = RtResourceMeta(reinterpret_cast<void*>(q));
        if (!m) return;
        ++hits;
        uint32_t w = 0, h = 0, f = 0; unpack_meta(m, w, h, f);
        if (bp < (int)sizeof(buf) - 56) {
            if (o2 < 0) bp += _snprintf_s(buf + bp, sizeof(buf) - bp, _TRUNCATE, "%s+0x%X[%ux%u f%u] ", tag, o1, w, h, f);
            else        bp += _snprintf_s(buf + bp, sizeof(buf) - bp, _TRUNCATE, "%s+0x%X->+0x%X[%ux%u f%u] ", tag, o1, (uint32_t)o2, w, h, f);
        }
        const bool fs = (w >= 1000 && h >= 500);
        if (fs && (!found || (disp_fmt(f) && !disp_fmt(bf)))) { best = q; bw = w; bh = h; bf = f; found = true; }
    };
    // Keep this focused. The prior blind two-level walk across every member pointer produced first-chance AV
    // storms in the live log. FH3 says the render target wrapper is at this+0x40, so scan direct object members
    // and only that wrapper's first fields.
    uintptr_t wrapper40 = 0;
    for (uint32_t off = 0x08; off <= 0x100; off += 8) {
        uintptr_t p; if (!seh_rd_qword(base + off, p) || p <= 0x10000) continue;
        consider(p, "a", off, -1);                                  // p itself a known RT?
        if (off == 0x40) wrapper40 = p;
    }
    if (wrapper40 > 0x10000) {
        for (uint32_t o2 = 0; o2 <= 0x180; o2 += 8) {
            uintptr_t q; if (seh_rd_qword(wrapper40 + o2, q) && q > 0x10000) consider(q, "a", 0x40, (int)o2);
        }
    }
    if (found) g_hud_layer.store(reinterpret_cast<ID3D12Resource*>(best), std::memory_order_relaxed);
    spdlog::info("[FH5UIRT] probe a1=0x{:X} knownRT={}({}fs) hits={} found={} hudRT=0x{:X} {}x{} f{} | {}",
                 base, known_rt, known_fs, hits, found ? 1 : 0, best, bw, bh, bf, buf);
}

void register_eye_source(ID3D12Resource* eye_texture) {
    if (!eye_texture) return;
    // LOCK-FREE append (called per present; the redirect's hot-path IsEyeSource reads this atomic array).
    int n = g_eye_src_n.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) if (g_eye_src_lf[i].load(std::memory_order_relaxed) == eye_texture) return;
    if (n < kMaxEyeSrc) {
        g_eye_src_lf[n].store(eye_texture, std::memory_order_relaxed);
        g_eye_src_n.store(n + 1, std::memory_order_release);
        spdlog::info("[FH5UIR] eye-source resource registered: 0x{:X} (total {})",
                     reinterpret_cast<uintptr_t>(eye_texture), n + 1);
    }
}

void set_eye_offset(float view_x, float view_y, float view_z, bool active) {
    g_off_x.store(view_x, std::memory_order_relaxed);
    g_off_y.store(view_y, std::memory_order_relaxed);
    g_off_z.store(view_z, std::memory_order_relaxed);
    g_active.store(active, std::memory_order_relaxed);
    if (!g_ctl_started.exchange(true)) {   // start the live-tuning control-file poller once
        if (HANDLE t = CreateThread(nullptr, 0, &ControlThread, nullptr, 0, nullptr)) {
            CloseHandle(t);
            spdlog::info("[FH5CTL] control-file poller started");
        } else {
            g_ctl_started.store(false);
            spdlog::warn("[FH5CTL] failed to start control-file poller");
        }
    }
}

float ctl_half_ipd()    { return g_ctl_half_ipd.load(std::memory_order_relaxed); }
float ctl_world_scale() { return g_ctl_world_scale.load(std::memory_order_relaxed); }
int   ctl_recenter_seq(){ return g_ctl_recenter_seq.load(std::memory_order_relaxed); }
int   ctl_rotation_mode(){ return g_ctl_rot_mode.load(std::memory_order_relaxed); }
bool  ctl_projection_enabled(){ return g_ctl_projection.load(std::memory_order_relaxed); }
int   ctl_pos_lane()    { return g_ctl_pos_lane.load(std::memory_order_relaxed); }
int   ctl_pokerot()     { return g_ctl_pokerot.load(std::memory_order_relaxed); }
int   ctl_pokerotvs()   { return g_ctl_pokerotvs.load(std::memory_order_relaxed); }
bool  ctl_dumpcam()     { return g_ctl_dumpcam.load(std::memory_order_relaxed); }
bool  ctl_hud_quad()    { return g_ctl_hud_quad.load(std::memory_order_relaxed); }
bool  ctl_hud_opaque()  { return g_ctl_hud_opaque.load(std::memory_order_relaxed); }
bool  ctl_hud_premul()  { return g_ctl_hud_premul.load(std::memory_order_relaxed); }
bool  ctl_hud_flipv()   { return g_ctl_hud_flipv.load(std::memory_order_relaxed); }
int   ctl_hud_phase()   { return g_ctl_hud_phase.load(std::memory_order_relaxed) ? 1 : 0; }
float ctl_hud_w()       { return g_ctl_hud_w.load(std::memory_order_relaxed); }
float ctl_hud_x()       { return g_ctl_hud_x.load(std::memory_order_relaxed); }
float ctl_hud_y()       { return g_ctl_hud_y.load(std::memory_order_relaxed); }
float ctl_hud_z()       { return g_ctl_hud_z.load(std::memory_order_relaxed); }
bool  ctl_ui_redirect() { return g_ctl_ui_redirect.load(std::memory_order_relaxed) != 0; }
int   ctl_ui_redirect_mode() { return g_ctl_ui_redirect.load(std::memory_order_relaxed); }
int   ctl_hud_plane()   { return g_ctl_hud_plane.load(std::memory_order_relaxed); }
const char* pos_lane_name(int lane) {
    switch (lane) {
    case kPosLaneCcam320: return "ccam320";
    case kPosLaneCcam320D550: return "ccam320_d550";
    case kPosLaneClone0: return "clone0";
    case kPosLaneClone1: return "clone1";
    case kPosLaneClone2: return "clone2";
    case kPosLaneDownstream: return "downstream";
    case kPosLaneOff: return "off";
    case kPosLaneViewTail: return "viewtail";
    case kPosLaneInput540: return "input540";
    case kPosLaneProducerA15: return "proda15";
    case kPosLaneCamSrc: return "camsrc";
    default: return "unknown";
    }
}
float ctl_up_fwd()      { return g_ctl_fwd.load(std::memory_order_relaxed); }
float ctl_up_strafe()   { return g_ctl_strafe.load(std::memory_order_relaxed); }
float ctl_up_up()       { return g_ctl_up.load(std::memory_order_relaxed); }
int   ctl_up_tgt()      { return g_ctl_tgt.load(std::memory_order_relaxed); }

unsigned long long ring_writes()     { return g_ring_writes.load(std::memory_order_relaxed); }
unsigned long long buffers_tracked() { return g_buf_count.load(std::memory_order_relaxed); }
unsigned long long cam_hits()        { return g_cam_hits.load(std::memory_order_relaxed); }
unsigned long long cbv6912_count()   { return g_cbv6912.load(std::memory_order_relaxed); }

} // namespace fh5cb
