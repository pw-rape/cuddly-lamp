#include "Menu.h"

#include "../../Dependencies/ImGui/imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr ImU32 C(unsigned r, unsigned g, unsigned b, unsigned a = 255)
    {
        return IM_COL32(r, g, b, a);
    }

    constexpr ImU32 Bg = C(10, 9, 15);
    constexpr ImU32 Surface = C(12, 11, 18);
    constexpr ImU32 Surface2 = C(15, 14, 22);
    constexpr ImU32 Border = C(24, 22, 33);
    constexpr ImU32 TextMain = C(205, 203, 211);
    constexpr ImU32 TextMuted = C(69, 67, 78);
    constexpr ImU32 Cyan = C(50, 234, 231);
    constexpr ImU32 Purple = C(51, 46, 87);

    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* iconFont = nullptr;
    int activeSection = 0;
    float fovColor[4] = { 0.95f, 0.95f, 0.96f, 1.0f };
    bool silent = true;
    bool ignoreNpcs = true;
    bool ignoreTeam = true;
    std::array<float, 6> values = { 0.0f, 0.01f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, 6> targetValues = values;
    std::array<float, 5> primaryAnim = { 1.0f, 0, 0, 0, 0 };
    std::array<float, 2> subAnim = { 1.0f, 0 };
    std::array<float, 3> toggleAnim = { 1.0f, 1.0f, 1.0f };
    float submenuExpand = 1.0f;
    float pageFade = 1.0f;

    float Animate(float value, float target, float speed)
    {
        return value + (target - value) * (1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime));
    }

    ImU32 Alpha(ImU32 color, float opacity)
    {
        const unsigned a = (color >> IM_COL32_A_SHIFT) & 0xffu;
        const unsigned out = static_cast<unsigned>(a * std::clamp(opacity, 0.0f, 1.0f));
        return (color & ~(0xffu << IM_COL32_A_SHIFT)) | (out << IM_COL32_A_SHIFT);
    }

    ImU32 Mix(ImU32 from, ImU32 to, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const auto channel = [t](ImU32 a, ImU32 b, int shift) {
            const float av = static_cast<float>((a >> shift) & 0xffu);
            const float bv = static_cast<float>((b >> shift) & 0xffu);
            return static_cast<unsigned>(av + (bv - av) * t + 0.5f);
        };
        return C(channel(from, to, IM_COL32_R_SHIFT), channel(from, to, IM_COL32_G_SHIFT),
            channel(from, to, IM_COL32_B_SHIFT), channel(from, to, IM_COL32_A_SHIFT));
    }

    void SelectSection(int section)
    {
        if (activeSection != section) {
            activeSection = section;
            pageFade = 0.0f;
        }
    }

    ImFont* Font(bool bold = false)
    {
        if (bold && semibold) return semibold;
        return regular ? regular : ImGui::GetFont();
    }

    void Label(ImDrawList* d, const ImVec2& o, float x, float y, const char* text,
        float size, ImU32 color = TextMain, bool bold = false)
    {
        d->AddText(Font(bold), size, ImVec2(o.x + x, o.y + y), color, text);
    }

    bool ButtonHit(const char* id, const ImVec2& o, float x, float y, float w, float h)
    {
        ImGui::SetCursorScreenPos(ImVec2(o.x + x, o.y + y));
        return ImGui::InvisibleButton(id, ImVec2(w, h));
    }

    float TextWidth(const char* text, float size, bool bold = false)
    {
        return Font(bold)->CalcTextSizeA(size, 10000.0f, 0.0f, text).x;
    }

    void Icon(ImDrawList* d, const ImVec2& o, float x, float y, const char* glyph, ImU32 color, float size = 17.0f)
    {
        d->AddText(iconFont ? iconFont : Font(true), size, ImVec2(o.x + x, o.y + y), color, glyph);
    }

    void NavIcon(ImDrawList* d, const ImVec2& o, int icon, float x, float y, ImU32 color)
    {
        static const char* glyphs[] = { u8"\uf05b", u8"\uf06e", u8"\uf0c0", u8"\uf0ad", u8"\uf085" };
        Icon(d, o, x, y, glyphs[std::clamp(icon, 0, 4)], color, 17.0f);
    }

    void DrawSidebar(ImDrawList* d, const ImVec2& o)
    {
        const float sx = 25, sw = 216;
        const bool combatActive = activeSection <= 1;
        submenuExpand = Animate(submenuExpand, combatActive ? 1.0f : 0.0f, 11.0f);
        for (int i = 0; i < 5; ++i)
            primaryAnim[i] = Animate(primaryAnim[i], (i == 0 ? combatActive : activeSection == i + 1) ? 1.0f : 0.0f, 13.0f);
        if (ButtonHit("##combat", o, sx, 92, sw, 36)) SelectSection(0);
        d->AddRectFilled(ImVec2(o.x + sx, o.y + 92), ImVec2(o.x + sx + sw, o.y + 128), Alpha(Surface2, primaryAnim[0]), 12);
        d->AddRect(ImVec2(o.x + sx, o.y + 92), ImVec2(o.x + sx + sw, o.y + 128), Alpha(Border, primaryAnim[0]), 12);
        NavIcon(d, o, 0, 39, 101, combatActive ? Cyan : C(61, 59, 68));
        Label(d, o, 68, 103, "Combat", 14, combatActive ? TextMain : C(61, 59, 68), true);

        if (submenuExpand > 0.01f)
        {
            const char* subNames[] = { "General", "Weapon" };
            const float subYs[] = { 142, 192 };
            for (int i = 0; i < 2; ++i) {
                subAnim[i] = Animate(subAnim[i], activeSection == i ? 1.0f : 0.0f, 13.0f);
                char id[24]; std::snprintf(id, sizeof(id), "##subnav%d", i);
                if (combatActive && ButtonHit(id, o, 53, subYs[i], 188, 36)) SelectSection(i);
                const float reveal = submenuExpand;
                d->AddRectFilled(ImVec2(o.x + 53, o.y + subYs[i]), ImVec2(o.x + 241, o.y + subYs[i] + 36), Alpha(Surface2, subAnim[i] * reveal), 12);
                d->AddRect(ImVec2(o.x + 53, o.y + subYs[i]), ImVec2(o.x + 241, o.y + subYs[i] + 36), Alpha(Border, subAnim[i] * reveal), 12);
                Label(d, o, 68, subYs[i] + 10, subNames[i], 13.5f,
                    Alpha(activeSection == i ? TextMain : C(61, 59, 68), reveal), true);
            }
        }

        const char* names[] = { "Visuals", "Entities", "Miscellaneous", "Settings" };
        const int icons[] = { 1, 2, 3, 4 };
        const float firstY = 142.0f + submenuExpand * 100.0f;
        for (int i = 0; i < 4; ++i)
        {
            const int section = i + 2;
            const float y = firstY + i * 50.0f;
            char id[24]; std::snprintf(id, sizeof(id), "##mainnav%d", i);
            if (ButtonHit(id, o, sx, y, sw, 36)) SelectSection(section);
            d->AddRectFilled(ImVec2(o.x + sx, o.y + y), ImVec2(o.x + sx + sw, o.y + y + 36), Alpha(Surface2, primaryAnim[i + 1]), 12);
            d->AddRect(ImVec2(o.x + sx, o.y + y), ImVec2(o.x + sx + sw, o.y + y + 36), Alpha(Border, primaryAnim[i + 1]), 12);
            const ImU32 col = activeSection == section ? TextMain : C(61, 59, 68);
            NavIcon(d, o, icons[i], 39, y + 8, activeSection == section ? Cyan : C(61, 59, 68));
            Label(d, o, 68, y + 10, names[i], 13.5f, col, true);
        }
    }

    void Pill(ImDrawList* d, const ImVec2& o, float x, float y, const char* text)
    {
        const float w = TextWidth(text, 12.5f) + 20;
        d->AddRectFilled(ImVec2(o.x + x - w, o.y + y), ImVec2(o.x + x, o.y + y + 27), C(28, 27, 35), 9);
        Label(d, o, x - w + 10, y + 6, text, 12.5f, TextMain);
    }

    void Divider(ImDrawList* d, const ImVec2& o, float x, float y, float w)
    {
        d->AddLine(ImVec2(o.x + x, o.y + y), ImVec2(o.x + x + w, o.y + y), C(22, 20, 29), 1.0f);
    }

    void Slider(ImDrawList* d, const ImVec2& o, float x, float y, float w, int idx,
        const char* title, const char* detail)
    {
        char id[24]; std::snprintf(id, sizeof(id), "##slider%d", idx);
        ButtonHit(id, o, x, y + 38, w, 18);
        if (ImGui::IsItemActive())
            targetValues[idx] = std::clamp((ImGui::GetIO().MousePos.x - (o.x + x)) / w, 0.0f, 1.0f);
        values[idx] = Animate(values[idx], targetValues[idx], ImGui::IsItemActive() ? 22.0f : 14.0f);
        Label(d, o, x, y, title, 14, TextMain, true);
        Label(d, o, x, y + 17, detail, 12, TextMuted);
        char val[16]; std::snprintf(val, sizeof(val), "%d", static_cast<int>(targetValues[idx] * 100.0f + 0.5f));
        Label(d, o, x + w - TextWidth(val, 13), y + 1, val, 13, TextMain);
        d->AddRectFilled(ImVec2(o.x + x, o.y + y + 39), ImVec2(o.x + x + w, o.y + y + 44), C(28, 27, 35), 4);
        d->AddRectFilled(ImVec2(o.x + x, o.y + y + 39), ImVec2(o.x + x + w * values[idx], o.y + y + 44), Cyan, 4);
        const float kx = o.x + x + w * values[idx];
        d->AddCircleFilled(ImVec2(kx, o.y + y + 41.5f), 6.5f, C(236, 237, 239));
        d->AddCircleFilled(ImVec2(kx, o.y + y + 41.5f), 3.4f, Cyan);
    }

    void Toggle(ImDrawList* d, const ImVec2& o, float x, float y, bool& value, const char* id, int index)
    {
        if (ButtonHit(id, o, x, y, 36, 25)) value = !value;
        toggleAnim[index] = Animate(toggleAnim[index], value ? 1.0f : 0.0f, 15.0f);
        const float t = toggleAnim[index];
        const ImU32 track = Mix(C(28, 27, 35), Purple, t);
        d->AddRectFilled(ImVec2(o.x + x, o.y + y), ImVec2(o.x + x + 36, o.y + y + 25), track, 13);
        const float cx = o.x + x + 12.5f + 11.0f * t;
        const ImU32 knob = Mix(C(237, 237, 237), C(71, 107, 123), t);
        d->AddCircleFilled(ImVec2(cx, o.y + y + 12.5f), 8, knob);
    }

    void ColorSwatch(ImDrawList* d, const ImVec2& o, float x, float y)
    {
        if (ButtonHit("##fov_color", o, x, y, 36, 27)) ImGui::OpenPopup("FOV color");
        d->AddRectFilled(ImVec2(o.x + x, o.y + y), ImVec2(o.x + x + 36, o.y + y + 27), C(28, 27, 35), 9);
        const ImU32 swatch = ImGui::ColorConvertFloat4ToU32(ImVec4(fovColor[0], fovColor[1], fovColor[2], fovColor[3]));
        d->AddRectFilled(ImVec2(o.x + x + 8, o.y + y + 5), ImVec2(o.x + x + 28, o.y + y + 22), swatch, 4);
        d->AddRect(ImVec2(o.x + x + 8, o.y + y + 5), ImVec2(o.x + x + 28, o.y + y + 22), C(86, 83, 94), 4);

        ImGui::SetNextWindowPos(ImVec2(o.x + x - 175, o.y + y + 34), ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(C(18, 17, 25)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Border));
        if (ImGui::BeginPopup("FOV color")) {
            ImGui::ColorPicker4("##picker", fovColor, ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoAlpha);
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void ToggleRow(ImDrawList* d, const ImVec2& o, float x, float y, float w, const char* title,
        const char* detail, bool& value, const char* id, int index)
    {
        Label(d, o, x, y, title, 14, value ? C(83, 80, 91) : C(65, 63, 73), true);
        Label(d, o, x, y + 17, detail, 12, C(56, 54, 64));
        Toggle(d, o, x + w - 36, y + 2, value, id, index);
    }

    void DrawGeneral(ImDrawList* d, const ImVec2& o)
    {
        Label(d, o, 322, 24, "General", 21, TextMain, true);
        Icon(d, o, 288, 24, u8"\uf66d", Cyan, 20.0f);

        d->AddRectFilled(ImVec2(o.x + 298, o.y + 92), ImVec2(o.x + 620, o.y + 156), Surface2, 22);
        d->AddRect(ImVec2(o.x + 298, o.y + 92), ImVec2(o.x + 620, o.y + 156), Border, 22);
        Label(d, o, 315, 110, "Weapon Group", 14, TextMain, true);
        Label(d, o, 315, 127, "The group of weapon settings to display", 12, TextMuted);
        Pill(d, o, 602, 111, "Default");

        d->AddRectFilled(ImVec2(o.x + 298, o.y + 185), ImVec2(o.x + 620, o.y + 569), Surface, 22);
        d->AddRect(ImVec2(o.x + 298, o.y + 185), ImVec2(o.x + 620, o.y + 569), Border, 22);
        Slider(d, o, 315, 200, 288, 0, "Field Of View", "The FOV the target has to be within");
        Divider(d, o, 315, 251, 288);
        Slider(d, o, 315, 264, 288, 1, "Smoothing", "The smoothness of the aimbot");
        Divider(d, o, 315, 315, 288);
        Slider(d, o, 315, 328, 288, 2, "Recoil", "The recoil of the weapon");
        Divider(d, o, 315, 379, 288);
        Slider(d, o, 315, 392, 288, 3, "Spread", "The spread of the bullets");
        Divider(d, o, 315, 443, 288);
        Slider(d, o, 315, 456, 288, 4, "Sway", "The sway from holding the weapon");
        Divider(d, o, 315, 507, 288);
        Label(d, o, 315, 525, "Hitbox", 14, TextMain, true);
        Label(d, o, 315, 542, "The hitbox to be aimed at", 12, TextMuted);
        Pill(d, o, 603, 524, "Head");

        d->AddRectFilled(ImVec2(o.x + 649, o.y + 92), ImVec2(o.x + 970, o.y + 585), Surface, 22);
        d->AddRect(ImVec2(o.x + 649, o.y + 92), ImVec2(o.x + 970, o.y + 585), Border, 22);
        Label(d, o, 666, 118, "FOV Circle", 14, TextMain, true);
        ColorSwatch(d, o, 917, 110);
        Divider(d, o, 666, 157, 287);
        Label(d, o, 666, 177, "Activate", 14, TextMain, true);
        Label(d, o, 666, 194, "The key pressed to activate the aimbot", 12, TextMuted);
        Pill(d, o, 953, 176, "Right Mouse");
        Divider(d, o, 666, 221, 287);
        ToggleRow(d, o, 666, 240, 287, "Silent", "Makes the aimbot invisible", silent, "##silent", 0);
        Divider(d, o, 666, 285, 287);
        ToggleRow(d, o, 666, 304, 287, "Ignore NPCs", "Prevents NPCs from being targetted", ignoreNpcs, "##npc", 1);
        Divider(d, o, 666, 349, 287);
        ToggleRow(d, o, 666, 368, 287, "Ignore Teammates", "Prevents teammates from being targetted", ignoreTeam, "##team", 2);
        Divider(d, o, 666, 413, 287);
        Slider(d, o, 666, 427, 287, 5, "Hit Chance", "The chance of hitting the target");
        Divider(d, o, 666, 478, 287);
    }

    void DrawPlaceholder(ImDrawList* d, const ImVec2& o)
    {
        static const char* names[] = { "General", "Weapon", "Visuals", "Entities", "Miscellaneous", "Settings" };
        Label(d, o, 288, 24, names[activeSection], 21, TextMain, true);
        d->AddRectFilled(ImVec2(o.x + 298, o.y + 92), ImVec2(o.x + 970, o.y + 585), Surface, 22);
        d->AddRect(ImVec2(o.x + 298, o.y + 92), ImVec2(o.x + 970, o.y + 585), Border, 22);
        Label(d, o, 330, 125, names[activeSection], 18, TextMain, true);
        Label(d, o, 330, 154, "Select General to view the recreated settings panel.", 13, TextMuted);
    }
}

void Menu::SetFonts(ImFont* regularFont, ImFont* semiBoldFont, ImFont* icons)
{
    regular = regularFont;
    semibold = semiBoldFont;
    iconFont = icons;
}

void Menu::Render()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(Bg));
    ImGui::Begin("##ultimate", nullptr, flags);

    const ImVec2 o = ImGui::GetWindowPos();
    ImDrawList* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(o, ImVec2(o.x + 1000, o.y + 620), Bg, 22, ImDrawFlags_RoundCornersAll);
    d->AddRectFilled(ImVec2(o.x + 269, o.y), ImVec2(o.x + 1000, o.y + 64), Surface, 0, ImDrawFlags_RoundCornersTopRight);
    d->AddLine(ImVec2(o.x + 269, o.y), ImVec2(o.x + 269, o.y + 620), Border);
    d->AddLine(ImVec2(o.x, o.y + 64), ImVec2(o.x + 1000, o.y + 64), Border);
    DrawSidebar(d, o);
    pageFade = Animate(pageFade, 1.0f, 10.0f);
    const int pageVertexStart = d->VtxBuffer.Size;
    if (activeSection == 0) DrawGeneral(d, o); else DrawPlaceholder(d, o);
    const float slide = (1.0f - pageFade) * 12.0f;
    for (int i = pageVertexStart; i < d->VtxBuffer.Size; ++i) {
        d->VtxBuffer[i].pos.x += slide;
        d->VtxBuffer[i].col = Alpha(d->VtxBuffer[i].col, pageFade);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
