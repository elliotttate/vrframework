#include "hooks/WindowsMessageHook.hpp"

// PORT FROM: REFramework/src/WindowsMessageHook.cpp — engine-agnostic, copy verbatim. STUB.

WindowsMessageHook::WindowsMessageHook(HWND wnd) : m_wnd{ wnd } {
    // PORT: SetWindowLongPtr(GWLP_WNDPROC) subclass; store m_original_proc.
}

WindowsMessageHook::~WindowsMessageHook() {
    // PORT: restore m_original_proc.
}

LRESULT WindowsMessageHook::on_message(HWND wnd, UINT message, WPARAM w, LPARAM l) {
    if (m_on_message && !m_on_message(wnd, message, w, l)) return 0;
    if (m_original_proc) return CallWindowProc(m_original_proc, wnd, message, w, l);
    return DefWindowProc(wnd, message, w, l);
}

void WindowsMessageHook::window_toggle_cursor(bool show) { /* PORT */ }
