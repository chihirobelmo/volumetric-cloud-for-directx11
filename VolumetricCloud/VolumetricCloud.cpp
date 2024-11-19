// This project uses Dear ImGui, licensed under the MIT License.
// See LICENSE for details.

/*

1.	Open Project Properties:
    1.	Right-click on your project in the Solution Explorer and select "Properties".
2.	Add Additional Include Directories:
    1.	Go to Configuration Properties -> VC++ Directories -> Include Directories.
    2.	Add the path to the DirectX SDK include directory, e.g., C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include.
3.	Add Additional Library Directories:
    1.	Go to Configuration Properties -> VC++ Directories -> Library Directories.
    2.	Add the path to the DirectX SDK library directory, e.g., C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x64.
4.	Add Additional Dependencies:
    1.	Go to Configuration Properties -> Linker -> Input -> Additional Dependencies.
    2.	Add d3d11.lib;D3DCompiler.lib; to the list.

*/

#define USE_IMGUI

#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

#include "Camera.h"
#include "postProcess.h"
#include "Renderer.h"
#include "Raymarching.h"
#include "Noise.h"
#include "Primitive.h"

#pragma comment(lib, "dxgi.lib")

#ifdef USE_IMGUI
    #include "imgui.h"
    #include "backends/imgui_impl_win32.h"
    #include "backends/imgui_impl_dx11.h"
#endif

void LogToFile(const char* message) {
    std::ofstream logFile("d3d_debug.log", std::ios::app);
    time_t now = time(nullptr);
    logFile << ": " << message << std::endl;
    logFile.close();
}

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace environment {

float total_distance_meter = 20/*nautical mile*/ * 1852/*nm to meters*/;

struct EnvironmentBuffer {
    XMVECTOR lightDir; // 3 floats
    XMVECTOR lightColor; // 3 floats
    XMVECTOR cloudAreaPos; // 3 floats
    XMVECTOR cloudAreaSize; // 3 floats
};

ComPtr<ID3D11Buffer> environment_buffer;

void InitBuffer();
void UpdateBuffer();

} // namespace environment

namespace mouse {

POINT lastPos;
bool is_dragging = false;

} // namespace mouse

namespace finalscene {

ComPtr<ID3D11RenderTargetView> rtv;

void CreateRenderTargetView();

} // namespace finalscene

namespace {

Noise fbm(256, 256, 256);
Raymarch cloud(512, 512);
Camera camera(80.0f);
PostProcess postProcess;

Primitive monolith;

} // namepace

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
HRESULT PreRender();
HRESULT Setup();
void CleanupDevice();
void Render();

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice())) {
        CleanupDevice();
        return 0;
    }

#ifdef USE_IMGUI
	// Imgui setup
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(Renderer::hWnd);
        ImGui_ImplDX11_Init(Renderer::device.Get(), Renderer::context.Get());
    }
#endif

    PreRender(); 
    Setup();

    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Render();
        }
    }

#ifdef USE_IMGUI
	// Imgui cleanup
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
#endif

    CleanupDevice();
    return (int)msg.wParam;
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow) {
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"DirectXExample";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Create window
    Renderer::hWnd = CreateWindow(L"DirectXExample", L"DirectX Example", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, Renderer::width, Renderer::height, nullptr, nullptr, hInstance, nullptr);
    if (!Renderer::hWnd)
        return E_FAIL;

    ShowWindow(Renderer::hWnd, nCmdShow);
    UpdateWindow(Renderer::hWnd);

    return S_OK;
}

void CreateDeviceAndSwapChain(UINT& width, UINT& height) {
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(Renderer::hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = Renderer::hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    
    // Add debug flags
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    // some laptop fail device creation with this flag:
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    LogToFile("Debug layer enabled");
    
    ComPtr<IDXGIFactory> factory;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"CreateDXGIFactory Failed", L"Error", MB_OK);
        return;
    }

    UINT i = 0;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIAdapter> bestAdapter;
    SIZE_T maxDedicatedVideoMemory = 0;

    while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
            maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
            bestAdapter = adapter;
        }

        i++;
    }
    if (!bestAdapter) {
        MessageBox(nullptr, L"No suitable adapter found", L"Error", MB_OK);
        return;
    }

    hr = D3D11CreateDeviceAndSwapChain(bestAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &sd, &Renderer::swapchain, &Renderer::device, &featureLevel, &Renderer::context);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChain Failed", L"Error", MB_OK);
        return;
    }

    ComPtr<ID3DUserDefinedAnnotation> annotation;
    Renderer::context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&annotation);

    LogToFile("Device created successfully");
}

HRESULT InitDevice() {
    CreateDeviceAndSwapChain(Renderer::width, Renderer::height);

    return S_OK;
}

HRESULT PreRender() {

    // noise makes its own viewport so we need to reset it later.
    fbm.CreateNoiseShaders(L"FBMTex.hlsl", "VS", "PS");
    fbm.CreateNoiseTexture3DResource();
	fbm.RenderNoiseTexture3D();

    return S_OK;
}

HRESULT Setup() {

    camera.Init();
    camera.LookAt(XMVectorSet(0,0,0,0));
	camera.LookAtFrom(270.0f, 20.0f, 250.0f);

	monolith.CreateRenderTargets(Renderer::width, Renderer::height);
	monolith.CreateShaders(L"Primitive.hlsl", "VS", "PS");
	monolith.CreateGeometry();

    cloud.CompileShader(L"RayMarch.hlsl", "VS", "PS");
    cloud.CreateSamplerState();
    cloud.CreateRenderTarget();
    cloud.SetupViewport();
    cloud.CreateVertex();
    cloud.SetVertexBuffer();

    Renderer::SetupViewport();
    postProcess.CreatePostProcessResources();
    postProcess.CreateRenderTexture(Renderer::width, Renderer::height);

    environment::InitBuffer();
    environment::UpdateBuffer();
    finalscene::CreateRenderTargetView();

    return S_OK;
}

void CleanupDevice() {
    if (Renderer::context) Renderer::context->ClearState();
}

void Render() {

    camera.Update(Renderer::width, Renderer::height);
    environment::UpdateBuffer();

	ID3D11Buffer* buffers[] = { camera.buffer.Get(), environment::environment_buffer.Get() };
    UINT bufferCount = sizeof(buffers) / sizeof(ID3D11Buffer*);

	// First Pass: Render monolith to texture
    {
        monolith.Begin(Renderer::width, Renderer::height);
        monolith.RenderBox(buffers, bufferCount);
        //monolith.End();
    }

    // Second Pass: Render clouds to texture using ray marching
    {
        // Clear render target first
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Renderer::context->ClearRenderTargetView(cloud.rtv.Get(), clearColor);
        Renderer::context->ClearDepthStencilView(cloud.dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        Renderer::context->OMSetRenderTargets(1, cloud.rtv.GetAddressOf(), cloud.dsv.Get());
        D3D11_VIEWPORT rayMarchingVP = {};
        rayMarchingVP.Width = static_cast<float>(cloud.width);
        rayMarchingVP.Height = static_cast<float>(cloud.height);
        rayMarchingVP.MinDepth = 0.0f;
        rayMarchingVP.MaxDepth = 1.0f;
        rayMarchingVP.TopLeftX = 0;
        rayMarchingVP.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &rayMarchingVP);

        // Update camera constants
        Renderer::context->VSSetConstantBuffers(0, 1, camera.buffer.GetAddressOf());
        Renderer::context->VSSetConstantBuffers(1, 1, environment::environment_buffer.GetAddressOf());

        Renderer::context->PSSetConstantBuffers(0, 1, camera.buffer.GetAddressOf());
        Renderer::context->PSSetConstantBuffers(1, 1, environment::environment_buffer.GetAddressOf());

        // Set resources for cloud rendering
        Renderer::context->PSSetShaderResources(0, 1, monolith.depthSRV_.GetAddressOf());
        Renderer::context->PSSetShaderResources(1, 1, fbm.srv.GetAddressOf());
        Renderer::context->PSSetSamplers(0, 1, cloud.depthSampler.GetAddressOf());
        Renderer::context->PSSetSamplers(1, 1, cloud.noiseSampler.GetAddressOf());

        // Render clouds with ray marching
        Renderer::context->VSSetShader(cloud.vertex_shader.Get(), nullptr, 0);
        Renderer::context->PSSetShader(cloud.pixel_shader.Get(), nullptr, 0);

        UINT stride = sizeof(Raymarch::Vertex);
        UINT offset = 0;
        Renderer::context->IASetVertexBuffers(0, 1, cloud.vertex_buffer.GetAddressOf(), &stride, &offset);
        Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        Renderer::context->Draw(4, 0);
    }

	// Third Pass: Stretch raymarch texture to full screen and also merge with monolith
    {
        // Set the final scene render target and viewport to full window size
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Renderer::context->ClearRenderTargetView(finalscene::rtv.Get(), clearColor);
        Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), nullptr);
        D3D11_VIEWPORT finalSceneVP = {};
        finalSceneVP.Width = static_cast<float>(Renderer::width);
        finalSceneVP.Height = static_cast<float>(Renderer::height);
        finalSceneVP.MinDepth = 0.0f;
        finalSceneVP.MaxDepth = 1.0f;
        finalSceneVP.TopLeftX = 0;
        finalSceneVP.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &finalSceneVP);

        // Set raymarch texture as source for post-
        ID3D11ShaderResourceView* srvs[] = { monolith.colorSRV_.Get(), cloud.srv.Get(), monolith.depthSRV_.Get(), cloud.dsrv.Get() };
        Renderer::context->PSSetShaderResources(0, sizeof(srvs)/sizeof(ID3D11ShaderResourceView), srvs);
        Renderer::context->PSSetSamplers(0, 1, postProcess.sampler.GetAddressOf());
        
        // Use post-process shaders to stretch the texture
        Renderer::context->VSSetShader(postProcess.vs.Get(), nullptr, 0);
        Renderer::context->PSSetShader(postProcess.ps.Get(), nullptr, 0);
        Renderer::context->Draw(4, 0);
    }

#ifdef USE_IMGUI
    // ImGui
    {
        // Start the ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Your ImGui code here
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::End();

        // Rendering
        ImGui::Render();
        Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
#endif

    Renderer::swapchain->Present(0, 0);

    // Clear shader resources
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    Renderer::context->PSSetShaderResources(0, 2, nullSRV);
}

void CreateFinalSceneRenderTarget() {
    ComPtr<ID3D11Texture2D> pBackBuffer;
    HRESULT hr = Renderer::swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)pBackBuffer.GetAddressOf());
    if (FAILED(hr)) return;

    hr = Renderer::device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &finalscene::rtv);
    if (FAILED(hr)) return;

    Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(Renderer::width);
    vp.Height = static_cast<float>(Renderer::height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void OnResize(UINT width, UINT height) {
    Renderer::width = width;
    Renderer::height = height;

    if (Renderer::device) {
        // Clear all render target bindings
        Renderer::context->OMSetRenderTargets(0, nullptr, nullptr);

        // Update camera projection for new aspect ratio
        camera.Update(width, height);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &viewport);
        
        // Reset all resources that depend on window size
        finalscene::rtv.Reset();
        postProcess.rtv.Reset();
        postProcess.srv.Reset();
        postProcess.tex.Reset();
        cloud.rtv.Reset();
        cloud.srv.Reset();
        cloud.tex.Reset();
		monolith.renderTargetView_.Reset();
		monolith.depthStencilView_.Reset();
		monolith.colorSRV_.Reset();
		monolith.depthSRV_.Reset();
		monolith.colorTex_.Reset();
		monolith.depthTex_.Reset();

        // Resize swap chain
        Renderer::swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Recreate resources with new size
        monolith.CreateRenderTargets(width, height);
        cloud.CreateRenderTarget();
        postProcess.CreateRenderTexture(width, height);
        CreateFinalSceneRenderTarget();
    }
}

#ifdef USE_IMGUI
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
    HDC hdc;

#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }
#endif

    switch (message) {
    case WM_LBUTTONDOWN:
        mouse::is_dragging = true;
        mouse::lastPos.x = GET_X_LPARAM(lParam);
        mouse::lastPos.y = GET_Y_LPARAM(lParam);
        SetCapture(hWnd);
        break;
    case WM_LBUTTONUP:
        mouse::is_dragging = false;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
        if (mouse::is_dragging) {
            POINT currentMousePos;
            currentMousePos.x = GET_X_LPARAM(lParam);
            currentMousePos.y = GET_Y_LPARAM(lParam);

            float dx = XMConvertToRadians(10.0f * static_cast<float>(currentMousePos.x - mouse::lastPos.x));
            float dy = XMConvertToRadians(10.0f * static_cast<float>(currentMousePos.y - mouse::lastPos.y));

            float az, el, dist;
			camera.CalcAzElDistToFocusPoint(az, el, dist);

			// Update azimuth and elevation angles
            az += dx;
            el += dy;

            camera.LookAtFrom(az, el, dist);
            camera.Update(Renderer::width, Renderer::width);

            mouse::lastPos = currentMousePos;
        }
        break;
    case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            float az, el, dist;
            camera.CalcAzElDistToFocusPoint(az, el, dist);

            float scaleSpeed = 0.05f;
            
            // Adjust radius based on wheel movement
            dist -= zDelta * scaleSpeed;
            
            // Clamp radius to reasonable bounds
            dist = max(1.0f, min(1000.0f, dist));
            
            // Update camera position maintaining direction
            camera.LookAtFrom(az, el, dist);
            camera.Update(Renderer::width, Renderer::width);
        }
        break;
    case WM_SIZE:
        if (Renderer::context) {
            Renderer::width = static_cast<float>(LOWORD(lParam));
            Renderer::height = static_cast<float>(HIWORD(lParam));
			OnResize(Renderer::width, Renderer::height);
            camera.Update(Renderer::width, Renderer::width);
            Renderer::swapchain->ResizeBuffers(0, Renderer::width, Renderer::height, DXGI_FORMAT_UNKNOWN, 0);
        }
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

void finalscene::CreateRenderTargetView() {
    // Create a render target view
    ComPtr<ID3D11Texture2D> pBackBuffer;
    Renderer::swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    Renderer::device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &finalscene::rtv);

    Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), nullptr);
}

void environment::InitBuffer() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(environment::EnvironmentBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA environmentInitData = {};
    environmentInitData.pSysMem = &bd;

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &environmentInitData, &environment::environment_buffer);
    if (FAILED(hr))
        return;
}

void environment::UpdateBuffer() {
	EnvironmentBuffer bf;
	bf.lightDir = XMVectorSet(0.0, -1.0, 0.0, 0.0);
	bf.lightColor = XMVectorSet(1.0, 1.0, 1.0, 0.0);
	bf.cloudAreaPos = XMVectorSet(0.0, 0.0, 0.0, 0.0);
	bf.cloudAreaSize = XMVectorSet(environment::total_distance_meter, 200, environment::total_distance_meter, 0.0);

    Renderer::context->UpdateSubresource(environment::environment_buffer.Get(), 0, nullptr, &bf, 0, 0);
}