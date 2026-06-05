// fh5vr/ModConfig.cpp — the CONSUMING repo's Mods::Mods() (Layer 3).
//
// The core (src/Mods.cpp) owns iteration/dispatch; this file declares WHICH mods exist for FH5 and
// registers the engine adapter with the framework. Mirrors the ports' ModConfig.cpp:
//
//   m_mods: VRConfig (settings) -> VR (the stereo driver) -> Fh5Adapter (the IEngineAdapter mod).
//
// Order matters: VRConfig before VR (VR reads its config), and the adapter last so its hooks install
// after the VR/runtime singletons exist. We also hand the adapter to g_framework so VR::push_stereo_to_adapter
// can reach it via Framework::get_engine_adapter().

#include "Framework.hpp"
#include "Mods.hpp"

#include <mods/VR.hpp>
#include <mods/VRConfig.hpp>

#include "Fh5Adapter.hpp"

Mods::Mods() {
    m_mods.emplace_back(VRConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(Fh5Adapter::get());
    // NOTE: the framework auto-registers the adapter (the IEngineAdapter in this list) into
    // get_engine_adapter() right after constructing Mods — we can't do it here because g_framework
    // isn't assigned until make_unique<Framework> returns. See Framework::Framework().
}
