#pragma once

// FH5 live UI screen detector — names the on-screen UI page by RTTI vtable, WITHOUT screenshots.
//
// WHY THIS (not the breadcrumb, not an active-page hook): the Stateflow crash-breadcrumb at 0x90907B0 is
// written only by the crash handler (proven empty in free-roam/menus/loading live), and the engine's active-
// page dispatcher is JUMPOUT-stubbed in the IDA DB so its global->top-page pointer can't be pinned statically
// (see _agent_reports/fh5_ui_active_page_capture_20260605.md). What IS proven is the classifier: every UI page
// is one of 405 controller classes with a UNIQUE offset-0 vtable RVA (fh5_ui_vtable_rtti_table_20260605.tsv),
// e.g. CopterHud=0x6407F18 (free-roam HUD), PauseMenuTiled=0x63C6710, Loading=0x6444E98.
//
// MECHANISM: a background thread scans committed-private heap for page-object vtables, validates the AVUI
// Visibility cache on each candidate, and publishes a screen only when the visible-candidate set is not noisy.
// This is diagnostic/recovery data for navigation; the scene/camera state remains the primary signal.

#include <cstddef>

namespace fh5ui {

// Launch the background screen-scanner thread (idempotent).
void start();

// Write the highest-priority currently-live UI page class name (e.g. "CopterHud", "PauseMenuTiled",
// "Loading") into out, or "unknown" if no known page object is live. Always null-terminated within cap.
void current_screen(char* out, size_t cap);

// Comma-separated unique visible page classes from the last scan pass. This is diagnostic but useful when
// the top-screen gate is deliberately conservative (for example, CarSelectGarage visible with stale HUD pages).
void visible_pages(char* out, size_t cap);

// Diagnostics for the heartbeat: number of live key-page objects currently tracked, and full-scan passes done.
unsigned live_page_count();
unsigned visible_page_count();
unsigned long long scan_passes();
bool screen_reliable();

} // namespace fh5ui
