# FH5 (Empress) UI — RTTI Vtable→Screen Classifier + Active-Page Capture Spec

Date: 2026-06-05
Target: `ForzaHorizon5.exe` (Empress build), image base `0x140000000`, **RVA = VA − 0x140000000**.
DB: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64` (IDA 9.0 idalib).
Corpus: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\` (643 shards) — primary read source.
Prereq read: `fh5_ui_screen_reader_spec_20260605.md` (proved page identity = one of 360 `UIPage<Name>`, controllers carry RTTI, factory = `TRefCountedPtr<UIPage>(SceneEntryData const&)`).

Companion machine-readable table: **`fh5_ui_vtable_rtti_table_20260605.tsv`** (405 rows: `friendly  mangled  vtable_rva  subobj_off  col_rva  td_rva  vt0_func_rva`).

---

## 0. TL;DR

- **Deliverable 1 (DONE, PROVEN).** Resolved the full MSVC RTTI chain for **405 of 452** UI page-controller classes → a unique **offset-0 vtable RVA** each. This is the runtime classifier: `*(void**)page` = vtable VA → `vtable_RVA = vtableVA − liveBase` → table lookup → screen name. Chain verified end-to-end on CopterHud. All gameplay-priority screens resolved (HUD, pause, map, loading/splash, replay, photo, front-end menus).
- **Key architecture fact (PROVEN):** the registration name is `UIPage<Name>` but the *runtime object's RTTI class is the controller* (e.g. registration `UIPageCopterHud` → factory `TRefCountedPtr<CopterHud>` → live object RTTI `.?AVCopterHud@@`). **451** registration names have a dedicated controller class (→ unique vtable); the remaining `UIPage<Name>` names with **no** dedicated controller are built as the generic base and ALL share vtable `0x5F1F320` (`.?AVUIPage@@`) — those are **not** RTTI-distinguishable from each other (e.g. `UIPageWelcomeToHorizon` has no controller).
- **Deliverable 2 (PARTIAL, as far as static allows).**
  - The active-page global→offset chain is **still not statically pinnable** — confirmed independently: the UI-core functions (`sub_141CAE7A0` registry, the page-collection iterators) are **JUMPOUT-stubbed** in the live DB, and the registrar globals (`qword_141CAEBF0`, `sub_141CAE9C0`) have **zero xrefs** in the analyzed DB. This matches the prior agent.
  - **Breadcrumb slots solved & explained.** `0x9090760` (`LAST_STATE_MACHINE_INSTANCE`) and `0x90907B0` (`LAST_STATE_MACHINE_STACK`) are **`std::string` members**. The writer is the generic `std::string::assign` `sub_1403CCDE0(&slot, valuePtr, len)`. They are **zero in free-roam by design**: they are only assigned by Stateflow state-machine push/pop, which does not run in gameplay free-roam — so they are **NOT** a reliable free-roam HUD anchor (good for menus only).
  - **Best hook recommendation (PROVEN-shaped, no global needed):** detour the **page factory `operator()`** (`TRefCountedPtr<X>(SceneEntryData const&)`), or—better for a single chokepoint—the **common page constructor / first-virtual on the new object**, to capture the fresh `UIPage*` and RTTI-classify it on every page open. Exact per-frame dispatcher RVA could not be pinned statically (stubbed); a 5-min live breakpoint walk finishes it (§5).

---

## 1. Toolchain confirmation

`python scripts\ida_decompile_at.py <i64> 0x4B1C0 --bytes 16 --no-pseudo` → `func_start=0x14004B1B0 ... [open 14.4s]`. Works. New scripts added (read-only DB use):
- `scripts\ida_rtti_vtable_map3.py` — the RTTI→vtable resolver used for Deliverable 1.
- (`ida_rtti_vtable_map.py`, `_map2.py` are earlier iterations; map3 is the correct one.)

---

## 2. Deliverable 1 — RTTI vtable-RVA → screen-name table

### 2a. The resolution chain (MSVC x64), and how each link was confirmed

Type descriptors in this DB are **NOT** IDA-named `??_R0...` (a `name:CopterHud` query returns nothing), so the standard "find `??_R0` by name" fails. Resolved by structure instead:

```
name string ".?AV<Class>@@"  @ .data (0x14806E000..0x149333000)
    -> TypeDescriptor (TD) struct = nameStringEA - 16     (TD_RVA)
    -> COL (??_R4): a .rdata record with  dword@+0 == 1 (x64 sig),
                    dword@+0x14 == self_RVA (== col_RVA),  dword@+0x0C == TD_RVA,
                    dword@+0x04 == subobject offset  (we keep offset==0 = primary base)
    -> vtable:  the .rdata qword slot whose value == COL_VA  is  vtable[-1];
                vtable = thatSlot + 8 ;  require vt[0] ∈ .text
    -> vtable_RVA = vtableVA - 0x140000000
```

**End-to-end verification — CopterHud (PROVEN by raw bytes):**
- TD name string `.?AVCopterHud@@` @ `0x148BC4328` → TD struct @ `0x148BC4318` (TD_RVA `0x8BC4318`).
- COL @ `0x147028A10`, bytes `01 00000000 00000000 18 43 BC 08 | 38 8A 02 07 | 10 8A 02 07` ⇒ sig=1, off=0, cdOff=0, **TD_RVA=0x08BC4318** (matches), self_RVA=`0x07028A10` (matches). ✔
- `qword@0x146407F10 == 0x147028A10` (the COL) ⇒ `0x146407F10` is **vtable[-1]**. ✔
- **vtable @ `0x146407F18` (RVA `0x6407F18`)**, `vt[0]=sub_143A8CD80` (∈ .text). ✔

> Note on the prompt's "vtable immediately FOLLOWS the COL": in this build the COL and the vtable are in **separate `.rdata` regions** (COLs cluster ~`0x69xxxxx–0x70xxxxx`, vtables ~`0x5Fxxxxx–0x64xxxxx`). The reliable adjacency is **`vtable[-1] == &COL`** (the qword right before vtable[0]), not COL-then-vtable contiguity. The mod doesn't need adjacency anyway — it goes `vtable → vtable[-1] → COL → COL+0x0C (TD_RVA) → TD+16 (name)`, OR just `vtable_RVA → static table lookup` (preferred; no per-frame RTTI walk).

### 2b. Coverage

- **405 / 452** controller classes resolved to a unique **offset-0** vtable (one row each — no duplicates).
- **47 misses**, all `no-TD-string` (the class has no MSVC TD in `.data` — typically thin wrappers/popups e.g. `HideSeekMenu`, `LabyrinthSelectMenu`, `MapLabyrinth`, `OptionsPopup`, `PostRaceMedals`, `RareCarDealershipLanding`, `SpectrumMissionItemsScene`, `UserContentPropPrefabPopup`…). These have no per-class RTTI; a live object of one would classify as its nearest RTTI'd base (often `UIPage`/`PopupBase`/`MapSceneBase`). Listed at the bottom of the TSV is implicit (they are simply absent).

### 2c. Top priority screens (vtable_RVA → name)

| vtable_RVA | friendly (RTTI class) | mangled | registration name(s) it backs |
|---|---|---|---|
| `0x6407F18` | **CopterHud** | `.?AVCopterHud@@` | `UIPageCopterHud` (free-roam/drive HUD) |
| `0x6321AF8` | **Hud** | `.?AVHud@@` | base race HUD |
| `0x6322558` | **LimitedHud** | `.?AVLimitedHud@@` | cutscene/limited HUD |
| `0x63C6710` | **PauseMenuTiled** | `.?AVPauseMenuTiled@@` | pause menu |
| `0x6424280` | **MapScene** | `.?AVMapScene@@` | map |
| `0x62D39B8` | **MapSceneInteractive** | `.?AVMapSceneInteractive@@` | interactive map |
| `0x6421110` | **MapSceneBase** | `.?AVMapSceneBase@@` | map base |
| `0x6447450` | **Splash** | `.?AVSplash@@` | `UIPageSplash` |
| `0x6432520` | **SplashE3** | `.?AVSplashE3@@` | E3 splash |
| `0x6444E98` | **Loading** | `.?AVLoading@@` | loading screen |
| `0x6430428` | **FMVLoading** | `.?AVFMVLoading@@` | FMV loading |
| `0x627BD40` | **BaseLoadingScreen** | `.?AVBaseLoadingScreen@@` | loading base |
| `0x63DF810` | **ReplayHud** | `.?AVReplayHud@@` | replay HUD |
| `0x6446760` | **PhotoModeScene** | `.?AVPhotoModeScene@@` | photo mode |
| `0x62D3020` | **GenericMenu** | `.?AVGenericMenu@@` | generic front-end menu |
| `0x5F1F320` | **UIPage** (base) | `.?AVUIPage@@` | ALL controller-less `UIPage<Name>` (e.g. `UIPageWelcomeToHorizon`) |

Other commonly-wanted rows (full set in the TSV): `OptionsMenu 0x630D240`, `DestinationMenu 0x642F750`, `TuneMenu 0x6419D18`, `UpgradesMenu 0x641C5C0`, `RivalsMenu 0x6295268`, `StartingGrid 0x63EB468`, `PostRaceFinishPosition 0x633F060`, `CarSelectGarage 0x63E4C48`, `InGameAnnouncement 0x62E2708`, `PropPlacementHud 0x63DEA70`, `RouteBlueprintHud 0x63E0200`, `MyHorizonLife 0x63964B8`, `FestivalPassScene 0x63871E0`.

### 2d. Controller-class vs `UIPage<Name>` relationship (PROVEN)

For each of the 360 registrations `sub_…()` does: `FNV1a(name) → register(registry, "UIPage<Name>", &hash, factory)`. The `factory`'s `operator()` returns `AVUI::TRefCountedPtr<Controller>` where:
- registration `UIPageCopterHud` → `_lambda_…,AVUI::TRefCountedPtr<CopterHud>,SceneEntryData const &` (corpus `fh5_000332.c:5118`). So `UIPageCopterHud` ⇒ runtime `CopterHud`.
- **451** distinct controller factory lambdas exist (corpus count); **1345** functor instances use the generic `TRefCountedPtr<UIPage>` (controller-less pages). So map a *runtime object's* RTTI class to a friendly screen name with the TSV; for controller-less pages you only get `UIPage` and must disambiguate another way (the page's bound `name`/SceneEntryData, or the breadcrumb when in a menu).

---

## 3. Deliverable 2 — active-page capture point

### 3a. What is PROVEN about the runtime page model
- A `UIPage*` **collection** is searched with `std::function<bool(UIPage*)>` (predicate `_lambda_589e9d713e8db3d541b10b955f819ec9_`, corpus `fh5_000105.c:9726`/`:10917`) and visited with `std::function<void(UIPage*)>` (`_lambda_5fa8144bee9f613db163f074ede7dd5b_`, `fh5_000343.c:9747`/`:16085`; a second visitor `_lambda_75b80ec8…` in `fh5_000345.c`). Only **3** distinct page-collection iterator lambdas exist → a single page-stack/active-set is iterated from a few places.
- Each page binds its input handler as `std::function<bool(UIPage::KeyArgs)>` via `std::_Binder<…,bool(Controller::*&)(UIPage::KeyArgs),Controller*,_Ph<1>>` — the bound `Controller*` **is the page object**. CopterHud's binder: corpus `fh5_000332.c:158`. ~975 such binds across pages. The top-level dispatcher invokes the active page's handler `std::function`.
- Pages connect to an AVUI UIScene that implements `AVUI::IUISceneDataAdopter` at object **offset 0x430** (`a1[134]`), object size ~`1136`B (corpus `fh5_000293.c:14475`). This scene object owns the page; it is the would-be "collection holder."

### 3b. Why the global → top-page chain is NOT pinned (evidence)
- `sub_141CAE7A0` (the folded registry accessor that `sub_141CAE9C0` belongs to) decompiles to `JUMPOUT(0x141CAE7A5)` — un-analyzed in the live DB. `--repair` converts it but it's a large multi-routine blob, not a clean singleton getter.
- `dxref:0x141CAE9C0` and `dxref:0x141CAEBF0` → **(none)**. The registrar globals are the *type system*, not the active-scene holder, and carry no tracked xrefs to walk.
- The page-collection iterators (the functor `operator()` bodies) live in the stubbed `.text`; their callers (the per-frame UI tick → active page) are not recoverable purely statically here.

Conclusion: a **stable global→offset chain to the live active `UIPage*` is not deliverable from static analysis of this DB**. It needs one short live step (§5).

### 3c. Breadcrumb writer — fully characterized (answers the live-zero finding)
- Init (corpus `fh5_000009.c:5063/5078`): `sub_1403CCDE0(&slotString, "LAST_STATE_MACHINE_INSTANCE"/"…STACK", len)` registers the slot **name**. Slot struct: `0x149090760` (INSTANCE) / `0x1490907B0` (STACK); `+0` vtable `&off_1467F41D0`, the **value is a `std::string`**, `dword +0x28 == 17` is a type tag.
- The **writer is `sub_1403CCDE0` @ RVA 0x3CCDE0** — decompiled, it is `std::string::assign(dst, srcPtr, len)` (grows/copies into the slot's std::string; corpus `fh5_000035.c:8486`). It is the *generic* crash-breadcrumb setter used hundreds of times (audio, weather, "Runtime.Core", etc.). The value placed in the two UI slots is **the current Stateflow state-machine instance/stack name** (string), assigned only on state push/pop.
- **Source of the stored string:** a Stateflow state name (the UI is a state machine — registration names `UIPageStateflowMenu`, `UIPageStateflowMessageBox`, string `"LAST_STATE_MACHINE_STACK"`). It is **not** the page RTTI name and **not** populated in free-roam (no menu SM active) — which is exactly the live observation. **Verdict: usable only as a menu-state hint, not a per-frame page anchor.**

### 3d. Recommended hook(s) for the mod (in priority order)

1. **PREFERRED — page-creation chokepoint (provable, low-rate, gives a real `UIPage*`).** Detour the page factory `operator()` (signature `TRefCountedPtr<Controller>(this_functor, SceneEntryData const&)`, x64 fastcall, returns the new ref-counted page in RAX / via the sret/`TRefCountedPtr` out-ptr). On each call the mod gets the freshly-constructed page object → run §2a RTTI classify → cache as "last opened page." Fires on every screen open (HUD, menu, loading, popup). The functor invoke addresses are reachable live by reading the `_Func_impl` vtable's invoke slot of any factory functor; statically the functor vtables are at the `fh5_000343/344/332` `_Func_impl_no_alloc<…TRefCountedPtr<X>,SceneEntryData const &>::\`vftable'` sites.
2. **ALT — the input dispatcher (true "active page each frame").** Hook the function that invokes the active page's `bool(UIPage::KeyArgs)` handler. The bound `this` (the page) is the `std::function`'s captured object. RVA not pinned statically (stubbed); pin live in §5. When found, RCX/`this+0x?` at the call is the active `UIPage*`.
3. **ALT — menu-only state string.** Read `std::string` at `0x90907B0` (STACK) / `0x9090760` (INSTANCE) for the current menu state name. Zero in free-roam. Cheap, but partial.

For all three, the classification step is identical and is the load-bearing deliverable:
```cpp
const char* ClassNameOf(void* page) {            // page = UIPage* (offset-0 vtable)
    auto base = (uint8_t*)GetModuleHandleW(nullptr);            // 0x140000000 live
    void** vt = *(void***)page;
    uint64_t vt_rva = (uint8_t*)vt - base;
    return LookupScreenName(vt_rva);             // static table from the TSV (Deliverable 1)
    // (fallback RTTI walk if vt_rva unknown:)
    // auto col = *(uint8_t**)((uint8_t*)vt - 8);
    // uint32_t tdRva = *(uint32_t*)(col + 0x0C);
    // return (const char*)(base + tdRva + 16);   // ".?AV<Class>@@"
}
```

---

## 4. Confidence ledger

| Claim | Confidence | Backing |
|---|---|---|
| Toolchain works (idalib decompile, ~14s open) | PROVEN | live output |
| RTTI chain `name→TD→COL→vtable[-1]→vtable` resolves; 405 classes → unique offset-0 vtable RVA | PROVEN | bytes-verified on CopterHud; structural scan, 1 row/class, all vt[0]∈.text |
| `UIPageCopterHud` registration ⇒ runtime object is `CopterHud` (RTTI `.?AVCopterHud@@`) | PROVEN | corpus factory `TRefCountedPtr<CopterHud>` `fh5_000332.c:5118` |
| Controller-less `UIPage<Name>` (e.g. WelcomeToHorizon) all share vtable `0x5F1F320` | PROVEN | 1345 generic `TRefCountedPtr<UIPage>` factory instances; no per-name TD |
| Priority screens (HUD/pause/map/splash/loading/replay/photo/front-end) all resolved | PROVEN | §2c table |
| 47 classes have no MSVC TD (not RTTI-classifiable per-class) | PROVEN | `no-TD-string` misses |
| Active-page global→offset chain not statically pinnable | PROVEN (negative) | registry stubbed + zero xrefs on registrar globals |
| Breadcrumb slots are `std::string`; writer = `sub_1403CCDE0` (= `string::assign`); value = Stateflow SM name; zero in free-roam | PROVEN (slots+writer) / INFERRED (exact SM-name source) | corpus `fh5_000009.c`+`fh5_000035.c`; matches live zero-read |
| A `UIPage*` collection is searched/visited (single page stack), 3 iterator lambdas | PROVEN | corpus bool/void `(UIPage*)` functor sites |
| Page factory `operator()` is a clean per-creation hook returning the live page | INFERRED (signature proven; exact invoke RVA via functor vtable, do live) | factory functor types in corpus |
| Per-frame input-dispatcher RVA + arg holding active `UIPage*` | NEEDS-LIVE-READ | dispatcher is in stubbed `.text`; §5 procedure |

---

## 5. Finish-live procedure (≈5–10 min, pins the dispatcher/global)

1. Inject; pick a known screen (pause menu). Read `std::string` at `0x90907B0` → confirm it holds the menu state name (validates breadcrumb encoding + that you're synced to live base).
2. Place a HW-exec BP on any resolved handler, easiest **CopterHud vt[0] `0x3A8CD80`** or its KeyArgs handler. When it fires in free-roam, **RCX = the live `CopterHud*`**; confirm `*(void**)RCX` == live(`0x6407F18`). This proves the classifier on a real object with zero further RE.
3. Walk up 1–2 frames from the KeyArgs handler to the dispatcher that pulled the page out of the collection; record `[singleton + offset]` that yielded the page → that is the stable global→active-page chain. Cross-check it classifies to the same name as step 2.

---

## 6. Artifacts
- **`fh5_ui_vtable_rtti_table_20260605.tsv`** — 405-row classifier (the Deliverable-1 lookup the mod loads). Columns: `friendly  mangled  vtable_rva  subobj_off(=0)  col_rva  td_rva  vt0_func_rva`.
- `scripts\ida_rtti_vtable_map3.py` — regenerates the table from the .i64 in one db-open (~158s): `python ida_rtti_vtable_map3.py <i64> --filter _controller_classes.txt --out out.tsv`.
- `scripts\_controller_classes.txt` — the 452 controller class names (from corpus factory functors).
- Key RVAs: CopterHud vtable `0x6407F18` (COL `0x7028A10`, TD `0x8BC4318`); UIPage base vtable `0x5F1F320`; breadcrumb std::strings `0x9090760`/`0x90907B0`; breadcrumb setter `sub_1403CCDE0` @ `0x3CCDE0`.
