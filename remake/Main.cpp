#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>

#include <filesystem>

#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_impl_dx11.h"
#include "Dependencies/ImGui/imgui_impl_win32.h"
#include "Core/Menu/Menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

namespace
{
    constexpr float UiScale = 1.0f;
    constexpr int LogicalWidth = 1000;
    constexpr int LogicalHeight = 620;
    constexpr int WindowWidth = static_cast<int>(LogicalWidth * UiScale + 0.5f);
    constexpr int WindowHeight = static_cast<int>(LogicalHeight * UiScale + 0.5f);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (backBuffer)
        {
            device->CreateRenderTargetView(backBuffer, nullptr, &renderTarget);
            backBuffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (renderTarget)
        {
            renderTarget->Release();
            renderTarget = nullptr;
        }
    }

    bool CreateDevice(HWND window)
    {
        DXGI_SWAP_CHAIN_DESC description{};
        description.BufferCount = 2;
        description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.OutputWindow = window;
        description.SampleDesc.Count = 1;
        description.Windowed = TRUE;
        description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        constexpr D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0
        };

        D3D_FEATURE_LEVEL selectedFeatureLevel{};
        const HRESULT result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
            &description, &swapChain, &device, &selectedFeatureLevel, &deviceContext);

        if (FAILED(result))
            return false;

        CreateRenderTarget();
        return renderTarget != nullptr;
    }

    void CleanupDevice()
    {
        CleanupRenderTarget();
        if (swapChain) { swapChain->Release(); swapChain = nullptr; }
        if (deviceContext) { deviceContext->Release(); deviceContext = nullptr; }
        if (device) { device->Release(); device = nullptr; }
    }

    std::filesystem::path GetAssetPath(const std::filesystem::path& relativePath)
    {
        wchar_t executablePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        const std::filesystem::path besideExecutable = std::filesystem::path(executablePath).parent_path() / L"Assets" / relativePath;
        if (std::filesystem::exists(besideExecutable))
            return besideExecutable;

        return std::filesystem::path(__FILE__).parent_path() / L"Assets" / relativePath;
    }

    LRESULT WINAPI WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
            return true;

        switch (message)
        {
        case WM_SIZE:
            if (device && wParam != SIZE_MINIMIZED)
            {
                CleanupRenderTarget();
                swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_NCHITTEST:
        {
            const LRESULT hit = DefWindowProcW(window, message, wParam, lParam);
            if (hit == HTCLIENT)
            {
                POINT cursor{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(window, &cursor);
                if (cursor.y < static_cast<int>(64.0f * UiScale))
                    return HTCAPTION;
            }
            return hit;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
                PostMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDPIAware();
    const WNDCLASSEXW windowClass{
        sizeof(WNDCLASSEXW), CS_CLASSDC, WindowProcedure, 0L, 0L, instance,
        nullptr, nullptr, nullptr, nullptr, L"RemakeWindow", nullptr
    };
    RegisterClassExW(&windowClass);

    const int x = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;
    HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"Ultimate", WS_POPUP,
        x, y, WindowWidth, WindowHeight, nullptr, nullptr, instance, nullptr);

    const DWORD roundedCornerPreference = 2;
    DwmSetWindowAttribute(window, 33, &roundedCornerPreference, sizeof(roundedCornerPreference));

    if (!window || !CreateDevice(window))
    {
        CleanupDevice();
        if (window)
            DestroyWindow(window);
        UnregisterClassW(windowClass.lpszClassName, instance);
        return 1;
    }

    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImFontConfig regularConfig{};
    regularConfig.OversampleH = 2;
    regularConfig.OversampleV = 2;
    const std::string regularPath = GetAssetPath(L"Fonts/Inter-Regular.ttf").string();
    ImFont* regularFont = io.Fonts->AddFontFromFileTTF(regularPath.c_str(), 16.0f, &regularConfig);

    ImFontConfig semiBoldConfig{};
    semiBoldConfig.OversampleH = 2;
    semiBoldConfig.OversampleV = 2;
    const std::string semiBoldPath = GetAssetPath(L"Fonts/Inter-SemiBold.ttf").string();
    ImFont* semiBoldFont = io.Fonts->AddFontFromFileTTF(semiBoldPath.c_str(), 16.0f, &semiBoldConfig);
    ImFontConfig iconConfig{};
    iconConfig.OversampleH = 2;
    iconConfig.OversampleV = 2;
    static const ImWchar iconRanges[] = { 0xf000, 0xf8ff, 0 };
    const std::string iconPath = GetAssetPath(L"Fonts/fa-solid-900.ttf").string();
    ImFont* iconFont = io.Fonts->AddFontFromFileTTF(iconPath.c_str(), 18.0f, &iconConfig, iconRanges);
    if (!regularFont)
        regularFont = io.Fonts->AddFontDefaultVector();
    if (!semiBoldFont)
        semiBoldFont = regularFont;
    if (!iconFont)
        iconFont = semiBoldFont;
    io.FontDefault = regularFont;
    Menu::SetFonts(regularFont, semiBoldFont, iconFont);

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 16.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, deviceContext);

    bool running = true;
    while (running)
    {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT)
                running = false;
        }
        if (!running)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();
        ImGui::Render();

        ImDrawData* drawData = ImGui::GetDrawData();
        constexpr float clearColor[4] = { 10.0f / 255.0f, 9.0f / 255.0f, 15.0f / 255.0f, 1.0f };
        deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);
        deviceContext->ClearRenderTargetView(renderTarget, clearColor);
        ImGui_ImplDX11_RenderDrawData(drawData);
        swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDevice();
    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, instance);
    return 0;
}
