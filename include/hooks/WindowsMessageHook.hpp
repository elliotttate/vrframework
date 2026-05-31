#pragma once

// PORT FROM: REFramework/src/WindowsMessageHook.hpp — engine-agnostic WndProc subclass.
// Copy verbatim.

#include <functional>
#include <windows.h>

class WindowsMessageHook {
public:
    typedef std::function<bool(HWND, UINT, WPARAM, LPARAM)> OnMessageFn;

    explicit WindowsMessageHook(HWND wnd);
    virtual ~WindowsMessageHook();

    LRESULT on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param);

    void on_message(OnMessageFn fn) { m_on_message = fn; }
    bool is_valid() const { return m_original_proc != nullptr; }
    void window_toggle_cursor(bool show);

protected:
    HWND m_wnd{ nullptr };
    WNDPROC m_original_proc{ nullptr };
    OnMessageFn m_on_message{ nullptr };
};
