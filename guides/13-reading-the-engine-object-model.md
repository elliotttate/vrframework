# 13 — Reading the Engine: Three Ways to Find the Player & Camera

## What this covers / why it matters

Every VR mod has to read and write *live engine state* it doesn't own: the camera transform, the player position, the projection matrix, the FOV, the active render context. You did not write this engine, you have no headers for it, and the symbols are stripped. The entire VR project hinges on one question: **given a running game, how do you get a typed pointer to the camera and change it before the frame is submitted?**

There is no single answer — there are three, and they form an escalating ladder of effort and fragility. The luckiest case is an engine that ships its own reflection database, where you can ask for `app.Camera` by name and the game hands it to you (Capcom RE Engine, via REFramework). The middle case is an engine with C++ RTTI and a community address library, where you match vtables and hand-write structs (Bethesda Creation Engine 2, via starfield2vr). The hardest case is a stripped engine with nothing — you pattern-scan a function, hook it, and read its arguments as hand-reconstructed structs (Ubisoft Anvil, via anvilengine2vr).

This guide teaches all three, grounded in the three real codebases, and ends with a method you can follow on a brand-new engine.

---

## The ladder, at a glance

| Strategy | Engine / project | What the engine gives you | What you hand-write | Fragility |
|---|---|---|---|---|
| **1. Full reflection** | RE Engine / REFramework | A complete runtime type DB: every type, field, method, by name | Almost nothing — query by string | Lowest. Survives patches; field *names* are stable |
| **2. RTTI + address lib + reclass** | Creation Engine 2 / starfield2vr | C++ RTTI vtables, MSVC mangled class names | Struct layouts (`NiCamera`, `PlayerCamera`) + an address library mapping IDs → offsets | Medium. Vtable names stable, struct offsets break on update |
| **3. Pure reclass / pattern-scan** | Anvil / anvilengine2vr | Nothing. Stripped binary | Everything: byte patterns to find functions, struct layouts for their arguments | Highest. Every offset and pattern is per-build |

The rule of thumb: **use the most engine-provided mechanism available.** Reflection beats RTTI beats raw scanning, because each step down means more hand-maintained data that breaks every time the game updates. You rarely get to choose — the engine decides for you — but knowing where you are on the ladder tells you how much of the work is reverse engineering versus plumbing.

---

## Strategy 1 — Full reflection (RE Engine)

### The principle

Some engines carry a **runtime type database**: a serialized description of every class, its fields (name, type, offset), and its methods (name, signature, function pointer). Managed/scripted engines need this for their own VM. The RE Engine has `via.clr.VM` — a CLR-like managed runtime — and a type DB (`RETypeDB`) that REFramework parses directly.

When this exists, reverse engineering nearly vanishes. You don't hunt for the camera's offset; you ask the database *"what is the field `Camera` on type `via.SceneView`?"* and it tells you the offset, the type, and how to read it. Field **names** are far more stable across patches than field **offsets**, so reflection-based mods routinely survive game updates that shatter offset-based ones.

### How REFramework wires into the DB

REFramework's SDK is essentially a typed wrapper around the engine's own reflection. The header tree is rooted at `ReClass.hpp`, which pulls in the whole type system:

```cpp
// REFramework/shared/sdk/ReClass.hpp:61
#include "RETypes.hpp"
#include "REType.hpp"
#include "RETypeLayouts.hpp"
#include "RETypeCLR.hpp"
#include "RETypeDB.hpp"
#include "RETypeDefinition.hpp"
```

`RETypeDB.hpp` opens with the operations you'll actually call — the entire vocabulary of "find things by name at runtime":

```cpp
// REFramework/shared/sdk/RETypeDB.hpp:41
sdk::RETypeDefinition* find_type_definition(std::string_view type_name);
sdk::REMethodDefinition* find_method_definition(std::string_view type_name, std::string_view method_name);
// ...
template <typename T = void>
T* get_native_singleton(std::string_view type_name);   // :69
template<typename T>
T* get_managed_singleton();                            // :73
```

That's the heart of it. Three things you can do once the DB is parsed:

1. **`find_type_definition("via.Transform")`** — resolve a class by its fully-qualified name.
2. **`get_managed_singleton(...)` / `get_native_singleton(...)`** — grab an engine singleton (a manager, the scene, the camera system) without knowing where it lives in memory.
3. Off a type or object, **get fields and call methods by name**.

The header even shows the parsed DB header it walks — magic, version, and the counts of every table:

```cpp
// REFramework/shared/sdk/RETypeDB.hpp:106
struct TDB {
    uint32_t magic;
    uint32_t version;
    uint32_t numTypes;
    // ...
    uint32_t numMethods;
    uint32_t numFields;
    uint32_t numProperties;
```

### Reading and calling on a live object

Once you hold a `REManagedObject*`, you read fields and invoke methods reflectively. The reflective property getter doesn't dereference a hard-coded offset — it looks up a `VariableDescriptor` and calls the engine's own get-function:

```cpp
// REFramework/shared/sdk/REManagedObject.hpp:47
template <typename T>
T REManagedObject::get_reflection_property(VariableDescriptor* desc) const {
    T data{};
    auto get_value_func = (void* (*)(VariableDescriptor*, ::REManagedObject*, void*))desc->get_function();
    if (get_value_func != nullptr) {
        get_value_func(desc, const_cast<::REManagedObject*>(this), &data);
    }
    return data;
}

// :65 — the convenience overload you actually call:
template <typename T>
T REManagedObject::get_reflection_property(std::string_view field) const {
    return get_reflection_property<T>(get_field_desc(field));
}
```

Method calls work the same way — resolve a `FunctionDescriptor` by name, then thunk through the VM's invoke path with a thread context:

```cpp
// REFramework/shared/sdk/REManagedObject.hpp:69
template <typename Arg>
std::unique_ptr<REManagedObject::ParamWrapper> REManagedObject::call_method(FunctionDescriptor* desc, const Arg& arg) {
    auto method_func = (ParamWrapper * (*)(MethodParams*, ::REThreadContext*)) desc->get_functionPtr();
    if (method_func != nullptr) {
        auto params = std::make_unique<ParamWrapper>(this);
        params->params.in_data = (void***)&arg;
        method_func(&params->params, sdk::get_thread_context());   // VM context, see REContext.hpp
        return std::move(params);
    }
    return nullptr;
}
```

The `get_thread_context()` here is the managed VM's thread context (`sdk::VMContext`, declared in `REContext.hpp:86`) — calling into the engine's CLR requires its context the same way calling a real CLR method does.

### What it looks like in practice

REFramework's own mods read like scripting against the engine. From `FirstPerson.cpp`, finding types purely by string:

```cpp
// REFramework/src/mods/FirstPerson.cpp:577
static auto via_render_mesh = sdk::find_type_definition("via.render.Mesh");
// :619
static auto via_transform   = sdk::find_type_definition("via.Transform");
```

And grabbing a gameplay singleton with no offset anywhere in sight:

```cpp
// REFramework/src/mods/FreeCam.cpp:365
const auto character_manager =
    sdk::get_managed_singleton<::REManagedObject>(game_namespace("CharacterManager"));
```

Note `game_namespace(...)` — the *same* mod compiles against RE2, RE3, RE4, MH, etc., because the type names differ only by a namespace prefix. That is the payoff of reflection: one codebase, many games, minimal per-game data.

### When to use it

- The engine has a managed runtime / reflection DB (Mono, IL2CPP, RE Engine's CLR, Unreal's UObject reflection, etc.).
- You want a mod that survives patches and ports across related titles.
- **Cost:** you must first reverse the *shape of the DB itself* — once. REFramework's `RETypeDB.hpp` is hundreds of lines of struct layouts for the database, and it carries per-version variants (`tdb84`, TDB67/70 splits visible in `ReClass.hpp:36-54`). But after that one-time investment, everything downstream is by-name and cheap.

---

## Strategy 2 — RTTI + address library + reclass (Creation Engine 2)

### The principle

Most native C++ game engines don't ship a managed reflection DB, but a great many are compiled with **MSVC RTTI enabled**. RTTI leaves behind, for every polymorphic class, a "type descriptor" containing the **mangled class name** as an ASCII string in the binary — strings like `.?AVNiCamera@@`. Those names are stable across builds even when offsets move. If you can find an object's vtable, you can walk back to its RTTI and confirm *"yes, this really is an `NiCamera`."*

This is the strategy starfield2vr uses for Bethesda's Creation Engine 2. It rests on three legs:

1. **An address library** (CommonLibSF) — a community-maintained table mapping stable numeric *IDs* to per-build *offsets*. When the game updates, only the table is regenerated; your code keeps referring to the same IDs.
2. **RTTI vtable matching** — locate singletons/objects by their mangled RTTI name instead of a raw address.
3. **Hand-written "reclass" structs** (the `sdk-lite` headers) — you transcribe each class's memory layout into a C++ struct so member access is just `camera->worldToCam`.

### Reclass structs: the heart of it

A reclass struct is a hand-built mirror of the engine's real class. You discover offsets in a reverse-engineering tool (ReClass.NET, IDA, Ghidra) and bake them in, padding the gaps you don't understand. starfield2vr's `NiCamera` is a textbook example:

```cpp
// starfield2vr/sdk-lite/include/RE/N/NiCamera.h:160
uint32_t      cameraHandleID{ 0xFFFFFF };  // 130
uint8_t       pad_0133[12];                // 134
float         offsetMatrix[4][4];          // 140 diagonal 1.0f
float         worldToCam[4][4];            // 180 last column changes when moving
NiFrustum     viewFrustum;                 // 0x1C0
uint8_t       clipSpaceType;               // 0 - perspective
float         minNearPlaneDist;            // 1DC
// ...
```

Three things to internalize from this snippet:

- **The trailing comments are offsets.** `// 180` means "this member lives at byte 0x180 of the object." That is the reverse-engineering work, frozen into a header.
- **`pad_XXXX[]` arrays** fill regions you haven't decoded. You don't need to understand every byte — only the ones you touch (here, `worldToCam` and `viewFrustum`).
- **Static asserts pin the layout.** The file ends with a wall of guards that fail to *compile* if the struct drifts:

```cpp
// starfield2vr/sdk-lite/include/RE/N/NiCamera.h:174
static_assert(offsetof(NiCamera, cameraHandleID) == 0x130);
static_assert(offsetof(NiCamera, offsetMatrix)   == 0x140);
static_assert(offsetof(NiCamera, viewFrustum)    == 0x1C0);
static_assert(sizeof(NiCamera)                   == 0x220);
static_assert(offsetof(NiCamera, worldToCam)     == 384);   // 0x180
```

These are not decoration — they are your safety net. If a refactor of the struct nudges a member, the build breaks at the assert instead of silently reading garbage at runtime and crashing the game.

The same pattern in `PlayerCamera.h` shows how you decode a class incrementally. Bethesda's camera state machine is fully mapped (every `TESCameraState*` pointer), but the float block is half-named: members you understood get real names (`fov`, `fovAdj`, `position[3]`, `horizontal_rotation`), and the rest keep their reverse-tool auto-names (`N00000A89`, `N000012F9`):

```cpp
// starfield2vr/sdk-lite/include/RE/P/PlayerCamera.h:85
float  N00000A89;            // 0x0258
float  N000012F9;            // 0x025C
float  fov;                  // 0x0260
// ...
float  position[3];          // 0x029C
float  horizontal_rotation;  // 0x02A8
// ...
static_assert(offsetof(PlayerCamera, fov)                 == 0x280);  // :128
static_assert(offsetof(PlayerCamera, horizontal_rotation) == 0x2C8);  // :127
static_assert(sizeof(PlayerCamera)                        == 0x2F0);
```

> Aside: notice the asserts say `fov == 0x280` while the comment on the member says `0x260`. That kind of comment/assert mismatch is *exactly* why the asserts exist — the compiler trusts `offsetof`, not the comment, and that discrepancy is a flag to re-verify in your RE tool. Treat comments as notes and asserts as truth.

### Inheritance is real C++ inheritance

Because the engine is native C++ with a real class hierarchy, your reclass structs mirror that hierarchy with actual inheritance — which automatically lays out the base members first. `NiCamera : public NiAVObject`, and `NiAVObject : public NiObject : public NiRefObject`. The base classes carry the vtable and the common transform data:

```cpp
// starfield2vr/sdk-lite/include/RE/N/NiAVObject.h:141
class NiAVObject : public NiObject {
    // ... ~30 virtuals declared so the vtable lines up ...
    BSFixedString  name;          // 10
    NiNode*        parent;        // 38
    NiTransform    local;         // 40
    NiTransform    world;         // 80
    NiTransform    previousWorld; // C0
    NiBound        worldBound;    // 100
    uint64_t       flags;         // 118
};
static_assert(sizeof(NiAVObject) == 0x130);
static_assert(offsetof(NiAVObject, parent) == 0x38);
```

You must declare the virtual functions **in order** even when you don't call them — every `virtual void* Unk57();` exists so the vtable layout matches and the `sizeof`/`offsetof` math comes out right. Get the virtual count wrong and every data member after the vtable shifts.

### Finding the objects: address library + RTTI vtables

A correct struct is useless without a *pointer* to a live instance. Creation Engine globals are singletons reached through a fixed global address — but that address moves every patch, so you go through the address library. starfield2vr wraps each singleton in a tiny accessor:

```cpp
// starfield2vr/src/CreationEngine/CreationEngineSingletonManager.cpp:17
RE::PlayerCamera* CreationEngineSingletonManager::GetPlayerCameraSingleton()
{
    static REL::Relocation<RE::PlayerCamera**> singleton{
        GameStore::MemoryOffsets::PlayerCamera::Singleton() };
    return *singleton;
}

// :23
RE::PlayerCharacter* CreationEngineSingletonManager::GetPlayerRef()
{
    static REL::Relocation<RE::PlayerCharacter**> singleton{
        GameStore::MemoryOffsets::GlobalPlayerRef() };
    return *singleton;
}
```

`REL::Relocation<T>` is CommonLib's address-library handle: you give it a stable *offset/ID* (from `MemoryOffsets`), it adds the runtime module base, and dereferences to the live pointer. The mod code says "the player camera singleton," never a literal hex address.

For objects whose global isn't known but whose *type* is, you use RTTI. The offsets table resolves a vtable by its MSVC mangled name, then indexes a slot:

```cpp
// starfield2vr/src/CreationEngine/memory/offsets.h:27
namespace FirstPersonState {
    inline uintptr_t GetRotationQuatV() {
        static auto pattern = ".?AVFirstPersonState@@";              // RTTI mangled name
        static auto addr = ((uintptr_t*)VTable("FirstPersonState::vftable[13]",
                                               pattern,
                                               OffsetsTable::GetOffset(459617)))[13];
        return addr;
    }
}
// :41 — same idea for BSFadeNode::UpdateWorld, slot [79]:
static auto addr = ((uintptr_t*)VTable("BSFadeNode::vftable", ".?AVBSFadeNode@@", ...))[79];
```

`VTable(name, ".?AV<Class>@@", id)` scans RTTI for that mangled string, walks to the vtable, and hands you a function pointer at a given slot — so you can hook a *virtual* (e.g. `FirstPersonState`'s 14th method) without a byte pattern, anchored to a name the compiler emitted.

### When to use it

- Native C++ engine **with RTTI**, and ideally a community address library already exists (Bethesda, many Source/idTech-adjacent titles).
- You can tolerate regenerating offsets per build but want name-anchored stability for vtables and singletons.
- **Cost:** you hand-author and maintain every struct. The address library and RTTI buy you *finding* objects cheaply; you still pay full price for *describing* them.

---

## Strategy 3 — Pure reclass via pattern-scan + hooked arguments (Anvil)

### The principle

Now the worst case: a stripped engine. No reflection DB, no RTTI names you can rely on, no address library. This is Ubisoft Anvil, and anvilengine2vr's answer is the most labor-intensive of the three — and the most broadly applicable, because it assumes *nothing* from the engine.

The strategy inverts the usual "find the object, then read it." Instead:

1. **Pattern-scan** to find a *function* of interest (the one that builds the projection matrix, copies the render context, computes the camera forward vector).
2. **Hook** that function with safetyhook.
3. **Read the object out of the function's arguments.** The engine itself hands you a `this` pointer or a context struct as `arg0` — you just need a struct that describes it.

You never search memory for the camera. You let the engine give it to you, at exactly the moment it's being used, and you reinterpret the pointer through a hand-built reclass struct.

### Step 1: find the function by byte pattern

Anvil's offsets file is a wall of byte signatures, each one a fingerprint of a function's prologue or a distinctive instruction sequence:

```cpp
// anvilengine2vr/games/valhalla/engine/memory/offsets.h:20
inline uintptr_t calc_projection_fn_addr() {
    static const auto pattern = "48 8B C4 53 48 81 EC 90 00 00 00 0F 29 70 E8 48 8B D9 F3";
    static auto addr = FuncRelocation("calc_projection_fn_addr", pattern, 0x817110);
    return addr;
}

// :38 — the gfx-context copy we'll hook below:
inline uintptr_t on_gfx_context_copy() {
    static const auto pattern = "0F 10 01 0F 10 49 10 0F 11 81 D0 06 00 00 0F 10 41 20 ...";
    static auto addr = FuncRelocation("on_gfx_context_copy", pattern, 0x8b0550);
    return addr;
}

// :53 — get the camera's forward vector:
inline uintptr_t camera_node_get_forward_addr() {
    static const auto pattern = "0F 10 51 70 48 8B C2 0F 28 1D";
    static auto addr = FuncRelocation("camera_node_get_forward_addr", pattern, 0x12d6400);
    return addr;
}
```

The trailing hex (`0x817110`) is a fallback offset for the known build; the *pattern* is what makes it survive minor patches. `FuncRelocation` scans the module for the byte signature and returns the function address. There is also `InstructionRelocation(..., op, len, ...)` (see `on_begin_frame_fn_addr` at `:32`) for when the thing you want is a *global pointer* referenced by a RIP-relative instruction rather than a function entry.

### Step 2: hand-define the struct the function operates on

Anvil's structs live in `games/valhalla/sdk/`. Without RTTI to name anything, these are pure reverse-engineering artifacts — even the member names are descriptive guesses (`pSomeResource_8`, `pSubObject_200`). The `GfxContext` is the render context the camera pipeline threads through, reconstructed offset by offset from how the decompiled function indexes it:

```cpp
// anvilengine2vr/games/valhalla/sdk/ACVRGfxContext.h:81
struct GfxContext {
    glm::mat4 viewMatrix;                       // 0x000 — base; the 'in_viewMatrix' arg
    char unknown_padding_0x040[0x200];
    Matrix4x4* pOverrideViewMatrix;             // 0x240 — optional view override
    // ...
    Vector4 projectionParams;                   // 0x260
    // ...
    // --- OUTPUT MATRICES ---
    Matrix4x4 viewProjectionMatrix;             // 0x310
    Matrix4x4 projectionMatrix;                 // 0x350
    Matrix4x4 inverseViewMatrix;                // 0x390
    Matrix4x4 inverseProjectionMatrix;          // 0x3D0
    Matrix4x4 worldToViewMatrix;                // 0x410
    // ...
    char unknown_padding_0x4C0[528];
    glm::mat4 pastViewMatrix;                   // for the AFR/TAA history problem
};
```

The annotations are pure RE notes — e.g. `0x240 (context + 576)`, `0x310 (a1 + 49)` — literally "this is `a1[49]` in the decompiler output." That is how you build these: open the function in Ghidra/IDA, watch which offsets of `a1` it reads and writes, and name them. Note `pastViewMatrix` near the end — that field exists specifically to solve the alternate-frame-rendering TAA smearing problem (covered in the frame-timing guides); finding *where* the engine stashes the previous frame's view matrix is itself a reverse-engineering result baked into this struct.

The same file reconstructs `CameraNode` (position, rotation, previous-frame pose):

```cpp
// anvilengine2vr/games/valhalla/sdk/ACVRGfxContext.h:44
struct CameraNode {
    void *vftable;
    char  data8[88];
    glm::vec4 prevPosition;
    glm::quat prevRotation;
    __int64   unk1[2];
    glm::vec4 position;     // current camera position
    glm::quat rotation;     // current camera rotation
    char  dataB0[780];
    // ... pointers to sub-calculators, mostly unknown ...
};
```

These structs use `#pragma pack(push, 1)` (top of the file) so the compiler never inserts its own padding — your explicit `char unknown_padding[...]` arrays are the *only* spacing, and offsets land exactly where you measured them.

### Step 3: hook the function and reinterpret the argument

The module declares one `safetyhook::InlineHook` per function and a matching static callback. The callback's signature *is* the reconstructed engine signature — every argument typed against your reclass structs:

```cpp
// anvilengine2vr/games/valhalla/engine/EngineCameraModule.h:30
safetyhook::InlineHook m_onCalcProjection{};
safetyhook::InlineHook m_onCalcFinalView{};
safetyhook::InlineHook m_onCameraGetForwardHook{};
safetyhook::InlineHook m_onCopyGfxContext{};

// :50 — context arrives as arg0:
static uintptr_t onCopyGfxContext(sdk::GfxContext* context, glm::mat4* viewMatrix);

// :48 — the camera node arrives as arg0:
static glm::vec4* onCameraGetForwardHook(sdk::CameraNode* node, glm::vec4* forward);

// :54 — the full view-calc hook: context in, all output matrices passed by pointer:
static uintptr_t onCalcFinalView(sdk::GfxContext* context, int* out_textureId, /* ... */,
                                 glm::mat4* in_viewMatrix,
                                 glm::mat4* out_viewProjectionMatrix,
                                 glm::mat4* out_projectionMatrix, /* ... */);
```

Installation is a pattern-scan feeding straight into a hook (`EngineCameraModule.cpp`):

```cpp
// anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:26
auto onCopyGfxContextFn = memory::on_gfx_context_copy();        // pattern-scanned address
m_onCopyGfxContext = safetyhook::create_inline(
    (void*)onCopyGfxContextFn, (void*)&EngineCameraModule::onCopyGfxContext);
```

Inside the callback, `context` is a live `GfxContext*` — and because the struct's layout matches, `context->viewMatrix`, `context->projectionMatrix`, and `context->pastViewMatrix` are all directly accessible. You modify them, then call the original through the hook trampoline. That is the whole game: **the engine handed you the camera; your struct lets you read it.**

### When to use it

- Stripped engine: no reflection, unreliable/absent RTTI, no address library (Anvil, many bespoke and console-origin engines).
- You have a disassembler and patience.
- **Cost:** the highest. Every function is a byte pattern that may break on any patch; every struct is hand-decoded; member names are guesses until proven. The upside is *zero assumptions about the engine* — this technique works on literally anything you can attach a debugger to.

---

## How to do this on a brand-new engine

You've been handed a game on an engine none of the three projects covers. Walk the ladder top-down — always prefer the cheapest mechanism the engine actually offers.

**0. Triage what the engine gives you (one afternoon).**
- Open the main executable in a disassembler. Search the string table for managed-runtime fingerprints: `mono`, `il2cpp`, `.NET`, or engine-specific markers (`via.` for RE Engine, `UObject`/`/Script/` for Unreal). **Found one → Strategy 1 territory.**
- Search for MSVC RTTI mangled names: the regex `\.\?AV.*@@`. A binary full of `.?AV...@@` strings is RTTI-rich. **Found those → Strategy 2 is viable.**
- Check whether a community address library or SDK already exists (CommonLib-style projects, modding wikis). If yes, you've skipped weeks of work.
- Nothing of the above → **Strategy 3.**

**1. Find one anchor.** You need a single reliable entry point into the render loop — the present call (you already hook DXGI `Present` for the overlay; see the injection guides) or a per-frame engine tick. From there you reach everything else.

**2. Get to the camera.**
- *Reflection path:* `find_type` for the camera/scene-view type, walk to the active camera, read its world transform and projection by field name. Done.
- *RTTI path:* find the camera's vtable by mangled name, confirm instances against it, and reach the singleton through the address library. Build the reclass struct for just the fields you touch (view matrix, projection, FOV, near/far).
- *Pattern path:* pattern-scan the projection-build or view-copy function, hook it, and read the context/`this` from arg0.

**3. Build reclass structs incrementally (Strategies 2 & 3).**
- In your RE tool, point at a live instance and identify the members you need *first*: the 4×4 view matrix, the projection matrix, position, rotation, FOV. Ignore the rest.
- Transcribe to a struct, filling unknown gaps with `char pad_XXXX[n]` and `#pragma pack(push, 1)`.
- **Add `static_assert(offsetof(...) == 0x...)` for every named member and `static_assert(sizeof(...) == 0x...)` for the whole struct.** This is non-negotiable — it converts silent runtime corruption into loud compile-time failure (exactly the discipline in `NiCamera.h:174` and `NiAVObject.h:191`).
- Name what you understand; leave auto-names (`N00000A89`) for what you don't. Partial decode is fine and normal.

**4. Anchor against the most stable thing available.** Prefer, in order: a *name* (reflection field name → RTTI mangled name) over a *byte pattern* over a *raw offset*. Names survive patches; patterns survive minor patches; raw offsets survive nothing.

**5. Re-verify after every game update.** Reflection mods usually just work. RTTI mods need the address library regenerated. Pattern/offset mods need every signature re-checked — keep your asserts and a quick smoke test that prints the camera matrix so a broken offset is obvious on launch, not three scenes in.

---

## Key takeaways

- **There is a ladder, and you want the top rung.** Reflection (engine-provided, by-name) > RTTI + address library (name-anchored, offsets per-build) > pure pattern-scan + reclass (everything hand-built). Each step down multiplies the hand-maintained data that breaks on patches.
- **Reflection makes the engine do the work.** REFramework asks the RE Engine's type DB for types, singletons, fields, and methods *by string* (`find_type_definition`, `get_managed_singleton`, `get_reflection_property`) — one codebase ports across every RE Engine game via `game_namespace()`.
- **RTTI buys you name-anchored *finding*; you still hand-write the *describing*.** starfield2vr matches mangled names like `.?AVNiCamera@@`, reaches singletons through `REL::Relocation` + the address library, and transcribes layouts (`NiCamera`, `PlayerCamera`) guarded by `static_assert`.
- **Pure reclass assumes nothing.** anvilengine2vr pattern-scans a function (`FuncRelocation` on a byte signature), hooks it with safetyhook, and reads the camera/context straight out of the hooked function's arguments via packed, offset-annotated structs (`GfxContext`, `CameraNode`).
- **`static_assert(offsetof/sizeof)` is your seatbelt.** It is the difference between a broken offset failing at compile time and the same offset silently corrupting the camera and crashing the game.
- **Always anchor to the most stable handle the engine offers** — a name beats a pattern beats a raw offset — and re-verify after every game update.

---

**Next:** [14 — Hooking the Frame: Present, Pacing, and the Three Counters](14-hooking-the-frame.md) — once you can *find* the camera and render context, the next problem is *when* to touch them. We pick up the begin-engine-frame / begin-render-frame / present hooks the camera modules above depend on, and the frame-timing counters that keep AFR stereo in sync.
