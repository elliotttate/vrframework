#pragma once

// PORT FROM: REFramework/src/mods/vr/runtimes/VRRuntime.hpp
// Cleaned: sdk/Math.hpp -> Math.hpp (glm). Otherwise engine-agnostic already.
// Concrete runtimes (OpenVR / OpenXR) subclass this; both are RE-free in REFramework.

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>

#include "Math.hpp"

struct VRRuntime {
    enum class Error : int64_t { UNSPECIFIED = -1, SUCCESS = 0 };
    enum class Type : uint8_t { NONE, OPENXR, OPENVR };
    enum class Eye : uint8_t { LEFT, RIGHT };
    enum class SynchronizeStage : int32_t { EARLY, LATE, VERY_LATE };

    virtual ~VRRuntime() {};

    virtual std::string_view name() const { return "NONE"; }
    virtual bool ready() const { return this->loaded; }
    virtual Type type() const { return Type::NONE; }
    virtual void destroy() { this->loaded = false; }

    virtual Error synchronize_frame() { return Error::SUCCESS; }
    virtual Error update_poses() { return Error::SUCCESS; }
    virtual Error update_render_target_size() { return Error::SUCCESS; }
    virtual Error consume_events(std::function<void(void*)> cb) { return Error::SUCCESS; }
    virtual Error update_matrices(float nearz, float farz) { return Error::SUCCESS; }
    virtual Error update_input() { return Error::SUCCESS; }

    virtual uint32_t get_width() const { return 0; }
    virtual uint32_t get_height() const { return 0; }

    bool is_openxr() const { return this->type() == Type::OPENXR; }
    bool is_openvr() const { return this->type() == Type::OPENVR; }

    bool loaded{ false };
    bool wants_reinitialize{ false };
    bool dll_missing{ false };
    bool needs_pose_update{ true };
    bool got_first_poses{ false };
    bool got_first_valid_poses{ false };
    bool got_first_sync{ false };
    bool wants_reset_origin{ true };

    std::optional<std::string> error{};

    std::array<Matrix4x4f, 2> projections{};
    std::array<Matrix4x4f, 2> eyes{};

    // Per-eye raw frustum bounds [left, right, top, bottom] — the ports read
    // get_runtime()->frustums[eye] to build the projection (see anvil onCalcProjection).
    std::array<std::array<float, 4>, 2> frustums{};
    Vector4f raw_projections[2]{};

    mutable std::shared_mutex projections_mtx{};
    mutable std::shared_mutex eyes_mtx{};
    mutable std::shared_mutex pose_mtx{};

    SynchronizeStage custom_stage{ SynchronizeStage::EARLY };
};
