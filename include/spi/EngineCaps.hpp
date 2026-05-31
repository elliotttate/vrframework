#pragma once

// NEW (no REFramework equivalent). Capabilities an adapter declares so the core knows
// how to drive it: which graphics API, how frames are submitted, what fix-ups apply,
// and the engine's coordinate basis. Replaces the hard-coded assumptions each port
// currently bakes into its renderer/camera modules.

#include <glm/glm.hpp>
#include <cstdint>

struct EngineCaps {
    enum class Graphics : uint8_t { D3D11, D3D12 };
    enum class Submission : uint8_t {
        SEQUENTIAL,   // both eyes rendered in one engine frame
        AFR,          // alternate-frame: left on even frames, right on odd (Creation 2 / Anvil)
    };

    Graphics   graphics{ Graphics::D3D12 };
    Submission submission{ Submission::AFR };

    bool has_taa{ true };            // engine runs TAA -> needs history reprojection fix
    bool needs_window_resize{ true };// core should drive the game window to HMD render size
    bool left_handed{ false };       // GLM_FORCE_LEFT_HANDED engine (Anvil = true)

    // Basis change between engine space and the VR runtime's space. The core uses this
    // to deliver ready-to-write matrices in StereoView (e.g. Anvil's Y-up<->Z-up swap).
    glm::mat4 engine_to_vr_basis{ 1.0f };
    glm::mat4 vr_to_engine_basis{ 1.0f };

    float near_plane{ 0.01f };
    float far_plane{ 3000.0f };
};
