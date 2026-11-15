#include "theme.h"

#include <cfloat>
#include <cstring>

namespace repp {

static Fonts g_fonts = {};

ImVec4 rgb(unsigned int hex, float alpha) {
    ImVec4 c = {};
    c.x = ((hex >> 16) & 0xFF) / 255.0f;
    c.y = ((hex >> 8) & 0xFF) / 255.0f;
    c.z = (hex & 0xFF) / 255.0f;
    c.w = alpha;
    return c;
}

Fonts setup_theme() {    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(10.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.TabRounding = 2.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kFaint;
    colors[ImGuiCol_WindowBg] = kBg;
    colors[ImGuiCol_ChildBg] = kBg;
    colors[ImGuiCol_PopupBg] = kRaised;
    colors[ImGuiCol_Border] = kHair;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = kPanel;
    colors[ImGuiCol_FrameBgHovered] = kHover;
    colors[ImGuiCol_FrameBgActive] = kRaised;
    colors[ImGuiCol_TitleBg] = kBg;
    colors[ImGuiCol_TitleBgActive] = kBg;
    colors[ImGuiCol_TitleBgCollapsed] = kBg;
    colors[ImGuiCol_MenuBarBg] = kPanel;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = kHair;
    colors[ImGuiCol_ScrollbarGrabHovered] = kFaint;
    colors[ImGuiCol_ScrollbarGrabActive] = kDim;
    colors[ImGuiCol_CheckMark] = kAmber;
    colors[ImGuiCol_SliderGrab] = kAmber;
    colors[ImGuiCol_SliderGrabActive] = kAmber;
    colors[ImGuiCol_Button] = kPanel;
    colors[ImGuiCol_ButtonHovered] = kHover;
    colors[ImGuiCol_ButtonActive] = kRaised;
    colors[ImGuiCol_Header] = kSelect;
    colors[ImGuiCol_HeaderHovered] = kHover;
    colors[ImGuiCol_HeaderActive] = kSelect;
    colors[ImGuiCol_Separator] = kHair;
    colors[ImGuiCol_SeparatorHovered] = kHair;
    colors[ImGuiCol_SeparatorActive] = kAmber;
    colors[ImGuiCol_ResizeGrip] = kHair;
    colors[ImGuiCol_ResizeGripHovered] = kDim;
    colors[ImGuiCol_ResizeGripActive] = kAmber;
    colors[ImGuiCol_PlotHistogram] = kAmber;
    colors[ImGuiCol_TableHeaderBg] = kPanel;
    colors[ImGuiCol_TableBorderStrong] = kHair;
    colors[ImGuiCol_TableBorderLight] = kHair;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TextSelectedBg] = kAmberSoft;
    colors[ImGuiCol_DragDropTarget] = kAmber;

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    Fonts fonts;
    const ImGuiIO& io = ImGui::GetIO();
    const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* segoeuib = "C:\\Windows\\Fonts\\segoeuib.ttf";
    const char* consolas = "C:\\Windows\\Fonts\\consola.ttf";

    auto add = [&](const char* path, float size, ImFont*& out, const ImWchar* ranges = nullptr) {
        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 1;
        out = io.Fonts->AddFontFromFileTTF(path, size, &config, ranges);
    };
    add(segoe, 15.0f, fonts.body);
    add(segoeuib, 15.0f, fonts.semibold);
    add(segoeuib, 17.0f, fonts.bold);
    add(segoeuib, 21.0f, fonts.display);
    add(consolas, 12.5f, fonts.mono);
    if (fonts.mono == nullptr) {
        fonts.mono = fonts.body;
    }

    g_fonts = fonts;
    return g_fonts;
}

const Fonts& fonts() {
    return g_fonts;
}

}
