#include "app.h"
#include "text.h"

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

#include <algorithm>
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

std::wstring csv_escape(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t c : value) {
        if (c == L'\"') {
            out += L"\"\"";
        } else if (c == L'\n') {
            out += L"\\n";
        } else if (c == L'\r') {
            out += L"\\r";
        } else {
            out += c;
        }
    }
    out += L"\"";
    return out;
}

std::wstring hex_bytes_csv(const uint8_t* data, size_t size) {
    std::wstring out;
    size_t shown = std::min<size_t>(size, 2048);
    for (size_t i = 0; i < shown; ++i) {
        wchar_t pair[4];
        swprintf_s(pair, L"%02X ", data[i]);
        out += pair;
    }
    if (size > shown) {
        out += L"...";
    }
    return out;
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

bool export_hive_csv(const RegistryHive& hive, const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::wstring csv = L"\uFEFFkey path,value name,type,data\n";
    for (const RegistryKey& key : hive.keys) {
        for (uint32_t index : key.values) {
            const RegistryValue& value = hive.values[index];
            std::wstring type = value_type_name(value.type);
            std::wstring data = (value.type == 3 || value.type == 8 || value.type == 9 || value.type == 10)
                                    ? hex_bytes_csv(value.data.data(), value.data.size())
                                    : value.preview;
            csv += csv_escape(key.path) + L"," + csv_escape(value.name) + L"," + csv_escape(type) + L"," +
                   csv_escape(data) + L"\n";
        }
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, csv.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed > 1) {
        std::string utf8;
        utf8.resize(static_cast<size_t>(needed - 1));
        WideCharToMultiByte(CP_UTF8, 0, csv.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
        DWORD written = 0;
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
    CloseHandle(file);
    return true;
}

void draw_key_node(AppState& state, size_t index, int depth) {
    const RegistryKey& key = state.hive.keys[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding |
                               ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (depth == 0) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    bool selected = static_cast<int>(index) == state.selectedKey;
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    bool hasChildren = false;
    for (uint32_t cell : key.subkeys) {
        auto it = state.cellToIndex.find(cell);
        if (it != state.cellToIndex.end() && it->second != index) {
            hasChildren = true;
            break;
        }
    }
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    std::string label = to_utf8(key.name);
    if (label.size() > 34) {
        label.resize(34);
        label += "...";
    }
    label += "##" + std::to_string(index);
    ImGui::PushStyleColor(ImGuiCol_Text, hasChildren ? kText : kDim);
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    ImGui::PopStyleColor();
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        state.selectedKey = static_cast<int>(index);
        state.apply_filter();
    }
    if (open && hasChildren) {
        for (uint32_t cell : key.subkeys) {
            auto it = state.cellToIndex.find(cell);
            if (it != state.cellToIndex.end()) {
                draw_key_node(state, it->second, depth + 1);
            }
        }
        ImGui::TreePop();
    }
}

}

bool AppState::load_hive(const std::wstring& path) {
    if (!hive_open(hive, path)) {
        hiveOpen = false;
        status = hive.error;
        return false;
    }
    hiveOpen = true;
    openedAt = filetime_now();
    cellToIndex.clear();
    for (size_t i = 0; i < hive.keys.size(); ++i) {
        cellToIndex[hive.keys[i].cellOffset] = i;
    }
    selectedKey = -1;
    for (size_t i = 0; i < hive.keys.size(); ++i) {
        if (!hive.keys[i].values.empty()) {
            selectedKey = static_cast<int>(i);
            break;
        }
    }
    if (selectedKey < 0 && !hive.keys.empty()) {
        selectedKey = 0;
    }
    apply_filter();
    status = L"Opened " + hive.rootName;
    return true;
}

void AppState::close_hive() {
    hive = RegistryHive();
    cellToIndex.clear();
    hiveOpen = false;
    selectedKey = -1;
    visibleValues.clear();
    query.clear();
    status = L"closed";
}

void AppState::apply_filter() {
    visibleValues.clear();
    if (!hiveOpen || selectedKey < 0 || static_cast<size_t>(selectedKey) >= hive.keys.size()) {
        return;
    }
    const RegistryKey& key = hive.keys[static_cast<size_t>(selectedKey)];
    std::wstring needle = lower(query);
    for (uint32_t index : key.values) {
        const RegistryValue& value = hive.values[index];
        if (matches_query(value, needle)) {
            visibleValues.push_back(index);
        }
    }
    sort_visible();
}

void AppState::sort_visible() {
    if (sortColumn.empty()) {
        sortColumn = L"name";
        sortAscending = true;
    }
    std::wstring col = lower(sortColumn);
    std::stable_sort(visibleValues.begin(), visibleValues.end(),
                     [this, &col](size_t lhs, size_t rhs) {
                         const RegistryValue& left = hive.values[lhs];
                         const RegistryValue& right = hive.values[rhs];
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

    ImGui::SetCursorPos(ImVec2(70.0f, 12.0f));
    const char* wordmark = "registry";
    ImGui::TextColored(kText, "%s", wordmark);
    float width = ImGui::CalcTextSize(wordmark).x;
    ImGui::SetCursorPos(ImVec2(70.0f + width, 12.0f));
    ImGui::TextColored(kAmber, "explorer++");
    ImGui::SetCursorPos(ImVec2(70.0f, 33.0f));
    ImGui::TextColored(kFaint, "registry hive inspector");

    float searchW = 320.0f;
    float searchX = ImGui::GetWindowWidth() * 0.5f - searchW * 0.5f;
    if (searchX < 300.0f) {
        searchX = 300.0f;
    }
    float searchY = (kTopBarH - 30.0f) * 0.5f;

    ImGui::SetCursorPos(ImVec2(searchX, searchY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
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
    ImGui::PushItemWidth(searchW);
    if (ImGui::InputTextWithHint("##search", "    Search values (Ctrl+F)", queryBuffer, sizeof(queryBuffer))) {
        state.query = from_utf8(queryBuffer);
        state.apply_filter();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    if (icons.magnifier != 0) {
        ImVec2 iconPos = ImVec2(searchX + 9.0f, searchY + (30.0f - 15.0f) * 0.5f);
        draw->AddImage(icons.magnifier, iconPos, ImVec2(iconPos.x + 15.0f, iconPos.y + 15.0f));
    }

    float buttonY = (kTopBarH - 30.0f) * 0.5f;
    float rightX = ImGui::GetWindowWidth() - 24.0f;

    rightX -= 112.0f;
    ImGui::SetCursorPos(ImVec2(rightX, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, kAmberSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0xE8A33C, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0xE8A33C, 0.34f));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(0xF8D98E));
    if (ImGui::Button("Export CSV", ImVec2(112.0f, 30.0f))) {
        if (state.hiveOpen) {
            wchar_t file[1024] = L"registry-export.csv";
            OPENFILENAMEW dialog = {};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFilter = L"CSV file (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
            dialog.lpstrFile = file;
            dialog.nMaxFile = static_cast<DWORD>(std::size(file));
            dialog.lpstrTitle = L"Export hive to CSV";
            dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetSaveFileNameW(&dialog)) {
                state.status = export_hive_csv(state.hive, file) ? L"Exported hive to CSV" : L"Export failed";
            }
        }
    }
    ImGui::PopStyleColor(4);
    if (icons.download != 0) {
        ImVec2 rectMin = ImGui::GetItemRectMin();
        float icon = 15.0f;
        ImVec2 iconMin = ImVec2(rectMin.x + 10.0f, rectMin.y + (30.0f - icon) * 0.5f);
        draw->AddImage(icons.download, iconMin, ImVec2(iconMin.x + icon, iconMin.y + icon));
    }

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

void draw_tree_panel(AppState& state, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::BeginChild("tree", ImVec2(width, 0.0f), false);

    ImGui::SetCursorPos(ImVec2(16.0f, 12.0f));
    ImGui::TextColored(kFaint, "HIVE");

    ImGui::SetCursorPos(ImVec2(0.0f, 34.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
    ImGui::BeginChild("tree_scroll", ImVec2(-1.0f, ImGui::GetContentRegionAvail().y), false);
    ImGui::PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(width, +100000.0f), true);
    if (!state.hiveOpen) {
        ImGui::SetCursorPos(ImVec2(16.0f, 24.0f));
        ImGui::PushTextWrapPos(width - 32.0f);
        ImGui::TextColored(kDim, "No hive open.");
        ImGui::TextColored(kFaint, "Open a .dat hive from the toolbar to browse its keys.");
        ImGui::PopTextWrapPos();
    } else {
        for (size_t i = 0; i < state.hive.keys.size(); ++i) {
            if (state.hive.keys[i].cellOffset == state.hive.rootOffset) {
                draw_key_node(state, i, 0);
            }
        }
    }
    ImGui::PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_values_panel(AppState& state) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::BeginChild("values", ImVec2(0.0f, 0.0f), false);

    const float headerH = 34.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::BeginChild("values_header", ImVec2(-1.0f, headerH), false, ImGuiWindowFlags_NoScrollbar);

    float numW = 52.0f;
    float nameW = 250.0f;
    float typeW = 150.0f;
    float dataW = std::max(200.0f, ImGui::GetContentRegionAvail().x - numW - nameW - typeW - 32.0f);

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

    if (!state.hiveOpen) {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextColored(kFaint, "Open a hive to see its values.");
    } else if (state.selectedKey < 0) {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextColored(kFaint, "Select a key on the left.");
    } else if (state.visibleValues.empty()) {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextColored(kFaint, "No values match.");
    } else {
        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("rows", 4, flags, ImVec2(-1.0f, 0.0f))) {
            ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, numW);
            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, nameW);
            ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed, typeW);
            ImGui::TableSetupColumn("data", ImGuiTableColumnFlags_WidthStretch);

            for (size_t i = 0; i < state.visibleValues.size(); ++i) {
                const RegistryValue& value = state.hive.values[state.visibleValues[i]];
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 52.0f);
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(kFaint, "%zu", i + 1);
                ImGui::TableSetColumnIndex(1);

                bool hovered = ImGui::IsMouseHoveringRect(ImGui::GetCursorScreenPos(),
                                                          ImVec2(ImGui::GetCursorScreenPos().x + nameW,
                                                                 ImGui::GetCursorScreenPos().y + 52.0f));
                ImGui::PushStyleColor(ImGuiCol_Header, kSelect);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kHover);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, kSelect);
                ImGui::PushStyleColor(ImGuiCol_Text, kText);
                ImGui::Selectable(to_utf8(value.name).c_str(), false, ImGuiSelectableFlags_SpanAllColumns |
                                                                    ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(0.0f, 52.0f));
                ImGui::PopStyleColor(4);
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Copy value name")) {
                        ImGui::SetClipboardText(to_utf8(value.name).c_str());
                    }
                    if (ImGui::MenuItem("Copy value data")) {
                        ImGui::SetClipboardText(to_utf8(value.preview).c_str());
                    }
                    const RegistryKey& key = state.hive.keys[static_cast<size_t>(state.selectedKey)];
                    if (ImGui::MenuItem("Copy key path")) {
                        ImGui::SetClipboardText(to_utf8(key.path).c_str());
                    }
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(kDim, "%s", to_utf8(value_type_name(value.type)).c_str());
                ImGui::TableSetColumnIndex(3);
                std::wstring data = value.preview;
                if (data.size() > 240) {
                    data.resize(240);
                    data += L"...";
                }
                ImGui::TextColored(kText, "%s", to_utf8(data).c_str());
            }
            ImGui::PopStyleColor(2);
            ImGui::EndTable();
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

    if (state.hiveOpen && state.selectedKey >= 0 &&
        static_cast<size_t>(state.selectedKey) < state.hive.keys.size()) {
        const RegistryKey& key = state.hive.keys[static_cast<size_t>(state.selectedKey)];
        ImGui::SetCursorPos(ImVec2(16.0f, 34.0f));
        ImGui::PushTextWrapPos(width - 16.0f);
        ImGui::TextColored(kText, "%s", to_utf8(key.path).c_str());
        ImGui::PopTextWrapPos();

        ImGui::SetCursorPos(ImVec2(16.0f, 88.0f));
        ImGui::TextColored(kFaint, "last write");
        ImGui::SetCursorPos(ImVec2(16.0f, 106.0f));
        ImGui::TextColored(kText, "%s", to_utf8(format_filetime(key.lastWrite)).c_str());

        ImGui::SetCursorPos(ImVec2(16.0f, 128.0f));
        ImGui::TextColored(kFaint, "flags");
        ImGui::SetCursorPos(ImVec2(16.0f, 146.0f));
        wchar_t flags[32];
        swprintf_s(flags, L"0x%08X", key.flags);
        ImGui::TextColored(kText, "%s", to_utf8(flags).c_str());

        ImGui::SetCursorPos(ImVec2(16.0f, 168.0f));
        ImGui::TextColored(kFaint, "subkeys / values");
        ImGui::SetCursorPos(ImVec2(16.0f, 186.0f));
        ImGui::TextColored(kText, "%zu / %zu", key.subkeys.size(), key.values.size());
    } else {
        ImGui::SetCursorPos(ImVec2(16.0f, 34.0f));
        ImGui::TextColored(kDim, "no key selected");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_status_bar(AppState& state, float contentHeight) {
    ImGui::SetCursorPos(ImVec2(0.0f, contentHeight - kStatusH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Separator, kHair);
    ImGui::BeginChild("statusbar", ImVec2(-1.0f, kStatusH), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(24.0f, (kStatusH - 14.0f) * 0.5f));
    ImGui::TextColored(kDim, "made by diquuuu");

    std::string right = "ready";
    if (state.hiveOpen) {
        right = std::to_string(state.hive.keys.size()) + " keys  /  " + std::to_string(state.hive.values.size()) +
                " values";
    }
    float width = ImGui::CalcTextSize(right.c_str()).x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - width - 24.0f,
                               (kStatusH - 14.0f) * 0.5f));
    ImGui::TextColored(kFaint, "%s", right.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}
