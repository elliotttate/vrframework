#pragma once

// FH5 menu navigator — read-the-state + drive-the-input, no screenshots.
//
// WHY: the launcher currently reaches gameplay by BLIND TIMING (press Enter, sleep 30s, press Enter,
// wait for the producer, Escape, sleep 18s). That is fragile. This module replaces it with a closed loop:
//
//   READ  — publish FH5's live flow state to E:\tmp\fh5_state.txt every ~200ms (producer main-camera rate,
//           near/far scene class, a "gameplay-camera-rendering" flag, and — when resolved — the active
//           camera class and the top UIPage/screen name). An external orchestrator (PowerShell) reads this
//           to know "what screen am I on" without any image.
//
//   DRIVE — synthesize controller input IN-PROCESS via an XInputGetState / XInputGetStateEx detour, commanded
//           through E:\tmp\fh5_nav.txt. FH5 menus are controller-first, so a virtual pad is far more reliable
//           than OS keystrokes (no window-focus games, works even with no physical controller connected).
//
// The orchestrator loop: read state -> branch on screen -> write a nav command -> repeat until free-roam
// driving (gameplay camera live + a driving camera class). See scripts/Navigate-FH5.ps1.

#include <cstdint>

namespace fh5nav {

// Install the XInputGetState / XInputGetStateEx inline detours so we can inject a virtual controller.
// Call ONCE from the dllmain bootstrap (after d3d12 is resident is fine; xinput is loaded on demand).
// Idempotent.
void install_xinput_hook();

// Start the background worker: polls E:\tmp\fh5_nav.txt for input commands, ticks the synthesized input,
// and writes E:\tmp\fh5_state.txt. Call from the adapter's on_initialize(). Idempotent.
void start();

} // namespace fh5nav
