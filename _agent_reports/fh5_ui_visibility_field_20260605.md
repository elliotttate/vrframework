# FH5 (Empress) UI — Per-Object "Shown vs Pooled" Visibility Field

Date: 2026-06-05
Target: `ForzaHorizon5.exe` (Empress build), image base `0x140000000`, **RVA = VA − 0x140000000**.
DB: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64` (IDA 9.0 idalib).
Corpus: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\` (643 shards).
Prereq read: `fh5_ui_active_page_capture_20260605.md`, `fh5_ui_screen_reader_spec_20260605.md`.

---

## 0. TL;DR — the answer

**Field: a flags DWORD on the AVUI `UIElement` base at object offset `0x70` (112).**
The page's **effective (cached) `Visibility`** lives in two bits of that dword:

| Visibility | enum # | bit 0x20 | bit 0x40 | meaning |
|---|---|---|---|---|
| **Visible**   | **2** | **SET** | clear | **SHOWN** |
| Hidden    | 1 | clear | SET   | hidden (pooled/off-screen) |
| Collapsed | 0 | clear | clear | collapsed (pooled) |

**`is_page_shown(page)  ⟺  (*(uint32_t*)((char*)page + 0x70) & 0x20) != 0`.**

A SHOWN page has bit `0x20` set (Visibility==Visible). A POOLED/hidden leftover (e.g. the resident
`BaseLoadingScreen`, vtable RVA `0x627BD40`) will have been set to Collapsed/Hidden, so bit `0x20`
is **clear**. Offset `0x70` is on the shared `UIElement` base (single-inheritance, subobject offset 0),
so it is the SAME offset for every page controller class (CopterHud, Loading, PauseMenuTiled, …).

Confidence: **PROVEN by decompile** — getter, setter, and the OnVisibilityChanged cache-writer all
agree on offset `0x70` and the `0x20`/`0x40` bit encoding. (Live-read validation step in §5.)

This is candidate **#1 from the prompt (the AVUI Visibility enum)** — except the numeric values are
**reversed vs WPF**: this engine uses **Collapsed=0, Hidden=1, Visible=2** (WPF is Visible=0).

---

## 1. Toolchain note (important for reproducing)

The live `.i64` leaves nearly all AVUI/UI `.text` un-analyzed: `ida_decompile_at.py` returns bare
`JUMPOUT(start+k)` stubs for these, and the corpus only has *thunks* (functor ctors) for AVUI core —
the real bodies are absent there too. To read them I added a **force-decompiler** that undefines the
function bytes, relinearizes instructions, re-adds the function, and decompiles:

- `scripts\ida_force_decompile.py <i64> <addr>...` — works on stubbed funcs AND on vtable-slot
  addresses that have no function at all (creates one). This is what produced every decompile below.
- Supporting helpers added: `scripts\ida_dump_vtable.py`, `scripts\ida_dump_data.py`,
  `scripts\ida_find_str.py`, `scripts\ida_disasm_range.py`.

(Cross-references are NOT built for UI in this DB — `xrefblk` returns none for UI globals/strings —
so refs were found by a manual RIP-relative `.text` scan, see §3.)

---

## 2. The proof — getter, enum values, setter, cache-writer

### 2a. `get_Visibility` reads the cache at `+0x70` — `sub_141801190` (RVA 0x1801190)
```c
__int64 __fastcall sub_141801190(__int64 a1)        // a1 = UIElement* (= page, offset-0)
{
  int v1 = *(_DWORD *)(a1 + 112);   // 0x70 flags dword
  if ( (v1 & 0x20) != 0 )
    return 2LL;                     // Visible
  else
    return (v1 & 0x40) != 0;        // 0x40 -> Hidden(1), else Collapsed(0)
}
```
Disassembly (exact):
```
mov  edx, [rcx+70h]     ; flags @ +0x70
shr  eax, 5 ; test al,1  ; bit 0x20 set? -> return 2 (Visible)
shr  edx, 6 ; and eax,1  ; else bit 0x40 -> 1 (Hidden) / 0 (Collapsed)
```

### 2b. The enum numeric values — AVUI::Visibility metadata (PROVEN from reflection DATA)
The `AVUI::Visibility` property value-type object is built by `sub_1417736F0` (RVA 0x17736F0); its
value-count method (`sub_141777E60`) returns **3**, and its name table at `unk_145F76CC0`
(RVA 0x5F76CC0) is `(number, UTF-16 name)` pairs:

| stored # | name (UTF-16, read from the table's name ptrs) |
|---|---|
| 0 | **Collapsed** |
| 1 | **Hidden** |
| 2 | **Visible** |

So this engine's `AVUI::Visibility` is **{Collapsed=0, Hidden=1, Visible=2}** — *reverse of WPF*.
(Names confirmed UTF-16: ptrs `0x5F3E160`="Collapsed", `0x5F7A898`="Hidden", `0x5F7A8A8`="Visible".)

### 2c. `set_Visibility` — `sub_1415088D0` (RVA 0x15088D0)
```c
// set_Visibility(element, enumValue):
result = get_Visibility(a1);                 // unk_141801190
if ( result != a2 )
  SetValue(a1, qword_1490A8688, box(a2));     // sub_1416F6F80 = DependencyProperty::SetValue
```
`qword_1490A8688` (RVA 0xA8688) is the **Visibility DependencyProperty** global (runtime-initialized;
`0xFFFF…` in the static image). Visibility is a DP/property-bag value — there is **no raw enum member** —
but every write runs the changed-handler below, which maintains the `+0x70` cache.

### 2d. OnVisibilityChanged writes the `+0x70` cache — `sub_1417CB5F0` (RVA 0x17CB5F0)
```c
v7 = newVisibilityEnum;                       // 0/1/2 unboxed from the DP value
if ( v7 == 1 ) {                              // Hidden
  *(_DWORD *)(a1 + 112) &= ~0x20u;
  *(_DWORD *)(a1 + 112) |=  0x40u;
} else if ( v7 == 2 ) {                       // Visible
  *(_DWORD *)(a1 + 112) = (*(_DWORD*)(a1+112) & 0xFFFFFF9F) | 0x20;   // 0xFFFFFF9F = ~(0x20|0x40)
} else {                                      // v7 == 0  Collapsed
  *(_DWORD *)(a1 + 112) &= 0xFFFFFF9F;        // clear both
}
```
Getter, setter and cache-writer are mutually consistent → the `+0x70`/`0x20`/`0x40` encoding is
not an inference, it is the implementation. The cache is updated on **every** show/hide (it is the
property-changed callback), so it tracks live state, not just construction.

### 2e. Corroboration that "show" sets Visible=2 (SetValue sites)
Multiple element "show" helpers call `SetValue(this, qword_1490A8688, box(2))` — e.g. inside
`sub_141468EB0` (`LODWORD(a14)=2; sub_1416F6F80(v41, qword_1490A8688, box(2))`). Element "hide"
helpers box `1`/`0`. Confirms 2=Visible=shown at the call-site level too.

---

## 3. How the field/refs were located (audit trail)
1. UIPage base vtable `0x5F1F320` (from the RTTI table) → dumped slots → force-decompiled the AVUI
   base virtuals; saw a layout/state flags dword at `+0x100`, plus the `+0x70` flags used by the
   Visibility accessors.
2. Found the `"Visibility"` .rdata string `0x5E77880` referenced from UI `.text`
   (`sub_1417E9D90`, the FrameworkElement property-name registrar) and the value-type builder
   `sub_1417736F0` → got the enum table + the DP global `qword_1490A8688`.
3. Manual RIP-relative scan of `.text` for refs to `qword_1490A8688` → 12 functions; decompiling them
   yielded `set_Visibility` (`sub_1415088D0`) whose comparator is `get_Visibility` (`sub_141801190`,
   reads `+0x70`).
4. Scan of `.text` for `OR/AND dword[reg+0x70], imm` → `sub_1417CB5F0` = the OnVisibilityChanged
   writer, which closes the loop.

---

## 4. C++ read sketch for the mod
```cpp
// page = the UIPage*/controller object whose +0 is the offset-0 controller vtable
// (i.e. exactly the pointer you already RTTI-classify with the vtable table).
enum AvuiVisibility { AVUI_COLLAPSED = 0, AVUI_HIDDEN = 1, AVUI_VISIBLE = 2 };

inline int  page_visibility(const void* page) {           // mirrors get_Visibility @0x1801190
    uint32_t f = *reinterpret_cast<const uint32_t*>((const char*)page + 0x70);
    if (f & 0x20) return AVUI_VISIBLE;                      // 2
    return (f & 0x40) ? AVUI_HIDDEN : AVUI_COLLAPSED;       // 1 : 0
}
inline bool is_page_shown(const void* page) {              // the one-liner you want
    return (*reinterpret_cast<const uint32_t*>((const char*)page + 0x70) & 0x20u) != 0;
}
```
Wire `is_page_shown(page)` into the screen scanner: classify every live page object by vtable
(Deliverable 1 / the TSV), then keep only those where `is_page_shown()` is true → that filters out the
pooled `BaseLoadingScreen` (and any other Collapsed/Hidden leftover) that "exists but isn't drawn".

---

## 5. Live validation (≈2 min, do once to be 100% sure)
1. Inject; find a known live page object (e.g. the free-roam `CopterHud`, vtable RVA `0x6407F18`, or
   the resident pooled `BaseLoadingScreen`, vtable RVA `0x627BD40`).
2. Read `uint32_t f = *(uint32_t*)(page + 0x70)`:
   - Free-roam, on the live HUD page: expect **`f & 0x20 == 0x20`** (Visible) → `is_page_shown()==true`.
   - The pooled `BaseLoadingScreen` after a load finished: expect **`f & 0x20 == 0`** (Collapsed/Hidden)
     → `is_page_shown()==false`. This is the exact case that motivated the task.
3. Optional cross-check: call the in-game getter `((int(__fastcall*)(void*))(base+0x1801190))(page)`
   and confirm it returns 2 for shown / 0 or 1 for pooled — it reads the same `+0x70` bits.

If a future build shifts the offset, re-derive it in seconds: `get_Visibility` is the function that does
`mov reg,[this+DISP]; shr;test 0x20; …` — DISP is the offset (AOB of the body:
`8B 51 70 8B C2 C1 E8 05 A8 01 74 06 B8 02 00 00 00 C3 C1 EA 06 0F B6 C2 83 E0 01 C3`).

---

## 6. Candidate ranking (per the prompt's priority list)
1. **AVUI `Visibility` (cached) at `+0x70`, test bit `0x20` — USE THIS.** Proven; single dword read;
   fixed base offset; directly means "currently Visible". Best signal for shown-vs-pooled. (Values are
   reversed vs WPF: Visible=2, not 0.)
2. `IsVisible` bool — not a separate cached bool in this engine; "is it visible" == the `+0x70` 0x20 bit
   above (the engine's own render/input/layout paths read these `+0x70` bits). No better field exists.
3. Scene-attached / `IUISceneDataAdopter` back-pointer (object `+0x430`, `a1[134]`) — exists, but it is a
   secondary-base vtable slot and is set at construction; it indicates "owns a scene adopter", not
   "currently shown". Weaker and not needed. The `+0x70` Visible bit is strictly better.

---

## 7. Confidence ledger
| Claim | Confidence | Backing |
|---|---|---|
| Visibility cache is the flags dword at object `+0x70` | PROVEN | getter `sub_141801190` + writer `sub_1417CB5F0` both touch `+0x70` |
| `0x20`=Visible, `0x40`=Hidden, both-clear=Collapsed | PROVEN | getter decode + changed-handler `sub_1417CB5F0` branches |
| Enum numbering Collapsed=0/Hidden=1/Visible=2 (reverse of WPF) | PROVEN | AVUI::Visibility value table `0x5F76CC0` (UTF-16 names) + count=3 (`sub_141777E60`) |
| Field is on the shared `UIElement` base → same offset all pages | PROVEN-shaped | single-inheritance offset-0 (prior report) + getter takes the offset-0 `this` |
| `set_Visibility`=`sub_1415088D0`, DP global=`qword_1490A8688` (RVA 0xA8688) | PROVEN | decompile; SetValue(this, DP, box(v)) |
| `is_page_shown = (*(u32*)(page+0x70) & 0x20)` filters pooled BaseLoadingScreen | INFERRED (validate §5) | matches Collapsed-after-load semantics; 2-min live read confirms |

---

## 8. Key RVAs
- `get_Visibility` `sub_141801190` (RVA **0x1801190**) — reads `[this+0x70]`.
- `set_Visibility` `sub_1415088D0` (RVA 0x15088D0).
- OnVisibilityChanged cache-writer `sub_1417CB5F0` (RVA 0x17CB5F0) — writes `[this+0x70]` 0x20/0x40.
- Visibility DependencyProperty global `qword_1490A8688` (RVA 0xA8688).
- AVUI::Visibility value-type builder `sub_1417736F0` (RVA 0x17736F0); value table `0x5F76CC0`.
- UIPage base vtable `0x5F1F320`; CopterHud `0x6407F18`; BaseLoadingScreen `0x627BD40`.
- **Field: object `+0x70`, dword; SHOWN ⟺ bit `0x20` set.**
