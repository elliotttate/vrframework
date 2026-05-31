#pragma once

// Engine-agnostic runtime settings shared by core + adapters.
// Per-game settings structs (e.g. ACValhallaSettings) are declared by the consuming
// repo and live alongside this in the ModSettings namespace.

namespace ModSettings {
    // Flags the core and every adapter understand. Per-engine adapters read these to
    // decide flat-screen fallback, quad-display for menus, the TAA/AFR history fix, etc.
    struct InternalSettings {
        bool forceFlatScreen{ false };   // render mono to the desktop, skip stereo submit
        bool showQuadDisplay{ false };   // engine is showing a 2D menu -> show a quad
        bool nvidiaAndTAAfix{ true };    // enable the AFR / TAA history double-buffering
        bool decoupledPitch{ false };    // keep horizon level; HMD pitch decoupled from body
        bool pawnControl{ false };       // head/aim drives character rotation
        bool preventZoom{ false };       // block engine-driven FOV zoom
        bool alternativeJoyLayout{ false };
    };

    inline InternalSettings g_internalSettings{};
}
