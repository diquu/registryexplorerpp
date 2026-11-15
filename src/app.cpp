#include "app.h"
#include "text.h"
#include "suspicious.h"

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwchar>
#include <cstring>

namespace repp {
namespace {




std::wstring filetime_now_wstring() {
    FILETIME ft = {};
    GetSystemTimeAsFileTime(&ft);
    SYSTEMTIME st = {};
    FileTimeToSystemTime(&ft, &st);
    wchar_t buffer[64];
    swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
               st.wSecond);
    return buffer;
}

bool matches_query(const RegistryValue& value, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    std::wstring lowered = lower(value.name);
    if (lowered.find(needle) != std::wstring::npos) {
        return true;
    }
    std::wstring preview = lower(value.preview);
    return preview.find(needle) != std::wstring::npos;
}

ImVec4 with_alpha(const ImVec4& color, float alpha) {
    return ImVec4(color.x, color.y, color.z, color.w * alpha);
}

float ease_out_cubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float approach(float value, float target, float dt, float duration) {
    float step = dt / duration;
    return value < target ? std::min(target, value + step) : std::max(target, value - step);
}

uint64_t tree_key(int hiveIndex, size_t index) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(hiveIndex)) << 32) | static_cast<uint64_t>(index);
}

bool tree_is_open(const AppState& state, int hiveIndex, size_t index, int depth) {
    auto it = state.treeOpen.find(tree_key(hiveIndex, index));
    return it != state.treeOpen.end() ? it->second : depth == 0;
}

std::string ellipsize(std::string text, float maxWidth) {
    if (maxWidth < 24.0f) {
        return std::string();
    }
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
        return text;
    }
    while (!text.empty() && ImGui::CalcTextSize((text + "...").c_str()).x > maxWidth) {
        text.pop_back();
    }
    text += "...";
    return text;
}

std::string hive_file_name(const RegistryHive& hive) {
    std::wstring file = hive.filePath;
    size_t slash = file.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        file = file.substr(slash + 1);
    }
    return to_utf8(file);
}

void draw_key_node(AppState& state, repp::Icons& icons, int hiveIndex, size_t index, int depth, float alpha,
                   float rowW) {
    if (alpha <= 0.02f || depth > 24) {
        return;
    }
    const RegistryKey& key = state.hives[static_cast<size_t>(hiveIndex)].hive.keys[index];
    const std::unordered_map<uint32_t, size_t>& cellToIndex = state.hives[static_cast<size_t>(hiveIndex)].cellToIndex;
    const float rowH = 27.0f;
    const float indent = 15.0f;
    const float padX = 10.0f;
    const float dt = ImGui::GetIO().DeltaTime;

    bool open = tree_is_open(state, hiveIndex, index, depth);
    float& anim = state.openAnim[tree_key(hiveIndex, index)];
    anim = approach(anim, open ? 1.0f : 0.0f, dt, 0.19f);

    bool hasChildren = false;
    size_t childCount = 0;
    for (uint32_t cell : key.subkeys) {
        auto it = cellToIndex.find(cell);
        if (it != cellToIndex.end() && it->second != index) {
            hasChildren = true;
            ++childCount;
        }
    }
    bool selected = static_cast<int>(index) == state.active().selectedKey;

    ImVec2 rowMin = ImGui::GetCursorScreenPos();
    ImVec2 rowMax(rowMin.x + rowW, rowMin.y + rowH);
    ImDrawList* list = ImGui::GetWindowDrawList();
    float midY = (rowMin.y + rowMax.y) * 0.5f;

    for (int level = 1; level <= depth; ++level) {
        float gx = rowMin.x + padX + (level - 1) * indent + 6.0f;
        list->AddLine(ImVec2(gx, rowMin.y), ImVec2(gx, rowMax.y),
                      ImGui::GetColorU32(with_alpha(kHair, 0.85f * alpha)));
    }

    float indentX = padX + static_cast<float>(depth) * indent;
    ImGui::InvisibleButton(("tree_" + std::to_string(hiveIndex) + "_" + std::to_string(index)).c_str(),
                           ImVec2(rowW, rowH));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    float& hover = state.hoverAnim[tree_key(hiveIndex, index)];
    hover = approach(hover, hovered ? 1.0f : 0.0f, dt, 0.11f);

    if (selected) {
        list->AddRectFilled(ImVec2(rowMin.x + 2.0f, rowMin.y + 1.0f), ImVec2(rowMax.x - 2.0f, rowMax.y - 1.0f),
                            ImGui::GetColorU32(with_alpha(kSelect, alpha)), 4.0f);
        list->AddRectFilled(ImVec2(rowMin.x + 2.0f, rowMin.y + 4.0f), ImVec2(rowMin.x + 3.0f, rowMax.y - 4.0f),
                            ImGui::GetColorU32(with_alpha(kAmber, alpha)), 0.5f);
    } else if (hover > 0.02f) {
        list->AddRectFilled(ImVec2(rowMin.x + 2.0f, rowMin.y + 1.0f), ImVec2(rowMax.x - 2.0f, rowMax.y - 1.0f),
                            ImGui::GetColorU32(with_alpha(kHover, hover * alpha)), 4.0f);
    }

    float chevronEase = ease_out_cubic(anim);
    if (hasChildren) {
        float cx = rowMin.x + indentX + 1.5f;
        float ang = chevronEase * 1.5707963f;
        float s = std::sin(ang), co = std::cos(ang);
        auto rot = [&](float x, float y) { return ImVec2(cx + x * co - y * s, midY + x * s + y * co); };
        ImU32 chev = ImGui::GetColorU32(with_alpha(selected ? kAmber : kDim, alpha));
        ImVec2 p0 = rot(-2.4f, -3.4f);
        ImVec2 p1 = rot(1.8f, 0.0f);
        ImVec2 p2 = rot(-2.4f, 3.4f);
        list->AddLine(p0, p1, chev, 1.6f);
        list->AddLine(p1, p2, chev, 1.6f);
    }

    float iconH = 13.0f;
    float iconX = rowMin.x + indentX + 11.0f;
    if (icons.keyLight != 0) {
        ImVec4 tint = selected ? kAmber : (hasChildren ? kDim : kFaint);
        list->AddImage(icons.keyLight, ImVec2(iconX, midY - iconH * 0.5f),
                       ImVec2(iconX + iconH, midY + iconH * 0.5f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       ImGui::GetColorU32(with_alpha(tint, alpha)));
    }

    float textX = iconX + iconH + 7.0f;
    std::string label = ellipsize(to_utf8(key.name), rowMax.x - textX - (hasChildren ? 40.0f : 12.0f));
    ImVec4 textCol = selected || hasChildren ? kText : kDim;
    list->AddText(ImVec2(textX, midY - ImGui::GetFontSize() * 0.5f),
                  ImGui::GetColorU32(with_alpha(textCol, alpha)), label.c_str());

    if (hasChildren) {
        std::string count = std::to_string(childCount);
        float countW = ImGui::CalcTextSize(count.c_str()).x;
        list->AddText(ImVec2(rowMax.x - countW - 12.0f, midY - ImGui::GetFontSize() * 0.5f),
                      ImGui::GetColorU32(with_alpha(kFaint, alpha)), count.c_str());
    }

    if (clicked) {
        if (hasChildren) {
            state.treeOpen[tree_key(hiveIndex, index)] = !open;
        }
        state.active().selectedKey = static_cast<int>(index);
        state.apply_filter();
    }

    if (hasChildren && anim > 0.02f) {
        float childAlpha = alpha * ease_out_cubic(anim);
        for (uint32_t cell : key.subkeys) {
            auto it = cellToIndex.find(cell);
            if (it != cellToIndex.end() && it->second != index) {
                draw_key_node(state, icons, hiveIndex, it->second, depth + 1, childAlpha, rowW);
            }
        }
    }
}

}

bool AppState::load_hive(const std::wstring& path) {
    HiveEntry entry;
    if (!hive_open(entry.hive, path)) {
        status = entry.hive.error;
        return false;
    }
    for (size_t i = 0; i < entry.hive.keys.size(); ++i) {
        entry.cellToIndex[entry.hive.keys[i].cellOffset] = i;
    }
    entry.selectedKey = -1;
    for (size_t i = 0; i < entry.hive.keys.size(); ++i) {
        if (!entry.hive.keys[i].values.empty()) {
            entry.selectedKey = static_cast<int>(i);
            break;
        }
    }
    if (entry.selectedKey < 0 && !entry.hive.keys.empty()) {
        entry.selectedKey = 0;
    }
    std::string openedName = hive_file_name(entry.hive);
    hives.push_back(std::move(entry));
    activeHive = static_cast<int>(hives.size()) - 1;
    if (hives.size() == 1) {
        tab = PanelTab::Tree;
    }
    refresh_after_hive_change();
    status = L"Opened " + from_utf8(openedName);
    return true;
}

void AppState::close_hive(int index) {
    if (index < 0 || static_cast<size_t>(index) >= hives.size()) {
        return;
    }
    const uint64_t hivePart = static_cast<uint64_t>(static_cast<uint32_t>(index));
    auto prune = [hivePart](auto& map) {
        for (auto it = map.begin(); it != map.end();) {
            if ((it->first >> 32) == hivePart) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };
    prune(treeOpen);
    prune(openAnim);
    prune(hoverAnim);
    hives.erase(hives.begin() + index);
    if (hives.empty()) {
        activeHive = -1;
    } else if (activeHive > index) {
        --activeHive;
    } else if (activeHive == index) {
        activeHive = std::min(index, static_cast<int>(hives.size()) - 1);
    }
    refresh_after_hive_change();
    status = L"Closed hive";
}

void AppState::refresh_after_hive_change() {
    scrollToValue = -1;
    apply_filter();
    scan_suspicious();
    if (!hiveOpen()) {
        status = L"no hive loaded";
    }
}

void AppState::scan_suspicious() {
    findings.clear();
    for (size_t h = 0; h < hives.size(); ++h) {
        std::vector<Finding> part = repp::scan_suspicious(hives[h].hive);
        for (Finding& finding : part) {
            finding.hiveIndex = static_cast<int>(h);
            findings.push_back(std::move(finding));
        }
    }
}

void AppState::recompute_search() {
    if (lastSearch == query) {
        return;
    }
    lastSearch = query;
    searchDismissed = false;
    searchHits.clear();
    searchKeyCount = 0;
    searchValueCount = 0;
    searchCursor = 0;
    std::wstring needle = lower(query);
    if (needle.empty()) {
        return;
    }
    const size_t cap = 80;
    for (size_t h = 0; h < hives.size() && searchHits.size() < cap; ++h) {
        const RegistryHive& hive = hives[h].hive;
        for (size_t k = 0; k < hive.keys.size() && searchHits.size() < cap; ++k) {
            if (lower(hive.keys[k].name).find(needle) != std::wstring::npos) {
                ++searchKeyCount;
                searchHits.push_back({static_cast<int>(h), static_cast<int>(k), -1});
            }
        }
    }
    for (size_t h = 0; h < hives.size() && searchHits.size() < cap; ++h) {
        const RegistryHive& hive = hives[h].hive;
        for (size_t k = 0; k < hive.keys.size() && searchHits.size() < cap; ++k) {
            for (uint32_t vi : hive.keys[k].values) {
                if (searchHits.size() >= cap) {
                    break;
                }
                const RegistryValue& value = hive.values[vi];
                bool nameHit = lower(value.name).find(needle) != std::wstring::npos;
                bool dataHit = lower(value.preview).find(needle) != std::wstring::npos;
                if (nameHit || dataHit) {
                    ++searchValueCount;
                    searchHits.push_back({static_cast<int>(h), static_cast<int>(k), static_cast<int>(vi)});
                }
            }
        }
    }
    searchCursor = 0;
}

void AppState::apply_filter() {
    flaggedValueIndices.clear();
    if (!hiveOpen()) {
        searchHits.clear();
        searchKeyCount = 0;
        searchValueCount = 0;
        lastSearch = query;
        return;
    }
    HiveEntry& entry = active();
    entry.visibleValues.clear();
    if (entry.selectedKey >= 0 && static_cast<size_t>(entry.selectedKey) < entry.hive.keys.size()) {
        const RegistryKey& key = entry.hive.keys[static_cast<size_t>(entry.selectedKey)];
        std::wstring needle = lower(query);
        for (uint32_t index : key.values) {
            const RegistryValue& value = entry.hive.values[index];
            if (matches_query(value, needle)) {
                entry.visibleValues.push_back(index);
            }
        }
        for (const Finding& finding : findings) {
            if (finding.hiveIndex != activeHive || finding.keyIndex != entry.selectedKey) {
                continue;
            }
            for (const std::wstring& name : finding.flaggedValues) {
                for (uint32_t vi : key.values) {
                    if (lower(entry.hive.values[vi].name) == lower(name)) {
                        flaggedValueIndices.insert(static_cast<int>(vi));
                    }
                }
            }
        }
    }
    sort_visible();
    recompute_search();
}

void AppState::sort_visible() {
    if (sortColumn.empty()) {
        sortColumn = L"name";
        sortAscending = true;
    }
    if (!hiveOpen()) {
        return;
    }
    HiveEntry& entry = active();
    std::wstring col = lower(sortColumn);
    std::stable_sort(entry.visibleValues.begin(), entry.visibleValues.end(),
                     [this, &col](size_t lhs, size_t rhs) {
                         const RegistryValue& left = hives[static_cast<size_t>(activeHive)].hive.values[lhs];
                         const RegistryValue& right = hives[static_cast<size_t>(activeHive)].hive.values[rhs];
                         int cmp = 0;
                         if (col == L"name") {
                             cmp = lower(left.name).compare(lower(right.name));
                         } else if (col == L"type") {
                             cmp = left.type < right.type ? -1 : (left.type == right.type ? 0 : 1);
                         } else if (col == L"data") {
                             cmp = lower(left.preview).compare(lower(right.preview));
                         }
                         return sortAscending ? cmp < 0 : cmp > 0;
                     });
}

void draw_top_bar(AppState& state, repp::Icons& icons) {
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::BeginChild("topbar", ImVec2(-1.0f, kTopBarH), false, ImGuiWindowFlags_NoScrollbar);

    const float y = (kTopBarH - 34.0f) * 0.5f;
    ImVec2 tilePos = ImVec2(24.0f, y);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(tilePos, ImVec2(tilePos.x + 34.0f, tilePos.y + 34.0f), ImGui::GetColorU32(kAmber), 8.0f);
    if (icons.key != 0) {
        float icon = 22.0f;
        ImVec2 iconMin = ImVec2(tilePos.x + (34.0f - icon) * 0.5f, tilePos.y + (34.0f - icon) * 0.5f);
        draw->AddImage(icons.key, iconMin, ImVec2(iconMin.x + icon, iconMin.y + icon));
    }
    ImGui::SetCursorPos(ImVec2(24.0f, y));
    ImGui::Dummy(ImVec2(34.0f, 34.0f));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Registry Explorer++");
    }

    ImGui::SetCursorPos(ImVec2(70.0f, 10.0f));
    const char* wordmark = "registry";
    ImGui::PushFont(fonts().display);
    ImGui::TextColored(kText, "%s", wordmark);
    float width = ImGui::CalcTextSize(wordmark).x;
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(70.0f + width, 10.0f));
    ImGui::PushFont(fonts().display);
    ImGui::TextColored(kAmber, "explorer++");
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(70.0f, 34.0f));
    ImGui::TextColored(kFaint, "made by diquu");

    float searchW = 320.0f;
    float searchX = ImGui::GetWindowWidth() * 0.5f - searchW * 0.5f;
    if (searchX < 300.0f) {
        searchX = 300.0f;
    }
    float searchY = (kTopBarH - 30.0f) * 0.5f;
    state.searchAnchorX = searchX;
    state.searchAnchorWidth = searchW;

    ImGui::SetCursorPos(ImVec2(searchX, searchY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(31.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, kHair);

    static char queryBuffer[512] = {};
    std::wstring current = state.query;
    if (!current.empty()) {
        std::string utf8 = to_utf8(current);
        size_t copyLen = std::min<size_t>(utf8.size(), sizeof(queryBuffer) - 1);
        memcpy(queryBuffer, utf8.c_str(), copyLen);
        queryBuffer[copyLen] = '\0';
    } else {
        queryBuffer[0] = '\0';
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F)) {
        ImGui::SetKeyboardFocusHere();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D)) {
        state.drawerOpen = !state.drawerOpen;
    }
    ImGui::PushItemWidth(searchW);
    if (ImGui::InputTextWithHint("##search", "Search hives (Ctrl+F)", queryBuffer, sizeof(queryBuffer))) {
        state.query = from_utf8(queryBuffer);
        state.apply_filter();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    if (icons.magnifier != 0) {
        ImVec2 iconPos = ImVec2(searchX + 8.0f, searchY + (30.0f - 15.0f) * 0.5f);
        draw->AddImage(icons.magnifier, iconPos, ImVec2(iconPos.x + 15.0f, iconPos.y + 15.0f));
    }
    if (!state.query.empty()) {
        ImGui::SetCursorPos(ImVec2(searchX + searchW - 24.0f, searchY));
        ImGui::InvisibleButton("clear_query", ImVec2(24.0f, 30.0f));
        bool clearHovered = ImGui::IsItemHovered();
        bool clearClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        if (clearHovered || clearClicked) {
            draw->AddRectFilled(ImVec2(searchX + searchW - 26.0f, searchY + 4.0f),
                                ImVec2(searchX + searchW - 4.0f, searchY + 26.0f),
                                ImGui::GetColorU32(kHover), 4.0f);
        }
        const char* times = "x";
        float timesW = ImGui::CalcTextSize(times).x;
        draw->AddText(ImVec2(searchX + searchW - 15.0f - timesW * 0.5f, searchY + 6.0f),
                      ImGui::GetColorU32(clearHovered ? kText : kDim), times);
        if (clearClicked) {
            state.query.clear();
            state.apply_filter();
        }
    }

    float buttonY = (kTopBarH - 30.0f) * 0.5f;
    float rightX = ImGui::GetWindowWidth() - 24.0f;

    rightX -= 120.0f;
    ImGui::SetCursorPos(ImVec2(rightX, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, state.drawerOpen ? kSelect : kPanel);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSelect);
    ImGui::PushStyleColor(ImGuiCol_Text, state.drawerOpen ? kText : kDim);
    if (ImGui::Button("Details", ImVec2(104.0f, 30.0f))) {
        state.drawerOpen = !state.drawerOpen;
    }
    ImGui::PopStyleColor(4);

    rightX -= 128.0f;
    ImGui::SetCursorPos(ImVec2(rightX, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, kPanel);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kRaised);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    if (ImGui::Button("Open hive", ImVec2(112.0f, 30.0f))) {
        wchar_t file[1024] = {};
        OPENFILENAMEW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.lpstrFilter = L"Registry hive (*.dat;*.hive)\0*.dat;*.hive\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = file;
        dialog.nMaxFile = static_cast<DWORD>(std::size(file));
        dialog.lpstrTitle = L"Open registry hive";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog)) {
            state.load_hive(file);
        }
    }
    ImGui::PopStyleColor(4);
    if (icons.folderOpen != 0) {
        ImVec2 rectMin = ImGui::GetItemRectMin();
        float icon = 15.0f;
        ImVec2 iconMin = ImVec2(rectMin.x + 10.0f, rectMin.y + (30.0f - icon) * 0.5f);
        draw->AddImage(icons.folderOpen, iconMin, ImVec2(iconMin.x + icon, iconMin.y + icon));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Separator, kHair);
    ImGui::Separator();
    ImGui::PopStyleColor();
}

void draw_findings_list(AppState& state, repp::Icons& icons, float width) {
    const float cardH = 64.0f;
    float cardW = width - 16.0f;

    for (size_t i = 0; i < state.findings.size(); ++i) {
        const Finding& finding = state.findings[i];
        ImVec4 sevColor;
        switch (finding.severity) {
            case Severity::Suspicious: sevColor = kSuspiciousColor; break;
            case Severity::Review: sevColor = kAmber; break;
            case Severity::Clean: sevColor = kCleanColor; break;
            default: sevColor = kFaint; break;
        }

        ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY()));
        ImVec2 cardMin = ImGui::GetCursorScreenPos();
        ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);
        ImDrawList* list = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton(("finding_" + std::to_string(i)).c_str(), ImVec2(cardW, cardH));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        list->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(hovered ? kSelect : kRaised), 5.0f);
        list->AddRect(cardMin, cardMax, ImGui::GetColorU32(kHair), 5.0f, 0, 1.0f);

        float iconH = 18.0f;
        float iconY = (cardMin.y + cardMax.y) * 0.5f - iconH * 0.5f;
        if (icons.shieldWarning != 0) {
            list->AddImage(icons.shieldWarning, ImVec2(cardMin.x + 13.0f, iconY),
                           ImVec2(cardMin.x + 13.0f + iconH, iconY + iconH), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           ImGui::GetColorU32(sevColor));
        }

        float textX = cardMin.x + 13.0f + iconH + 11.0f;
        float midY = (cardMin.y + cardMax.y) * 0.5f;
        std::string hiveTag;
        if (finding.hiveIndex >= 0 && finding.hiveIndex < static_cast<int>(state.hives.size()) &&
            state.hives.size() > 1) {
            hiveTag = hive_file_name(state.hives[static_cast<size_t>(finding.hiveIndex)].hive);
            for (char& c : hiveTag) {
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            }
        }
        float tagW = hiveTag.empty() ? 0.0f : ImGui::CalcTextSize(hiveTag.c_str()).x + 8.0f;
        std::string rule = ellipsize(to_utf8(finding.rule), cardMax.x - textX - tagW - 12.0f);
        ImGui::PushFont(fonts().semibold);
        list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textX, midY - ImGui::GetFontSize() - 3.0f),
                      ImGui::GetColorU32(kText), rule.c_str());
        ImGui::PopFont();
        if (!hiveTag.empty()) {
            ImGui::PushFont(fonts().mono);
            list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2(cardMax.x - tagW - 4.0f, midY - ImGui::GetFontSize() - 3.0f),
                          ImGui::GetColorU32(kFaint), hiveTag.c_str());
            ImGui::PopFont();
        }

        std::wstring evidence = finding.evidence;
        size_t nl = evidence.find(L'\n');
        if (nl != std::wstring::npos) {
            evidence.resize(nl);
        }
        ImVec4 statusColor;
        switch (finding.severity) {
            case Severity::Suspicious: statusColor = kSuspiciousColor; break;
            case Severity::Review: statusColor = kAmber; break;
            case Severity::Clean: statusColor = kCleanColor; break;
            default: statusColor = kDim; break;
        }
        std::string statusText = to_utf8(finding.status);
        std::string evidenceText = ellipsize(to_utf8(evidence), cardMax.x - textX - ImGui::CalcTextSize(statusText.c_str()).x - 54.0f);
        list->AddText(ImVec2(textX, midY + 2.0f), ImGui::GetColorU32(statusColor), statusText.c_str());
        float statusW = ImGui::CalcTextSize(statusText.c_str()).x;
        list->AddText(ImVec2(textX + statusW + 8.0f, midY + 2.0f), ImGui::GetColorU32(kFaint), evidenceText.c_str());

        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(430.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, kText);
            ImGui::TextWrapped("%s", to_utf8(finding.evidence).c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        if (clicked && finding.keyIndex >= 0) {
            if (finding.hiveIndex >= 0 && finding.hiveIndex < static_cast<int>(state.hives.size())) {
                state.activeHive = finding.hiveIndex;
            }
            state.active().selectedKey = finding.keyIndex;
            state.tab = AppState::Tree;
            state.scrollToValue = -1;
            state.apply_filter();
            state.status = L"Jumped to key";
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }
}

void draw_brand_splash(AppState& state, repp::Icons& icons, float width, const char* caption) {
    float centerX = width * 0.5f;
    float blockY = ImGui::GetCursorPosY() + 42.0f;

    float iconH = 44.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImTextureID splashIcon = icons.keyLight != 0 ? icons.keyLight : icons.key;
    if (splashIcon != 0) {
        ImGui::GetWindowDrawList()->AddImage(splashIcon,
                                              ImVec2(origin.x + centerX - iconH * 0.5f, origin.y + blockY),
                                              ImVec2(origin.x + centerX + iconH * 0.5f,
                                                     origin.y + blockY + iconH),
                                              ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                              ImGui::GetColorU32(with_alpha(kDim, 0.6f)));
    }

    std::string line1 = "registry explorer++";
    float w1 = ImGui::CalcTextSize(line1.c_str()).x;
    ImGui::SetCursorPos(ImVec2(centerX - w1 * 0.5f, blockY + 56.0f));
    ImGui::PushFont(fonts().semibold);
    ImGui::TextColored(kDim, "%s", line1.c_str());
    ImGui::PopFont();

    float captionW = ImGui::CalcTextSize(caption).x;
    ImGui::SetCursorPos(ImVec2(centerX - captionW * 0.5f, blockY + 78.0f));
    ImGui::TextColored(kFaint, "%s", caption);

    std::string credit = "made by diquu";
    float creditW = ImGui::CalcTextSize(credit.c_str()).x;
    ImGui::SetCursorPos(ImVec2(centerX - creditW * 0.5f, blockY + 104.0f));
    ImGui::TextColored(kFaint, "%s", credit.c_str());
    (void)state;
}

void draw_tree_panel(AppState& state, repp::Icons& icons, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::BeginChild("tree", ImVec2(width, 0.0f), false, ImGuiWindowFlags_NoScrollbar);

    int suspiciousCount = 0;
    for (const Finding& finding : state.findings) {
        if (finding.severity == Severity::Suspicious) {
            ++suspiciousCount;
        }
    }

    const float tabY = 12.0f;
    const float tabH = 30.0f;
    float tabW = (width - 24.0f - 6.0f) * 0.5f;

    auto tabButton = [&](const char* label, ImTextureID icon, int index) {
        bool active = static_cast<int>(state.tab) == index;
        float x = ImGui::GetCursorPosX();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(label, ImVec2(tabW, tabH));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImDrawList* list = ImGui::GetWindowDrawList();
        ImVec2 bMin(origin.x, origin.y);
        ImVec2 bMax(origin.x + tabW, origin.y + tabH);
        if (active) {
            list->AddRectFilled(bMin, bMax, ImGui::GetColorU32(kSelect), 4.0f);
        } else if (hovered) {
            list->AddRectFilled(bMin, bMax, ImGui::GetColorU32(kHover), 4.0f);
        }

        std::string badge;
        if (index == 1 && suspiciousCount > 0) {
            badge = std::to_string(suspiciousCount);
        }
        std::string text = label;
        ImVec4 iconTint = active ? kAmber : kDim;
        ImVec4 textTint = active ? kText : kDim;
        float iconH = 15.0f;
        float textW = ImGui::CalcTextSize(text.c_str()).x;
        float badgeW = badge.empty() ? 0.0f : ImGui::CalcTextSize(badge.c_str()).x + 9.0f;
        float total = iconH + 6.0f + textW + badgeW;
        float startX = origin.x + (tabW - total) * 0.5f;
        float midY = (bMin.y + bMax.y) * 0.5f;
        if (icon != 0) {
            list->AddImage(icon, ImVec2(startX, midY - iconH * 0.5f), ImVec2(startX + iconH, midY + iconH * 0.5f),
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(iconTint));
        }
        float textX = startX + iconH + 6.0f;
        list->AddText(ImVec2(textX, midY - ImGui::GetFontSize() * 0.5f), ImGui::GetColorU32(textTint),
                      text.c_str());
        if (!badge.empty()) {
            float pulse = 0.72f + 0.28f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.1f);
            ImU32 badgeCol = ImGui::GetColorU32(with_alpha(kSuspiciousColor, pulse));
            float badgeH = 16.0f;
            float badgeX = textX + textW + 8.0f;
            list->AddRectFilled(ImVec2(badgeX, midY - badgeH * 0.5f),
                                ImVec2(badgeX + ImGui::CalcTextSize(badge.c_str()).x + 9.0f, midY + badgeH * 0.5f),
                                badgeCol, badgeH * 0.5f);
            list->AddText(ImVec2(badgeX + 4.5f, midY - ImGui::GetFontSize() * 0.5f),
                          ImGui::GetColorU32(kText), badge.c_str());
        }
        if (clicked) {
            state.tab = static_cast<AppState::PanelTab>(index);
        }
        ImGui::SetCursorPos(ImVec2(x + tabW + 6.0f, tabY));
    };

    ImGui::SetCursorPos(ImVec2(12.0f, tabY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
    tabButton("TREE", icons.treeStructure, 0);
    tabButton("SUSPICIOUS", icons.shieldWarning, 1);
    ImGui::PopStyleVar();

    // hive switcher (scales to any number of open hives)
    float captionY = tabY + tabH + 7.0f;
    {
        ImGui::SetCursorPos(ImVec2(12.0f, captionY));
        float btnW = width - 24.0f;
        float btnH = 24.0f;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("hive_switcher", ImVec2(btnW, btnH));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImDrawList* list = ImGui::GetWindowDrawList();
        ImVec2 bMin(origin.x, origin.y);
        ImVec2 bMax(origin.x + btnW, origin.y + btnH);
        list->AddRectFilled(bMin, bMax, ImGui::GetColorU32(hovered ? kHover : kRaised), 4.0f);
        if (icons.keyLight != 0) {
            float iconH = 13.0f;
            list->AddImage(icons.keyLight, ImVec2(bMin.x + 9.0f, (bMin.y + bMax.y) * 0.5f - iconH * 0.5f),
                           ImVec2(bMin.x + 9.0f + iconH, (bMin.y + bMax.y) * 0.5f + iconH * 0.5f),
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(kAmber));
        }
        std::string label = state.hiveOpen() ? ellipsize(hive_file_name(state.cur_hive()), btnW - 66.0f)
                                             : std::string("no hive loaded");
        ImGui::PushFont(fonts().mono);
        list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(bMin.x + 28.0f, (bMin.y + bMax.y) * 0.5f - ImGui::GetFontSize() * 0.5f),
                      ImGui::GetColorU32(state.hiveOpen() ? kText : kFaint), label.c_str());
        ImGui::PopFont();
        float rightX = bMax.x - 10.0f;
        if (state.hives.size() > 1) {
            std::string count = std::to_string(state.hives.size());
            float countW = ImGui::CalcTextSize(count.c_str()).x + 10.0f;
            list->AddRectFilled(ImVec2(rightX - countW, (bMin.y + bMax.y) * 0.5f - 8.0f),
                                ImVec2(rightX, (bMin.y + bMax.y) * 0.5f + 8.0f),
                                ImGui::GetColorU32(kSelect), 8.0f);
            list->AddText(ImVec2(rightX - countW + 5.0f, (bMin.y + bMax.y) * 0.5f - ImGui::GetFontSize() * 0.5f),
                          ImGui::GetColorU32(kText), count.c_str());
            rightX -= countW + 8.0f;
        }
        // chevron-down stroke
        float cy = (bMin.y + bMax.y) * 0.5f;
        float cx = rightX - 6.0f;
        list->AddLine(ImVec2(cx - 4.0f, cy - 1.5f), ImVec2(cx, cy + 2.5f), ImGui::GetColorU32(kDim), 1.6f);
        list->AddLine(ImVec2(cx, cy + 2.5f), ImVec2(cx + 4.0f, cy - 1.5f), ImGui::GetColorU32(kDim), 1.6f);
        if (clicked) {
            ImGui::OpenPopup("hive_switcher_popup");
        }
        if (hovered) {
            ImGui::SetTooltip("%zu hive(s) open - click to switch, close or add", state.hives.size());
        }
    }
    if (ImGui::BeginPopup("hive_switcher_popup", ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::TextColored(kFaint, "OPEN HIVES");
        for (size_t h = 0; h < state.hives.size(); ++h) {
            std::string file = hive_file_name(state.hives[h].hive);
            bool activeChip = static_cast<int>(h) == state.activeHive;
            std::string rowId = "hive_row_" + std::to_string(h);
            ImGui::PushID(rowId.c_str());
            ImGui::PushStyleColor(ImGuiCol_Button, activeChip ? kSelect : kPanel);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSelect);
            std::string rowText = (activeChip ? "> " : "  ") + file;
            if (ImGui::Button(rowText.c_str(), ImVec2(196.0f, 26.0f))) {
                if (!activeChip) {
                    state.activeHive = static_cast<int>(h);
                    state.scrollToValue = -1;
                    state.apply_filter();
                    state.status = L"Switched hive";
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, kPanel);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHover);
            ImGui::PushStyleColor(ImGuiCol_Text, kDim);
            if (ImGui::Button("x", ImVec2(26.0f, 26.0f))) {
                state.close_hive(static_cast<int>(h));
                ImGui::CloseCurrentPopup();
                ImGui::PopID();
                ImGui::PopStyleColor(4);
                continue;
            }
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, kPanel);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHover);
        ImGui::PushStyleColor(ImGuiCol_Text, kText);
        if (ImGui::Button("+ Add hives...", ImVec2(226.0f, 26.0f))) {
            wchar_t files[16384] = {};
            OPENFILENAMEW dialog = {};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFilter = L"Registry hive (*.dat;*.hive)\0*.dat;*.hive\0All files (*.*)\0*.*\0";
            dialog.lpstrFile = files;
            dialog.nMaxFile = static_cast<DWORD>(std::size(files));
            dialog.lpstrTitle = L"Open registry hives (multi-select)";
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT |
                           OFN_EXPLORER;
            if (GetOpenFileNameW(&dialog)) {
                if (files[wcslen(files) + 1] == L'\0') {
                    state.load_hive(files);
                } else {
                    const wchar_t* cursor = files;
                    std::wstring dir = cursor;
                    cursor += dir.size() + 1;
                    while (*cursor) {
                        state.load_hive(dir + L"\\" + cursor);
                        cursor += wcslen(cursor) + 1;
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, captionY + 15.0f));
    {
        ImVec2 win = ImGui::GetWindowPos();
        float lineY = ImGui::GetCursorScreenPos().y;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(win.x + 13.0f, lineY), ImVec2(win.x + width - 13.0f, lineY),
                                            ImGui::GetColorU32(kHair));
    }

    float scrollY = captionY + 22.0f;
    ImGui::SetCursorPos(ImVec2(0.0f, scrollY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (state.tab == AppState::Suspicious) {
        ImGui::BeginChild("findings_scroll", ImVec2(-1.0f, ImGui::GetContentRegionAvail().y), false);
        if (state.findings.empty()) {
            if (!state.hiveOpen()) {
                draw_brand_splash(state, icons, width, "offline hive inspector");
            } else {
                float iconH = 38.0f;
                float centerX = width * 0.5f;
                float blockY = ImGui::GetContentRegionAvail().y * 0.5f - 60.0f;
                ImVec2 origin = ImGui::GetCursorScreenPos();
                if (icons.shieldCheck != 0) {
                    ImGui::GetWindowDrawList()->AddImage(
                        icons.shieldCheck,
                        ImVec2(origin.x + centerX - iconH * 0.5f, origin.y + blockY),
                        ImVec2(origin.x + centerX + iconH * 0.5f, origin.y + blockY + iconH),
                        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                        ImGui::GetColorU32(with_alpha(kCleanColor, 0.85f)));
                }
                std::string line = state.hives.size() > 1 ? "nothing to flag in any open hive" : "nothing to flag in this hive";
                float lw = ImGui::CalcTextSize(line.c_str()).x;
                ImGui::SetCursorPos(ImVec2(centerX - lw * 0.5f, blockY + 50.0f));
                ImGui::TextColored(kCleanColor, "%s", line.c_str());
                std::string credit = "made by diquu";
                float cw = ImGui::CalcTextSize(credit.c_str()).x;
                ImGui::SetCursorPos(ImVec2(centerX - cw * 0.5f, blockY + 74.0f));
                ImGui::TextColored(kFaint, "%s", credit.c_str());
            }
        } else {
            draw_findings_list(state, icons, width);
        }
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("tree_scroll", ImVec2(-1.0f, ImGui::GetContentRegionAvail().y), false);
        float rowW = ImGui::GetContentRegionAvail().x;
        if (!state.hiveOpen()) {
            draw_brand_splash(state, icons, width, "offline hive inspector");
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            const RegistryHive& hive = state.cur_hive();
            for (size_t i = 0; i < hive.keys.size(); ++i) {
                if (hive.keys[i].cellOffset == hive.rootOffset) {
                    draw_key_node(state, icons, state.activeHive, i, 0, 1.0f, rowW);
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
    }
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_values_panel(AppState& state, repp::Icons& icons) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::BeginChild("values", ImVec2(0.0f, 0.0f), false);

    const float headerH = 34.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    const float numW = 52.0f;
    const float nameW = std::min(250.0f, std::max(150.0f, avail * 0.26f));
    const float typeW = 150.0f;
    const float dataW = std::max(120.0f, avail - numW - nameW - typeW);
    const float nameX = numW;
    const float typeX = numW + nameW;
    const float dataX = typeX + typeW;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::BeginChild("values_header", ImVec2(-1.0f, headerH), false, ImGuiWindowFlags_NoScrollbar);

    auto sortHeader = [&](const char* label, float width, const char* column) {
        bool active = lower(state.sortColumn) == lower(column);
        std::string text = label;
        if (active) {
            text += state.sortAscending ? "  ^" : "  v";
        }
        ImGui::PushStyleColor(ImGuiCol_Button, active ? kSelect : kPanel);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSelect);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? kText : kFaint);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
        ImGui::PushID(label);
        if (ImGui::Button(text.c_str(), ImVec2(width, headerH))) {
            std::wstring col = from_utf8(column);
            if (lower(state.sortColumn) == lower(col)) {
                state.sortAscending = !state.sortAscending;
            } else {
                state.sortColumn = col;
                state.sortAscending = true;
            }
            state.sort_visible();
        }
        ImGui::PopID();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    };

    sortHeader("#", numW, "id");
    ImGui::SameLine(0.0f, 0.0f);
    sortHeader("NAME", nameW, "name");
    ImGui::SameLine(0.0f, 0.0f);
    sortHeader("TYPE", typeW, "type");
    ImGui::SameLine(0.0f, 0.0f);
    sortHeader("DATA", dataW, "data");

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Separator, kHair);
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(0.0f, headerH + 1.0f));
    ImGui::BeginChild("values_rows", ImVec2(-1.0f, ImGui::GetContentRegionAvail().y), false);

    if (!state.hiveOpen()) {
        draw_brand_splash(state, icons, ImGui::GetContentRegionAvail().x,
                          "open a .dat hive with the toolbar to inspect its values");
    } else if (state.active().selectedKey < 0) {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextColored(kDim, "select a key on the left");
        ImGui::SetCursorPos(ImVec2(24.0f, 46.0f));
        ImGui::TextColored(kFaint, "its values will appear here");
    } else if (state.active().visibleValues.empty()) {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        if (state.query.empty()) {
            ImGui::TextColored(kDim, "this key has no values");
        } else {
            ImGui::TextColored(kDim, "no values match your search");
            ImGui::SetCursorPos(ImVec2(24.0f, 48.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, kRaised);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHover);
            ImGui::PushStyleColor(ImGuiCol_Text, kText);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
            if (ImGui::Button("Clear search")) {
                state.query.clear();
                state.apply_filter();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
    } else {
        const float rowH = 54.0f;
        float contentW = ImGui::GetContentRegionAvail().x;
        const HiveEntry& entry = state.active();
        const RegistryHive& hive = entry.hive;

        ImVec2 origin = ImGui::GetCursorScreenPos();

        for (size_t i = 0; i < entry.visibleValues.size(); ++i) {
            const RegistryValue& value = hive.values[entry.visibleValues[i]];
            const int globalIndex = static_cast<int>(entry.visibleValues[i]);
            float y = static_cast<float>(i) * rowH;
            ImVec2 rowMin(origin.x, origin.y + y);
            ImVec2 rowMax(origin.x + contentW, origin.y + y + rowH);

            ImDrawList* list = ImGui::GetWindowDrawList();
            bool flagged = state.flaggedValueIndices.count(globalIndex) != 0;
            bool matchesSearch = !state.query.empty() &&
                                 lower(value.name).find(lower(state.query)) != std::wstring::npos;
            if ((i & 1) != 0) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.025f)));
            }
            if (matchesSearch) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(with_alpha(kAccent, 0.10f)));
            }
            if (flagged) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(with_alpha(kAmber, 0.16f)));
            }
            if (ImGui::InvisibleButton(("row" + std::to_string(i)).c_str(), ImVec2(contentW, rowH))) {
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(kHover));
            }
            if (state.scrollToValue == globalIndex) {
                list->AddRect(rowMin, rowMax, ImGui::GetColorU32(kAmber), 2.0f, 0, 2.0f);
                ImGui::SetScrollHereY(0.35f);
                state.scrollToValue = -1;
            }

            list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(rowMin.x + 18.0f, rowMin.y + 19.0f),
                          ImGui::GetColorU32(kFaint), std::to_string(i + 1).c_str());

            auto ellipsize = [](std::string s, float maxWidth) {
                if (maxWidth < 40.0f) {
                    return s;
                }
                if (ImGui::CalcTextSize(s.c_str()).x <= maxWidth) {
                    return s;
                }
                while (!s.empty() && ImGui::CalcTextSize((s + "...").c_str()).x > maxWidth) {
                    s.pop_back();
                }
                s += "...";
                return s;
            };

            std::string name = ellipsize(to_utf8(value.name), nameW - 16.0f);
            list->AddText(ImVec2(rowMin.x + nameX, rowMin.y + 19.0f), ImGui::GetColorU32(kText), name.c_str());

            std::string type = to_utf8(value_type_name(value.type));
            ImVec4 typeColor = kDim;
            if (value.type == 4 || value.type == 5 || value.type == 11) {
                typeColor = kAmber;
            } else if (value.type == 3) {
                typeColor = kFaint;
            }
            list->AddText(ImVec2(rowMin.x + typeX, rowMin.y + 19.0f), ImGui::GetColorU32(typeColor),
                          type.c_str());

            std::string data = to_utf8(value.preview);
            if (data.size() > 96) {
                data.resize(95);
            }
            float dataLimit = contentW - dataX - (flagged ? 78.0f : 16.0f);
            data = ellipsize(data, dataLimit);
            list->AddText(ImVec2(rowMin.x + dataX, rowMin.y + 19.0f), ImGui::GetColorU32(kText), data.c_str());

            if (flagged) {
                const char* tag = "FLAG";
                float tagW = ImGui::CalcTextSize(tag).x + 12.0f;
                float tagH = 17.0f;
                float tagX = rowMax.x - tagW - 10.0f;
                float tagY = rowMin.y + (rowH - tagH) * 0.5f;
                list->AddRectFilled(ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH),
                                    ImGui::GetColorU32(kAmber), tagH * 0.5f);
                list->AddText(ImVec2(tagX + 6.0f, tagY + (tagH - ImGui::GetFontSize()) * 0.5f),
                              ImGui::GetColorU32(ImVec4(0.16f, 0.11f, 0.02f, 1.0f)), tag);
            }

            if (hovered) {
                ImGui::SetTooltip("%s", to_utf8(value.preview).c_str());
            }
            if (ImGui::BeginPopupContextItem(("ctx" + std::to_string(i)).c_str())) {
                if (ImGui::MenuItem("Copy value name")) {
                    ImGui::SetClipboardText(to_utf8(value.name).c_str());
                }
                if (ImGui::MenuItem("Copy value data")) {
                    ImGui::SetClipboardText(to_utf8(value.preview).c_str());
                }
    const RegistryKey& key = state.hives[static_cast<size_t>(state.activeHive)].hive
                                 .keys[static_cast<size_t>(state.active().selectedKey)];
                if (ImGui::MenuItem("Copy key path")) {
                    ImGui::SetClipboardText(to_utf8(key.path).c_str());
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_drawer(AppState& state, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::BeginChild("drawer", ImVec2(width, 0.0f), false);
    ImGui::PushStyleColor(ImGuiCol_Separator, kHair);
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(width - 40.0f, 14.0f));
    if (ImGui::Button("x", ImVec2(26.0f, 24.0f))) {
        state.drawerOpen = false;
    }

    ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
    ImGui::TextColored(kFaint, "KEY");

    if (state.hiveOpen() && state.active().selectedKey >= 0 &&
        static_cast<size_t>(state.active().selectedKey) < state.cur_hive().keys.size()) {
        const RegistryKey& key = state.cur_hive().keys[static_cast<size_t>(state.active().selectedKey)];

        std::string path = to_utf8(key.path);
        ImVec2 boxMin = ImVec2(16.0f, 36.0f);
        float boxW = width - 32.0f;
        float boxH = 118.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ImGui::GetWindowPos().x + boxMin.x,
                                                          ImGui::GetWindowPos().y + boxMin.y),
                                                    ImVec2(ImGui::GetWindowPos().x + boxMin.x + boxW,
                                                           ImGui::GetWindowPos().y + boxMin.y + boxH),
                                                    ImGui::GetColorU32(kSelect), 2.0f);
        ImGui::SetCursorPos(ImVec2(28.0f, 48.0f));
        ImGui::PushFont(fonts().mono);
        ImGui::PushTextWrapPos(width - 28.0f);
        ImGui::TextColored(kText, "%s", path.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(16.0f, 168.0f));
        ImGui::TextColored(kFaint, "LAST WRITE");
        ImGui::SetCursorPos(ImVec2(16.0f, 186.0f));
        ImGui::TextColored(kText, "%s", to_utf8(format_filetime(key.lastWrite)).c_str());

        ImGui::SetCursorPos(ImVec2(16.0f, 208.0f));
        ImGui::TextColored(kFaint, "FLAGS");
        ImGui::SetCursorPos(ImVec2(16.0f, 226.0f));
        wchar_t flags[32];
        swprintf_s(flags, L"0x%08X", key.flags);
        ImGui::TextColored(kText, "%s", to_utf8(flags).c_str());

        ImGui::SetCursorPos(ImVec2(16.0f, 248.0f));
        ImGui::TextColored(kFaint, "SUBKEYS / VALUES");
        ImGui::SetCursorPos(ImVec2(16.0f, 266.0f));
        ImGui::TextColored(kText, "%zu  /  %zu", key.subkeys.size(), key.values.size());

        ImGui::SetCursorPos(ImVec2(16.0f, 300.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, kHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSelect);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSelect);
        ImGui::PushStyleColor(ImGuiCol_Text, kText);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));
        if (ImGui::Button("Copy key path")) {
            ImGui::SetClipboardText(path.c_str());
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    } else {
        ImGui::SetCursorPos(ImVec2(16.0f, 34.0f));
        ImGui::TextColored(kDim, "no key selected");
    }

    float footerY = ImGui::GetWindowHeight() - 40.0f;
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(winPos.x + 16.0f, winPos.y + footerY),
                                         ImVec2(winPos.x + winSize.x - 16.0f, winPos.y + footerY),
                                         ImGui::GetColorU32(kHair));
    ImGui::SetCursorPos(ImVec2(16.0f, footerY + 11.0f));
    ImGui::TextColored(kFaint, "registry explorer++  -  made by diquu");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_search_dropdown(AppState& state, repp::Icons& icons) {
    if (state.query.empty() || state.searchDismissed) {
        return;
    }
    const float rowH = 32.0f;
    const float headerH = 28.0f;
    const float listW = state.searchAnchorWidth;
    const size_t maxRows = 9;
    size_t rows = state.searchHits.size();
    float listH = std::min(static_cast<float>(rows), static_cast<float>(maxRows)) * rowH;
    float height = headerH + listH + 10.0f;
    if (rows == 0) {
        height = headerH + 34.0f;
    }

    int hits = static_cast<int>(rows);
    if (ImGui::Shortcut(ImGuiKey_DownArrow) && hits > 0) {
        state.searchCursor = std::min(state.searchCursor + 1, hits - 1);
    }
    if (ImGui::Shortcut(ImGuiKey_UpArrow) && hits > 0) {
        state.searchCursor = std::max(state.searchCursor - 1, 0);
    }
    bool enter = ImGui::Shortcut(ImGuiKey_Enter);
    if (ImGui::Shortcut(ImGuiKey_Escape)) {
        state.searchDismissed = true;
        return;
    }

    auto activate = [&](const SearchHit& hit) {
        if (hit.hiveIndex >= 0 && hit.hiveIndex < static_cast<int>(state.hives.size()) &&
            hit.hiveIndex != state.activeHive) {
            state.activeHive = hit.hiveIndex;
            state.scrollToValue = -1;
            state.apply_filter();
        }
        state.active().selectedKey = hit.keyIndex;
        state.tab = AppState::Tree;
        state.scrollToValue = hit.valueIndex >= 0 ? hit.valueIndex : -1;
        state.apply_filter();
        state.searchDismissed = true;
        state.status = hit.valueIndex >= 0 ? L"Jumped to value" : L"Jumped to key";
    };

    ImGui::SetCursorPos(ImVec2(state.searchAnchorX, kTopBarH + 2.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, kHair);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::BeginChild("search_results", ImVec2(listW, height), true, ImGuiWindowFlags_NoScrollbar);

    std::string header = std::to_string(rows) + " match" + (rows == 1 ? "" : "es") + "  ·  " +
                         std::to_string(state.searchKeyCount) + " keys  ·  " +
                         std::to_string(state.searchValueCount) + " values";
    ImGui::SetCursorPos(ImVec2(10.0f, 4.0f));
    ImGui::TextColored(kFaint, "%s", header.c_str());
    const char* hint = "enter jumps";
    float hintW = ImGui::CalcTextSize(hint).x;
    ImGui::SetCursorPos(ImVec2(listW - hintW - 10.0f, 4.0f));
    ImGui::TextColored(kFaint, "%s", hint);
    {
        ImVec2 win = ImGui::GetWindowPos();
        float lineY = win.y + headerH - 2.0f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(win.x + 10.0f, lineY), ImVec2(win.x + listW - 10.0f, lineY),
                                            ImGui::GetColorU32(kHair));
    }

    if (rows == 0) {
        ImGui::SetCursorPos(ImVec2(10.0f, headerH + 8.0f));
        ImGui::TextColored(kFaint, "no matches in the open hive(s)");
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::SetCursorPos(ImVec2(6.0f, headerH + 6.0f));
        ImGui::BeginChild("hits", ImVec2(listW - 12.0f, listH), false);
        float rowW = ImGui::GetContentRegionAvail().x;
        for (size_t i = 0; i < rows; ++i) {
            const SearchHit& hit = state.searchHits[i];
            const RegistryHive& hive = state.hives[static_cast<size_t>(hit.hiveIndex)].hive;
            const RegistryKey& key = hive.keys[static_cast<size_t>(hit.keyIndex)];
            bool activeRow = static_cast<int>(i) == state.searchCursor;

            ImVec2 rowMin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(("hit_" + std::to_string(i)).c_str(), ImVec2(rowW, rowH));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            ImDrawList* list = ImGui::GetWindowDrawList();
            ImVec2 rowMax(rowMin.x + rowW, rowMin.y + rowH);
            if (activeRow) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(kSelect), 4.0f);
            } else if (hovered) {
                list->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(kHover), 4.0f);
            }
            float midY = (rowMin.y + rowMax.y) * 0.5f;
            float iconX = rowMin.x + 8.0f;
            float iconH = 13.0f;

            std::string nameText;
            std::string contextText;
            if (hit.valueIndex >= 0) {
                const RegistryValue& value = hive.values[static_cast<size_t>(hit.valueIndex)];
                ImVec2 chipMin(iconX, midY - 8.0f);
                ImVec2 chipMax(iconX + 16.0f, midY + 8.0f);
                list->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(kAccentSoft), 4.0f);
                std::string ab = "ab";
                float abW = ImGui::CalcTextSize(ab.c_str()).x;
                list->AddText(ImVec2(chipMin.x + (16.0f - abW) * 0.5f, midY - ImGui::GetFontSize() * 0.5f),
                              ImGui::GetColorU32(kAccent), ab.c_str());
                nameText = to_utf8(value.name);
                if (nameText.empty()) {
                    nameText = "(default)";
                }
                contextText = "@ " + to_utf8(key.name);
            } else {
                if (icons.keyLight != 0) {
                    list->AddImage(icons.keyLight, ImVec2(iconX + 1.5f, midY - iconH * 0.5f),
                                   ImVec2(iconX + iconH + 1.5f, midY + iconH * 0.5f), ImVec2(0.0f, 0.0f),
                                   ImVec2(1.0f, 1.0f), ImGui::GetColorU32(kAccent));
                }
                nameText = to_utf8(key.name);
                std::wstring path = key.path;
                size_t slash = path.find_last_of(L'\\');
                contextText = slash != std::wstring::npos ? to_utf8(path.substr(0, slash)) : to_utf8(path);
            }

            std::string hiveTag;
            if (state.hives.size() > 1) {
                hiveTag = hive_file_name(hive);
                for (char& c : hiveTag) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            }
            float tagW = hiveTag.empty() ? 0.0f : ImGui::CalcTextSize(hiveTag.c_str()).x + 10.0f;
            float nameX = iconX + 24.0f;
            float maxTextW = rowMax.x - nameX - tagW - 12.0f;
            std::string name = ellipsize(nameText, maxTextW);
            std::string context = ellipsize(contextText, maxTextW);
            list->AddText(ImVec2(nameX, midY - ImGui::GetFontSize() + 2.0f), ImGui::GetColorU32(kText),
                          name.c_str());
            list->AddText(ImVec2(nameX, midY + 2.0f), ImGui::GetColorU32(kFaint), context.c_str());
            if (!hiveTag.empty()) {
                ImGui::PushFont(fonts().mono);
                list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                              ImVec2(rowMax.x - tagW - 4.0f, midY - ImGui::GetFontSize() * 0.5f),
                              ImGui::GetColorU32(kFaint), hiveTag.c_str());
                ImGui::PopFont();
            }
            if (clicked || (activeRow && enter)) {
                activate(hit);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::EndChild();
}

void draw_status_bar(AppState& state, float contentHeight) {
    ImGui::SetCursorPos(ImVec2(0.0f, contentHeight - kStatusH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Separator, kHair);
    ImGui::BeginChild("statusbar", ImVec2(-1.0f, kStatusH), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(24.0f, (kStatusH - 14.0f) * 0.5f));
    ImGui::TextColored(kDim, "made by diquu");

    std::string right = "ready";
    if (state.hiveOpen()) {
        right = hive_file_name(state.cur_hive()) + "  |  " + std::to_string(state.cur_hive().keys.size()) +
                " keys  /  " + std::to_string(state.cur_hive().values.size()) + " values";
        if (state.hives.size() > 1) {
            right += "  |  " + std::to_string(state.hives.size()) + " hives";
        }
    }
    float width = ImGui::CalcTextSize(right.c_str()).x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - width - 24.0f,
                               (kStatusH - 14.0f) * 0.5f));
    ImGui::TextColored(kFaint, "%s", right.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}
