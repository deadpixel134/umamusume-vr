#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const GUID iid_texture2d = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

static int log_contains(const char* needle) {
    WCHAR base[MAX_PATH], path[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (!length || length >= MAX_PATH - 64) return 0;
    swprintf(path, MAX_PATH, L"%ls\\UmaVR\\immersive-001.log", base);
    FILE* file = _wfopen(path, L"rb"); if (!file) return 0;
    char data[16384]; size_t got = fread(data, 1, sizeof(data) - 1, file); fclose(file); data[got] = 0;
    return strstr(data, needle) != NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    WNDCLASSW wc = {0}; wc.lpfnWndProc = window_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"UmaVrPanelFakeGame";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 3;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"UmaVR immersive fake game", WS_OVERLAPPEDWINDOW,
        50, 50, 640, 360, NULL, NULL, wc.hInstance, NULL);
    if (!window) return 4;
    ShowWindow(window, SW_SHOW); UpdateWindow(window);
    if (!LoadLibraryA(argv[1])) return 5;

    DXGI_SWAP_CHAIN_DESC desc = {0}; desc.BufferDesc.Width = 640; desc.BufferDesc.Height = 360;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.OutputWindow = window; desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0}, selected;
    IDXGISwapChain* chain = NULL; ID3D11Device* device = NULL; ID3D11DeviceContext* context = NULL;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3,
        D3D11_SDK_VERSION, &desc, &chain, &device, &selected, &context);
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3,
        D3D11_SDK_VERSION, &desc, &chain, &device, &selected, &context);
    if (FAILED(hr) || !chain) return 7;

    ID3D11RenderTargetView* rtv = NULL;
    for (int i = 0; i < 480; ++i) { /* up to ~8 s at ~60 fps */
        if (!rtv) {
            ID3D11Texture2D* tex = NULL;
            if (SUCCEEDED(IDXGISwapChain_GetBuffer(chain, 0, &iid_texture2d, (void**)&tex)) && tex) {
                D3D11_RENDER_TARGET_VIEW_DESC rd; ZeroMemory(&rd, sizeof(rd));
                D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(tex, &td);
                rd.Format = td.Format;
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)tex, &rd, &rtv);
                ID3D11Texture2D_Release(tex);
            }
            if (!rtv) { Sleep(50); continue; }
        }
        float t = i * 0.02f;
        float color[4] = {0.9f * (0.5f + 0.5f * sinf(t)), 0.2f, 0.6f * (0.5f + 0.5f * cosf(t * 0.7f)), 1.0f};
        ID3D11DeviceContext_ClearRenderTargetView(context, rtv, color);
        IDXGISwapChain_Present(chain, 1, 0);
        Sleep(16);
        if (log_contains("\"event\":\"session_summary\"") ||
            (i > 240 && log_contains("\"event\":\"disabled\""))) break;
    }
    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    if (chain) IDXGISwapChain_Release(chain);
    DestroyWindow(window);
    Sleep(2500); /* let the probe finish teardown before process exit */
    return SUCCEEDED(hr) ? 0 : 8;
}
