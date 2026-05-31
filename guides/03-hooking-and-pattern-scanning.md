# 03 — Hooking & Finding Things: Pattern Scanning and Address Resolution

**What this covers / why it matters.** To inject VR into an engine you don't own, you need to do two things the engine never intended: *run your code* at specific moments (frame begin, projection calc, present), and *read/write* specific bytes of memory (the view matrix, the frame counter, a vtable slot). You have no headers, no symbols, no `.lib` to link against — just a stripped, optimized, retail `.exe` that changes layout every patch. This document is the toolkit for both halves of that problem: **hooking** (the four ways to redirect a function, and the two libraries the projects use to do it) and **address resolution** (how to find the function or global to hook in the first place, robustly enough that it survives game updates). Get this layer right and the rest of the port is "just" graphics. Get it wrong and you crash on launch after every Tuesday patch.

This is Layer-3 territory in our three-layer model (per-game offsets/patterns), but the *mechanism* — `FuncRelocation`, `InstructionRelocation`, `VTable` — lives in the reusable core. So this doc straddles the boundary on purpose.

---

## 1. The two problems, stated plainly

Every hook is the same shape:

> "When the engine calls function *F*, run *my* code first (or instead), then optionally let *F* continue."

That breaks into two independent sub-problems:

1. **Where is F?** You have an address-resolution problem. The engine's `BeginRenderFrame` is at some RVA (relative virtual address) that you must compute at runtime from the loaded module base. This is *pattern scanning / address libraries*.
2. **How do I divert F once I have it?** You have a code-patching problem: overwrite the first bytes of F with a jump to your function, preserving the ability to call the original. This is *hooking*.

The projects keep these cleanly separated. `offsets.h` (Layer 3) answers **where**; `safetyhook` / `HookManager` (Layer 1) answers **how**. Let's take *how* first, because it's the simpler half, then spend most of the doc on *where*, which is where ports actually break.

---

## 2. Hook types: four ways to divert a call

There is no single "hook." There are four mechanisms, each with a different cost/risk profile. You will use all four across a single port.

| Type | What you overwrite | When to use | Failure mode |
|------|--------------------|-------------|--------------|
| **Inline / detour** | First bytes of the target function with a `jmp` | The default. You have the function's address and want pre/post control. | Breaks if the first instructions are not relocatable (rare). |
| **Mid-function** | Bytes at an *interior* address; the hook saves/restores all registers | You need a value that only exists mid-function (a register holding a matrix pointer), or the function entry is shared/ambiguous. | Fragile — interior code shifts more than entries between patches. |
| **VTable** | A pointer slot in a C++ virtual table | The target is virtual and you have an object instance; you want to hook *all* instances of a class cleanly and reversibly. | Wrong instance/vtable; multiple inheritance offsets. |
| **Pointer / IAT** | An import-address-table entry or a stored callback pointer | Hooking another DLL's exported function (e.g. `Present` via the DXGI import) without touching its code. | The engine resolves the function some other way and never reads your slot. |

### Inline hooks (the workhorse)

An inline hook overwrites the first few bytes of the target with a relative `jmp` to your replacement. The bytes you stomped are copied into a *trampoline* — a small slab of executable memory that runs the original prologue and then jumps back into the function body. Calling the trampoline = calling the original.

The ports use **safetyhook** for this. Look at how Anvil installs its two frame hooks (`anvilengine2vr/games/valhalla/engine/EngineRendererModule.cpp:13`):

```cpp
auto beginEngineFrameFunct = memory::on_begin_frame_fn_addr();
m_beginEngineFrameHook = safetyhook::create_inline(
    (void*)beginEngineFrameFunct, (void*)&EngineRendererModule::on_begin_engine_frame);
```

The replacement calls the original through the stored hook object (`anvilengine2vr/games/valhalla/engine/EngineRendererModule.cpp:46`):

```cpp
const auto result = instance->m_beginRenderFrameHook.call<uintptr_t>(context);
```

`.call<T>(args...)` invokes the trampoline with the original signature. That one line is the whole "let the engine continue" contract — everything before it is your pre-hook, everything after is your post-hook.

REFramework wraps the same idea in a `FunctionHook` RAII class (`REFramework/shared/utility/FunctionHook.cpp:33`):

```cpp
m_inline_hook = safetyhook::InlineHook::create(m_target, m_destination);
```

Note the careful enumeration of failure modes right below it — `FAILED_TO_DECODE_INSTRUCTION`, `IP_RELATIVE_INSTRUCTION_OUT_OF_RANGE`, `SHORT_JUMP_IN_TRAMPOLINE` (`FunctionHook.cpp:41-58`). Those are not theoretical: a trampoline must relocate the prologue, and if the prologue contains a RIP-relative instruction whose displacement no longer reaches its target from the trampoline's new location, the hook *must* refuse rather than silently corrupt the function. A good hook library fails loudly here; a bad one crashes three frames later.

### VTable hooks (reversible, instance-aware)

When the target is a C++ virtual method, you can hook the *vtable slot* instead of the function body. You never touch executable code — you swap one pointer in a table — which makes it trivially reversible and avoids trampoline relocation entirely.

REFramework's `HookManager::add_vtable` is the reference implementation (`REFramework/src/HookManager.cpp:617`). It looks up (or builds) a per-object `HookedVTable`, then hooks the individual method by its virtual index (`HookManager.cpp:706`):

```cpp
if (!hook->vtable_hook->hook_method(fn->get_virtual_index(), (void*)hook_fn->facilitator_fn)) { ... }
return hook->vtable_hook->get_original<uintptr_t>(fn->get_virtual_index());
```

The key abstraction is `get_virtual_index()`: REFramework knows the index *from the engine's own reflection data*, so it never hardcodes "slot 13." The ports, lacking reflection, hardcode the index but discover the vtable by RTTI name (see §6). Starfield does exactly this — `((uintptr_t*)VTable(...))[13]` indexes slot 13 of the `FirstPersonState` vtable (`starfield2vr/src/CreationEngine/memory/offsets.h:30`).

### Mid-function hooks

Sometimes the value you want only exists *inside* a function — a matrix pointer that's been loaded into `rsi`, say. A mid-function hook patches an interior address and hands your callback a snapshot of all registers, so you can read/modify them and resume. safetyhook calls this `create_mid`. Use it sparingly: interior addresses are the *most* volatile thing to pin across patches, so a mid-hook should always be paired with a pattern (not a bare offset) so it re-resolves on update.

### The "is this a thunk?" subtlety

Optimizing compilers and incremental linkers love to emit a one-line stub that just `jmp`s to the real function. If you hook the stub, you hook nothing useful (and may miss internal call sites that bypass the stub). REFramework defends against this with `detail::get_actual_function`, which disassembles the first few instructions and follows a leading `jmp` to the real target (`REFramework/src/HookManager.cpp:35`):

```cpp
if (hde.opcode == 0xE9) { // jmp.
    actual_fn = (void*)(ip + hde.imm.imm32);
    break;
}
```

It stops at `ret`/`int3` so it won't walk off into garbage (`HookManager.cpp:23-33`). When you write your own resolver, budget for this: the address your pattern lands on is not always the address you want to hook.

---

## 3. MinHook vs safetyhook: the library split

The lineage left a fingerprint here. **REFramework historically used MinHook** — its `dependencies/minhook/` is still vendored and used by the low-level D3D/integrity hooks (`REFramework/src/mods/Hooks.cpp`, `IntegrityCheckBypass.cpp`). **The ports use safetyhook**, and REFramework's *shared* `FunctionHook` has itself migrated to safetyhook (`FunctionHook.cpp:33`). So the practical split today is:

| | MinHook | safetyhook |
|--|---------|-----------|
| **Used by** | REFramework core D3D/IAT plumbing, integrity bypass | All three ports; REFramework's shared `FunctionHook` |
| **API style** | C, global state (`MH_CreateHook` / `MH_EnableHook` / `MH_Initialize`) | C++, RAII objects (`InlineHook`, `MidHook`); hook lifetime = object lifetime |
| **Trampoline safety** | Works, mature, but errors are coarse | Rich typed errors (`std::expected`), explicit IP-relative range checks |
| **Mid-function hooks** | Not first-class | First-class `create_mid` with a register-context struct |
| **Mental model** | "install a hook in the global table" | "own a hook object; drop it to unhook" |

Why did the ports standardize on safetyhook? Two reasons that matter for VR specifically:

1. **RAII matches the Mod lifecycle.** A VR mod gets enabled/disabled, reloaded, and torn down. Hooks-as-objects means "unhook" is just "destroy the member," which is exactly how `EngineRendererModule` holds `m_beginEngineFrameHook` / `m_beginRenderFrameHook`.
2. **Typed trampoline errors.** Frame-pacing hooks sit on hot, prologue-dense functions. safetyhook telling you *exactly* why a trampoline failed (vs. MinHook returning a generic status) saves hours when a patch reshuffles a prologue.

If you start a brand-new port, follow the ports: take safetyhook, wrap it in a tiny RAII helper like `FunctionHook`, and only reach for MinHook if you're integrating with REFramework's existing D3D layer that already speaks it.

> **REFramework's `HookManager` is a third thing.** It's not "a hooking library" — it's a *reflection-driven* hook factory that JIT-assembles a per-function "facilitator" stub with asmjit (`HookManager.cpp:150`) so a single generic pre/post callback can hook *any* engine method whose signature it learned from the type database (`fn->get_param_types()`, `fn->get_return_type()`, `HookManager.cpp:588`). That's a luxury you only get when the engine ships a reflection system (RE Engine does; Creation/Anvil don't). For Creation/Anvil you write a typed C++ replacement per function and call `.call<T>()` yourself. Know that `HookManager` exists, but don't expect to reproduce it without engine reflection.

---

## 4. Finding addresses without symbols: AOB / signature scanning

Now the hard half. You have an address-less retail binary. How do you find `BeginRenderFrame`?

The durable answer is **signature scanning** (a.k.a. AOB — "array of bytes" scanning): you identify the function by a unique sequence of its own machine-code bytes, then search the loaded module for that sequence at runtime. Because the bytes *are* the code, they survive relocation (ASLR) — you find the function wherever Windows mapped it — and they often survive minor patches too, as long as that particular code didn't change.

The core implements the scanner in ~40 lines. First, parse an IDA-style pattern string (`vrframework/src/memory/memory_mul.cpp:30`):

```cpp
struct PatByte { uint8_t value; bool wildcard; };
std::vector<PatByte> parse(std::string_view p) {
    ...
    if (c == '?') { out.push_back({ 0, true }); ... }   // wildcard byte
    else { /* hex pair -> byte */ out.push_back({ (uint8_t)((hi<<4)|lo), false }); }
}
```

Then a naive linear scan over the module, skipping wildcard positions (`vrframework/src/memory/memory_mul.cpp:63`):

```cpp
for (size_t i = 0; i + pat.size() <= size; ++i) {
    bool match = true;
    for (size_t j = 0; j < pat.size(); ++j)
        if (!pat[j].wildcard && data[i + j] != pat[j].value) { match = false; break; }
    if (match) return base + i;
}
```

The module bounds come from the OS loader, not a guess (`memory_mul.cpp:9-24`): `GetModuleHandleW(nullptr)` for the base, `GetModuleInformation(...).SizeOfImage` for the size. Everything downstream is "find a byte pattern in `[base, base+size)`."

### Wildcards: the whole point

A pattern with no wildcards is brittle — any immediate operand (a constant, an offset, a relative call target) that changes between builds breaks it. Wildcards (`?` / `??`) mark "this byte may vary, don't compare it." Look at a real Anvil signature (`anvilengine2vr/games/valhalla/engine/memory/offsets.h:14`):

```cpp
static const auto pattern = "40 53 48 83 EC 20 48 8B D9 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D";
```

The `48 8B 0D ? ? ? ?` is `mov rcx, [rip+disp32]` — the *opcode* is stable but the *displacement* (four bytes) points at a global whose address shifts every build, so those four bytes are wildcarded. Likewise `E8 ? ? ? ?` is a `call rel32` whose target moves. You wildcard the operands and keep the opcodes. That's the craft: a good signature is mostly opcodes and structural bytes, with operands blanked out, chosen to be **unique** in the module while being **stable** across builds.

Starfield's signatures are longer and gnarlier because Creation Engine functions are large and it leans on stable prologues plus distinctive constants (`starfield2vr/src/CreationEngine/memory/offsets.h:162`):

```cpp
// CreationRenderer::GetDXGIState — keyed off two literal magic comparisons
auto pattern = "8B C1 44 8B C1 25 1C 58 04 00 3D 1C 58 04 00 75 06 B8 C3 0A 00 00 C3 ...";
```

The repeated `1C 58 04 00` is a literal constant the function compares against — an excellent, near-unique anchor.

---

## 5. RIP-relative operand resolution (`InstructionRelocation`)

Signature scanning lands you *on an instruction*. But often the thing you actually want isn't that instruction — it's what the instruction *references*. On x64, globals are reached via RIP-relative addressing: `mov rax, [rip+disp32]`. The pattern finds the `mov`; you still have to do the pointer arithmetic to recover the global's address. That's `InstructionRelocation`.

The math is exactly the CPU's own effective-address computation (`vrframework/src/memory/memory_mul.cpp:101`):

```cpp
const auto addr = *ref;  // where the pattern matched
const auto val  = addr + *reinterpret_cast<int32_t*>(addr + offset_begin) + instruction_size;
```

- `offset_begin` = byte offset from the match to the start of the 4-byte displacement.
- `instruction_size` = length of the *whole* instruction (RIP points at the *next* instruction, so you add the full length).
- The `int32_t` displacement is **signed** — globals can sit before or after the instruction.

Read a concrete call site (`starfield2vr/src/CreationEngine/memory/offsets.h:216`):

```cpp
// PlayerCamera singleton: 48 8B 05 ? ? ? ?  ==  mov rax, [rip+disp32]
auto pattern = "48 8B 05 ? ? ? ? 48 8B 48 10 ...";
static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(937788), 937788);
```

`48 8B 05` is 3 bytes (so `offset_begin = 3`, the displacement starts right after), and `mov rax,[rip+disp32]` is 7 bytes total (`instruction_size = 7`). Anvil uses the identical `(3, 7)` recipe for its `lea`/`mov`-referenced globals (`anvilengine2vr/games/valhalla/engine/memory/offsets.h:46`, `:67`):

```cpp
// 48 8D 0D ? ? ? ?  ==  lea rcx, [rip+disp32]   -> also (3, 7)
static auto addr = InstructionRelocation("g_current_ui_pointer_addr", pattern, 3, 7, 0x62c1bc0);
```

And the 6-byte form `8B 15 ? ? ? ?` (`mov edx,[rip+disp32]`) uses `(2, 6)` (`starfield2vr/.../offsets.h:180`). The rule generalizes: `offset_begin` = bytes before the disp32, `instruction_size` = total instruction length. Get one of those numbers wrong and you compute a plausible-looking but completely wrong pointer — which is why every entry pins both an `a_id` for logging and a fallback for cross-checking.

> **Rule of thumb for new engines.** If your pattern sits *on* the function you want to hook → `FuncRelocation`. If your pattern sits on an instruction that *references* a global you want to read → `InstructionRelocation` with the right `(offset_begin, instruction_size)`.

---

## 6. RTTI vtable lookup (`VTable`)

C++ engines compiled with RTTI embed, for every polymorphic class, a string like `.?AVFirstPersonState@@` (an MSVC mangled type name) reachable from the vtable via the `RTTICompleteObjectLocator`. That string is *far* more stable across patches than any byte pattern, because the class name doesn't change when the compiler reshuffles code. So the most robust way to find a class vtable is: scan for the RTTI type-descriptor string, walk back to the locator, walk to the vtable.

That's what `VTable(name, type_name, fallback)` does. The core's signature is wired up (`vrframework/include/memory/memory_mul.h:38`) but the RTTI walk is still a port-from-praydog TODO (`vrframework/src/memory/memory_mul.cpp:116`) — it currently trusts the fallback. The working reference is Starfield's `ScanHelper.h::VTable`, which calls praydog's `utility::rtti::find_vtable` (`starfield2vr/src/CreationEngine/memory/ScanHelper.h:17`):

```cpp
auto ref = utility::rtti::find_vtable(mod, table);   // table == ".?AVFirstPersonState@@"
```

Callers then index the slot they want (`starfield2vr/src/CreationEngine/memory/offsets.h:29`):

```cpp
static auto pattern = ".?AVFirstPersonState@@";
static auto addr = ((uintptr_t*)VTable("FirstPersonState::vftable[13]", pattern,
                                       OffsetsTable::GetOffset(459617)))[13];
```

This is the cleanest resolution method available — prefer it whenever the target is a virtual method on an RTTI-bearing class. The only fragile part left is the *index* (`[13]`), which can shift if the class gains/loses virtuals; the type *name* essentially never changes.

---

## 7. Fallback static offsets — and why you keep both

Every resolver above takes a *fallback offset* alongside its pattern. This is the single most important reliability decision in the whole layer, so it's worth dwelling on.

A pattern scan is **robust but slow and occasionally ambiguous**. A static offset (`base + 0x817110`) is **instant and exact but valid for exactly one build**. The projects keep *both* and switch between them by build flag (`vrframework/src/memory/memory_mul.cpp:74`):

```cpp
#if defined(_DEBUG) || defined(SIGNATURE_SCAN)
static constexpr bool kDoScan = true;
#else
static constexpr bool kDoScan = false;
#endif
```

The behavior, seen in `FuncRelocation` (`memory_mul.cpp:80`):

- **Signature-scan builds** (`SIGNATURE_SCAN`/`_DEBUG`): scan the module. If the scanned offset disagrees with the recorded fallback, *log a warning* and use the scan result:
  ```cpp
  if (fallback && off != fallback)
      spdlog::warn("FuncRelocation '{}' mismatch: scanned={:x} fallback={:x}", name, off, fallback);
  return *ref;
  ```
- **Release builds**: skip scanning entirely; trust `fallback + module_base()`.

This is a deliberate workflow, not a hack:

1. **Ship release with static offsets** — zero scan cost at startup, deterministic addresses.
2. **When a patch breaks the game**, flip on `SIGNATURE_SCAN`, run, and the mismatch warnings tell you *exactly which addresses moved and to what new offset*. The pattern is your "re-find the function after the patch" tool.
3. **Copy the new offsets back** into the table, ship a new release.

The mismatch warning is the linchpin — it turns "the game updated, everything's broken" into a precise diff. The original Starfield helper carries the same dual logic (`starfield2vr/src/CreationEngine/memory/ScanHelper.h:36-56`), warning on `scanned != static`. Note also the defensive `mod_end = base + size - 0x100` margin in that file (`ScanHelper.h:9`) so a scan near the tail never reads past the module.

> The vrframework `VTable` stub (`memory_mul.cpp:116`) shows the degraded case: with the RTTI walk not yet ported, it *only* has the fallback path. It still works for a pinned build — it just can't self-heal after a patch until the scan is implemented. That's the whole argument for "keep both" in one line of code.

---

## 8. Address libraries: `REL::ID` / CommonLibSF

Bethesda modding solved the "addresses move every patch" problem at community scale with an **address library**: a giant offline-maintained table mapping a stable, build-independent **ID** to that build's offset. Instead of `base + 0x817110`, you write `REL::ID{ 937788 }` and a per-version database resolves it. The community ships a new database per game patch; your mod's IDs never change.

`sdk-lite/include/REL/Relocation.h` is the trimmed CommonLibSF `Relocation<T>` — a thin, typed wrapper over a resolved address. The interesting parts:

- **It is callable.** `operator()` forwards to the address as a function with the right ABI (`starfield2vr/sdk-lite/include/REL/Relocation.h:198`), including the nasty x64 detail that a member function returning a non-POD struct takes a hidden return-pointer argument (`is_x64_pod`, `invoke_member_function_non_pod`, `Relocation.h:108-122`). So a resolved address behaves like a typed function pointer.
- **It patches vtables.** `write_vfunc(idx, newFunc)` swaps slot `idx` and hands back the original (`Relocation.h:206`) — VTable hooking expressed as a one-liner.
- **It carries assembly constants** for byte-patching: `NOP`…`NOP9`, `JMP8/JMP32`, `RET`, `INT3` (`Relocation.h:125-138`) — handy when a "hook" is really "NOP out this check," as in Anvil's `disable_taa2_addr` site.

Starfield's `OffsetsTable` is the bridge between the two worlds. It stores a hand-recorded offset per ID *and* cross-checks it against the address library at runtime (`starfield2vr/src/CreationEngine/memory/offsets_table.h:42`):

```cpp
uintptr_t GetOffset(int ID) {
    auto offset = offsetMap[ID].STEAM_OFFSET;
#ifdef XBOX_STORE
    offset = offset_map.XBOX_OFFSET;        // same ID, different store build
#endif
#ifndef USE_STARFIELD_SDK_LITE
    uintptr_t library_offset = REL::ID{ a_id }.offset();
    if (offset != library_offset)
        spdlog::error("Offset does not match offset by ID ... scan={:x} lib={:x}", ...);
#endif
    return offset;
}
```

So Starfield has **three** independent sources of truth for each address — a recorded offset, an AOB signature, and an address-library ID — and they validate each other. That's belt-and-suspenders, and it's why the Starfield port survives patches with minimal hand-fixing.

REFramework needs none of this: it asks the **engine's own reflection** for method addresses by name (`fn->get_function()`, `HookManager.cpp:550`). Three tiers, same goal:

| Engine | Has reflection? | Primary resolution | Robustness source |
|--------|-----------------|--------------------|-------------------|
| RE Engine (REFramework) | Yes | Type DB: look up method by name → address | Engine guarantees it |
| Creation 2 (Starfield) | No | Address-library ID + AOB + recorded offset, cross-checked | Community DB + signatures |
| Anvil (ports) | No | AOB pattern + static fallback | Signatures + manual re-record |

---

## 9. How one engine binary serves many game versions

Three orthogonal kinds of "version" exist, and the projects handle each differently:

**(a) Storefront variants of the same game.** Steam and Xbox/Game Pass ship *different binaries* of the same Starfield build. Same logical function, different offset. `OffsetMapping` carries both columns (`starfield2vr/src/CreationEngine/memory/offsets_table.h:5`) and `GetOffset` picks by `#ifdef XBOX_STORE`. One ID, two offsets, one source build.

**(b) Patches over time.** The game updates; offsets move. AOB signatures + the address-library ID re-resolve the same logical function at its new home. The static offset is the *fast path*; the pattern is the *recovery path* (§7). This is why you keep both.

**(c) Different games on the same engine.** Anvil runs Odyssey, Valhalla, and Mirage; they share an engine but each has its own functions and offsets. Anvil's structure encodes this directly: `games/<Title>/engine/memory/offsets.h`, one file per title (the Valhalla one is `anvilengine2vr/games/valhalla/engine/memory/offsets.h`), all built against the *same* Layer-1/Layer-2 code. The engine adapter is shared; only the Layer-3 data file differs.

The core sketches the *next* step — pulling offsets out of compiled `.h` entirely and into a runtime manifest, so one binary serves every title without recompiling (`vrframework/include/memory/memory_mul.h:47`):

```cpp
class OffsetManifest {
    bool load(const std::string& path);              // e.g. "games/valhalla.toml"
    uintptr_t resolve(const std::string& name) const; // scan-or-fallback by name
};
```

It's a stub today (`vrframework/src/memory/memory_mul.cpp:130` logs a TODO), but the intent is the logical endpoint of this whole doc: **resolution metadata is data, not code.** Each `OffsetEntry` is just a `{ pattern, fallback }` pair (`memory_mul.h:43`) — exactly the two arguments `FuncRelocation` already takes — so the manifest is a serialization of what's currently hardcoded in `offsets.h`. Ship the engine binary once; ship a per-game `.toml` of patterns + fallbacks; never recompile to support a new title or patch.

---

## How to apply this to a brand-new engine

1. **Get module bounds from the OS** (`GetModuleHandleW(nullptr)` + `SizeOfImage`), never hardcode them.
2. **Find one function by AOB**, hook it with `safetyhook::create_inline`, log from inside it. That proves your scanner, your bounds, and your injection all work end-to-end before you touch graphics.
3. **For each address, decide the resolution flavor:** on the function → `FuncRelocation`; on an instruction referencing a global → `InstructionRelocation(pattern, offset_begin, instruction_size)`; virtual method on an RTTI class → `VTable(name, ".?AV...@@")`.
4. **Always record a fallback offset next to every pattern**, and wire the `SIGNATURE_SCAN` mismatch warning. That warning is your patch-recovery tool.
5. **Watch for thunks** — follow a leading `jmp` before hooking (`get_actual_function`).
6. **If the engine has reflection, use it** and skip most of §4–§8. If it doesn't, consider whether a community address library already exists (it does for Bethesda; it doesn't for Anvil).
7. **Keep resolution metadata as data** — one offsets file per game/store, with an eye toward an external manifest so the binary outlives the patch cycle.

---

## Key takeaways

- **Two separable problems:** *where* a function is (address resolution) and *how* to divert it (hooking). Keep them in separate files — the projects do.
- **Four hook types**, each for a situation: inline (default), mid-function (interior values, fragile), vtable (virtual + reversible), pointer/IAT (cross-DLL). The ports standardized on **safetyhook** (RAII, typed errors, `create_mid`); REFramework still vendors **MinHook** for its low-level D3D plumbing.
- **AOB signature scanning** finds functions by their own machine-code bytes; **wildcards blank out volatile operands** (`call rel32`, RIP displacements) so a signature is unique-but-stable.
- **`InstructionRelocation` recovers what an instruction references** via signed RIP-relative math: `addr + disp32 + instruction_size`, with `(offset_begin, instruction_size)` = `(3,7)` for `48 8B 05`-style 7-byte loads, `(2,6)` for 6-byte forms.
- **RTTI `.?AV...@@` lookup** is the most patch-stable way to find a class vtable; index the slot you need.
- **Always keep a pattern *and* a static fallback.** Release trusts the offset for speed; `SIGNATURE_SCAN` builds re-scan and *warn on mismatch* — that warning is your post-patch repair tool.
- **Address libraries (`REL::ID` / CommonLibSF)** give Bethesda mods build-independent IDs; Starfield cross-checks recorded offset vs. AOB vs. library ID — three sources validating one address.
- **One binary, many versions:** store columns for storefront variants, AOB+ID for patches, per-`<Title>` offsets files for sibling games — and the endgame is an external `OffsetManifest` so resolution is *data*, not recompiled code.

**Next:** `04-d3d-hooks-and-the-imgui-overlay.md` — with functions findable and hookable, we hook the swapchain itself: `Present`/`ResizeBuffers` on D3D11 and D3D12, standing up the shared render target and the ImGui overlay that every mod draws through.
