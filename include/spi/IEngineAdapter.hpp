#pragma once

// NEW (no REFramework equivalent) — the heart of the universalization.
//
// Every port (anvil/starfield, and RE Engine if ported back) implements the SAME four
// responsibilities, just hooking different functions:
//
//   1. install hooks           find + hook the engine's frame/camera/UI functions
//   2. frame pacing            report timeline events -> drives VR via FrameTimeline
//   3. stereo injection        write per-eye view+projection into engine memory
//   4. HUD / input reprojection
//
// REFramework expressed these as a tangle of RE-typed virtuals on the Mod base. Here
// they are one explicit interface. A new engine = implement this (~the methods that
// matter) + ship an offsets manifest. The core never names an engine type.

#include <memory>
#include <optional>
#include <string>

#include "Mod.hpp"
#include "spi/EngineCaps.hpp"
#include "spi/StereoView.hpp"
#include "spi/FrameTimeline.hpp"

class IEngineAdapter : public Mod {
public:
    ~IEngineAdapter() override = default;

    // --- identity & capabilities ---------------------------------------------
    virtual EngineCaps capabilities() const = 0;

    // --- 1. hook installation -------------------------------------------------
    // Resolve addresses (pattern scan / manifest) and install hooks. Return an error
    // string to abort init. Called on the d3d thread once the framework is ready.
    virtual bool install_hooks() = 0;

    // --- 2. frame pacing ------------------------------------------------------
    // The adapter owns a FrameTimeline (the core wires its callbacks to VR). The adapter
    // exposes it so the core/VR can read counters and the skip-present flag.
    virtual FrameTimeline& timeline() = 0;

    // --- 3. stereo injection --------------------------------------------------
    // Core hands ready per-eye matrices (already basis-converted via EngineCaps); the
    // adapter writes them wherever the engine keeps its camera. Called per eye/frame.
    virtual void apply_stereo(const StereoView& view) = 0;

    // The engine's current world camera (for decoupled pitch, aiming, world-to-screen).
    virtual glm::mat4 get_world_camera() const { return glm::mat4{ 1.0f }; }

    // --- 4. HUD / input -------------------------------------------------------
    virtual void reproject_hud(float scale_x, float scale_y) {}
    virtual void on_engine_input() {}   // remap controller -> engine input if needed

    // --- optional engine-feature toggles -------------------------------------
    // e.g. disable TAA/DOF/letterbox effects that fight VR. Called early.
    virtual void disable_incompatible_effects() {}
};

// Convenience: the consuming repo's adapter typically also exposes a singleton
// Get() and is registered in Mods::Mods(), exactly like the ports' EngineEntry.
