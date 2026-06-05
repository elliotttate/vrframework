// PORT FROM: REFramework/src/mods/vr/runtimes/OpenXR.cpp
// Engine-neutral OpenXR runtime: session lifecycle, pose update, per-eye view/projection.
// The action/binding/input/imgui machinery from REFramework's OpenXR was intentionally
// cut — that is the engine adapter's concern, not Layer-1 "stereo out".
//
// Validated flow reference: FH5CameraProbe/src/XrSimTest.cpp (same instance->system->
// session->spaces->swapchains->locate->endframe sequence, proven against SimXR).

#include <cmath>
#include <cstdio>
#include <cstring>

#include <spdlog/spdlog.h>

#include "vr/runtimes/OpenXR.hpp"

namespace runtimes {

// ---------------------------------------------------------------------------
// Reverse-Z, D3D-style (0..1 depth) asymmetric projection from XrFovf tangents.
// Equivalent to OpenXR-SDK's XrMatrix4x4f_CreateProjection(GRAPHICS_D3D, ...) with
// near/far swapped for reverse-Z. Column-major (glm) storage.
//
// near/far come from the engine (EngineCaps); the FOV tangents come from the runtime.
// For reverse-Z we map near->1, far->0 for best depth precision (see guide 08 §5).
// ---------------------------------------------------------------------------
static Matrix4x4f create_projection(const XrFovf& fov, float nearz, float farz) {
    const float tan_left = std::tan(fov.angleLeft);
    const float tan_right = std::tan(fov.angleRight);
    const float tan_up = std::tan(fov.angleUp);
    const float tan_down = std::tan(fov.angleDown);

    const float tan_width = tan_right - tan_left;
    const float tan_height = tan_up - tan_down;

    Matrix4x4f m{0.0f};

    // Horizontal/vertical scale + asymmetric skew (the off-center frustum is the point).
    m[0][0] = 2.0f / tan_width;
    m[1][1] = 2.0f / tan_height;
    m[2][0] = (tan_right + tan_left) / tan_width;
    m[2][1] = (tan_up + tan_down) / tan_height;
    m[2][3] = -1.0f; // right-handed, w = -z (D3D clip, matches XrSimTest/anvil convention)

    // Reverse-Z depth remap: z' in [0,1], near maps to 1, far maps to 0.
    // Standard (non-reversed) would be: m[2][2] = far/(near-far); m[3][2] = (near*far)/(near-far).
    // Reverse-Z swaps the roles so precision is concentrated at the far plane.
    m[2][2] = nearz / (farz - nearz);
    m[3][2] = (farz * nearz) / (farz - nearz);

    return m;
}

VRRuntime::Error OpenXR::synchronize_frame() {
    std::scoped_lock _{sync_mtx};

    // Can't sync a frame between xrBeginFrame and xrEndFrame.
    if (!this->session_ready || this->frame_began) {
        return VRRuntime::Error::UNSPECIFIED;
    }

    if (this->frame_synced) {
        return VRRuntime::Error::SUCCESS;
    }

    XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO};
    this->frame_state = {XR_TYPE_FRAME_STATE};
    auto result = xrWaitFrame(this->session, &frame_wait_info, &this->frame_state);

    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrWaitFrame failed: {}", this->get_result_string(result));
        return (VRRuntime::Error)result;
    }

    this->got_first_sync = true;
    this->frame_synced = true;
    return VRRuntime::Error::SUCCESS;
}

VRRuntime::Error OpenXR::update_poses() {
    std::scoped_lock _{this->sync_mtx};
    std::unique_lock __{this->pose_mtx};

    if (!this->session_ready) {
        return VRRuntime::Error::SUCCESS;
    }

    this->view_state = {XR_TYPE_VIEW_STATE};
    this->stage_view_state = {XR_TYPE_VIEW_STATE};

    uint32_t view_count{};

    // Guard against bogus predicted display times seen on some runtimes (VDXR) before
    // the first valid pose is produced.
    if (!this->got_first_valid_poses) {
        if (this->frame_state.predictedDisplayTime <= this->frame_state.predictedDisplayPeriod) {
            return VRRuntime::Error::SUCCESS;
        }
        if (this->frame_state.predictedDisplayTime == 11111111) {
            return VRRuntime::Error::SUCCESS;
        }
    }

    if (this->frame_state.predictedDisplayTime <= 1000) {
        return VRRuntime::Error::SUCCESS;
    }

    const auto display_time = this->frame_state.predictedDisplayTime +
        (XrDuration)(this->frame_state.predictedDisplayPeriod * this->prediction_scale);

    // Views relative to VIEW space (eye-to-head) -> drives per-eye view matrices.
    XrViewLocateInfo view_locate_info{XR_TYPE_VIEW_LOCATE_INFO};
    view_locate_info.viewConfigurationType = this->view_config;
    view_locate_info.displayTime = display_time;
    view_locate_info.space = this->view_space;

    auto result = xrLocateViews(this->session, &view_locate_info, &this->view_state,
        (uint32_t)this->views.size(), &view_count, this->views.data());
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrLocateViews (view space) failed: {}", this->get_result_string(result));
        return (VRRuntime::Error)result;
    }

    // Views relative to STAGE/LOCAL space -> drives the projection-layer pose+fov on submit.
    view_locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
    view_locate_info.viewConfigurationType = this->view_config;
    view_locate_info.displayTime = display_time;
    view_locate_info.space = this->stage_space;

    result = xrLocateViews(this->session, &view_locate_info, &this->stage_view_state,
        (uint32_t)this->stage_views.size(), &view_count, this->stage_views.data());
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrLocateViews (stage space) failed: {}", this->get_result_string(result));
        return (VRRuntime::Error)result;
    }

    // HMD pose relative to stage space.
    this->view_space_location = {XR_TYPE_SPACE_LOCATION};
    result = xrLocateSpace(this->view_space, this->stage_space, display_time, &this->view_space_location);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrLocateSpace (view space) failed: {}", this->get_result_string(result));
        return (VRRuntime::Error)result;
    }

    if (!this->got_first_valid_poses) {
        this->got_first_valid_poses = (this->view_space_location.locationFlags &
            (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) != 0;
    }

    this->needs_pose_update = false;
    this->got_first_poses = true;
    return VRRuntime::Error::SUCCESS;
}

VRRuntime::Error OpenXR::update_render_target_size() {
    uint32_t view_count{};
    auto result = xrEnumerateViewConfigurationViews(this->instance, this->system, this->view_config, 0, &view_count, nullptr);
    if (result != XR_SUCCESS) {
        this->error = "Could not get view configuration views: " + this->get_result_string(result);
        spdlog::error("[VR] {}", this->error.value());
        return (VRRuntime::Error)result;
    }

    this->view_configs.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    result = xrEnumerateViewConfigurationViews(this->instance, this->system, this->view_config, view_count, &view_count, this->view_configs.data());
    if (result != XR_SUCCESS) {
        this->error = "Could not get view configuration views: " + this->get_result_string(result);
        spdlog::error("[VR] {}", this->error.value());
        return (VRRuntime::Error)result;
    }

    return VRRuntime::Error::SUCCESS;
}

uint32_t OpenXR::get_width() const {
    if (this->view_configs.empty()) {
        return 0;
    }
    return (uint32_t)((float)this->view_configs[0].recommendedImageRectWidth * this->resolution_scale);
}

uint32_t OpenXR::get_height() const {
    if (this->view_configs.empty()) {
        return 0;
    }
    return (uint32_t)((float)this->view_configs[0].recommendedImageRectHeight * this->resolution_scale);
}

VRRuntime::Error OpenXR::consume_events(std::function<void(void*)> callback) {
    std::scoped_lock _{sync_mtx};

    XrEventDataBuffer edb{XR_TYPE_EVENT_DATA_BUFFER};
    auto result = xrPollEvent(this->instance, &edb);
    const auto bh = (XrEventDataBaseHeader*)&edb;

    while (result == XR_SUCCESS) {
        spdlog::info("[VR] xrEvent: {}", this->get_structure_string(bh->type));

        if (callback) {
            callback(&edb);
        }

        if (bh->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto ev = (XrEventDataSessionStateChanged*)&edb;
            this->session_state = ev->state;

            if (ev->state == XR_SESSION_STATE_READY) {
                spdlog::info("[VR] XR_SESSION_STATE_READY");

                XrSessionBeginInfo session_begin_info{XR_TYPE_SESSION_BEGIN_INFO};
                session_begin_info.primaryViewConfigurationType = this->view_config;

                auto begin_result = xrBeginSession(this->session, &session_begin_info);
                if (begin_result != XR_SUCCESS) {
                    this->error = std::string{"xrBeginSession failed: "} + this->get_result_string(begin_result);
                    spdlog::error("[VR] {}", this->error.value());
                } else {
                    this->session_ready = true;
                    synchronize_frame();
                }
            } else if (ev->state == XR_SESSION_STATE_LOSS_PENDING) {
                spdlog::info("[VR] XR_SESSION_STATE_LOSS_PENDING");
                this->wants_reinitialize = true;
            } else if (ev->state == XR_SESSION_STATE_STOPPING) {
                spdlog::info("[VR] XR_SESSION_STATE_STOPPING");
                if (this->ready()) {
                    xrEndSession(this->session);
                    this->session_ready = false;
                    this->frame_synced = false;
                    this->frame_began = false;
                }
            }
        } else if (bh->type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
            this->wants_reset_origin = true;
        }

        edb = {XR_TYPE_EVENT_DATA_BUFFER};
        result = xrPollEvent(this->instance, &edb);
    }

    if (result != XR_EVENT_UNAVAILABLE) {
        spdlog::error("[VR] xrPollEvent failed: {}", this->get_result_string(result));
        return (VRRuntime::Error)result;
    }

    return VRRuntime::Error::SUCCESS;
}

VRRuntime::Error OpenXR::update_matrices(float nearz, float farz) {
    if (!this->session_ready || this->views.empty()) {
        return VRRuntime::Error::SUCCESS;
    }

    std::unique_lock __{this->eyes_mtx};
    std::unique_lock ___{this->pose_mtx};

    for (auto i = 0; i < 2 && i < (int)this->views.size(); ++i) {
        const auto& pose = this->views[i].pose;
        const auto& fov = this->views[i].fov;

        // Projection (reverse-Z), and stash the raw tangents so adapters can rebuild it
        // their own way (see guide 08 §3, anvil onCalcProjection reads frustums[eye]).
        this->projections[i] = create_projection(fov, nearz, farz);

        this->frustums[i] = {
            std::tan(fov.angleLeft),
            std::tan(fov.angleRight),
            std::tan(fov.angleUp),
            std::tan(fov.angleDown),
        };

        this->raw_projections[i] = Vector4f{fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown};

        // Eye (eye-to-head) transform from the VIEW-space pose.
        // NOTE: this reinterpret-cast (faithful to REFramework) assumes glm::quat stores
        // {x,y,z,w} to match XrQuaternionf. Build with GLM_FORCE_QUAT_DATA_XYZW (see CMake notes).
        this->eyes[i] = Matrix4x4f{*(glm::quat*)&pose.orientation};
        this->eyes[i][3] = Vector4f{*(Vector3f*)&pose.position, 1.0f};
    }

    return VRRuntime::Error::SUCCESS;
}

void OpenXR::destroy() {
    if (!this->loaded) {
        return;
    }

    std::scoped_lock _{sync_mtx};

    if (this->session != XR_NULL_HANDLE) {
        if (this->session_ready) {
            xrEndSession(this->session);
        }
        xrDestroySession(this->session);
    }

    if (this->instance != XR_NULL_HANDLE) {
        xrDestroyInstance(this->instance);
        this->instance = XR_NULL_HANDLE;
    }

    this->session = XR_NULL_HANDLE;
    this->session_ready = false;
    this->system = XR_NULL_SYSTEM_ID;
    this->frame_synced = false;
    this->frame_began = false;
    this->loaded = false;
}

std::string OpenXR::get_result_string(XrResult result) const {
    char buf[XR_MAX_RESULT_STRING_SIZE]{};
    if (this->instance != XR_NULL_HANDLE) {
        xrResultToString(this->instance, result, buf);
        return buf;
    }
    return std::to_string((int32_t)result);
}

std::string OpenXR::get_structure_string(XrStructureType type) const {
    char buf[XR_MAX_STRUCTURE_NAME_SIZE]{};
    if (this->instance != XR_NULL_HANDLE) {
        xrStructureTypeToString(this->instance, type, buf);
        return buf;
    }
    return std::to_string((int32_t)type);
}

// ---------------------------------------------------------------------------
// initialize(): the full instance->system->session->spaces flow, ported from
// REFramework VR::initialize_openxr() but self-contained (no VR/D3D12Component coupling).
// Validated equivalent: XrSimTest.cpp lines 82-145.
// ---------------------------------------------------------------------------
std::optional<std::string> OpenXR::initialize(ID3D12Device* d3d12_device, ID3D12CommandQueue* d3d12_queue) {
    spdlog::info("[VR] Initializing OpenXR runtime");

    this->needs_pose_update = true;
    this->got_first_poses = false;

    XrResult result{XR_SUCCESS};

    // --- Dynamic loader: LoadLibrary(openxr_loader.dll) + resolve xrGetInstanceProcAddr/xrCreateInstance.
    if (!xrd::load_loader()) {
        this->error = "Could not load openxr_loader.dll (dynamic OpenXR loader)";
        spdlog::error("[VR] {}", this->error.value());
        return this->error;
    }

    // --- Instance (enable D3D12) ---
    if (this->instance == XR_NULL_HANDLE) {
        const char* extensions[] = {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};

        XrInstanceCreateInfo instance_create_info{XR_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.enabledExtensionCount = 1;
        instance_create_info.enabledExtensionNames = extensions;
        std::snprintf(instance_create_info.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "vrframework");
        instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        result = xrCreateInstance(&instance_create_info, &this->instance);
        if (result != XR_SUCCESS) {
            this->error = "Could not create OpenXR instance: " + std::to_string((int32_t)result);
            spdlog::error("[VR] {}", this->error.value());
            return this->error;
        }

        // Resolve the rest of the entry points against the new instance.
        if (!xrd::load_instance_fns(this->instance)) {
            this->error = "Could not resolve OpenXR instance entry points";
            spdlog::error("[VR] {}", this->error.value());
            return this->error;
        }
    }

    // --- System ---
    if (this->system == XR_NULL_SYSTEM_ID) {
        XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
        system_info.formFactor = this->form_factor;

        result = xrGetSystem(this->instance, &system_info, &this->system);
        if (result != XR_SUCCESS) {
            this->error = "Could not get OpenXR system: " + this->get_result_string(result);
            spdlog::error("[VR] {}", this->error.value());
            return this->error;
        }
    }

    // --- D3D12 graphics requirements (must call before xrCreateSession per spec) ---
    PFN_xrGetD3D12GraphicsRequirementsKHR get_requirements = nullptr;
    result = xrGetInstanceProcAddr(this->instance, "xrGetD3D12GraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&get_requirements);
    if (result != XR_SUCCESS || get_requirements == nullptr) {
        this->error = "Could not resolve xrGetD3D12GraphicsRequirementsKHR";
        spdlog::error("[VR] {}", this->error.value());
        return this->error;
    }

    XrGraphicsRequirementsD3D12KHR graphics_requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    result = get_requirements(this->instance, this->system, &graphics_requirements);
    if (result != XR_SUCCESS) {
        this->error = "xrGetD3D12GraphicsRequirementsKHR failed: " + this->get_result_string(result);
        spdlog::error("[VR] {}", this->error.value());
        return this->error;
    }

    // --- Session (chain the D3D12 binding: device + queue from the framework's hook) ---
    this->graphics_binding = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    this->graphics_binding.device = d3d12_device;
    this->graphics_binding.queue = d3d12_queue;

    XrSessionCreateInfo session_create_info{XR_TYPE_SESSION_CREATE_INFO};
    session_create_info.next = &this->graphics_binding;
    session_create_info.systemId = this->system;

    result = xrCreateSession(this->instance, &session_create_info, &this->session);
    if (result != XR_SUCCESS) {
        this->error = "Could not create OpenXR session: " + this->get_result_string(result);
        spdlog::error("[VR] {}", this->error.value());
        return this->error;
    }

    // --- Reference spaces: LOCAL (stage/projection) + VIEW (eye-to-head) ---
    if (this->stage_space == XR_NULL_HANDLE) {
        XrReferenceSpaceCreateInfo space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space_create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        space_create_info.poseInReferenceSpace.orientation.w = 1.0f;

        result = xrCreateReferenceSpace(this->session, &space_create_info, &this->stage_space);
        if (result != XR_SUCCESS) {
            this->error = "Could not create OpenXR stage space: " + this->get_result_string(result);
            spdlog::error("[VR] {}", this->error.value());
            return this->error;
        }
    }

    if (this->view_space == XR_NULL_HANDLE) {
        XrReferenceSpaceCreateInfo space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space_create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        space_create_info.poseInReferenceSpace.orientation.w = 1.0f;

        result = xrCreateReferenceSpace(this->session, &space_create_info, &this->view_space);
        if (result != XR_SUCCESS) {
            this->error = "Could not create OpenXR view space: " + this->get_result_string(result);
            spdlog::error("[VR] {}", this->error.value());
            return this->error;
        }
    }

    // --- System properties (informational) ---
    XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
    if (xrGetSystemProperties(this->instance, this->system, &system_properties) == XR_SUCCESS) {
        spdlog::info("[VR] OpenXR system: {} (vendor {})", system_properties.systemName, system_properties.vendorId);
    }

    // --- View configuration -> per-eye recommended sizes ---
    if (auto err = update_render_target_size(); err != VRRuntime::Error::SUCCESS) {
        return this->error.value_or("update_render_target_size failed");
    }

    if (this->view_configs.empty()) {
        this->error = "No view configurations found";
        spdlog::error("[VR] {}", this->error.value());
        return this->error;
    }

    this->views.resize(this->view_configs.size(), {XR_TYPE_VIEW});
    this->stage_views.resize(this->view_configs.size(), {XR_TYPE_VIEW});

    this->loaded = true;
    spdlog::info("[VR] OpenXR initialized: {} views, {}x{} per eye",
        this->view_configs.size(), get_width(), get_height());

    return std::nullopt;
}

XrResult OpenXR::begin_frame() {
    std::scoped_lock _{sync_mtx};

    if (!this->ready() || !this->got_first_poses || !this->frame_synced) {
        return XR_ERROR_SESSION_NOT_READY;
    }

    if (this->frame_began) {
        return XR_SUCCESS;
    }

    XrFrameBeginInfo frame_begin_info{XR_TYPE_FRAME_BEGIN_INFO};
    auto result = xrBeginFrame(this->session, &frame_begin_info);

    if (result != XR_SUCCESS && result != XR_FRAME_DISCARDED) {
        spdlog::error("[VR] xrBeginFrame failed: {}", this->get_result_string(result));
    }

    // If we somehow lost sync, resync and retry once.
    if (result == XR_ERROR_CALL_ORDER_INVALID) {
        synchronize_frame();
        result = xrBeginFrame(this->session, &frame_begin_info);
    }

    // XR_FRAME_DISCARDED still counts as "begun" (endFrame was not called last time).
    this->frame_began = (result == XR_SUCCESS || result == XR_FRAME_DISCARDED);
    return result;
}

XrResult OpenXR::end_frame(const std::vector<XrCompositionLayerBaseHeader*>& extra_layers) {
    std::scoped_lock _{sync_mtx};

    if (!this->ready() || !this->got_first_poses || !this->frame_synced) {
        return XR_ERROR_SESSION_NOT_READY;
    }

    if (!this->frame_began) {
        spdlog::info("[VR] end_frame called while frame not begun");
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    std::vector<XrCompositionLayerBaseHeader*> layers{};
    std::vector<XrCompositionLayerProjectionView> projection_layer_views{};
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

    // Only submit layers when the runtime wants us to render, or xrEndFrame errors.
    if (this->frame_state.shouldRender == XR_TRUE && !this->swapchains.empty()) {
        projection_layer_views.resize(this->stage_views.size(), {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

        for (size_t i = 0; i < projection_layer_views.size() && i < this->swapchains.size(); ++i) {
            const auto& swapchain = this->swapchains[i];

            projection_layer_views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            projection_layer_views[i].pose = this->stage_views[i].pose;
            projection_layer_views[i].fov = this->stage_views[i].fov;
            projection_layer_views[i].subImage.swapchain = swapchain.handle;
            projection_layer_views[i].subImage.imageRect.offset = {0, 0};
            projection_layer_views[i].subImage.imageRect.extent = {swapchain.width, swapchain.height};
        }

        layer.space = this->stage_space;
        layer.viewCount = (uint32_t)projection_layer_views.size();
        layer.views = projection_layer_views.data();
        layers.push_back((XrCompositionLayerBaseHeader*)&layer);
    }

    for (auto* extra : extra_layers) {
        layers.push_back(extra);
    }

    XrFrameEndInfo frame_end_info{XR_TYPE_FRAME_END_INFO};
    frame_end_info.displayTime = this->frame_state.predictedDisplayTime;
    frame_end_info.environmentBlendMode = this->blend_mode;
    frame_end_info.layerCount = (uint32_t)layers.size();
    frame_end_info.layers = layers.data();

    auto result = xrEndFrame(this->session, &frame_end_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrEndFrame failed: {}", this->get_result_string(result));
    }

    this->frame_began = false;
    this->frame_synced = false;
    return result;
}

} // namespace runtimes
