#pragma once

// NEW (no REFramework equivalent). The reusable frame-sync state machine.
//
// Today each port reinvents this: anvil uses two hooks (on_begin_engine_frame /
// on_begin_render_frame); starfield decodes Reflex markers (0/1/2/4/6) inside
// setReflexMarkerInternal. Both maintain the same engine/render/presenter counters and
// the same "detect drift -> skip a present" recovery. That logic is engine-agnostic and
// belongs here, parameterized by which engine events fire.
//
// An adapter just reports timeline EVENTS; FrameTimeline owns the counters, the
// even/odd eye cadence, and drift detection, then calls back into VR at the right edges.

#include <cstdint>
#include <functional>

class FrameTimeline {
public:
    // Logical points in a frame an engine can report. Map your engine's real hooks/markers
    // onto these (see EngineCaps + the adapter's install_hooks()).
    enum class Event : uint8_t {
        ENGINE_FRAME_BEGIN,   // simulation/update frame starts
        WAIT_RENDER,          // engine about to wait for the GPU / acquire (Reflex marker ~6)
        RENDER_FRAME_BEGIN,   // GPU recording for this frame starts
        PRESENT_BEGIN,        // submit/present edge
        PRESENT_END,
    };

    struct Callbacks {
        std::function<void(uint32_t frame)> on_wait_rendering;    // -> VR::on_wait_rendering
        std::function<void(uint32_t frame)> on_begin_rendering;   // -> VR::on_begin_rendering
        std::function<void(uint32_t frame)> on_update_hmd_state;  // -> VR::update_hmd_state
        std::function<void()>               on_request_imgui;     // -> Framework::run_imgui_frame
    };

    void set_callbacks(Callbacks cb) { m_cb = std::move(cb); }

    // Adapter calls this from its hooks/markers. `frame` is the engine's frame index when
    // it has one (Creation 2 passes oldFrameIndex); pass 0 to let the timeline count.
    void report(Event ev, uint32_t frame = 0);

    uint32_t engine_frame() const { return m_engine_frame; }
    uint32_t render_frame() const { return m_render_frame; }
    uint32_t presenter_frame() const { return m_presenter_frame; }

    // True when the next present should be dropped to re-align L/R cadence.
    bool wants_skip_present() const { return m_skip_next_present; }
    void clear_skip_present() { m_skip_next_present = false; }

    // Which eye this frame belongs to under AFR (even=LEFT, odd=RIGHT).
    bool is_left_eye_frame() const { return (m_presenter_frame % 2) == 0; }

private:
    Callbacks m_cb{};
    uint32_t m_engine_frame{ 0 };
    uint32_t m_render_frame{ 0 };
    uint32_t m_presenter_frame{ 0 };
    uint32_t m_frames_since_reset{ 0 };
    bool     m_sync_started{ false };
    bool     m_skip_next_present{ false };
};
