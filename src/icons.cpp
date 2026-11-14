#include "icons.h"

#include <d3d11.h>
#include <windows.h>

#include <cwchar>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

namespace repp {

std::wstring exe_dir() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return L"";
    }
    std::wstring path(buffer, length);
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L"";
    }
    return path.substr(0, slash);
}

bool Icons::load(ID3D11Device* device, const std::wstring& iconsDir) {
    if (device == nullptr) {
        return false;
    }

    auto loadOne = [&](ImTextureID& slot, const wchar_t* fileName) {
        std::wstring path = iconsDir + L"\\" + fileName;
        std::string narrow;
        narrow.reserve(path.size());
        for (wchar_t ch : path) {
            narrow += static_cast<char>(ch < 128 ? ch : '?');
        }

        int width = 0, height = 0, channels = 0;
        unsigned char* pixels = stbi_load(narrow.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            stbi_image_free(pixels);
            return false;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D* texture = nullptr;
        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = pixels;
        data.SysMemPitch = static_cast<UINT>(width * 4);
        data.SysMemSlicePitch = 0;
        HRESULT hr = device->CreateTexture2D(&desc, &data, &texture);
        stbi_image_free(pixels);
        if (FAILED(hr) || texture == nullptr) {
            return false;
        }

        ID3D11ShaderResourceView* view = nullptr;
        hr = device->CreateShaderResourceView(texture, nullptr, &view);
        texture->Release();
        if (FAILED(hr) || view == nullptr) {
            return false;
        }
        slot = reinterpret_cast<ImTextureID>(view);
        return true;
    };

    bool ok = true;
    ok = loadOne(key, L"key-128.png") && ok;
    ok = loadOne(folderOpen, L"folder-open-128.png") && ok;
    ok = loadOne(download, L"download-128.png") && ok;
    ok = loadOne(magnifier, L"magnifier-128.png") && ok;
    return ok;
}

void Icons::release() {
    for (ImTextureID* slot : {&key, &folderOpen, &download, &magnifier}) {
        if (*slot != 0) {
            reinterpret_cast<ID3D11ShaderResourceView*>(*slot)->Release();
            *slot = 0;
        }
    }
}

Icons load_icons(ID3D11Device* device) {
    Icons icons = {};
    std::wstring base = exe_dir();

    const std::wstring candidates[] = {
        base + L"\\..\\data",
        base + L"\\data",
        L"data",
    };

    for (const std::wstring& candidate : candidates) {
        std::wstring iconsDir = candidate + L"\\icons";
        DWORD attributes = GetFileAttributesW(iconsDir.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            icons.load(device, iconsDir);
            break;
        }
    }
    return icons;
}

void release_icons(Icons& icons) {
    icons.release();
}

}
