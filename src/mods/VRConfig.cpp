#include "mods/VRConfig.hpp"

std::shared_ptr<VRConfig>& VRConfig::get() {
    static auto instance = std::make_shared<VRConfig>();
    return instance;
}

void VRConfig::on_draw_ui() {
    if (!ImGui::CollapsingHeader(get_name().data())) return;
    m_world_scale->draw("World Scale");
    m_snap_turn->draw("Snap Turn");
}

void VRConfig::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (IModValue& o : m_options) o.config_load(cfg, set_defaults);
}

void VRConfig::on_config_save(utility::Config& cfg) {
    for (IModValue& o : m_options) o.config_save(cfg);
}
