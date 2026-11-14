#pragma once

#include "registry_hive.h"
#include "theme.h"
#include "icons.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace repp {

struct AppState {
    RegistryHive hive;
    std::unordered_map<uint32_t, size_t> cellToIndex;
    bool hiveOpen = false;
    uint64_t openedAt = 0;

    int selectedKey = -1;
    std::wstring query;
    std::wstring sortColumn = L"name";
    bool sortAscending = true;
    bool drawerOpen = false;
    bool searchFocused = false;
    std::wstring status;

    std::vector<size_t> visibleValues;

    bool load_hive(const std::wstring& path);
    void close_hive();
    void apply_filter();
    void sort_visible();
};

void draw_top_bar(AppState& state, repp::Icons& icons);
void draw_tree_panel(AppState& state, float width);
void draw_values_panel(AppState& state);
void draw_drawer(AppState& state, float width);
void draw_status_bar(AppState& state, float contentHeight);

}
