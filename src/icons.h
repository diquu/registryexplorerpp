#pragma once

#include <string>

#ifndef IMGUI_DISABLE
#include "../vendor/imgui/imgui.h"
#endif

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace repp {

struct Icons {
    ImTextureID key = 0;
    ImTextureID folderOpen = 0;
    ImTextureID magnifier = 0;
    ImTextureID shieldWarning = 0;
    ImTextureID shieldCheck = 0;
    ImTextureID treeStructure = 0;
    ImTextureID keyLight = 0;

    bool load(ID3D11Device* device, const std::wstring& basePath);
    void release();
};

std::wstring exe_dir();
Icons load_icons(ID3D11Device* device);
void release_icons(Icons& icons);

}
