#pragma once

#include "../vendor/imgui/imgui.h"

namespace repp {

ImVec4 rgb(unsigned int hex, float alpha = 1.0f);

constexpr float kTopBarH = 64.0f;
constexpr float kStatusH = 26.0f;

inline const ImVec4 kBg{0x0B / 255.0f, 0x0D / 255.0f, 0x0E / 255.0f, 1.0f};
const ImVec4 kPanel{0x10 / 255.0f, 0x13 / 255.0f, 0x14 / 255.0f, 1.0f};
const ImVec4 kRaised{0x16 / 255.0f, 0x1A / 255.0f, 0x1B / 255.0f, 1.0f};
const ImVec4 kHair{0x22 / 255.0f, 0x28 / 255.0f, 0x2A / 255.0f, 1.0f};
const ImVec4 kText{0xE9 / 255.0f, 0xE7 / 255.0f, 0xE2 / 255.0f, 1.0f};
const ImVec4 kDim{0x9A / 255.0f, 0xA3 / 255.0f, 0xA5 / 255.0f, 1.0f};
const ImVec4 kFaint{0x82 / 255.0f, 0x8D / 255.0f, 0x90 / 255.0f, 1.0f};
const ImVec4 kAmber{0xE8 / 255.0f, 0xA3 / 255.0f, 0x3C / 255.0f, 1.0f};
const ImVec4 kAmberSoft{0xE8 / 255.0f, 0xA3 / 255.0f, 0x3C / 255.0f, 0.16f};
const ImVec4 kSelect{0x19 / 255.0f, 0x1F / 255.0f, 0x20 / 255.0f, 1.0f};
const ImVec4 kHover{0x12 / 255.0f, 0x16 / 255.0f, 0x17 / 255.0f, 1.0f};
const ImVec4 kAccent{0x2D / 255.0f, 0xD4 / 255.0f, 0xBF / 255.0f, 1.0f};
const ImVec4 kAccentSoft{0x2D / 255.0f, 0xD4 / 255.0f, 0xBF / 255.0f, 0.14f};
const ImVec4 kSuspiciousColor{0xE2 / 255.0f, 0x5B / 255.0f, 0x4B / 255.0f, 1.0f};
const ImVec4 kCleanColor{0x3F / 255.0f, 0xB6 / 255.0f, 0x8B / 255.0f, 1.0f};

struct Fonts {
    ImFont* body = nullptr;
    ImFont* semibold = nullptr;
    ImFont* bold = nullptr;
    ImFont* mono = nullptr;
    ImFont* display = nullptr;
};

Fonts setup_theme();
const Fonts& fonts();

}
