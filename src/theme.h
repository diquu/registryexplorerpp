#pragma once

#include "../vendor/imgui/imgui.h"

namespace repp {

ImVec4 rgb(unsigned int hex, float alpha = 1.0f);

constexpr float kTopBarH = 64.0f;
constexpr float kStatusH = 26.0f;

inline const ImVec4 kBg{0x0F / 255.0f, 0x12 / 255.0f, 0x16 / 255.0f, 1.0f};
const ImVec4 kPanel{0x14 / 255.0f, 0x18 / 255.0f, 0x1E / 255.0f, 1.0f};
const ImVec4 kRaised{0x1A / 255.0f, 0x1F / 255.0f, 0x27 / 255.0f, 1.0f};
const ImVec4 kHair{0x24 / 255.0f, 0x2B / 255.0f, 0x34 / 255.0f, 1.0f};
const ImVec4 kText{0xE7 / 255.0f, 0xEA / 255.0f, 0xEE / 255.0f, 1.0f};
const ImVec4 kDim{0x9B / 255.0f, 0xA6 / 255.0f, 0xB3 / 255.0f, 1.0f};
const ImVec4 kFaint{0x78 / 255.0f, 0x83 / 255.0f, 0x8F / 255.0f, 1.0f};
const ImVec4 kAmber{0xFB / 255.0f, 0xBF / 255.0f, 0x24 / 255.0f, 1.0f};
const ImVec4 kAmberSoft{0xFB / 255.0f, 0xBF / 255.0f, 0x24 / 255.0f, 0.16f};
const ImVec4 kSelect{0x15 / 255.0f, 0x39 / 255.0f, 0x38 / 255.0f, 1.0f};
const ImVec4 kHover{0x1C / 255.0f, 0x23 / 255.0f, 0x2C / 255.0f, 1.0f};
const ImVec4 kAccent{0x2D / 255.0f, 0xD4 / 255.0f, 0xBF / 255.0f, 1.0f};
const ImVec4 kAccentSoft{0x2D / 255.0f, 0xD4 / 255.0f, 0xBF / 255.0f, 0.14f};
const ImVec4 kSuspiciousColor{0xF8 / 255.0f, 0x71 / 255.0f, 0x71 / 255.0f, 1.0f};
const ImVec4 kCleanColor{0x34 / 255.0f, 0xD3 / 255.0f, 0x99 / 255.0f, 1.0f};

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
