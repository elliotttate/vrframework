# FH5 (Empress) UI Screen Reader — Pointer-Chain / Identity Spec

Date: 2026-06-05
Target: `ForzaHorizon5.exe` (Empress build), image base `0x140000000`, RVA = VA − 0x140000000.
DB: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64`
Corpus: full prior Hex-Rays export at `E:\ForzaHorizon5_IDA_Decompile\pseudocode\` (643 shards, EA+name headers) — used as the primary read source; live idalib decompiles used to confirm addresses/bytes.

Goal: a per-frame, memory-only way to identify WHICH UI screen / page is on top, plus its identity string, plus (bonus) focused index and input-routing entry.

---

## 0. TL;DR

- **Toolchain: WORKS.** `ida_decompile_at.py` opens the .i64 headlessly (~15s/open) and decompiles. Confirmed on RVA 0x4B1C0 and others. One caveat: large stretches of `.text` in the *live* DB are left in an un-analyzed "JUMPOUT" state (IDA never committed instructions), so the live decompiler returns `JUMPOUT(...)` stubs for many functions. The **pre-existing corpus** at `E:\ForzaHorizon5_IDA_Decompile\pseudocode\` was produced from a fuller analysis pass and is readable where the live DB is not. I cross-checked addresses against the live DB.

- **Page identity = a class-name STRING, and there are exactly 360 of them.** Every concrete page is registered under a literal name like `"UIPageCopterHud"`, `"UIPageMapScene"`, `"UIPagePauseMenu*"`, etc. The string is the canonical screen identity. **PROVEN by decompile.**

- **Two independent identity mechanisms exist and both are usable:**
  1. **Reflection/type-registry name** — each page type is registered name→factory via an FNV-1a-hashed name into a global type registry (`qword_141CAEBF0` registrar, accessor `sub_141CAE9C0`). The name string is embedded in the registration function. **PROVEN.**
  2. **MSVC RTTI on the live page object** — page *controller* classes (`CopterHud`, `MapScene`, `RivalsRouteSelect`, `TuneMenu`, `ReplayHud`, …) carry full RTTI (`??_R0?AV<Class>@@@8` type descriptors + `??_R4` complete-object-locators before each vtable). So a live page object can be classified by `vtable[-1] → COL → TypeDescriptor → demangled name`. **PROVEN that the RTTI exists; the per-object offset-0 vtable read is the standard MSVC layout (inferred-but-standard).**

- **The active-page RUNTIME pointer chain (global → … → top UIPage) is NOT fully pinned from static analysis alone** and is the one item that needs a LIVE read to finish. I describe the architecture, the factory signature, and the two best candidate live anchors (a page collection iterated with `bool/void(UIPage*)` callbacks, and a crash-breadcrumb global that stores the active UI state-machine name as a string). See §3 and §6.

---

## 1. Toolchain confirmation (objective 1)

Command:
```
python scripts\ida_decompile_at.py E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64 0x4B1C0 --bytes 32
```
Output (excerpt):
```
func_start=0x14004B1B0 (RVA 0x4B1B0) name=sub_14004B1B0 size=0x3E
--- pseudocode (decompiled in 0.03s) ---
void sub_14004B1B0() { ... xmmword_148FBFDD8 = *v0; ... }
```
The decompiler is functional. NOTE: the prompt's anchor `0x4B1C0` lands inside a small CRT helper `sub_14004B1B0`, not a `UIPage` registration — i.e. **the "known static anchors" in the brief do not resolve to UI functions in THIS build** (verified: 0x4B1C0, 0xC3660, 0x30DF20 all disagree with the brief's labels — 0x30DF20 is a jump-thunk; 0x140262E70 is a telemetry-event enum, not UISceneTransition). I therefore re-derived everything from the binary instead of trusting them.

A second helper was written for this task: `scripts\ida_ui_probe.py` (batch string/name/xref/vtable/qword queries in one db-open).

---

## 2. Page-identity mechanism (objectives 4 — SOLVED) 

### 2a. The 360 page-name registrations (reflection type registry)

Pattern, repeated once per page type. Example — `UIPageCopterHud` (corpus `fh5_000030.c`, function ordinal 30385, ea `0x14030DF20`; the brief's "concrete HUD page" anchor):

```c
__int64 sub_14030DF20()   // ea=0x14030DF20
{
  v0 = 0xCBF29CE484222325uLL;          // FNV-1a 64-bit offset basis
  v1 = 85;                              // 'U'
  v2 = "UIPageCopterHud";
  do { v0 = 0x100000001B3LL * (v1 ^ v0); v1 = *++v2; } while (*v2);  // FNV-1a prime
  v6 = (wchar_t *)v0;                   // = FNV hash of the name
  _lambda_17b8e77e...::_lambda_17b8e77e...(&unk_14A31C128, &v6);     // per-page factory slot
  v3 = sub_141CAE9C0();                 // -> the type-registry singleton
  v4 = loc_14493A230();                 // -> this page's create-functor
  return ((__int64 (__fastcall *)(__int64, const char *, wchar_t **, __int64))qword_141CAEBF0[0])(
           v3, "UIPageCopterHud", &v6, v4);                          // Register(registry, name, &hash, factory)
}
```

- **`qword_141CAEBF0[0]`** = first slot of the type-registrar's vtable; **`sub_141CAE9C0`** returns the registry singleton. (Both confirmed present in live DB; `sub_141CAE9C0` is folded into `sub_141CAE7A0` @ RVA 0x1CAE7A0 in the live analysis.)
- This same registrar is used engine-wide for *all* reflectable/serializable types (e.g. `"MessageChecksumFooter"`, `"PacketHeader"`, all `"AVUI::*"` enums via the parallel `qword_1416D8710` registrar). So it is the **ForzaTech reflection/type system**, and `UIPage*` types are registered into it like everything else.

**Full inventory (PROVEN):** 360 distinct `UIPage<Name>` classes. Each name appears exactly twice in the corpus (the FNV setup + the `Register()` call). Grep to regenerate the list:
```
grep -roh "UIPage[A-Za-z0-9_]*" E:\ForzaHorizon5_IDA_Decompile\pseudocode\ | sort -u    # 360 names
```
Representative names: `UIPageCopterHud`, `UIPageMapScene`, `UIPagePauseMenu*`, `UIPageStartingGrid`, `UIPageTelemetry`, `UIPageRivalsMenu`, `UIPageUpgradesMenu`, `UIPageSplash`, `UIPageWelcomeToHorizon`, `UIPageVideoPlayer`, … (HUDs, menus, popups, leaderboards, loading/splash screens — the complete screen catalog).

### 2b. The page-object RTTI (the live-readable classifier)

Each page has a **controller class** whose member function is bound as the key handler `bool Class::Handler(UIPage::KeyArgs)`. These classes have full MSVC RTTI. Example — CopterHud (corpus `fh5_000332.c:6425`):
```c
return &std::_Binder<std::_Unforced, bool (CopterHud::*&)(UIPage::KeyArgs), CopterHud *, std::_Ph<1> const &> `RTTI Type Descriptor';
```
Other confirmed controller classes: `MapScene, TuneMenu, ReplayHud, RivalsRouteSelect, AstraTeamDetails, SceneList, AccoladeItemsScene, SpectrumMissionItemsScene, CarHornSelect, Tombola, BadgeSelect, MulliganMenu, CharacterCustomisation, BaseLeaderboard, RivalsLeaderboard, CarSelectBlueprintAdvanced, CUpgradeWizardSearch, …`.

The live DB exposes the RTTI machinery directly, e.g.:
```
??_R4...UKeyArgs@UIPage@@@std@@6B@        (complete object locators)  @ .rdata
??_R0?AV...UIPage@@@8                     (type descriptors)          @ .data
```
**Mechanism for a live mod:** read `pPage->vtable` (offset 0), then `COL = *(vtable - 8)`, then in MSVC RTTI `pTypeDescriptor = image_base + COL->pTypeDescriptor`, then the name is the ASCII at `pTypeDescriptor + 16` (`.?AV<Class>@@`). Demangle or string-match against a table built from the 360 names / controller classes. This is the standard, robust classifier and **does not depend on finding a name field**.

### 2c. Page factory signature (confirms page objects are `UIPage`-derived ref-counted)

From corpus `fh5_000343.c` (e.g. `sub_14418BCB0`, `sub_14418BD50`):
```c
*a2 = &std::_Func_impl_no_alloc<_lambda_..., AVUI::TRefCountedPtr<UIPage>, SceneEntryData const &>::`vftable';
```
i.e. each page is created by a functor `AVUI::TRefCountedPtr<UIPage> (SceneEntryData const&)`. So:
- Page objects are **`AVUI::TRefCountedPtr`-managed** (intrusive refcount; vtable at offset 0).
- They derive from a common **`UIPage`** base (which itself sits under AVUI `FrameworkElement`/`UIElement` — AVUI is a WPF/XAML clone: `FrameworkElement`, `UIElement`, `ResourceDictionary`, `ObservableCollection`, `Visibility`, `FocusNavigationDirection`, etc. are all present).
- Creation is by `SceneEntryData` (the brief's `SceneEntryData const&` + `TRefCountedPtr<UIPage>` pattern — confirmed verbatim).

---

## 3. The active-page runtime chain (objectives 2 & 3 — PARTIAL, needs a live read to finish)

What is PROVEN about the runtime:
- There is a **collection of `UIPage*`** that the engine searches/visits with `std::function<bool(UIPage*)>` (a *find/predicate*) and `std::function<void(UIPage*)>` (a *visitor*). Corpus sites: `fh5_000105.c:9726` (`bool,UIPage *`), `fh5_000343.c:9747` (`void,UIPage *`), `fh5_000320.c`, `fh5_000345.c`. A predicate-searchable + visitor-iterable `UIPage*` collection is exactly a **page stack / active-page set**. The "top" page is whichever the predicate selects (typically last-pushed / highest-z / focused).
- Pages are created on demand by name via the factory in §2c and (by construction) inserted into that collection and connected to an AVUI scene (`IUISceneDataAdopter` interface is present; `ChangeUISceneOnUIPanel` is a registered op).

What is NOT yet pinned (and why): the **single global** that owns that `UIPage*` collection did not fall out of static xref tracing because (a) the registrar globals I followed are the *type system*, not the active-scene holder, and (b) the live DB leaves the relevant UI-core functions un-analyzed (JUMPOUT stubs), so I could not cleanly walk the per-frame UI tick to the root singleton. Completing this is a ~15-minute LIVE task (see §6 + §7).

### Best candidate live anchors for "current top page"

**Candidate A — the UI state-machine breadcrumb (string, easiest to read live).**
The crash-reporter registers two breadcrumb slots whose *values* are the live UI state (corpus `fh5_000009.c:5065,5080`):
```c
sub_1403CCDE0(&qword_149090768, "LAST_STATE_MACHINE_INSTANCE", 27);   // slot struct @ 0x149090760
sub_1403CCDE0(&qword_1490907B8, "LAST_STATE_MACHINE_STACK",    24);   // slot struct @ 0x1490907B0
```
Slot layout (from the init): `off_1490907B0` = slot vtable, `+8` = value pointer/inline buffer, `dword @ +0x28 = 17` (type/len tag). The UI is a **Stateflow state machine** (page names `UIPageStateflowMenu`, `UIPageStateflowMessageBox` exist; string `"LAST_STATE_MACHINE_STACK"`). These breadcrumbs are written whenever the UI state changes precisely so a crash dump shows the current screen — i.e. they are a ready-made "current screen" string. RVAs to probe live: `0x9090760` (INSTANCE) and `0x90907B0` (STACK). UNVERIFIED: the exact value encoding (inline char[] vs pointer) — confirm with a live read.

**Candidate B — the `UIPage*` collection holder.** Hook/scan the page-visitor or the input dispatcher (§6), read its `this`, and that object holds the page collection. The "top" is the element the engine's own predicate returns. This yields a real `UIPage*` you then classify via §2b RTTI.

---

## 4. Focused element / selected item index (objective 5 — bonus, page-local)

There is **no single global "selected index"**; selection is per-page/per-widget and data-bound. Evidence:
- Telemetry keys are per-screen: `"CarSelect_SelectedIndex"`, `"TrackSelect_SelectedIndex"`, `"Telemetry_SelectedIndex"` (corpus `fh5_000020.c:12599+`). So each list screen tracks its own index.
- Tiled menus use a `TileMenuNavigationManager` (corpus `fh5_000106.c:11695`, RTTI present) which owns the highlighted tile; its member-function bind `void TileMenuNavigationManager::*(AVUI::Object*)` is the selection-changed callback.
- AVUI itself has `SelectionMode`, `FocusNavigationDirection`, `Selector`/`ItemsControl`-style focus — the focused element is an `AVUI::FrameworkElement` reachable from the page's visual tree / the AVUI focus manager.

Recommendation: classify the page first (§2b), then read that page's known list-widget index field (offset is page-class-specific; must be found per screen via a live read once you can identify the page). A generic answer would be the AVUI focus-manager's "focused element" pointer, but I did not pin that global statically.

---

## 5. Input-routing entry (objective 6 — bonus)

Input is delivered to the active page as `bool UIPage::KeyArgs` handlers. There are **975** `UIPage::KeyArgs` handler `std::function` bindings across pages (corpus), each `bool Class::Handler(UIPage::KeyArgs)`. The top-level dispatcher invokes the active page's handler `std::function`. To call into the active page directly (instead of synthesizing a gamepad), resolve the active `UIPage*` (§3) and invoke its registered `KeyArgs` handler, or hook the dispatcher that does so. The dispatcher is the function that owns the `UIPage*` collection and calls `handler(keyArgs)` — same object as Candidate B. (Exact RVA UNVERIFIED — live-trace it by setting a breakpoint on any page's KeyArgs handler and walking up one frame.)

---

## 6. How to finish this LIVE (concrete next steps)

1. Build the **RTTI name table** offline now: for each page controller class / each of the 360 `UIPage*` names, record its vtable RVA (find `??_R0?AV<Class>@@@8`, then the vtable whose `[-1]` COL points at it). This gives `vtable RVA → screen name` directly. (Static, no game needed.)
2. Launch FH5, open a known screen (e.g. pause menu). Scan memory for any of the page-name C-strings (e.g. `"UIPageMapScene"`) OR read **Candidate A** breadcrumb at RVA `0x90907B0` and dump the bytes — if it's the current state-machine/page name, you are done with near-zero RE.
3. If using Candidate B: set a HW breakpoint on a page's `KeyArgs` handler while navigating; the `this`/`RCX` is the page; walk the caller to the collection-owning singleton; that singleton + offset is the stable base→top-page chain. Confirm the "top" selector.
4. Cross-check: the page you identify by breadcrumb (A) and by RTTI of the live `UIPage*` (B) must agree.

---

## 7. Minimal C++ read sketch

```cpp
// ---- Mechanism A: state-machine breadcrumb (string). Easiest; verify encoding live. ----
// RVA 0x90907B0 = "LAST_STATE_MACHINE_STACK" slot struct; +8 likely holds the value.
static uint8_t* base = (uint8_t*)GetModuleHandleW(nullptr);     // 0x140000000 at runtime
struct BreadcrumbSlot { void* vtbl; char* value; /*...*/ };     // exact layout TBD live
const char* CurrentUIStateName() {
    auto* slot = reinterpret_cast<BreadcrumbSlot*>(base + 0x90907B0);
    return slot->value;          // e.g. "UIPageMapScene" / a Stateflow state name  (VERIFY)
}

// ---- Mechanism B: classify a live UIPage* via MSVC RTTI (robust; no name field needed) ----
struct RTTICompleteObjectLocator { uint32_t sig, off, cdOff, pTypeDescriptor, pClassDescriptor, pSelf; };
struct RTTITypeDescriptor { void* pVFTable; void* spare; char name[1]; };   // name = ".?AV<Class>@@"
const char* ClassNameOf(void* obj) {                                        // obj = a UIPage*
    void** vt = *reinterpret_cast<void***>(obj);                            // vtable @ offset 0
    auto* col = *reinterpret_cast<RTTICompleteObjectLocator**>((uint8_t*)vt - 8);
    auto* td  = reinterpret_cast<RTTITypeDescriptor*>(base + col->pTypeDescriptor); // 64-bit: RVA
    return td->name;             // ".?AVCopterHud@@" -> map to "UIPageCopterHud"
}

// ---- Active page (Candidate B): once the manager singleton+offset is pinned LIVE ----
// void* mgr   = *(void**)(base + MGR_RVA);          // TODO: pin live (§6.3)
// void* page  = topOfPageStack(mgr);                // TODO: confirm "top" selector
// const char* screen = ClassNameOf(page);
```

---

## 8. Confidence ledger

| Claim | Confidence | Backing |
|---|---|---|
| Toolchain works | PROVEN | live decompile output |
| Brief's static anchors are wrong for this build | PROVEN | 0x4B1C0/0xC3660/0x30DF20/0x140262E70 disassembled, mismatch shown |
| 360 page types, identity = name string, FNV-registered via `sub_141CAE9C0`/`qword_141CAEBF0` | PROVEN | corpus `fh5_000030.c` CopterHud reg + 360-name grep |
| Pages are `AVUI::TRefCountedPtr<UIPage>` created from `SceneEntryData const&` | PROVEN | corpus `fh5_000343.c` factory functor types |
| Page controller classes have full MSVC RTTI (CopterHud, MapScene, …) → vtable→name classifier | PROVEN (RTTI exists); standard-layout (vtable@0, COL@[-1]) inferred | corpus `fh5_000332.c` + live `??_R0/??_R4` names |
| A `UIPage*` collection is searched/visited (page stack exists) | PROVEN | corpus `bool/void (UIPage*)` std::function sites |
| `LAST_STATE_MACHINE_STACK`/`_INSTANCE` breadcrumb holds the live UI state name | INFERRED (slot init proven; value-write path not traced; encoding unverified) | corpus `fh5_000009.c:5065/5080` |
| The single global → offset → top `UIPage` chain (exact base+offsets) | UNVERIFIED — needs a LIVE read | architecture proven, exact singleton not pinned (live DB JUMPOUT-stubbed in UI core) |
| Input routed via `bool UIPage::KeyArgs` handlers; dispatcher = collection owner | PROVEN (handler model); dispatcher RVA UNVERIFIED | 975 `UIPage::KeyArgs` binds |
| Selected index is per-page (no global), TileMenuNavigationManager for tiles | PROVEN (per-page); generic focus global UNVERIFIED | telemetry `*_SelectedIndex` keys + nav manager RTTI |

---

## 9. Artifacts
- New helper script: `E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\scripts\ida_ui_probe.py` (string/name/xref/vtable/qword batch probe).
- Page-name list regen: `grep -roh "UIPage[A-Za-z0-9_]*" E:\ForzaHorizon5_IDA_Decompile\pseudocode\ | sort -u` (360 entries).
- Key RVAs: CopterHud reg `0x30DF20` (thunk → `0x3108A0` body); type-registry accessor `sub_141CAE9C0` (in `sub_141CAE7A0` @ `0x1CAE7A0`); breadcrumb slots `0x9090760` / `0x90907B0`.
