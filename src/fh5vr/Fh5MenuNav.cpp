// FH5 menu navigator — see Fh5MenuNav.hpp.
//
// Two halves:
//   (1) Input injection — inline detours on XInputGetState (and the undocumented ordinal-100
//       XInputGetStateEx that Forza uses for the Guide button). The detour calls the original, then merges
//       a synthesized virtual-pad snapshot for user_index 0. If no physical pad is connected the original
//       returns ERROR_DEVICE_NOT_CONNECTED; while we are injecting we override that to ERROR_SUCCESS so the
//       game still sees the press. dwPacketNumber is bumped on every modified read so the game notices.
//   (2) State publish — a worker thread samples the producer's main-camera rate (gameplay camera actively
//       rendering), classifies the scene by near/far, and writes E:\tmp\fh5_state.txt for the orchestrator.
//
// All raw memory and file I/O is SEH/null guarded; this runs inside the game and must never crash it.

#include "Fh5MenuNav.hpp"
#include "Fh5Adapter.hpp"    // fh5diag:: producer diagnostics
#include "Fh5CamDriver.hpp"  // fh5cam::active_camera_name (world3d -> free-roam vs garage disambiguation)
#include "Fh5UiScreen.hpp"   // fh5ui::current_screen (live UIPage-vtable screen detection)

#include <utility/Hooks.hpp>

#include <spdlog/spdlog.h>

#include <windows.h>
#include <Xinput.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>

namespace fh5nav {
namespace {

// ---------------------------------------------------------------------------
// File paths (mirror the existing E:\tmp\fh5vr_ctl.txt convention).
// ---------------------------------------------------------------------------
constexpr const char* kNavPath   = "E:\\tmp\\fh5_nav.txt";     // orchestrator -> mod (input commands)
constexpr const char* kStatePath = "E:\\tmp\\fh5_state.txt";   // mod -> orchestrator (live flow state)

// Empress build Stateflow crash-breadcrumb slots. The value string is useful in menus/loading; in free-roam
// it can legitimately be empty because no menu state-machine is active.
constexpr uintptr_t kStateMachineInstanceRva = 0x9090760ull;   // LAST_STATE_MACHINE_INSTANCE slot struct
constexpr uintptr_t kStateMachineStackRva    = 0x90907B0ull;   // LAST_STATE_MACHINE_STACK slot struct
constexpr size_t    kScreenNameMax           = 192;

// The Guide/Xbox button — present in XInputGetStateEx's state, not in the public XInput.h.
constexpr WORD XINPUT_GUIDE = 0x0400;

// A discrete menu "press": hold the button down this long so the game registers a clean edge, then release.
constexpr uint64_t kPressMs = 120;

// ---------------------------------------------------------------------------
// Synthesized virtual-pad snapshot: written by the worker, read by the hot detour via a seqlock.
// ---------------------------------------------------------------------------
struct InjectState {
    WORD  buttons = 0;
    BYTE  lt = 0, rt = 0;
    SHORT lx = 0, ly = 0, rx = 0, ry = 0;
    bool  any() const { return buttons || lt || rt || lx || ly || rx || ry; }
};

std::atomic<uint32_t> g_inj_gen{ 0 };   // even = stable, odd = writer in progress
InjectState           g_inj{};
std::atomic<bool>     g_inj_active{ false };   // fast gate so the detour skips work when idle
std::atomic<bool>     g_virtual_pad_latched{ false }; // after first command, stay connected-neutral
std::atomic<uint32_t> g_fake_packet{ 1 };

void publish_inject(const InjectState& s) {
    if (s.any()) {
        g_virtual_pad_latched.store(true, std::memory_order_release);
    }
    g_inj_gen.fetch_add(1, std::memory_order_acq_rel);
    g_inj = s;
    g_inj_gen.fetch_add(1, std::memory_order_release);
    g_inj_active.store(s.any(), std::memory_order_release);
}

bool snapshot_inject(InjectState& out) {
    for (int spin = 0; spin < 8; ++spin) {
        const uint32_t g0 = g_inj_gen.load(std::memory_order_acquire);
        if (g0 & 1u) continue;
        out = g_inj;
        const uint32_t g1 = g_inj_gen.load(std::memory_order_acquire);
        if (g0 == g1) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// XInput detours.
// ---------------------------------------------------------------------------
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

std::unique_ptr<FunctionHook> g_hook_xigs;     // XInputGetState
std::unique_ptr<FunctionHook> g_hook_xigsex;   // XInputGetStateEx (ordinal 100)
std::atomic<bool> g_xinput_hooked{ false };    // at least one export hooked
std::atomic<uint32_t> g_xinput_diag_bits{ 0 }; // once-only hook failure diagnostics
std::mutex        g_xinput_mtx;                // serialize install attempts (bootstrap vs worker-retry threads)

// Merge the synthesized snapshot into the state the original returned. Returns the (possibly overridden)
// result code. Only touches user_index 0.
DWORD merge_injection(DWORD result, DWORD user_index, XINPUT_STATE* state) {
    if (user_index != 0 || state == nullptr) return result;
    const bool active = g_inj_active.load(std::memory_order_acquire);
    const bool latched = g_virtual_pad_latched.load(std::memory_order_acquire);
    if (!active) {
        // If no physical controller is connected, do not let FH5 see a connect->disconnect edge after a
        // short synthetic press. Keep a neutral virtual pad connected for the rest of the process.
        if (latched && result != ERROR_SUCCESS) {
            __try {
                ZeroMemory(state, sizeof(*state));
                state->dwPacketNumber = g_fake_packet.load(std::memory_order_relaxed);
                return ERROR_SUCCESS;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return result;
            }
        }
        return result;
    }

    InjectState inj{};
    if (!snapshot_inject(inj) || !inj.any()) return result;

    __try {
        if (result != ERROR_SUCCESS) {            // no physical pad — fabricate a connected one
            ZeroMemory(state, sizeof(*state));
            result = ERROR_SUCCESS;
        }
        state->Gamepad.wButtons |= inj.buttons;
        if (inj.lt > state->Gamepad.bLeftTrigger)  state->Gamepad.bLeftTrigger  = inj.lt;
        if (inj.rt > state->Gamepad.bRightTrigger) state->Gamepad.bRightTrigger = inj.rt;
        if (inj.lx) state->Gamepad.sThumbLX = inj.lx;
        if (inj.ly) state->Gamepad.sThumbLY = inj.ly;
        if (inj.rx) state->Gamepad.sThumbRX = inj.rx;
        if (inj.ry) state->Gamepad.sThumbRY = inj.ry;
        state->dwPacketNumber = g_fake_packet.fetch_add(1, std::memory_order_relaxed);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // leave whatever the original produced
    }
    return result;
}

DWORD WINAPI Hook_XInputGetState(DWORD user_index, XINPUT_STATE* state) {
    auto original = g_hook_xigs->get_original<XInputGetState_t>();
    const DWORD r = original ? original(user_index, state) : ERROR_DEVICE_NOT_CONNECTED;
    return merge_injection(r, user_index, state);
}

DWORD WINAPI Hook_XInputGetStateEx(DWORD user_index, XINPUT_STATE* state) {
    auto original = g_hook_xigsex->get_original<XInputGetState_t>();
    const DWORD r = original ? original(user_index, state) : ERROR_DEVICE_NOT_CONNECTED;
    return merge_injection(r, user_index, state);
}

// Hook XInputGetState (+ ordinal-100 XInputGetStateEx) in one xinput module, but ONLY if it is already
// loaded — that is the module the game actually calls. We never LoadLibrary a module (forcing one in could
// hook a copy the game never calls); the worker retries install_xinput_hook until the real module is resident.
// NOTE: FH5 (a 2021 Win10/11 title) uses xinput1_4; if a live test shows no injection, the game may route
// input through a different module (xinput9_1_0) or GameInput/WGI — in which case the orchestrator's proven
// keyboard path (FH5Window.ps1 PostMessage) remains the reliable fallback.
void try_hook_one(const wchar_t* dll, const char* label) {
    HMODULE h = GetModuleHandleW(dll);
    if (h == nullptr) return;   // not loaded yet; the worker will retry once the game loads it

    uint32_t bit_base = 0;
    if (_stricmp(label, "xinput1_3") == 0) bit_base = 2;
    else if (_stricmp(label, "xinput9_1_0") == 0) bit_base = 4;
    const auto warn_hook_failed_once = [&](uint32_t bit, const char* fn, void* p) {
        const uint32_t mask = 1u << bit;
        if ((g_xinput_diag_bits.fetch_or(mask, std::memory_order_relaxed) & mask) == 0) {
            spdlog::warn("[FH5NAV] {} hook FAILED in {} @0x{:X}", fn, label, reinterpret_cast<uintptr_t>(p));
        }
    };

    // Assign the global BEFORE create(): create() makes the detour live, and the detour dereferences the
    // global to reach the trampoline. If we created first and assigned after, a call landing in that window
    // would hit a null global. (Matches the producer-hook ordering in Fh5Adapter.cpp.) Reset on failure.
    if (g_hook_xigs == nullptr) {
        if (auto p = reinterpret_cast<void*>(GetProcAddress(h, "XInputGetState"))) {
            g_hook_xigs = std::make_unique<FunctionHook>(Address{ p }, &Hook_XInputGetState);
            if (g_hook_xigs->create()) { spdlog::info("[FH5NAV] hooked XInputGetState in {}", label); }
            else                       { warn_hook_failed_once(bit_base, "XInputGetState", p); g_hook_xigs.reset(); }
        }
    }
    if (g_hook_xigsex == nullptr) {
        // ordinal 100 = XInputGetStateEx (includes the Guide button); Forza uses it.
        if (auto p = reinterpret_cast<void*>(GetProcAddress(h, reinterpret_cast<LPCSTR>(100)))) {
            g_hook_xigsex = std::make_unique<FunctionHook>(Address{ p }, &Hook_XInputGetStateEx);
            if (g_hook_xigsex->create()) { spdlog::info("[FH5NAV] hooked XInputGetStateEx (ord 100) in {}", label); }
            else                         { warn_hook_failed_once(bit_base + 1, "XInputGetStateEx", p); g_hook_xigsex.reset(); }
        }
    }
}

// ---------------------------------------------------------------------------
// Command parsing: map button names -> XINPUT bits.
// ---------------------------------------------------------------------------
WORD button_from_name(const char* name) {
    struct Map { const char* n; WORD b; };
    static const Map kMap[] = {
        { "A", XINPUT_GAMEPAD_A }, { "B", XINPUT_GAMEPAD_B }, { "X", XINPUT_GAMEPAD_X }, { "Y", XINPUT_GAMEPAD_Y },
        { "START", XINPUT_GAMEPAD_START }, { "BACK", XINPUT_GAMEPAD_BACK }, { "GUIDE", XINPUT_GUIDE },
        { "DPAD_UP", XINPUT_GAMEPAD_DPAD_UP }, { "DPAD_DOWN", XINPUT_GAMEPAD_DPAD_DOWN },
        { "DPAD_LEFT", XINPUT_GAMEPAD_DPAD_LEFT }, { "DPAD_RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT },
        { "UP", XINPUT_GAMEPAD_DPAD_UP }, { "DOWN", XINPUT_GAMEPAD_DPAD_DOWN },
        { "LEFT", XINPUT_GAMEPAD_DPAD_LEFT }, { "RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT },
        { "LB", XINPUT_GAMEPAD_LEFT_SHOULDER }, { "RB", XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { "LS", XINPUT_GAMEPAD_LEFT_THUMB }, { "RS", XINPUT_GAMEPAD_RIGHT_THUMB },
    };
    for (const auto& m : kMap) {
        if (_stricmp(name, m.n) == 0) return m.b;
    }
    return 0;
}

// An active injection segment: hold `state` until end_ms (NowMs), then go neutral.
struct Segment {
    InjectState state{};
    uint64_t    end_ms = 0;
    bool        active = false;
};

uint64_t NowMs() {
    static LARGE_INTEGER freq{}; static LARGE_INTEGER start{};
    if (freq.QuadPart == 0) { QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&start); }
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    return static_cast<uint64_t>((now.QuadPart - start.QuadPart) * 1000 / freq.QuadPart);
}

// ---- SEH-guarded raw memory/string reads ---------------------------------
// Breadcrumb decoding intentionally tries several layouts. The static report proved the slots, but the exact
// live encoding can be either MSVC std::string or a small custom slot with a pointer/inline value at +8.
bool safe_copy(uintptr_t address, void* out, size_t size) {
    if (!address || out == nullptr || size == 0) return false;
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t module_base() {
    static uintptr_t base = 0;
    if (!base) {
        base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ForzaHorizon5.exe"));
        if (!base) base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    }
    return base;
}

bool is_printable_ascii(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u >= 0x20 && u <= 0x7e;
}

bool copy_sanitized_ascii(const char* src, size_t len, char* out, size_t out_sz) {
    if (src == nullptr || out == nullptr || out_sz == 0 || len == 0) return false;
    if (len >= out_sz) len = out_sz - 1;

    size_t printable = 0;
    for (size_t i = 0; i < len; ++i) {
        if (is_printable_ascii(src[i])) {
            ++printable;
        }
    }
    if (printable == 0 || printable * 4 < len * 3) return false;  // reject mostly-binary buffers

    for (size_t i = 0; i < len; ++i) {
        const char c = src[i];
        out[i] = is_printable_ascii(c) ? c : '_';
    }
    out[len] = '\0';
    return true;
}

bool read_ascii_cstr(uintptr_t address, char* out, size_t out_sz, size_t max_len = kScreenNameMax - 1) {
    if (out == nullptr || out_sz == 0 || address < 0x10000) return false;

    char buf[kScreenNameMax]{};
    size_t want = max_len + 1;
    if (want > sizeof(buf)) want = sizeof(buf);
    if (!safe_copy(address, buf, want)) return false;

    size_t len = 0;
    while (len < want && buf[len] != '\0') ++len;
    if (len == 0 || len >= want) return false;
    return copy_sanitized_ascii(buf, len, out, out_sz);
}

bool read_inline_cstr(uintptr_t address, char* out, size_t out_sz, size_t bytes = 32) {
    if (bytes > kScreenNameMax) bytes = kScreenNameMax;
    char buf[kScreenNameMax]{};
    if (!safe_copy(address, buf, bytes)) return false;

    size_t len = 0;
    while (len < bytes && buf[len] != '\0') ++len;
    if (len == 0 || len >= bytes) return false;
    return copy_sanitized_ascii(buf, len, out, out_sz);
}

bool read_msvc_string(uintptr_t string_address, char* out, size_t out_sz) {
    uint64_t size = 0;
    uint64_t cap = 0;
    if (!safe_copy(string_address + 0x10, &size, sizeof(size)) ||
        !safe_copy(string_address + 0x18, &cap, sizeof(cap))) {
        return false;
    }
    if (size == 0 || size >= kScreenNameMax || size > cap || cap > 4096) {
        return false;
    }

    if (cap < 16) {
        char buf[16]{};
        if (!safe_copy(string_address, buf, sizeof(buf))) return false;
        return copy_sanitized_ascii(buf, static_cast<size_t>(size), out, out_sz);
    }

    uintptr_t heap_ptr = 0;
    if (!safe_copy(string_address, &heap_ptr, sizeof(heap_ptr))) return false;
    return read_ascii_cstr(heap_ptr, out, out_sz, static_cast<size_t>(size));
}

bool read_breadcrumb_slot(uintptr_t slot_address, char* out, size_t out_sz) {
    if (out == nullptr || out_sz == 0) return false;
    out[0] = '\0';

    // Proven static layout: slot struct at RVA, value std::string starts at +8.
    if (read_msvc_string(slot_address + 8, out, out_sz)) return true;

    // Fallbacks for a custom crash-breadcrumb slot: direct pointer at +8, then inline chars at +8.
    uintptr_t direct = 0;
    if (safe_copy(slot_address + 8, &direct, sizeof(direct)) &&
        read_ascii_cstr(direct, out, out_sz)) {
        return true;
    }
    if (read_inline_cstr(slot_address + 8, out, out_sz, 64)) return true;

    return false;
}

void read_ui_state(char* screen, size_t screen_sz, char* stack, size_t stack_sz,
                   char* instance, size_t instance_sz) {
    if (screen && screen_sz)   _snprintf_s(screen,   screen_sz,   _TRUNCATE, "unknown");
    if (stack && stack_sz)     _snprintf_s(stack,    stack_sz,    _TRUNCATE, "unknown");
    if (instance && instance_sz) _snprintf_s(instance, instance_sz, _TRUNCATE, "unknown");

    const uintptr_t base = module_base();
    if (!base) return;

    char stack_buf[kScreenNameMax]{};
    char inst_buf[kScreenNameMax]{};
    const bool have_stack = read_breadcrumb_slot(base + kStateMachineStackRva, stack_buf, sizeof(stack_buf));
    const bool have_inst  = read_breadcrumb_slot(base + kStateMachineInstanceRva, inst_buf, sizeof(inst_buf));

    if (have_stack && stack && stack_sz) {
        _snprintf_s(stack, stack_sz, _TRUNCATE, "%s", stack_buf);
    }
    if (have_inst && instance && instance_sz) {
        _snprintf_s(instance, instance_sz, _TRUNCATE, "%s", inst_buf);
    }
    if (screen && screen_sz) {
        if (have_stack) {
            _snprintf_s(screen, screen_sz, _TRUNCATE, "%s", stack_buf);
        } else if (have_inst) {
            _snprintf_s(screen, screen_sz, _TRUNCATE, "%s", inst_buf);
        }
    }
}

// Parse one nav command line into a segment. Returns false if unrecognized (segment left untouched).
// Verbs:  press <BTN> | hold <BTN> <ms> | lt <0..255> <ms> | rt <0..255> <ms>
//         ls <x> <y> <ms> | rs <x> <y> <ms>   (x,y in [-1..1])   |   clear
bool parse_command(const char* cmd, Segment& seg) {
    char verb[32]{}; char arg[32]{};
    if (sscanf_s(cmd, "%31s", verb, (unsigned)sizeof(verb)) != 1) return false;

    const uint64_t now = NowMs();
    if (_stricmp(verb, "clear") == 0 || _stricmp(verb, "none") == 0) {
        seg = Segment{}; seg.active = true; seg.end_ms = now; return true;   // immediately neutral
    }
    if (_stricmp(verb, "press") == 0) {
        if (sscanf_s(cmd, "%*s %31s", arg, (unsigned)sizeof(arg)) != 1) return false;
        WORD b = button_from_name(arg); if (!b) return false;
        seg = Segment{}; seg.state.buttons = b; seg.end_ms = now + kPressMs; seg.active = true; return true;
    }
    if (_stricmp(verb, "hold") == 0) {
        unsigned ms = 0;
        if (sscanf_s(cmd, "%*s %31s %u", arg, (unsigned)sizeof(arg), &ms) != 2) return false;
        WORD b = button_from_name(arg); if (!b) return false;
        seg = Segment{}; seg.state.buttons = b; seg.end_ms = now + ms; seg.active = true; return true;
    }
    if (_stricmp(verb, "lt") == 0 || _stricmp(verb, "rt") == 0) {
        unsigned val = 0, ms = 0;
        if (sscanf_s(cmd, "%*s %u %u", &val, &ms) != 2) return false;
        seg = Segment{};
        if (_stricmp(verb, "lt") == 0) seg.state.lt = (BYTE)(val > 255 ? 255 : val);
        else                          seg.state.rt = (BYTE)(val > 255 ? 255 : val);
        seg.end_ms = now + ms; seg.active = true; return true;
    }
    if (_stricmp(verb, "ls") == 0 || _stricmp(verb, "rs") == 0) {
        float x = 0, y = 0; unsigned ms = 0;
        if (sscanf_s(cmd, "%*s %f %f %u", &x, &y, &ms) != 3) return false;
        auto to_axis = [](float v) -> SHORT {
            if (v >  1.0f) v =  1.0f; if (v < -1.0f) v = -1.0f;
            return (SHORT)(v * 32767.0f);
        };
        seg = Segment{};
        if (_stricmp(verb, "ls") == 0) { seg.state.lx = to_axis(x); seg.state.ly = to_axis(y); }
        else                          { seg.state.rx = to_axis(x); seg.state.ry = to_axis(y); }
        seg.end_ms = now + ms; seg.active = true; return true;
    }
    return false;
}

// Read the nav file; return true and fill (seq,cmd) when present.
bool read_nav_file(long& seq_out, char* cmd_out, size_t cmd_sz) {
    FILE* f = nullptr;
    if (fopen_s(&f, kNavPath, "rb") != 0 || f == nullptr) return false;
    long seq = -1; bool have_cmd = false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        long s;
        if (sscanf_s(line, "seq=%ld", &s) == 1) { seq = s; continue; }
        if (strncmp(line, "cmd=", 4) == 0) {
            const char* p = line + 4;
            size_t n = strlen(p);
            while (n > 0 && (p[n-1] == '\n' || p[n-1] == '\r' || p[n-1] == ' ')) --n;
            if (n >= cmd_sz) n = cmd_sz - 1;
            memcpy(cmd_out, p, n); cmd_out[n] = '\0'; have_cmd = true;
        }
    }
    fclose(f);
    if (seq < 0 || !have_cmd) return false;
    seq_out = seq;
    return true;
}

// ---------------------------------------------------------------------------
// State publish.
// ---------------------------------------------------------------------------
// Sticky scene classification by far-bucket RECENCY (see g_lastShowcase/WorldMs rationale in Fh5Adapter.cpp).
// Inputs are GetTickCount64 ms. showcase wins while a far>30000 frame is recent (intro running); world3d only
// once just far~5000 frames remain; menu_or_loading when no main camera has rendered recently. This replaces
// the per-read far snapshot that flapped because the intro renders far~5000 and far~50000 cameras together.
constexpr uint64_t kSceneRecencyMs = 750;
const char* scene_class(uint64_t now_tick, uint64_t showcase_ms, uint64_t world_ms, bool& gameplay_out) {
    const bool showcase = showcase_ms != 0 && now_tick >= showcase_ms && (now_tick - showcase_ms) < kSceneRecencyMs;
    const bool world    = world_ms    != 0 && now_tick >= world_ms    && (now_tick - world_ms)    < kSceneRecencyMs;
    gameplay_out = showcase || world;
    if (showcase) return "showcase";
    if (world)    return "world3d";
    return "menu_or_loading";
}

bool token_equals(const char* s, size_t n, const char* lit) {
    return lit && std::strlen(lit) == n && std::strncmp(s, lit, n) == 0;
}

bool is_raw_recovery_page(const char* s, size_t n) {
    // Raw visible-page scans intentionally exclude HUD/loading and PauseMenuTiled. PauseMenuTiled remains a
    // reliable top-page signal when the scanner can prove it, but as a raw page it is a known pooled/stale
    // object during normal cockpit gameplay.
    return token_equals(s, n, "GenericMenu") ||
           token_equals(s, n, "OptionsMenu") ||
           token_equals(s, n, "HudOptions") ||
           token_equals(s, n, "DestinationMenu") ||
           token_equals(s, n, "TuneMenu") ||
           token_equals(s, n, "UpgradesMenu") ||
           token_equals(s, n, "RivalsMenu") ||
           token_equals(s, n, "StartingGrid") ||
           token_equals(s, n, "CarSelectGarage") ||
           token_equals(s, n, "MyHorizonLife") ||
           token_equals(s, n, "FestivalPassScene") ||
           token_equals(s, n, "AccoladeCategoriesScene") ||
           token_equals(s, n, "MapScene") ||
           token_equals(s, n, "MapSceneInteractive") ||
           token_equals(s, n, "MapSceneBase") ||
           token_equals(s, n, "PhotoModeScene");
}

void append_token(char* out, size_t cap, size_t& pos, const char* s, size_t n) {
    if (!out || cap == 0 || !s || n == 0 || pos >= cap) return;
    const int wrote = _snprintf_s(out + pos, cap - pos, _TRUNCATE, "%s%.*s",
                                  pos ? "," : "", (int)n, s);
    if (wrote > 0) pos += (size_t)wrote;
}

void build_raw_recovery_pages(const char* raw_pages, char* out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!raw_pages || !raw_pages[0]) return;
    size_t pos = 0;
    const char* p = raw_pages;
    while (*p) {
        while (*p == ',' || *p == ' ') ++p;
        const char* b = p;
        while (*p && *p != ',') ++p;
        const size_t n = (size_t)(p - b);
        if (is_raw_recovery_page(b, n)) append_token(out, cap, pos, b, n);
    }
}

void write_state_file(uint64_t t_ms, uint64_t prod_calls, uint64_t main_hits, float main_rate,
                      float near_, float far_, const char* scene, bool gameplay, const char* camera,
                      const char* screen, const char* stack, const char* instance,
                      const char* ui_screen, bool ui_reliable, unsigned ui_tracked,
                      unsigned ui_visible, unsigned long long ui_scans, const char* ui_pages,
                      const char* ui_blocking_pages, long last_seq) {
    char tmp[MAX_PATH];
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s.tmp", kStatePath);

    FILE* f = nullptr;
    if (fopen_s(&f, tmp, "wb") != 0 || f == nullptr) return;
    fprintf(f,
        "t=%llu\n"
        "prod_calls=%llu\n"
        "main_hits=%llu\n"
        "main_rate=%.1f\n"
        "near=%.4f\n"
        "far=%.1f\n"
        "gameplay=%d\n"
        "scene=%s\n"
        "screen=%s\n"          // Stateflow breadcrumb now; true UIPage active-page hook can replace later
        "state_stack=%s\n"
        "state_instance=%s\n"
        "ui_screen=%s\n"
        "ui_reliable=%d\n"
        "ui_tracked=%u\n"
        "ui_visible=%u\n"
        "ui_scans=%llu\n"
        "ui_pages=%s\n"
        "ui_blocking_pages=%s\n"
        "camera=%s\n"          // active camera class: CCamFollow*=free-roam, CCamFollowExtended=garage
        "xinput_hooked=%d\n"
        "inject_seq=%ld\n",
        (unsigned long long)t_ms, (unsigned long long)prod_calls, (unsigned long long)main_hits,
        main_rate, near_, far_, gameplay ? 1 : 0, scene,
        screen ? screen : "unknown", stack ? stack : "unknown", instance ? instance : "unknown",
        ui_screen ? ui_screen : "unknown", ui_reliable ? 1 : 0, ui_tracked, ui_visible, ui_scans,
        ui_pages ? ui_pages : "",
        ui_blocking_pages ? ui_blocking_pages : "",
        camera, g_xinput_hooked.load(std::memory_order_acquire) ? 1 : 0, last_seq);
    fclose(f);

    MoveFileExA(tmp, kStatePath, MOVEFILE_REPLACE_EXISTING);   // atomic-ish swap so readers never see a partial
}

// ---------------------------------------------------------------------------
// Worker: input tick (fast) + state publish (throttled).
// ---------------------------------------------------------------------------
std::atomic<bool> g_started{ false };

DWORD WINAPI WorkerThread(void*) {
    spdlog::info("[FH5NAV] menu-nav worker up (nav={} state={})", kNavPath, kStatePath);
    spdlog::info("[FH5NAV] xinput_hooked_at_worker_start={}",
                 g_xinput_hooked.load(std::memory_order_acquire) ? 1 : 0);

    Segment seg{};
    long last_seq = -1;

    uint64_t last_state_ms = 0;
    uint64_t last_main_sample_ms = 0;
    uint64_t last_main_hits = 0;
    float    main_rate = 0.0f;
    bool      was_active = false;
    uint64_t last_hook_try_ms = 0;
    bool      warned_no_xinput = false;

    for (;;) {
        const uint64_t now = NowMs();

        // (0) keep trying to hook the game's xinput module until it's resident (it may not be loaded at
        // bootstrap). Cheap GetModuleHandle probes; stops once hooked. Keyboard nav is the fallback if the
        // game never routes through xinput (e.g. GameInput/WGI).
        if (!g_xinput_hooked.load(std::memory_order_acquire) && now - last_hook_try_ms >= 250) {
            last_hook_try_ms = now;
            install_xinput_hook();
            if (!g_xinput_hooked.load(std::memory_order_acquire) && !warned_no_xinput && now > 15000) {
                warned_no_xinput = true;
                spdlog::warn("[FH5NAV] no xinput module hooked after 15s — input injection unavailable "
                             "(orchestrator should use the keyboard PostMessage fallback)");
            }
        }

        // (1) pick up a new nav command (only act on a seq increase)
        long seq = -1; char cmd[200]{};
        if (read_nav_file(seq, cmd, sizeof(cmd)) && seq != last_seq) {
            last_seq = seq;
            Segment parsed{};
            if (parse_command(cmd, parsed)) {
                seg = parsed;
                spdlog::info("[FH5NAV] cmd seq={} '{}'", seq, cmd);
            } else {
                spdlog::warn("[FH5NAV] unparsed cmd seq={} '{}'", seq, cmd);
            }
        }

        // (2) tick the synthesized input: hold the segment until its end, then go neutral
        InjectState out{};
        if (seg.active && now < seg.end_ms) {
            out = seg.state;
        } else if (seg.active) {
            seg = Segment{};   // expired -> neutral
        }
        const bool active = out.any();
        if (active || was_active) {        // publish on every change and while active; cheap when idle
            publish_inject(out);
        }
        was_active = active;

        // (3) publish flow state ~5x/sec
        if (now - last_state_ms >= 200) {
            last_state_ms = now;

            const uint64_t main_hits  = fh5diag::producer_main_hits();
            const uint64_t prod_calls = fh5diag::producer_calls();
            const float    near_      = fh5diag::last_near();
            const float    far_       = fh5diag::last_far();

            if (last_main_sample_ms != 0 && now > last_main_sample_ms) {
                const float dt = (now - last_main_sample_ms) / 1000.0f;
                main_rate = (float)(main_hits - last_main_hits) / (dt > 0.001f ? dt : 0.001f);
            }
            last_main_sample_ms = now;
            last_main_hits = main_hits;

            // Sticky scene from far-bucket recency (GetTickCount64 ms — same clock as g_lastShowcase/WorldMs),
            // not the flapping per-read far snapshot. gameplay = a main camera rendered within the window.
            bool gameplay = false;
            const char* scene = scene_class(GetTickCount64(), fh5diag::last_showcase_ms(),
                                            fh5diag::last_world_ms(), gameplay);
            char camera[40];
            fh5cam::active_camera_name(camera, sizeof(camera));   // "CCamFollowLow" etc, "unknown" if unresolved
            char screen[kScreenNameMax];
            char stack[kScreenNameMax];
            char instance[kScreenNameMax];
            read_ui_state(screen, sizeof(screen), stack, sizeof(stack), instance, sizeof(instance));
            // Prefer the live UIPage-vtable scan only when it reports a low-noise visible-page set. The raw
            // scanner counts are still written for diagnostics; scene/camera state remains the primary signal.
            char ui_page[64];
            fh5ui::current_screen(ui_page, sizeof(ui_page));
            char ui_pages[256];
            fh5ui::visible_pages(ui_pages, sizeof(ui_pages));
            char ui_blocking_pages[256];
            build_raw_recovery_pages(ui_pages, ui_blocking_pages, sizeof(ui_blocking_pages));
            const bool ui_reliable = fh5ui::screen_reliable();
            const unsigned ui_tracked = fh5ui::live_page_count();
            const unsigned ui_visible = fh5ui::visible_page_count();
            const unsigned long long ui_scans = fh5ui::scan_passes();
            if (ui_reliable && std::strcmp(ui_page, "unknown") != 0) {
                _snprintf_s(screen, sizeof(screen), _TRUNCATE, "%s", ui_page);
            }
            write_state_file(now, prod_calls, main_hits, main_rate, near_, far_, scene, gameplay, camera,
                             screen, stack, instance, ui_page, ui_reliable, ui_tracked, ui_visible,
                             ui_scans, ui_pages, ui_blocking_pages, last_seq);
        }

        Sleep(8);
    }
    // unreachable
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void install_xinput_hook() {
    if (g_xinput_hooked.load(std::memory_order_acquire)) return;
    std::scoped_lock lk{ g_xinput_mtx };
    if (g_xinput_hooked.load(std::memory_order_relaxed)) return;

    // Hook whichever xinput module the game has ALREADY loaded — that's the one it calls. We do NOT
    // LoadLibrary a module here: forcing xinput1_4 in could hook a copy the game never calls (it may use
    // xinput9_1_0). The worker re-invokes this every ~250ms, so we hook the real module the moment it loads.
    try_hook_one(L"xinput1_4.dll",   "xinput1_4");
    try_hook_one(L"xinput1_3.dll",   "xinput1_3");
    try_hook_one(L"xinput9_1_0.dll", "xinput9_1_0");

    if (g_hook_xigs != nullptr || g_hook_xigsex != nullptr) {
        g_xinput_hooked.store(true, std::memory_order_release);
    }
}

void start() {
    if (g_started.exchange(true)) return;
    install_xinput_hook();   // safe to retry; no-op if already installed
    if (HANDLE t = CreateThread(nullptr, 0, &WorkerThread, nullptr, 0, nullptr)) {
        CloseHandle(t);
    } else {
        g_started.store(false);
        spdlog::error("[FH5NAV] failed to create menu-nav worker thread");
    }
}

} // namespace fh5nav
