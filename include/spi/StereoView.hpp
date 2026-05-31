#pragma once

// NEW (no REFramework equivalent). The per-eye view+projection the core computes from
// the VR runtime and hands to the engine adapter. The adapter's only job is to write
// these into wherever the engine keeps its camera (a constant buffer, a view matrix
// out-param, a camera node — engine-specific). This is the seam that lets the same
// stereo math serve Anvil, Creation 2 and RE Engine unchanged.

#include <glm/glm.hpp>
#include <cstdint>

struct StereoView {
    enum class Eye : uint8_t { LEFT = 0, RIGHT = 1 };

    glm::mat4 view[2]{ glm::mat4{ 1.0f }, glm::mat4{ 1.0f } };        // per-eye world->view
    glm::mat4 projection[2]{ glm::mat4{ 1.0f }, glm::mat4{ 1.0f } };  // per-eye projection
    glm::mat4 hmd_transform{ 1.0f };                                  // raw HMD pose (room space)
    glm::mat4 rotation_offset{ 1.0f };                               // recenter / snap-turn offset

    // Engine basis change (e.g. Anvil's Y_UP_TO_Z_UP_BASIS). The adapter supplies it
    // once via EngineCaps; the core pre-multiplies so the adapter receives ready matrices.
    Eye current_render_eye{ Eye::LEFT };
    int  presenter_frame{ 0 };  // for AFR ping-pong (which buffer this eye lands in)
};
