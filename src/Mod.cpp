#include "Mod.hpp"

// PORT FROM: REFramework/src/Mod.hpp (ModKey methods were inline there; moved to a TU).

bool ModKey::draw(std::string_view name) {
    if (name.empty()) return false;
    ImGui::PushID(this);
    ImGui::Button(name.data());
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[0]) m_waiting_for_new_key = true;

    if (m_waiting_for_new_key) {
        const auto& keys = g_framework->get_keyboard_state();
        for (int32_t k = 0; k < (int32_t)keys.size(); ++k) {
            if (k == VK_LBUTTON || k == VK_RBUTTON) continue;
            if (keys[k]) {
                const bool erase = (k == VK_ESCAPE || k == VK_BACK);
                m_value = erase ? UNBOUND_KEY : k;
                m_waiting_for_new_key = false;
                break;
            }
        }
        ImGui::SameLine();
        ImGui::Text("Press any key...");
    } else {
        ImGui::SameLine();
        if (m_value >= 0 && m_value <= 255) ImGui::Text("%i", m_value);
        else ImGui::Text("Not bound");
    }
    ImGui::PopID();
    return true;
}

bool ModKey::is_key_down() const {
    if (m_value < 0 || m_value > 255) return false;
    if (m_value == VK_LBUTTON || m_value == VK_RBUTTON) return false;
    return g_framework->get_keyboard_state()[(uint8_t)m_value] != 0;
}

bool ModKey::is_key_down_once() {
    const auto down = is_key_down();
    if (!m_was_key_down && down) { m_was_key_down = true; return true; }
    if (!down) m_was_key_down = false;
    return false;
}
