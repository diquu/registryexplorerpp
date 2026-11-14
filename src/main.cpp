#include "theme.h"
#include "registry_hive.h"
#include "app.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <cwchar>
#include <cstdio>
#include <io.h>
#include <iostream>
#include <string>

#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"
#include "../vendor/imgui/backends/imgui_impl_dx11.h"
#include "../vendor/imgui/backends/imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr int INITIAL_WIDTH = 1440;
constexpr int INITIAL_HEIGHT = 900;

bool create_device(HWND window);
void create_render_target();
void cleanup_render_target();
void resize(UINT width, UINT height);

HWND g_window = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;
UINT g_width = INITIAL_WIDTH;
UINT g_height = INITIAL_HEIGHT;

bool create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC descriptor = {};
    descriptor.BufferCount = 2;
    descriptor.BufferDesc.Width = g_width;
    descriptor.BufferDesc.Height = g_height;
    descriptor.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descriptor.BufferDesc.RefreshRate.Numerator = 60;
    descriptor.BufferDesc.RefreshRate.Denominator = 1;
    descriptor.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    descriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descriptor.OutputWindow = window;
    descriptor.SampleDesc.Count = 1;
    descriptor.SampleDesc.Quality = 0;
    descriptor.Windowed = TRUE;
    descriptor.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, requested,
                                                    2, D3D11_SDK_VERSION, &descriptor, &g_swapchain, &g_device,
                                                    &featureLevel, &g_context);
    if (FAILED(result)) {
        return false;
    }

    create_render_target();
    return true;
}

void create_render_target() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (backBuffer != nullptr) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
        backBuffer->Release();
    }
}

void cleanup_render_target() {
    if (g_renderTargetView != nullptr) {
        g_renderTargetView->Release();
        g_renderTargetView = nullptr;
    }
}

void resize(UINT width, UINT height) {
    if (g_swapchain == nullptr || g_device == nullptr) {
        return;
    }
    cleanup_render_target();
    g_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    g_width = width;
    g_height = height;
    create_render_target();
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return 1;
    }
    switch (message) {
        case WM_SIZE:
            resize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 1000;
            info->ptMinTrackSize.y = 680;
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    repp::AppState state;
    if (argv != nullptr) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--dump") == 0 && i + 1 < argc) {
                repp::RegistryHive hive;
                bool ok = repp::hive_open(hive, argv[i + 1]);
                if (!ok) {
                    std::wcout << L"error: " << hive.error << L"\n";
                    std::wcout.flush();
                    LocalFree(argv);
                    return 2;
                }
                std::wstring dump = L"hive=" + hive.filePath + L" keys=" + std::to_wstring(hive.keys.size()) +
                                    L" values=" + std::to_wstring(hive.values.size()) + L" root=" + hive.rootName +
                                    L"\n";
                for (size_t i = 0; i < hive.keys.size(); ++i) {
                    const repp::RegistryKey& key = hive.keys[i];
                    dump += L"KEY " + key.path + L"  (offset=0x" + std::to_wstring(key.cellOffset) +
                              L", subkeys=" + std::to_wstring(key.subkeys.size()) +
                              L", values=" + std::to_wstring(key.values.size()) +
                              L", lastwrite=" + repp::format_filetime(key.lastWrite) + L")\n";
                    for (uint32_t valueIndex : key.values) {
                        const repp::RegistryValue& value = hive.values[valueIndex];
                        dump += L"    " + value.name + L"  [" + repp::value_type_name(value.type) + L"]  " +
                                 value.preview + L"\n";
                    }
                }
                int needed = WideCharToMultiByte(CP_UTF8, 0, dump.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (needed > 0) {
                    std::string utf8(static_cast<size_t>(needed - 1), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, dump.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
                    _write(1, utf8.data(), static_cast<unsigned int>(utf8.size()));
                }
                LocalFree(argv);
                return 0;
            }
            if ((wcscmp(argv[i], L"--standard") == 0) || (wcscmp(argv[i], L"--everywhere") == 0)) {
                continue;
            }
            if (GetFileAttributesW(argv[i]) != INVALID_FILE_ATTRIBUTES) {
                state.load_hive(argv[i]);
                break;
            }
        }
        LocalFree(argv);
    }

    WNDCLASSEXW wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_CLASSDC;
    wndClass.lpfnWndProc = window_proc;
    wndClass.hInstance = instance;
    wndClass.lpszClassName = L"registryexplorerpp";
    wndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wndClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (RegisterClassExW(&wndClass) == 0) {
        return 1;
    }

    g_window = CreateWindowExW(0, wndClass.lpszClassName, L"registryexplorer++", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WIDTH, INITIAL_HEIGHT, nullptr, nullptr,
                               instance, nullptr);
    if (g_window == nullptr) {
        return 1;
    }

    if (!create_device(g_window)) {
        return 1;
    }

    repp::Icons icons = repp::load_icons(g_device);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(g_window, 19, &dark, sizeof(dark));

    ShowWindow(g_window, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(g_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    repp::Fonts fonts = repp::setup_theme();

    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);

    bool done = false;
    while (!done) {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImVec2 size = ImGui::GetIO().DisplaySize;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, repp::kBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("host", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
        ImGui::SetWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetWindowSize(ImVec2(size.x, size.y), ImGuiCond_Always);

        float bodyTop = repp::kTopBarH;
        float bodyBottom = size.y - repp::kStatusH;
        float railW = 300.0f;
        float drawerW = state.drawerOpen ? 340.0f : 0.0f;

        repp::draw_top_bar(state, icons);

        ImGui::SetCursorPos(ImVec2(0.0f, bodyTop));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, repp::kBg);
        ImGui::BeginChild("body", ImVec2(-1.0f, bodyBottom - bodyTop), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        repp::draw_tree_panel(state, railW);

        ImGui::SetCursorPos(ImVec2(railW, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, repp::kBg);
        ImGui::BeginChild("center", ImVec2(size.x - railW - drawerW, bodyBottom - bodyTop), false,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(railW, 0.0f));
        repp::draw_values_panel(state);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (state.drawerOpen) {
            ImGui::SetCursorPos(ImVec2(size.x - drawerW, 0.0f));
            ImGui::BeginChild("drawerwrap", ImVec2(drawerW, bodyBottom - bodyTop), false,
                              ImGuiWindowFlags_NoScrollbar);
            repp::draw_drawer(state, drawerW);
            ImGui::EndChild();
        }

        repp::draw_status_bar(state, bodyBottom - bodyTop);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Render();
        const float clear[4] = {0x0B / 255.0f, 0x0D / 255.0f, 0x0E / 255.0f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_renderTargetView, nullptr);
        g_context->ClearRenderTargetView(g_renderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapchain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    repp::release_icons(icons);
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanup_render_target();
    if (g_swapchain != nullptr) {
        g_swapchain->Release();
    }
    if (g_context != nullptr) {
        g_context->Release();
    }
    if (g_device != nullptr) {
        g_device->Release();
    }
    DestroyWindow(g_window);
    UnregisterClassW(wndClass.lpszClassName, instance);
    return 0;
}
