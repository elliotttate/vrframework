# 06 — Talking to the Headset: OpenVR & OpenXR

## What this covers / why it matters

Everything in the previous guides — the d3d hooks, the imgui overlay, the AFR frame loop — exists to feed pixels and poses to *something*. That something is the **VR runtime**: the software (SteamVR/OpenVR, or an OpenXR runtime like Oculus, Virtual Desktop's VDXR, or WMR) that owns the headset, reads its sensors, predicts where your head will be when the frame is displayed, and runs the *compositor* that warps your submitted images onto the panels at the last possible millisecond.

The mod needs four things from that runtime, every frame:

1. **Per-eye projection matrices** (the headset's exact FOV/asymmetry, not the game's).
2. **Per-eye eye-to-head transforms** (where each lens sits relative to the head center — your IPD).
3. **The head pose** (predicted position + orientation), to compose into the game's view matrix.
4. **A synchronization heartbeat** — `WaitGetPoses`/`xrWaitFrame` — that paces the engine to the headset's display.

The problem this guide solves: there are *two* incompatible APIs that provide those four things (OpenVR and OpenXR), and you want the other 95% of your codebase to never know or care which one is live. The answer is a single abstract base class, `VRRuntime`, with two concrete subclasses. Get the abstraction right and every camera/timing/input module downstream becomes runtime-agnostic for free.

All citations below are real and verified. The canonical implementation is REFramework; the cleaned-up, engine-free port lives in `vrframework`.

---

## The abstraction: one interface, two backends

The entire contract lives in one header. Read it top to bottom — it is short and it *is* the design:

- `REFramework/src/mods/vr/runtimes/VRRuntime.hpp`
- `vrframework/include/vr/VRRuntime.hpp` (the port; `sdk/Math.hpp` → `Math.hpp`/glm, otherwise identical)

The base struct is a classic virtual interface with sane no-op defaults so a half-implemented backend still links and runs:

```cpp
struct VRRuntime {
    enum class Type : uint8_t { NONE, OPENXR, OPENVR };
    enum class Eye  : uint8_t { LEFT, RIGHT };
    enum class SynchronizeStage : int32_t { EARLY, LATE, VERY_LATE };

    virtual Type  type() const { return Type::NONE; }
    virtual bool  ready() const { return this->loaded; }

    virtual Error synchronize_frame()         { return Error::SUCCESS; }
    virtual Error update_poses()              { return Error::SUCCESS; }
    virtual Error update_render_target_size() { return Error::SUCCESS; }
    virtual Error consume_events(std::function<void(void*)> cb) { return Error::SUCCESS; }
    virtual Error update_matrices(float nearz, float farz)      { return Error::SUCCESS; }
    virtual Error update_input()              { return Error::SUCCESS; }
    virtual uint32_t get_width()  const { return 0; }
    virtual uint32_t get_height() const { return 0; }
    // ...
};
```
(`VRRuntime.hpp:14-95`)

Two things make this abstraction actually work in practice rather than just on paper:

**1. The *outputs* live in the base class, not the subclasses.** The projection matrices, eye transforms, raw frustum bounds, and their mutexes are fields on `VRRuntime` itself:

```cpp
std::array<Matrix4x4f, 2> projections{};
std::array<Matrix4x4f, 2> eyes{};
Vector4f raw_projections[2]{};
mutable std::shared_mutex projections_mtx{};
mutable std::shared_mutex eyes_mtx{};
mutable std::shared_mutex pose_mtx{};
```
(`VRRuntime.hpp:121-128`)

So the camera code calls `get_runtime()->update_matrices(nearz, farz)` (a virtual that fans out to OpenVR or OpenXR) and then reads `get_runtime()->projections[eye]` — a plain field — under a shared lock. The reader never branches on runtime type. OpenVR fills those arrays from `IVRSystem`; OpenXR fills them from `XrView`. The consumer is none the wiser. The vrframework port adds an explicit `frustums[2]` array for the same reason — engine adapters read `get_runtime()->frustums[eye]` to build an asymmetric projection without knowing the source (`vrframework/include/vr/VRRuntime.hpp:57-60`).

**2. A pile of `bool` state flags encode the lifecycle**, and they're also in the base class so the orchestrator can poll them uniformly:

```cpp
bool loaded{false};
bool wants_reinitialize{false};
bool dll_missing{false};
bool needs_pose_update{true};
bool got_first_poses{false};
bool got_first_valid_poses{false};
bool got_first_sync{false};
bool wants_reset_origin{true};
```
(`VRRuntime.hpp:105-117`)

We'll meet each of these as we walk the lifecycle. The two helpers `is_openxr()` / `is_openvr()` exist for the *handful* of places that genuinely must branch (mostly frame submission, which differs structurally). Everywhere else, virtual dispatch keeps the branch out of your code.

### Why support both at all?

| | OpenVR | OpenXR |
|---|---|---|
| Vendor | Valve (SteamVR) | Khronos open standard |
| DLL the mod loads | `openvr_api.dll` | `openxr_loader.dll` |
| Status | Mature, ubiquitous, effectively legacy | The future; native to Oculus/WMR/VDXR/Pico |
| Frame submit model | `WaitGetPoses` + `Submit` per texture | `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame` + swapchains + composition layers |
| Latency on non-Steam HMDs | Goes through SteamVR's compositor | Talks to the native runtime directly (lower latency) |
| Input | Action manifests + `IVRInput` | Action sets + interaction profiles |

OpenVR is the path of least resistance — almost everyone has SteamVR. But for a Quest user on Virtual Desktop, or a WMR headset, going through SteamVR adds a compositor hop and latency. OpenXR talks to the native runtime directly. Supporting both means you serve the SteamVR majority *and* give power users the lower-latency native path. Because both hide behind `VRRuntime`, that's a backend choice at init time, not a fork of your rendering code.

REFramework's selection policy is blunt and effective: **try OpenVR first; fall back to OpenXR only if OpenVR didn't load** (`VR.cpp:620-642`). If `openvr_api.dll` is present next to the exe you get OpenVR; to force OpenXR you delete that dll and drop `openxr_loader.dll` instead. The mod even prints that exact instruction into the OpenXR error slot when OpenVR wins (`VR.cpp:638-641`).

---

## Initialization

Both backends are constructed unconditionally and held as `shared_ptr`s; `m_runtime` is then pointed at whichever one loaded. Initialization is a *d3d-thread* concern — it needs the live graphics device — so it runs from `on_initialize_d3d_thread()` (`VR.cpp:619`).

### OpenVR init (the short version)

```cpp
m_openvr = std::make_shared<runtimes::OpenVR>();
m_openvr->loaded = false;
// ... LoadLibraryA("openvr_api.dll"); if it fails:
m_openvr->dll_missing = true;
m_openvr->error = "Could not load openvr_api.dll";
// ... otherwise:
m_openvr->hmd = vr::VR_Init(&error, vr::VRApplication_Scene);   // VR.cpp:791
m_openvr->update_render_target_size();                          // VR.cpp:805
m_openvr->loaded = true;
m_runtime = m_openvr;                                           // VR.cpp:828
```
(`VR.cpp:768-828`)

The sequence: load the dll (set `dll_missing` if absent), `VR_Init` as a *scene* application, confirm the `IVRSystem*` is non-null, query the render target size, initialize the compositor, hijack input/overlay, then flip `loaded = true` and adopt it as `m_runtime`. Note `dll_missing` is distinct from `error` — a missing dll is "user didn't install this backend," not "this backend is broken."

### OpenXR init (more ceremony)

OpenXR is a multi-step handshake. Read `OpenXR.hpp:106-139` for the field set you must keep alive: an `XrInstance`, `XrSession`, `XrSystemId`, two reference spaces (`stage_space` for world-locked content, `view_space` for generating view matrices), the view config, blend mode, and the per-view structs.

```cpp
m_openxr = std::make_shared<runtimes::OpenXR>();
// LoadLibrary("openxr_loader.dll") else dll_missing + error  (VR.cpp:891-892)
xrCreateInstance(&instance_create_info, &m_openxr->instance);  // VR.cpp:929
xrGetSystem(m_openxr->instance, &system_info, &m_openxr->system);
xrCreateSession(m_openxr->instance, &session_create_info, &m_openxr->session);
xrCreateReferenceSpace(...&m_openxr->stage_space);             // VR.cpp:996
m_openxr->update_render_target_size();                         // VR.cpp:1044
```
(`VR.cpp:884-1044`)

The graphics-binding wrinkle: OpenXR needs to know your D3D device/queue at session creation, which is why `OpenXR.hpp` defines `XR_USE_GRAPHICS_API_D3D11` *and* `XR_USE_GRAPHICS_API_D3D12` (`OpenXR.hpp:11-12`) — the same backend serves both, and the binding struct passed to `xrCreateSession` differs per renderer.

### Render target size

Both backends implement `update_render_target_size()` / `get_width()` / `get_height()`, but they get the numbers very differently:

```cpp
// OpenVR: one call, the runtime hands you the recommended size
VRRuntime::Error OpenVR::update_render_target_size() {
    this->hmd->GetRecommendedRenderTargetSize(&this->w, &this->h);
    return VRRuntime::Error::SUCCESS;
}
```
(`OpenVR.cpp:34-46`)

```cpp
// OpenXR: enumerate view configurations, read recommendedImageRect*, scale it
uint32_t OpenXR::get_width() const {
    if (this->view_configs.empty()) return 0;
    return (uint32_t)((float)this->view_configs[0].recommendedImageRectWidth * this->resolution_scale);
}
```
(`OpenXR.cpp:136-172`)

OpenXR also exposes a `resolution_scale` knob (`OpenXR.hpp:139`) so the user can trade sharpness for framerate; OpenVR's equivalent lives in SteamVR settings and arrives via an event (more on that below). **This is the size your AFR render targets must match** — get it wrong and the compositor stretches or crops your eye image.

---

## Getting per-eye projections and eye-to-head transforms

This is `update_matrices(nearz, farz)`, and it's the cleanest illustration of why the abstraction earns its keep: two completely different APIs, identical output into `projections[2]` and `eyes[2]`.

### OpenVR

```cpp
VRRuntime::Error OpenVR::update_matrices(float nearz, float farz) {
    std::unique_lock __{ this->eyes_mtx };
    const auto local_left  = this->hmd->GetEyeToHeadTransform(vr::Eye_Left);
    const auto local_right = this->hmd->GetEyeToHeadTransform(vr::Eye_Right);
    this->eyes[vr::Eye_Left]  = glm::rowMajor4(Matrix4x4f{ *(Matrix3x4f*)&local_left });
    this->eyes[vr::Eye_Right] = glm::rowMajor4(Matrix4x4f{ *(Matrix3x4f*)&local_right });

    auto pleft  = this->hmd->GetProjectionMatrix(vr::Eye_Left,  nearz, farz);
    auto pright = this->hmd->GetProjectionMatrix(vr::Eye_Right, nearz, farz);
    this->projections[vr::Eye_Left]  = glm::rowMajor4(Matrix4x4f{ *(Matrix4x4f*)&pleft });
    this->projections[vr::Eye_Right] = glm::rowMajor4(Matrix4x4f{ *(Matrix4x4f*)&pright });

    this->hmd->GetProjectionRaw(vr::Eye_Left,  &this->raw_projections[vr::Eye_Left][0], ...);
    this->hmd->GetProjectionRaw(vr::Eye_Right, &this->raw_projections[vr::Eye_Right][0], ...);
}
```
(`OpenVR.cpp:83-101`)

OpenVR hands you finished matrices. Note `GetEyeToHeadTransform` returns a 3×4 — the code reinterprets it into a 4×4 and `rowMajor4`s it into the engine's convention. `GetProjectionRaw` additionally returns the raw frustum tangents (`left/right/top/bottom`), stored in `raw_projections` for engines that want to rebuild the projection themselves.

### OpenXR

```cpp
VRRuntime::Error OpenXR::update_matrices(float nearz, float farz) {
    if (!this->session_ready || this->views.empty()) return VRRuntime::Error::SUCCESS;
    std::unique_lock __{ this->eyes_mtx };
    std::unique_lock ___{ this->pose_mtx };

    for (auto i = 0; i < 2; ++i) {
        const auto& pose = this->views[i].pose;
        const auto& fov  = this->views[i].fov;

        XrMatrix4x4f_CreateProjection((XrMatrix4x4f*)&this->projections[i], GRAPHICS_D3D,
            tan(fov.angleLeft), tan(fov.angleRight), tan(fov.angleUp), tan(fov.angleDown),
            nearz, farz);

        this->eyes[i]    = Matrix4x4f{*(glm::quat*)&pose.orientation};
        this->eyes[i][3] = Vector4f{*(Vector3f*)&pose.position, 1.0f};
    }
}
```
(`OpenXR.cpp:244-265`)

OpenXR gives you *raw FOV angles* (`fov.angleLeft/Right/Up/Down`) and a *pose*, and you build the projection yourself with the `xr_linear.h` helper. The eye transform is assembled from the view's orientation quaternion plus position. Notice `GRAPHICS_D3D` — the projection's depth convention (Z range, handedness) is API-specific, so the matrix builder is told which graphics API it's targeting. **This is a common footgun:** if you feed an OpenGL-convention projection into D3D you get inverted or clipped depth.

The headset's FOV is *asymmetric* (the left and right halves of each lens have different angles) and the two eyes are *not* mirror images. You cannot fake this with a symmetric `perspective()`. That's the whole reason these matrices come from the runtime rather than from the game.

> Both implementations lock before writing, and both write into base-class arrays. The camera module (guide 07) reads `eyes[eye]` and `projections[eye]` under `eyes_mtx`/`projections_mtx` and never asks which backend produced them.

---

## The pose lifecycle and frame synchronization

This is the subtle part, and where the `bool` flags pay off. The runtime's job here is twofold: hand you a *predicted* head pose, and *pace* your engine so you render exactly when the compositor is ready.

### `synchronize_frame()` — the heartbeat

```cpp
// OpenVR: the famous WaitGetPoses
VRRuntime::Error OpenVR::synchronize_frame() {
    if (this->got_first_poses && !this->is_hmd_active) return VRRuntime::Error::SUCCESS;
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
    auto ret = vr::VRCompositor()->WaitGetPoses(
        this->real_render_poses.data(), vr::k_unMaxTrackedDeviceCount,
        this->real_game_poses.data(),   vr::k_unMaxTrackedDeviceCount);
    if (ret == vr::VRCompositorError_None) {
        this->got_first_valid_poses = true;
        this->got_first_sync = true;
    }
    return (VRRuntime::Error)ret;
}
```
(`OpenVR.cpp:6-20`)

`WaitGetPoses` is a **blocking** call. It returns when the compositor decides it's time for the app to start the next frame — that's the throttle that keeps you locked to the display's cadence. It also fills two pose arrays: `render_poses` (for *this* frame's rendering) and `game_poses` (predicted further ahead, for game logic). The crucial fact, called out right in the header:

```cpp
// in the case of OpenVR we always need at least one initial WaitGetPoses before the game will render
// even if we don't have anything to submit yet, otherwise the compositor
// will return VRCompositorError_DoNotHaveFocus
bool needs_pose_update{true};
```
(`VRRuntime.hpp:109-112`)

**The first `WaitGetPoses` must happen before the game renders.** If you try to submit a frame before ever syncing, the compositor refuses you focus and you get a black headset. `got_first_sync` records that the heartbeat has started at least once; `got_first_poses` records that the mod has copied a usable pose out of it.

OpenXR splits the heartbeat across three calls. `synchronize_frame()` is `xrWaitFrame`:

```cpp
VRRuntime::Error OpenXR::synchronize_frame() {
    std::scoped_lock _{sync_mtx};
    if (!this->session_ready || this->frame_began) return VRRuntime::Error::UNSPECIFIED;
    if (this->frame_synced) return VRRuntime::Error::SUCCESS;

    XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO};
    this->frame_state = {XR_TYPE_FRAME_STATE};
    auto result = xrWaitFrame(this->session, &frame_wait_info, &this->frame_state);
    if (result == XR_SUCCESS) { this->got_first_sync = true; this->frame_synced = true; }
}
```
(`OpenXR.cpp:16-45`)

`xrWaitFrame` is the pacing call (it also throttles), and it fills `frame_state` with the all-important `predictedDisplayTime` — the timestamp the compositor expects to scan out this frame. Note the guard: **you cannot `xrWaitFrame` between `xrBeginFrame` and `xrEndFrame`** (`frame_began`), and you must not sync twice (`frame_synced`). OpenXR's call-ordering rules are strict and violating them returns `XR_ERROR_CALL_ORDER_INVALID`; `begin_frame()` even recovers from that by re-syncing and retrying (`OpenXR.cpp:1280-1283`).

### `update_poses()` — locating views at the predicted time

```cpp
VRRuntime::Error OpenVR::update_poses() {
    if (!this->ready()) return VRRuntime::Error::SUCCESS;
    std::unique_lock _{ this->pose_mtx };
    memcpy(this->render_poses.data(), this->real_render_poses.data(), sizeof(this->render_poses));
    this->needs_pose_update = false;
}
```
(`OpenVR.cpp:22-32`)

For OpenVR this is trivial — copy the freshly-waited poses into the working set under the pose lock. The comment in `update_hmd_state` explains *why* the copy exists: `WaitGetPoses` blocks, so the mod snapshots its output into `render_poses` under a fast mutex rather than making game logic wait on the blocking call (`VR.cpp:1381-1383`).

OpenXR's `update_poses()` does the real work — it *locates* the views at the predicted display time:

```cpp
const auto display_time = this->frame_state.predictedDisplayTime
    + (XrDuration)(this->frame_state.predictedDisplayPeriod * this->prediction_scale);

XrViewLocateInfo view_locate_info{XR_TYPE_VIEW_LOCATE_INFO};
view_locate_info.displayTime = display_time;
view_locate_info.space       = this->view_space;
xrLocateViews(this->session, &view_locate_info, &this->view_state,
              (uint32_t)this->views.size(), &view_count, this->views.data());
// ... again for stage_space -> stage_views
xrLocateSpace(this->view_space, this->stage_space, display_time, &this->view_space_location);
for (auto& hand : this->hands) xrLocateSpace(hand.space, this->stage_space, display_time, &hand.location);
```
(`OpenXR.cpp:84-125`)

Two locates per frame, deliberately: `views` (relative to `view_space`, used to build the *view matrices* the game consumes) and `stage_views` (relative to `stage_space`, world-locked, used later for the *composition layer* you submit). The `prediction_scale` knob nudges the predicted time forward to compensate for the engine's own latency.

OpenXR is also defensive about garbage poses before tracking has converged — it refuses to set `got_first_valid_poses` until `predictedDisplayTime` looks sane, with two named workarounds for VDXR returning nonsense like `11111111`:

```cpp
if (this->frame_state.predictedDisplayTime == 11111111) {
    spdlog::info("[VR] Frame state predicted display time is 11111111!");
    return VRRuntime::Error::SUCCESS;
}
```
(`OpenXR.cpp:65-82`)

`got_first_valid_poses` is then gated on the actual location flags — position/orientation must be marked valid before the mod trusts the pose (`OpenXR.cpp:127-129`). This is the difference between "the API returned" and "the data is real," and conflating them gives you a frame or two of head-snapped-to-origin on startup.

### How the orchestrator drives it

`VR::update_hmd_state()` shows the canonical per-frame order, and how it threads the runtime-specific bits through the abstraction (`VR.cpp:1358-1407`):

1. If `SynchronizeStage::EARLY`, call `synchronize_frame()`. For OpenXR, also `begin_frame()` (`xrBeginFrame`) right after a successful sync.
2. `update_poses()`.
3. Handle `wants_reset_origin` (recenter) if the runtime asked for it.
4. `update_matrices(nearz, farz)`.
5. Mark `got_first_poses = true`.

That whole function branches on `is_openxr()` exactly *twice* — both times only to issue the OpenXR-specific `begin_frame`. Pose copying, recentering, matrix update: all virtual, all backend-blind. The `SynchronizeStage` enum (`EARLY`/`LATE`/`VERY_LATE`) lets an engine adapter choose *when* in its pipeline the blocking sync happens — some engines tolerate it early, some need it deferred to just before submit (`VR.cpp:3052-3058`). The two forks (starfield/anvil) pick the stage that matches where their frame hooks fire.

---

## Consuming runtime events

The runtime talks back: the user changed SteamVR's render scale, recentered, opened the dashboard, or the OpenXR session changed state. `consume_events()` drains that queue and lets a caller-supplied callback observe each event before the runtime handles the ones it cares about.

```cpp
VRRuntime::Error OpenVR::consume_events(std::function<void(void*)> callback) {
    vr::VREvent_t event{};
    while (this->hmd->PollNextEvent(&event, sizeof(event))) {
        if (callback) callback(&event);
        switch ((vr::EVREventType)event.eventType) {
            case vr::VREvent_SteamVRSectionSettingChanged:
                update_render_target_size();           // user changed resolution
                break;
            case vr::VREvent_SeatedZeroPoseReset:
            case vr::VREvent_StandingZeroPoseReset:
                this->wants_reset_origin = true;       // user recentered
                break;
            case vr::VREvent_DashboardActivated:
                this->handle_pause = true;             // pause the game
                break;
        }
    }
}
```
(`OpenVR.cpp:48-81`)

OpenXR's loop is `xrPollEvent`, and its most important job is driving the **session state machine** — which is where the *real* lifecycle of an OpenXR app lives:

```cpp
if (bh->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
    this->session_state = ev->state;
    if (ev->state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo session_begin_info{XR_TYPE_SESSION_BEGIN_INFO};
        session_begin_info.primaryViewConfigurationType = this->view_config;
        xrBeginSession(this->session, &session_begin_info);
        this->session_ready = true;
        synchronize_frame();
    } else if (ev->state == XR_SESSION_STATE_LOSS_PENDING) {
        this->wants_reinitialize = true;
    } else if (ev->state == XR_SESSION_STATE_STOPPING) {
        xrEndSession(this->session);
        this->session_ready = false;
        this->frame_synced = false;
        this->frame_began  = false;
    }
}
```
(`OpenXR.cpp:189-230`)

This is a *critical* difference between the two backends. With OpenVR you `VR_Init` and you're running. With OpenXR, **the session is not usable until the runtime sends `STATE_READY`** — only then may you `xrBeginSession`, and only after that does `session_ready` become true (which `OpenXR::ready()` requires, `OpenXR.hpp:43-45`). If the headset goes to sleep you get `STOPPING`; if the runtime is dying you get `LOSS_PENDING` and must reinitialize. A naive port that ignores session events will sit forever waiting for poses that never come.

Both backends also translate runtime-level recenter signals into the same base-class flag: OpenVR sets `wants_reset_origin` on a zero-pose-reset event, OpenXR sets it on `REFERENCE_SPACE_CHANGE_PENDING` (`OpenXR.cpp:228-229`). The orchestrator handles recentering identically afterward — abstraction intact.

---

## DLL-missing and reinitialization handling

Two failure modes, two flags.

**DLL missing (`dll_missing`).** Set at load time when `LoadLibrary` on `openvr_api.dll` / `openxr_loader.dll` fails (`VR.cpp:774, 891`). It is *not* an error in the "something broke" sense — it's how the selection logic knows a backend simply isn't installed, so it can quietly fall through to the other one. The init code is careful to clear `dll_missing` on both runtimes inside its catch-all so a thrown exception isn't misreported as a missing dll (`VR.cpp:693-694`).

**Reinitialize (`wants_reinitialize`).** Any code path can set this flag to request a clean teardown-and-rebuild of the live runtime, without the requester needing to know how. It gets set when:

- OpenXR's session goes `LOSS_PENDING` or `STOPPING` (`OpenXR.cpp:213, 223`).
- OpenVR input updates start timing out badly (>30 ms), a sign the runtime is wedged (`VR.cpp:1431-1435`).
- The user clicks "reinitialize" in the mod menu (`VR.cpp:3763`).
- The OpenXR bindings editor saves new bindings and needs to re-attach the action set (`OpenXR.cpp:1253`).

It gets *serviced* in one place, at a safe point in the frame, again dispatching through the abstraction:

```cpp
if (!inside_on_end && runtime->wants_reinitialize) {
    std::scoped_lock _{m_openvr_mtx};
    if (runtime->is_openvr())      { m_openvr->wants_reinitialize = false; reinitialize_openvr(); }
    else if (runtime->is_openxr()) { m_openxr->wants_reinitialize = false; reinitialize_openxr(); }
}
```
(`VR.cpp:2983-2993`)

Crucially it is checked `!inside_on_end` — you never tear down the runtime mid-submit. The flag defers the rebuild to a known-safe boundary. This is the pattern to copy: **requesters set a flag; one owner services it at a safe point.** It keeps teardown off the hot path and out of arbitrary call stacks.

`destroy()` is the matching teardown half of the contract — `OpenVR::destroy()` calls `vr::VR_Shutdown()` (`OpenVR.cpp:103-107`); `OpenXR::destroy()` ends the session if active, then destroys session and instance and resets every state flag (`OpenXR.cpp:335-360`). Reinitialize is `destroy()` followed by the init path.

---

## How the abstraction keeps the rest of the code runtime-agnostic

Step back and look at what a downstream module — say the camera (guide 07) or the frame timeline (guide 05) — actually touches:

```cpp
auto runtime = vr->get_runtime();          // VRRuntime*
runtime->synchronize_frame();              // virtual: WaitGetPoses OR xrWaitFrame
runtime->update_poses();                   // virtual: memcpy OR xrLocateViews
runtime->update_matrices(nearz, farz);     // virtual: IVRSystem OR XrView math
const auto p = runtime->projections[eye];  // plain field, filled by either backend
const auto e = runtime->eyes[eye];         // plain field
if (runtime->ready() && runtime->got_first_poses) { /* submit */ }
```

Not one line of that knows whether SteamVR or an OpenXR runtime is on the other end. The *only* legitimate places to branch on `is_openxr()` are:

1. **Frame submission**, because OpenVR's `Submit`-per-texture and OpenXR's swapchain/composition-layer model are structurally different (the d3d component, guide 04).
2. **`xrBeginFrame`/`xrEndFrame`**, which have no OpenVR equivalent and bracket the OpenXR frame (`VR.cpp:1371`, `OpenXR.cpp:1256-1349`).

Everything else — pose lifecycle, matrices, recenter, render target size, events, reinit — is uniform. That is the payoff. When `starfield2vr` and `anvilengine2vr` forked the shared `vrframework` core, they did *not* re-touch the runtime layer. They wrote new *engine adapters* (Layer 2) that call this same `VRRuntime` interface. The headset-facing code was written once and inherited twice.

### Applying this to a brand-new engine

You almost never need to modify the runtime layer at all. Lift `VRRuntime` + `OpenVR` + `OpenXR` wholesale. Your work is in the *adapter* that drives them:

1. **Init** the runtime on your d3d thread once you have the device (mirror `initialize_openvr`/`initialize_openxr`). Try OpenVR, fall back to OpenXR.
2. **Size** your AFR eye targets to `get_runtime()->get_width()/get_height()`.
3. Find the engine hook where you can afford to **block**, and call `synchronize_frame()` there. This choice is the hard part (see guide 05 on frame timing) — pick the `SynchronizeStage` that matches it. Ensure the *first* sync happens before the engine's first present, or the compositor denies you focus.
4. Each frame: `update_poses()` → `update_matrices(nearz, farz)` → read `eyes[]`/`projections[]`/`frustums[]` and inject them into the engine's view/projection (guide 07).
5. **Drain events** every frame via `consume_events()`; honor `wants_reset_origin`.
6. For OpenXR, **service the session state machine** — nothing works until `STATE_READY`.
7. Poll `wants_reinitialize` at a safe (non-submit) boundary and rebuild if set.

If your adapter only ever calls the virtual methods and reads the base-class fields, your engine support is automatically OpenVR-and-OpenXR with zero extra effort.

---

## Key takeaways

- **One interface, two backends.** `VRRuntime` is a virtual base with no-op defaults; `OpenVR` and `OpenXR` subclass it. Outputs (`projections`, `eyes`, `frustums`, `raw_projections`) and lifecycle flags live in the *base* class, so consumers read plain fields under a shared lock and never branch on backend.
- **Support both because they serve different users.** OpenVR = the SteamVR majority; OpenXR = lower latency on native runtimes (Oculus/WMR/VDXR). Selection is "try OpenVR, fall back to OpenXR," decided once at init.
- **Per-eye matrices must come from the runtime.** Headset FOV is asymmetric and per-eye; you can't synthesize it. OpenVR hands you finished matrices; OpenXR hands you FOV angles + pose and you build them (with the correct *graphics-API* depth convention).
- **The pose lifecycle is the subtle part.** The first `synchronize_frame()` (`WaitGetPoses` / `xrWaitFrame`) must run before the game renders, or the compositor denies focus. The `got_first_sync` / `got_first_poses` / `got_first_valid_poses` flags distinguish "API returned," "we copied a pose," and "the pose is actually valid."
- **OpenXR has a session state machine; OpenVR does not.** Nothing works until `consume_events()` sees `STATE_READY` and calls `xrBeginSession`. Ignoring session events is the classic OpenXR-port bug.
- **Failure handling is two flags.** `dll_missing` = backend not installed (fall through quietly); `wants_reinitialize` = rebuild requested by anyone, serviced by one owner at a safe non-submit boundary.
- **The reward is a runtime-agnostic codebase.** Camera, timing, and input code touch only virtual methods and base-class fields. The two engine forks inherited the entire headset layer without modifying it.

---

**Next:** `07-camera-and-stereo-rendering.md` — composing the HMD pose into the engine's view matrix (the `Y_UP_TO_Z_UP_BASIS` change, eye-to-head, decoupled pitch and head-aim) and submitting per-eye images under AFR.
