#pragma once

#include "registry_hive.h"
#include "theme.h"
#include "icons.h"
#include "suspicious.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace repp {

struct HiveEntry {
    RegistryHive hive;
    std::unordered_map<uint32_t, size_t> cellToIndex;
    int selectedKey = -1;
    std::vector<size_t> visibleValues;
};

struct SearchHit {
    int hiveIndex = 0;
    int keyIndex = -1;
    int valueIndex = -1;
};

struct AppState {
    std::vector<HiveEntry> hives;
    int activeHive = -1;

    std::wstring query;
    std::wstring sortColumn = L"name";
    bool sortAscending = true;
    bool drawerOpen = false;
    bool searchFocused = false;
    std::wstring status;

    enum PanelTab { Tree, Suspicious } tab = Tree;
    std::vector<Finding> findings;

    std::unordered_map<uint64_t, bool> treeOpen;
    std::unordered_map<uint64_t, float> openAnim;
    std::unordered_map<uint64_t, float> hoverAnim;

    std::vector<SearchHit> searchHits;
    int searchKeyCount = 0;
    int searchValueCount = 0;
    int searchCursor = 0;
    bool searchDismissed = false;
    float searchAnchorX = 0.0f;
    float searchAnchorWidth = 0.0f;
    std::wstring lastSearch;
    int scrollToValue = -1;
    std::unordered_set<int> flaggedValueIndices;

    bool hiveOpen() const { return activeHive >= 0; }
    const RegistryHive& cur_hive() const { return hives[static_cast<size_t>(activeHive)].hive; }
    RegistryHive& cur_hive() { return hives[static_cast<size_t>(activeHive)].hive; }
    HiveEntry& active() { return hives[static_cast<size_t>(activeHive)]; }
    const HiveEntry& active() const { return hives[static_cast<size_t>(activeHive)]; }

    bool load_hive(const std::wstring& path);
    void close_hive(int index);
    void apply_filter();
    void sort_visible();
    void scan_suspicious();
    void recompute_search();
    void refresh_after_hive_change();
};

void draw_top_bar(AppState& state, repp::Icons& icons);
void draw_tree_panel(AppState& state, repp::Icons& icons, float width);
void draw_values_panel(AppState& state, repp::Icons& icons);
void draw_drawer(AppState& state, float width);
void draw_status_bar(AppState& state, float contentHeight);
void draw_search_dropdown(AppState& state, repp::Icons& icons);

}
