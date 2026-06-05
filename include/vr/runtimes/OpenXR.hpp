#pragma once

// PORT FROM: REFramework/src/mods/vr/runtimes/OpenXR.{cpp,hpp}
// Engine-neutral OpenXR runtime. Stripped of the RE-Engine action/binding/input
// machinery (those become IEngineAdapter concerns); kept the session lifecycle,
// per-eye swapchain bookkeeping, pose/view location, and projection build.
//
// The full OpenXR D3D12 submission flow this implements is the one validated end-to-end
// in FH5CameraProbe/src/XrSimTest.cpp against the SimXR test runtime.
//
// The xr* call sites are unchanged, but the loader (openxr_loader.dll) is DYNAMIC-LOADED at runtime
// (XR_NO_PROTOTYPES + global dispatch pointers in OpenXRDispatch.hpp) rather than statically linked —
// see that header for why (CRT collision with the /MT static loader lib). The extension entrypoint
// (xrGetD3D12GraphicsRequirementsKHR) is still resolved via xrGetInstanceProcAddr at runtime.

#include <array>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

#include "vr/runtimes/OpenXRDispatch.hpp"

#include "vr/VRRuntime.hpp"

namespace runtimes {
struct OpenXR final : public VRRuntime {
    OpenXR() {
        this->custom_stage = SynchronizeStage::EARLY;
    }

    virtual ~OpenXR() {
        this->destroy();
    }

    struct Swapchain {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{};
        int32_t height{};
    };

    VRRuntime::Type type() const override { return VRRuntime::Type::OPENXR; }
    std::string_view name() const override { return "OpenXR"; }
    bool ready() const override { return VRRuntime::ready() && this->session_ready; }

    VRRuntime::Error synchronize_frame() override;
    VRRuntime::Error update_poses() override;
    VRRuntime::Error update_render_target_size() override;
    uint32_t get_width() const override;
    uint32_t get_height() const override;
    VRRuntime::Error consume_events(std::function<void(void*)> callback) override;
    VRRuntime::Error update_matrices(float nearz, float farz) override;

    void destroy() override;

public:
    // --- engine-neutral lifecycle (owns instance/system/session/spaces) -------
    // d3d12_device/d3d12_queue come from the framework's D3D12Hook. Implements the
    // full REFramework VR::initialize_openxr() flow in one self-contained call.
    std::optional<std::string> initialize(ID3D12Device* d3d12_device, ID3D12CommandQueue* d3d12_queue);

    XrResult begin_frame();
    XrResult end_frame(const std::vector<XrCompositionLayerBaseHeader*>& extra_layers = {});

    std::string get_result_string(XrResult result) const;
    std::string get_structure_string(XrStructureType type) const;

public:
    // --- OpenXR state ---------------------------------------------------------
    double prediction_scale{0.0};
    bool session_ready{false};
    bool frame_began{false};
    bool frame_synced{false};

    std::recursive_mutex sync_mtx{};

    XrInstance instance{XR_NULL_HANDLE};
    XrSession session{XR_NULL_HANDLE};
    XrSpace stage_space{XR_NULL_HANDLE}; // LOCAL space — projection layer reference space
    XrSpace view_space{XR_NULL_HANDLE};  // VIEW space — for HMD-relative view matrices
    XrSystemId system{XR_NULL_SYSTEM_ID};
    XrFormFactor form_factor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    XrViewConfigurationType view_config{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
    XrEnvironmentBlendMode blend_mode{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};

    XrViewState view_state{XR_TYPE_VIEW_STATE};
    XrViewState stage_view_state{XR_TYPE_VIEW_STATE};
    XrFrameState frame_state{XR_TYPE_FRAME_STATE};
    XrSessionState session_state{XR_SESSION_STATE_UNKNOWN};
    XrSpaceLocation view_space_location{XR_TYPE_SPACE_LOCATION};

    std::vector<XrViewConfigurationView> view_configs{};
    std::vector<Swapchain> swapchains{};      // filled by D3D12Component when it creates swapchains
    std::vector<XrView> views{};              // VIEW-space relative views (eye->head, used for eyes/projection)
    std::vector<XrView> stage_views{};        // STAGE/LOCAL-space views (used for the projection-layer pose/fov)

    float resolution_scale{1.0f};

    // The D3D12 graphics binding (device+queue) is chained into xrCreateSession.
    XrGraphicsBindingD3D12KHR graphics_binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
};
} // namespace runtimes
