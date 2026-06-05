// PORT FROM: REFramework/src/WindowsMessageHook.cpp — engine-agnostic WndProc subclass.
//
// Adapted to vrframework's header (which exposes on_message() as a method + an
// on_message(fn) setter, rather than REFramework's public std::function member). The
// mechanism is identical: SetWindowLongPtr(GWLP_WNDPROC) to install a static thunk that
// routes into the single live hook instance.

#include <mutex>

#include <spdlog/spdlog.h>

#include "hooks/WindowsMessageHook.hpp"

#define VRF_TOGGLE_CURSOR (WM_APP + 1)

static WindowsMessageHook* g_windows_message_hook{ nullptr };
static std::recursive_mutex g_proc_mutex{};

static LRESULT WINAPI window_proc(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    std::scoped_lock _{ g_proc_mutex };

    if (g_windows_message_hook == nullptr) {
        return DefWindowProc(wnd, message, w_param, l_param);
    }

    return g_windows_message_hook->on_message(wnd, message, w_param, l_param);
}

WindowsMessageHook::WindowsMessageHook(HWND wnd) : m_wnd{ wnd } {
    std::scoped_lock _{ g_proc_mutex };
    spdlog::info("Initializing WindowsMessageHook");

    g_windows_message_hook = this;

    // Save the original window procedure, then install our thunk.
    m_original_proc = (WNDPROC)GetWindowLongPtr(m_wnd, GWLP_WNDPROC);
    SetWindowLongPtr(m_wnd, GWLP_WNDPROC, (LONG_PTR)&window_proc);

    spdlog::info("Hooked Windows message handler");
}

WindowsMessageHook::~WindowsMessageHook() {
    std::scoped_lock _{ g_proc_mutex };
    spdlog::info("Destroying WindowsMessageHook");

    // Don't restore unless our thunk is still the active proc and ours to restore.
    if (m_wnd != nullptr && m_original_proc != nullptr) {
        auto current_proc = (WNDPROC)GetWindowLongPtr(m_wnd, GWLP_WNDPROC);
        if (current_proc == &window_proc) {
            SetWindowLongPtr(m_wnd, GWLP_WNDPROC, (LONG_PTR)m_original_proc);
        }
    }

    m_wnd = nullptr;
    m_original_proc = nullptr;
    g_windows_message_hook = nullptr;
}

LRESULT WindowsMessageHook::on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    // Our own internal toggle message: actually show/hide the cursor here (the request
    // was PostMessage'd from window_toggle_cursor so it runs on the window thread).
    if (message == VRF_TOGGLE_CURSOR) {
        ::ShowCursor((bool)w_param);
        return 0;
    }

    if (m_on_message) {
        // If the callback returns false we swallow the message (don't pass to the game).
        if (!m_on_message(wnd, message, w_param, l_param)) {
            return DefWindowProc(wnd, message, w_param, l_param);
        }
    }

    if (m_original_proc != nullptr) {
        return CallWindowProc(m_original_proc, wnd, message, w_param, l_param);
    }

    return DefWindowProc(wnd, message, w_param, l_param);
}

void WindowsMessageHook::window_toggle_cursor(bool show) {
    if (m_wnd != nullptr) {
        ::PostMessage(m_wnd, VRF_TOGGLE_CURSOR, show, 1);
    }
}
