#include "spi/FrameTimeline.hpp"

// The frame-sync state machine, generalized from the two implementations in the ports:
//  - starfield: setReflexMarkerInternal decoding markers 6/0/1 -> wait+begin+hmd+imgui,
//    marker 2 -> render_frame, marker 4 -> presenter_frame, with a >100-frame drift guard.
//  - anvil: on_begin_engine_frame -> imgui; on_begin_render_frame -> begin+hmd.
// Mapping each engine's events onto FrameTimeline::Event lets one implementation serve both.

void FrameTimeline::report(Event ev, uint32_t frame) {
    switch (ev) {
    case Event::ENGINE_FRAME_BEGIN:
        m_engine_frame = frame ? frame : (m_engine_frame + 1);
        if (m_cb.on_request_imgui) m_cb.on_request_imgui();
        break;

    case Event::WAIT_RENDER:
        if (m_cb.on_wait_rendering) m_cb.on_wait_rendering(frame ? frame : m_engine_frame);
        m_engine_frame = frame ? frame : m_engine_frame;
        if (m_cb.on_begin_rendering) m_cb.on_begin_rendering(m_engine_frame);
        if (m_cb.on_update_hmd_state) m_cb.on_update_hmd_state(m_engine_frame);
        // Track L/R cadence: even engine frame begins a left-eye pair.
        m_sync_started = (m_engine_frame % 2) == 0;
        ++m_frames_since_reset;
        break;

    case Event::RENDER_FRAME_BEGIN:
        m_render_frame = frame ? frame : m_engine_frame;
        break;

    case Event::PRESENT_BEGIN:
        m_presenter_frame = frame ? frame : m_render_frame;
        // Drift guard (port heuristic): after warmup, an unexpected mid-cadence present
        // means L/R fell out of step — drop one present to re-align.
        if (m_frames_since_reset > 100 && m_sync_started) {
            m_skip_next_present = true;
            m_frames_since_reset = 0;
            m_sync_started = false;
        }
        break;

    case Event::PRESENT_END:
        break;
    }
}
