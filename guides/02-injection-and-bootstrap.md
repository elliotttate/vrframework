# 02 — Getting In: Injection & Bootstrap

**What this covers / why it matters.** Before you can render a single stereo frame, your code has to be *running inside the game's process* — sharing its address space, its D3D device, its window message loop. You don't have the engine's source, you can't recompile it, and you can't ask it nicely to load you. This document is about the very first link in the chain: how a few kilobytes of your DLL get loaded by a game you don't control, how it survives the Windows loader without deadlocking the process, and how it waits for the engine to be ready before it starts touching anything. Get this wrong and the game crashes on the splash screen — or worse, hangs with no output at all. All three reference projects solve it the same fundamental way (a proxy DLL), but they diverge sharply in *which* DLL they impersonate and *how much* defensive work they do around the Windows loader. We'll walk the technique, ground it in each codebase, and end with how to choose for a brand-new engine.

---

## 1. The core problem: getting code into a process you don't own

There are only a handful of ways to make a foreign process run your code:

| Technique | How it works | Trade-off |
|---|---|---|
| **Proxy / replacement DLL** | Drop a DLL named like one the game already loads (`dxgi.dll`, `dinput8.dll`) next to the exe; Windows loads *yours* first. | No external launcher, no admin, survives game updates. Must forward the real exports. **This is what all three projects use.** |
| **Manual injector** | A separate `.exe` calls `OpenProcess` + `CreateRemoteThread` + `LoadLibrary` to push your DLL into a running game. | Flexible, can inject late. Needs a separate process, looks exactly like a cheat to anti-cheat, needs handle privileges. |
| **AppInit / Detours / Shims** | OS-level mechanisms that inject into many processes. | Global, fragile, heavily flagged by AV. Almost never the right call for a single-game mod. |

The proxy-DLL approach wins for VR mods because the deliverable is dead simple: the user copies one file next to the game's `.exe` and launches the game normally. No launcher to maintain, nothing running in the background, nothing that has to find the game process by name.

### Why a *graphics/input* DLL specifically?

You can only proxy a DLL the game actually loads, and you want one that loads **early** and that you have a legitimate reason to be intimate with. Graphics and input libraries are perfect on both counts:

- `dxgi.dll` — the DirectX Graphics Infrastructure. Every D3D11/D3D12 game loads it, early, to create its swapchain. Proxying it puts you in the process before the renderer even exists. This is what the **ports** (starfield2vr, anvilengine2vr) use.
- `dinput8.dll` — legacy DirectInput. Loaded early by a huge range of games for controller/keyboard input. This is what **REFramework** uses.

Both are libraries you'll end up hooking anyway (you need the swapchain for rendering, the input device for the overlay), so being *inside* them is convenient rather than arbitrary.

---

## 2. How a proxy DLL actually loads first

Windows resolves a DLL dependency by searching a fixed path order. With default safe-DLL-search-mode, **the directory containing the executable is searched before the system directory** (`C:\Windows\System32`). So if the game imports `dxgi.dll`, and there's a `dxgi.dll` sitting next to `Game.exe`, the loader binds the import to *your* file — not the real one in System32.

That's the whole trick. But it comes with an obligation: the game still expects every export it imports from the real `dxgi.dll` to exist and behave. If the game calls `CreateDXGIFactory` and your proxy doesn't provide it, the import fails to resolve and the process dies before `main`. So a proxy DLL must **forward** (or re-implement and delegate) every export the game uses.

There are two ways to satisfy that obligation:

1. **Export forwarding via the linker** — `#pragma comment(linker, "/EXPORT:Name=realdll.Name")`, or a `.def` file, tells the loader "anyone asking me for `Name`, go get it from the real DLL." Zero runtime code for the pass-through exports.
2. **Manual re-export** — you declare a function with the same name, load the real system DLL yourself with `LoadLibrary` from the *system* directory, `GetProcAddress` the real function, and call through. This is what you do for the *one* export you actually care about, so you can run your own code on the way through.

REFramework's `Main.cpp` shows the manual style explicitly for the DirectInput entry point:

```cpp
__declspec(dllexport) HRESULT WINAPI
    direct_input8_create(HINSTANCE hinst, DWORD dw_version, const IID& riidltf,
                         LPVOID* ppv_out, LPUNKNOWN punk_outer) {
#pragma comment(linker, "/EXPORT:DirectInput8Create=direct_input8_create")
    ...
    auto result = g_original_dinput8_create(hinst, dw_version, riidltf, ppv_out, punk_outer);
    ...
}
```

(`REFramework/src/Main.cpp:59`). Note the `#pragma` trick on line 63: the C++ function is named `direct_input8_create` (the real name `DirectInput8Create` collides with the declaration pulled in from `dinput.h`), so the linker is told to export it *under the real name*. Cute, and necessary.

The "real" DLL is loaded explicitly from the system directory so the proxy doesn't recursively load itself:

```cpp
wchar_t buffer[MAX_PATH]{0};
if (GetSystemDirectoryW(buffer, MAX_PATH) != 0) {
    if ((g_dinput = LoadLibraryW((std::wstring{buffer} + L"\\dinput8.dll").c_str())) == NULL) {
        failed();
        ...
    }
    g_original_dinput8_create =
        (decltype(DirectInput8Create)*)GetProcAddress(g_dinput, "DirectInput8Create");
```

(`REFramework/src/Main.cpp:39`). If you `LoadLibraryW(L"dinput8.dll")` *without* the absolute system path, the loader would find your proxy again and you'd recurse into oblivion. Always resolve the genuine DLL by absolute system path.

The ports (`starfield2vr/src/Main.cpp`, `anvilengine2vr/src/Main.cpp`) push this plumbing into a helper — note the `#include "dgxiProxy.h"` at the top of both files. The export-forwarding boilerplate for `dxgi.dll` lives there, keeping `Main.cpp` focused on bootstrap logic rather than the dozens of DXGI exports that need pass-through.

---

## 3. `DllMain`: the most dangerous function in the codebase

When the loader maps your proxy, it calls your `DllMain` with `DLL_PROCESS_ATTACH`. This is your first instruction executed inside the game. It is also executed **under the Windows loader lock** — a global, process-wide critical section.

The rule that governs everything here: **do almost nothing in `DllMain`.** Specifically, you must not:

- Call `LoadLibrary` (or anything that triggers it) — you can deadlock against the loader lock.
- Wait on another thread that needs the loader lock — classic deadlock.
- Do meaningful work, allocate heavily, or block. The whole process is frozen until you return.

So the universal pattern across all three projects is identical: **spawn a worker thread and return immediately.** The new thread runs *outside* the loader lock and can safely do everything `DllMain` cannot.

The ports make this minimal and almost identical to each other:

```cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitThread, NULL, 0, NULL);
    case DLL_THREAD_ATTACH:
    ...
```

(`anvilengine2vr/src/Main.cpp:35`, identical structure in `starfield2vr/src/Main.cpp:35`). `CreateThread` is one of the few things you *can* safely call under loader lock — it doesn't run the new thread's code until the lock is released. The thread routine `InitThread` is where real life begins.

> **A latent bug worth noticing.** In both ports the `case DLL_PROCESS_ATTACH:` has no `break` before the next `case`. It falls through into the `DLL_THREAD_ATTACH`/`DETACH`/`PROCESS_DETACH` cases — which here all just `break`, so it's harmless. But it's exactly the kind of fall-through that bites you later if you add code to those cases. Note also that `DisableThreadLibraryCalls(hModule)` is commented out (line 43): enabling it suppresses the per-thread `DLL_THREAD_ATTACH`/`DETACH` notifications, a small perf win for a DLL that doesn't care about thread lifecycle. They left it off, probably to keep the door open for thread-local work.

REFramework does **more** in `DllMain`, and the comments tell you exactly why each thing has to happen there rather than on the worker thread:

```cpp
BOOL APIENTRY DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        REFramework::set_reframework_module((HMODULE)handle);
        ...
        // Need to pin the safetyhook allocator because it destroys itself
        static auto sh_allocator = safetyhook::Allocator::global();
        ...
        AllocateBuffer((LPVOID)halfway_module); // minhook

        // GameIdentity must be initialized before the integrity hooks below
        sdk::GameIdentity::initialize();

        IntegrityCheckBypass::setup_pristine_syscall();
        IntegrityCheckBypass::hook_add_vectored_exception_handler();
        IntegrityCheckBypass::hook_rtl_exit_user_process();

        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)startup_thread, handle, 0, nullptr);
    }
    return TRUE;
}
```

(`REFramework/src/Main.cpp:119`). The reason this work is *in* `DllMain` and not deferred to `startup_thread` is **timing, not preference**: the anti-tamper / integrity hooks (lines 145–147) and the hook-buffer allocation (line 138) must be installed *before the game's own code gets a chance to run its integrity checks or before other libraries load*. Capcom's RE Engine titles run integrity verification early; if you wait for a leisurely worker thread, you've already lost the race. So REFramework does the bare minimum of *ordering-critical* setup synchronously and pushes everything else — the actual framework, the hooks, the overlay — onto `startup_thread`. The comment on line 140 is explicit: "GameIdentity must be initialized before the integrity hooks below."

This is the key generalizable lesson: **`DllMain` is for ordering-critical, loader-safe setup only.** Everything else goes on the thread.

---

## 4. The worker thread and the `Sleep` trick

Once you're on the worker thread, you're free of the loader lock. But you have a new problem: **is the engine ready?** Your `DllMain` runs when your proxy is *mapped*, which is during the game's own startup — long before the renderer exists, before the swapchain is created, before the main window has a valid `HWND`. If you try to hook D3D or grab the swapchain too early, there's nothing there yet.

The ports solve this with the bluntest possible instrument — sleep, then go:

```cpp
void InitThread(HINSTANCE hModule) {
    Sleep(5000);
    g_framework = std::make_unique<Framework>(hModule);
    ...
}
```

(`starfield2vr/src/Main.cpp:23`). Anvil's is the same, with one extra line before the sleep:

```cpp
void InitThread(HINSTANCE hModule) {
    EngineTwicks::DisableBadEffects();
    Sleep(5000);
    g_framework = std::make_unique<Framework>(hModule);
    ...
}
```

(`anvilengine2vr/src/Main.cpp:23`).

**Why `Sleep(5000)`?** It's a five-second pause that lets the engine finish booting — load its renderer, create the device and swapchain, settle into its main loop — *before* the framework constructor starts hooking things and grabbing the swapchain. It is crude, it is racy, and it is a placeholder you should be slightly ashamed of, but it works remarkably well in practice because game startup reliably takes longer than five seconds. The alternative — polling for the engine to be ready, or waiting on the first `Present` call — is more correct but more code; for a mod under active development, the sleep buys you a working bootstrap while you build everything else.

> **`EngineTwicks::DisableBadEffects()` before the sleep (Anvil only).** This runs *first*, before the engine is fully up, because it's patching engine settings/effects (e.g. disabling post-process passes that fight stereo rendering) that need to be off before the renderer initializes them. Ordering again: some things must happen *before* the engine settles, most things *after*.

The two commented-out blocks in every `DllMain` are the debugging variant of the same idea:

```cpp
//        do {
//            Sleep(250);
//        } while (!IsDebuggerPresent());
```

(present in all three `Main.cpp` files). Uncomment this and the DLL spins, sleeping 250 ms at a time, until you attach a debugger. That gives you a window to attach to the game process *before* any of your real init runs — invaluable when you're debugging a crash that happens during bootstrap itself.

REFramework's worker thread (`startup_thread`, `REFramework/src/Main.cpp:83`) skips the fixed sleep because it doesn't need it the same way — the `REFramework` constructor itself handles "wait until D3D is ready" via hooks on the rendering path, and the integrity-bypass groundwork is already laid in `DllMain`. It does, however, do its debug-console setup (`AllocConsole`, lines 88–93, `#ifndef NDEBUG`) and the post-init cloaking work (`spoof_module_paths_in_exe_dir`, `unlink`, lines 104–115) that hides the injected module from the game's module list on certain titles (Monster Hunter Rise, Dragon's Dogma 2). That cloaking is anti-cheat-aware behavior the ports don't need because their target engines don't scan as aggressively.

---

## 5. Hooking the proxied API: input as a worked example

Owning the proxy gets you *in*; it doesn't automatically get you the *interception* you want. For DXGI the interesting hook is `Present` (covered in the next doc), reached by vtable-hooking the swapchain. For DirectInput, REFramework shows the canonical "create a throwaway object to steal a vtable" pattern in `DInputHook.cpp`:

```cpp
auto dinput8 = LoadLibrary("dinput8.dll");
auto dinput8_create = (DirectInput8CreateFn)GetProcAddress(dinput8, "DirectInput8Create");
...
dinput8_create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8W, (LPVOID*)&dinput, nullptr);
dinput->CreateDevice(GUID_SysKeyboard, &device, nullptr);
...
// Get the addresses of the methods we want to hook.
auto get_device_state = (*(uintptr_t**)device)[9];
```

(`REFramework/src/DInputHook.cpp:43`). COM objects are laid out as a pointer to a vtable; index `[9]` of `IDirectInputDevice`'s vtable is `GetDeviceState`. You create a *temporary* keyboard device purely to read its vtable, grab the function pointer, release the device, and then install a `FunctionHook` over that address (line 82). Every device of that type shares the vtable, so hooking it once intercepts the game's real device too.

The hook itself reads game input and forwards it to the framework, while supporting an "ignore input" mode so the VR overlay can capture the keyboard without the game also reacting:

```cpp
if (res == DI_OK && !m_is_ignoring_input && data != nullptr && size == 256) {
    g_framework->on_direct_input_keys(*(std::array<uint8_t, 256>*)data);
}
```

(`REFramework/src/DInputHook.cpp:105`). The `m_is_ignoring_input` flag (declared in `DInputHook.hpp:47`, toggled by `ignore_input()`/`acknowledge_input()`) is how the overlay "eats" input: when the ImGui menu is open, the mod still calls the original `GetDeviceState` to drain the device's buffered events (so they don't pile up), but doesn't pass them to the game. This is the same pattern you'll reuse for *every* hook: call original, decide whether the game sees the result, optionally feed a copy to your own systems.

---

## 6. Comparing the three bootstraps

| Concern | REFramework | starfield2vr | anvilengine2vr |
|---|---|---|---|
| Proxied DLL | `dinput8.dll` | `dxgi.dll` (`dgxiProxy.h`) | `dxgi.dll` (`dgxiProxy.h`) |
| `DllMain` work | Heavy: pin allocators, init `GameIdentity`, install integrity-bypass + exception hooks, *then* spawn thread | Minimal: spawn thread only | Minimal: spawn thread only |
| Worker entry | `startup_thread` | `InitThread` | `InitThread` |
| "Engine ready" wait | Handled by render-path hooks (no fixed sleep) | `Sleep(5000)` | `EngineTwicks::DisableBadEffects()` → `Sleep(5000)` |
| Anti-cheat cloaking | Yes — `spoof_module_paths_in_exe_dir`, `unlink` per-title | None | None |
| Framework handoff | `g_framework = make_unique<REFramework>(module)` | `g_framework = make_unique<Framework>(hModule)` | `g_framework = make_unique<Framework>(hModule)` |

What's striking is how little the ports kept. REFramework is the mature, hostile-environment codebase: it fights integrity checks, hides itself, and orders its `DllMain` work with surgical care because RE Engine games push back. The ports target engines (Creation Engine 2, Anvil) that don't actively scan for injected modules, so mutars stripped the bootstrap down to its essence — proxy `dxgi`, spawn a thread, sleep, construct the framework. The shared structure (`g_framework = make_unique<...>` as the single handoff into the universal core) is the seam where Layer 1 takes over from the platform-specific entry code.

---

## 7. Applying this to a brand-new engine

The general principle → concrete instance → new-engine recipe:

1. **Pick a DLL the target loads early and that you'll hook anyway.** D3D11/D3D12 games: `dxgi.dll` is almost always the right answer (you need the swapchain). If the game's renderer is reached some other way, `dinput8.dll` or `winmm.dll` are common early-load alternates. Confirm with a tool like Dependency Walker / `tasklist /m` against a running instance.
2. **Get the export-forwarding right.** Forward *every* export the game imports (use a `.def` file or per-export `#pragma`), and re-implement only the one or two you care about, delegating to the real system DLL loaded by **absolute** `GetSystemDirectory` path (see `REFramework/src/Main.cpp:39`). Test that the game launches *with your proxy doing nothing* before you add any logic — if it crashes here, your forwarding is incomplete.
3. **Keep `DllMain` tiny.** Spawn a thread, return `TRUE`. Only add synchronous work if ordering forces it (integrity checks, settings that must be patched pre-init), and comment *why*, the way REFramework does.
4. **On the worker thread, wait for the engine.** Start with `Sleep(5000)` to get moving; replace it later with a real readiness signal (poll for the swapchain, or defer construction until your first `Present` hook fires).
5. **Keep the debugger-wait stub handy.** Paste the `while(!IsDebuggerPresent()) Sleep(250)` loop into `DllMain` whenever you need to attach before bootstrap runs.
6. **Hand off to the universal core with a single global.** `g_framework = std::make_unique<Framework>(module)`. Everything downstream — D3D hooks, overlay, Mod system, VR runtime — lives behind that one object.

---

## Key takeaways

- **Proxy DLLs are the right injection vector for VR mods**: one file next to the exe, no launcher, no admin, survives updates. All three projects use them.
- **Proxy a DLL the game loads early and that you'll hook anyway** — `dxgi.dll` for D3D11/12 renderers (the ports), `dinput8.dll` for input (REFramework). You must forward every export, re-implementing only the ones you intercept, delegating the original to the system DLL loaded by absolute path.
- **`DllMain` runs under the loader lock — do almost nothing in it.** Spawn a worker thread and return. The only exceptions are ordering-critical, loader-safe setup (REFramework's integrity hooks), and the comments must justify each one.
- **Wait for the engine before you touch it.** The ports' `Sleep(5000)` is crude but effective; the principled version waits on a real readiness signal like the first `Present`.
- **Defensive depth scales with the target's hostility.** REFramework cloaks itself and bypasses integrity checks because RE Engine pushes back; the ports skip all of it because their engines don't scan.
- **The seam into the universal core is one line**: `g_framework = make_unique<Framework>(module)`. Platform-specific bootstrap ends there; the reusable Layer-1 machinery begins.

**Next:** *03 — Hooking the Renderer: DXGI, `Present`, and the D3D Device* — how the framework you just constructed grabs the swapchain, hooks `Present`/`ResizeBuffers`, and gets its hands on the device and command queue it needs to draw the overlay and submit frames to the headset.
