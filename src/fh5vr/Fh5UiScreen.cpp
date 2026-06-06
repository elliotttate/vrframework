// FH5 live UI screen detector — see Fh5UiScreen.hpp.
//
// Read-only heap scan for known UI-page controller vtables (RTTI classifier from
// _agent_reports/fh5_ui_vtable_rtti_table_20260605.tsv). A page object's offset-0 qword is its vtable; when a
// committed-private qword equals one of our known page-vtable VAs, it is a candidate UIPage/controller object.
//
// The full scan is chunked with yields so it never stalls the game; found pages are cached and re-validated
// cheaply each tick, so only NEW screens cost a (slow) full pass. All reads are SEH-guarded — a scan can never
// corrupt the game, only (briefly) cost memory bandwidth.

#include "Fh5UiScreen.hpp"
#include "Fh5Adapter.hpp"   // fh5diag:: gameplay-camera render recency (gates pooled loading screens)

#include <spdlog/spdlog.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace fh5ui {
namespace {

constexpr uintptr_t kIdaImageBase = 0x140000000ull;

// Priority = "what is on top" when several pages are live (a pause menu over the HUD reports the pause menu).
enum Prio { P_BASE = 5, P_HUD = 20, P_REPLAY = 50, P_PHOTO = 60, P_MAP = 70, P_MENU = 80, P_PAUSE = 90, P_LOADING = 100 };

struct Screen { uintptr_t rva; const char* name; int prio; };

// Key screens (vtable RVA from the 405-row TSV; covers every state the navigator needs). CopterHud is the
// free-roam/drive HUD — its presence is a clean free-roam signal with no cinematic ambiguity.
constexpr Screen kScreens[] = {
    { 0x6407F18, "CopterHud",                 P_HUD },     // free-roam / drive HUD
    { 0x6321AF8, "Hud",                       P_HUD },     // race HUD
    { 0x6322558, "LimitedHud",                P_HUD },     // cutscene/limited HUD
    { 0x6322EC0, "LimitedHudMinorNotification", P_HUD },
    { 0x63C6710, "PauseMenuTiled",            P_PAUSE },   // pause menu
    { 0x6424280, "MapScene",                  P_MAP },
    { 0x62D39B8, "MapSceneInteractive",       P_MAP },
    { 0x6421110, "MapSceneBase",              P_MAP },
    { 0x6447450, "Splash",                    P_LOADING },
    { 0x6432520, "SplashE3",                  P_LOADING },
    { 0x6444E98, "Loading",                   P_LOADING },
    { 0x6430428, "FMVLoading",                P_LOADING },
    { 0x6430DA0, "SeasonTransitionFMVLoading",P_LOADING },
    { 0x627BD40, "BaseLoadingScreen",         P_LOADING },
    { 0x62821A8, "WaitForInstallFromSplash",  P_LOADING },
    { 0x63DF810, "ReplayHud",                 P_REPLAY },
    { 0x6446760, "PhotoModeScene",            P_PHOTO },
    { 0x62D3020, "GenericMenu",               P_MENU },    // front-end menu
    { 0x630D240, "OptionsMenu",               P_MENU },
    { 0x63B0048, "HudOptions",                P_MENU },
    { 0x642F750, "DestinationMenu",           P_MENU },
    { 0x6419D18, "TuneMenu",                  P_MENU },
    { 0x641C5C0, "UpgradesMenu",              P_MENU },
    { 0x6295268, "RivalsMenu",                P_MENU },
    { 0x63EB468, "StartingGrid",              P_MENU },
    { 0x63E4C48, "CarSelectGarage",           P_MENU },
    { 0x63964B8, "MyHorizonLife",             P_MENU },
    { 0x63871E0, "FestivalPassScene",         P_MENU },
    { 0x62496D0, "AccoladeCategoriesScene",   P_MENU },
    { 0x5F1F320, "UIPage",                    P_BASE },    // controller-less pages all share this base vtable
};
constexpr int kScreenCount = (int)(sizeof(kScreens) / sizeof(kScreens[0]));

// ---------------------------------------------------------------------------
uintptr_t g_base = 0;
uintptr_t module_base() {
    if (!g_base) {
        g_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ForzaHorizon5.exe"));
        if (!g_base) g_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    }
    return g_base;
}

// Resolved-once: runtime vtable VA per screen + sorted (va, screen index) for the fast lookup, plus the
// [min,max] VA window for a cheap per-qword reject.
uintptr_t g_vtable_va[kScreenCount]{};
struct SortedVa { uintptr_t va; int idx; };
SortedVa g_sorted[kScreenCount]{};
int g_sorted_count = 0;
uintptr_t g_min_va = 0, g_max_va = 0;

void build_tables() {
    const uintptr_t base = module_base();
    g_sorted_count = kScreenCount;
    for (int i = 0; i < kScreenCount; ++i) {
        g_vtable_va[i] = base + (kScreens[i].rva);          // RVA already image-relative; base has no ASLR (0x140000000)
        g_sorted[i] = { g_vtable_va[i], i };
    }
    std::sort(g_sorted, g_sorted + g_sorted_count, [](const SortedVa& a, const SortedVa& b) { return a.va < b.va; });
    g_min_va = g_sorted[0].va;
    g_max_va = g_sorted[g_sorted_count - 1].va;
}

int screen_index_for_va(uintptr_t va) {
    if (va < g_min_va || va > g_max_va) return -1;
    int lo = 0, hi = g_sorted_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (g_sorted[mid].va == va) return g_sorted[mid].idx;
        if (g_sorted[mid].va < va) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

bool safe_read_qword(uintptr_t addr, uintptr_t& out) {
    __try { out = *reinterpret_cast<const uintptr_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
}

bool safe_copy(uintptr_t addr, void* dst, size_t n) {
    __try { std::memcpy(dst, reinterpret_cast<const void*>(addr), n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// AVUI UIElement cached effective-visibility flags at object +0x70 (PROVEN by decompile: get_Visibility
// sub_141801190, OnVisibilityChanged cache-writer sub_1417CB5F0; offset is on the single-inheritance
// UIElement base so it's identical for every page class). bit 0x20 set = Visible = SHOWN; clear =
// Hidden/Collapsed. FH5 POOLS pages (a finished BaseLoadingScreen stays resident but is set Collapsed), so
// this bit is the robust "is it actually on screen" test that "the object exists" cannot give.
constexpr uintptr_t kVisibilityFlagsOffset = 0x70;
constexpr uint32_t  kVisibleBit = 0x20;
bool is_page_shown(uintptr_t obj) {
    uint32_t flags = 0;
    __try { flags = *reinterpret_cast<const uint32_t*>(obj + kVisibilityFlagsOffset); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (flags & kVisibleBit) != 0;
}

bool has_expected_vtable(uintptr_t obj, int idx) {
    if (idx < 0 || idx >= kScreenCount) return false;
    if ((obj & 0x7ull) != 0) return false;
    uintptr_t v = 0;
    return safe_read_qword(obj, v) && v == g_vtable_va[idx];
}

bool ranges_overlap(uintptr_t a0, uintptr_t a1, uintptr_t b0, uintptr_t b1) {
    return a0 < b1 && b0 < a1;
}

bool is_scannable(const MEMORY_BASIC_INFORMATION& mbi) {
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) return false;
    if (mbi.Protect & PAGE_GUARD) return false;
    // D3D/NVIDIA staging heaps can be very large committed-private regions. The UI page objects are normal
    // process heap allocations; avoid sweeping huge graphics/private allocations during startup resizes.
    constexpr SIZE_T kMaxUiHeapRegion = 128ull * 1024ull * 1024ull;
    if (mbi.RegionSize > kMaxUiHeapRegion) return false;
    const DWORD p = mbi.Protect & 0xff;
    return p == PAGE_READWRITE || p == PAGE_WRITECOPY;   // page objects live in writable private heap
}

// ---------------------------------------------------------------------------
// Published result (highest-priority live screen name). Mutex-guarded; updates ~5x/s, reads are rare.
// ---------------------------------------------------------------------------
std::mutex g_pub_mtx;
char       g_pub[64] = "unknown";
char       g_visible_pages[256] = "";
std::atomic<unsigned> g_live_count{ 0 };
std::atomic<unsigned> g_visible_count{ 0 };
std::atomic<unsigned long long> g_passes{ 0 };
std::atomic<bool> g_reliable{ false };
std::atomic<bool> g_started{ false };

void publish(const char* name, bool reliable, const char* visible_pages) {
    std::scoped_lock lk{ g_pub_mtx };
    _snprintf_s(g_pub, sizeof(g_pub), _TRUNCATE, "%s", name ? name : "unknown");
    _snprintf_s(g_visible_pages, sizeof(g_visible_pages), _TRUNCATE, "%s", visible_pages ? visible_pages : "");
    g_reliable.store(reliable, std::memory_order_release);
}

struct Found { uintptr_t addr; int idx; };

// One chunked full pass: find live page objects, append (addr, screen idx) to `out` (deduped vs itself).
void full_scan(std::vector<Found>& out) {
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    uintptr_t cursor = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t end = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    constexpr size_t kChunk = 4 * 1024 * 1024;
    std::vector<uint8_t> buf;
    buf.reserve(kChunk);

    while (cursor < end && out.size() < 256) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi))) { cursor += 0x1000; continue; }
        const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

        if (is_scannable(mbi)) {
            uintptr_t chunk = std::max(cursor, reinterpret_cast<uintptr_t>(mbi.BaseAddress));
            const uintptr_t rend = std::min(region_end, end);
            while (chunk + sizeof(uintptr_t) <= rend && out.size() < 256) {
                const size_t n = (size_t)std::min<uintptr_t>(rend - chunk, kChunk);
                buf.resize(n);
                const uintptr_t scratch_begin = reinterpret_cast<uintptr_t>(buf.data());
                const uintptr_t scratch_end = scratch_begin + buf.capacity();
                if (ranges_overlap(chunk, chunk + n, scratch_begin, scratch_end)) {
                    chunk += n;
                    continue;
                }
                if (safe_copy(chunk, buf.data(), n)) {
                    for (size_t i = 0; i + sizeof(uintptr_t) <= n; i += 8) {   // 8-aligned qwords
                        uintptr_t v; std::memcpy(&v, buf.data() + i, sizeof(v));
                        if (v < g_min_va || v > g_max_va) continue;            // fast reject
                        const int idx = screen_index_for_va(v);
                        if (idx < 0) continue;
                        const uintptr_t addr = chunk + i;
                        if (!has_expected_vtable(addr, idx)) continue;
                        bool dup = false;
                        for (const Found& f : out) { if (f.addr == addr) { dup = true; break; } }
                        if (!dup) out.push_back({ addr, idx });
                    }
                }
                chunk += n;
                Sleep(1);   // yield between 4MB chunks so the scan never monopolizes a core
            }
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }
    g_passes.fetch_add(1, std::memory_order_relaxed);
}

DWORD WINAPI WorkerThread(void*) {
    if (!module_base()) { spdlog::warn("[FH5UI] no module base; screen scanner idle"); return 1; }
    build_tables();
    spdlog::info("[FH5UI] screen scanner up: {} key page vtables, VA window 0x{:X}-0x{:X}",
                 kScreenCount, g_min_va, g_max_va);

    std::vector<Found> cache;            // currently-believed-live page objects
    uint64_t last_full_ms = 0;

    for (;;) {
        const uint64_t now = GetTickCount64();

        // (1) cheap re-validation: a page destroyed/reused no longer carries its screen vtable -> drop it.
        cache.erase(std::remove_if(cache.begin(), cache.end(), [](const Found& f) {
            return !has_expected_vtable(f.addr, f.idx);
        }), cache.end());

        // (2) periodic full pass to discover NEW pages (the slow part; everything else is cheap).
        if (now - last_full_ms >= 2500) {
            last_full_ms = now;
            std::vector<Found> found;
            full_scan(found);
            for (const Found& f : found) {
                bool have = false;
                for (const Found& c : cache) { if (c.addr == f.addr) { have = true; break; } }
                if (!have) cache.push_back(f);
            }
        }

        // (3) report the highest-priority live screen. WHEN A GAMEPLAY CAMERA IS ACTIVELY RENDERING, a
        // loading-screen object is a POOLED LEFTOVER (FH5 keeps BaseLoadingScreen/etc alive after the load),
        // not actually shown — exclude P_LOADING pages then, or the persistent loader masks the HUD/menus.
        const uint64_t nt = GetTickCount64();
        const uint64_t w = fh5diag::last_world_ms(), sc = fh5diag::last_showcase_ms();
        const bool render3d = (w && nt >= w && nt - w < 1000) || (sc && nt >= sc && nt - sc < 1000);
        int best_idx = -1, best_prio = -1;
        unsigned shown_instances = 0;
        unsigned shown_classes = 0;
        bool shown_seen[kScreenCount] = {};
        char shown_list[256];
        size_t shown_pos = 0;
        shown_list[0] = '\0';
        for (const Found& f : cache) {
            if (!is_page_shown(f.addr)) continue;                         // pooled/hidden leftover -> not on screen
            if (render3d && kScreens[f.idx].prio == P_LOADING) continue;  // belt-and-suspenders for loaders
            ++shown_instances;
            if (!shown_seen[f.idx]) {
                shown_seen[f.idx] = true;
                ++shown_classes;
                const int n = _snprintf_s(shown_list + shown_pos, sizeof(shown_list) - shown_pos, _TRUNCATE,
                                          "%s%s", shown_pos ? "," : "", kScreens[f.idx].name);
                if (n > 0) shown_pos += (size_t)n;
            }
            if (kScreens[f.idx].prio > best_prio) { best_prio = kScreens[f.idx].prio; best_idx = f.idx; }
        }
        // If the scan says many unrelated screens are visible at once, it is reporting heap noise, not an
        // active page stack. Publish the raw counts but do not let the navigator trust the name.
        const bool reliable = best_idx >= 0 && shown_instances > 0 && shown_instances <= 3 && shown_classes == 1;
        g_live_count.store((unsigned)cache.size(), std::memory_order_relaxed);
        g_visible_count.store(shown_instances, std::memory_order_relaxed);
        publish(reliable ? kScreens[best_idx].name : "unknown", reliable, shown_list);

        // Periodic diagnostic: the full set of LIVE page classes (deduped). If many persist at once, pooling is
        // widespread and a per-object visibility check is required; if only loading screens persist, the
        // render3d gate above suffices. Reveals exactly which pages are pooled vs which track open/close.
        static uint64_t last_log_ms = 0;
        if (now - last_log_ms >= 4000) {
            last_log_ms = now;
            char list[512]; size_t pos = 0; list[0] = '\0';
            bool seen[kScreenCount] = {};
            for (const Found& f : cache) {
                if (f.idx < 0 || f.idx >= kScreenCount || seen[f.idx]) continue;
                seen[f.idx] = true;
                const int w = _snprintf_s(list + pos, sizeof(list) - pos, _TRUNCATE,   // "*" = AVUI-visible (shown)
                                          "%s%s%s", pos ? "," : "", kScreens[f.idx].name, is_page_shown(f.addr) ? "*" : "");
                if (w < 0) break;
                pos += (size_t)w;
            }
            spdlog::info("[FH5UI] screen={} reliable={} tracked={} visible={} visibleClasses={} render3d={} passes={} pages=[{}]",
                         reliable ? kScreens[best_idx].name : "unknown",
                         reliable ? 1 : 0,
                         (unsigned)cache.size(), shown_instances, shown_classes,
                         render3d ? 1 : 0, g_passes.load(std::memory_order_relaxed), list);
        }

        Sleep(200);
    }
    // unreachable
}

} // namespace

// ---------------------------------------------------------------------------
void start() {
    if (g_started.exchange(true)) return;
    if (HANDLE t = CreateThread(nullptr, 0, &WorkerThread, nullptr, 0, nullptr)) {
        CloseHandle(t);
    } else {
        g_started.store(false);
        spdlog::error("[FH5UI] failed to create screen scanner thread");
    }
}

void current_screen(char* out, size_t cap) {
    if (!out || cap == 0) return;
    std::scoped_lock lk{ g_pub_mtx };
    _snprintf_s(out, cap, _TRUNCATE, "%s", g_pub);
}

void visible_pages(char* out, size_t cap) {
    if (!out || cap == 0) return;
    std::scoped_lock lk{ g_pub_mtx };
    _snprintf_s(out, cap, _TRUNCATE, "%s", g_visible_pages);
}

unsigned live_page_count() { return g_live_count.load(std::memory_order_relaxed); }
unsigned visible_page_count() { return g_visible_count.load(std::memory_order_relaxed); }
unsigned long long scan_passes() { return g_passes.load(std::memory_order_relaxed); }
bool screen_reliable() { return g_reliable.load(std::memory_order_acquire); }

} // namespace fh5ui
