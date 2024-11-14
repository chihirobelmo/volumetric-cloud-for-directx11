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
#include "Renderer.h"
#include "Raymarching.h"

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




namespace postprocess {

// Post-process resources
ComPtr<ID3D11VertexShader> vs;
ComPtr<ID3D11PixelShader> ps;
ComPtr<ID3D11SamplerState> sampler;

// Render to texture resources
ComPtr<ID3D11Texture2D> tex;
ComPtr<ID3D11RenderTargetView> rtv;
ComPtr<ID3D11ShaderResourceView> srv;

void CreatePostProcessResources();
void CreateRenderTexture(UINT width, UINT height);

} // namespace postprocess



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



// pre-render noise texture 3d for cloud rendering
namespace noise {

struct Vertex3D {
    XMFLOAT3 position;    // 12 bytes (3 * 4)
    XMFLOAT3 texcoord;    // 12 bytes (3 * 4) - Changed from XMFLOAT2 to XMFLOAT3
    // Total: 24 bytes
};

int NOISE_TEX_SIZE = 256;

// noise
ComPtr<ID3D11InputLayout> layout;
ComPtr<ID3D11VertexShader> vs;
ComPtr<ID3D11PixelShader> ps;
ComPtr<ID3D11SamplerState> sampler;
ComPtr<ID3D11ShaderResourceView> srv;
ComPtr<ID3D11Texture3D> tex;
ComPtr<ID3D11RenderTargetView> rtv;

void CreateNoiseShaders();
void CreateNoiseTexture3D();

} // namespace noise



// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
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
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    LogToFile("Debug layer enabled");

    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels),
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

    // noise makes its own viewport so we need to reset it later.
    noise::CreateNoiseShaders();
    noise::CreateNoiseTexture3D();

    Camera::InitializeCamera();
    Camera::InitBuffer();
    Camera::UpdateProjectionMatrix(Renderer::width, Renderer::height);
    Camera::UpdateBuffer();

    // environment buffer
    environment::InitBuffer();
    environment::UpdateBuffer();

    Raymarch::CompileTheVertexShader();
    Raymarch::CompileThePixelShader();
    Raymarch::CreateSamplerState();
    Raymarch::CreateRenderTarget(); // Add this line to create the render target
    Raymarch::SetupViewport();
    Raymarch::CreateVertex();
    Raymarch::SetVertexBuffer();

    Renderer::SetupViewport();
    postprocess::CreatePostProcessResources();
    postprocess::CreateRenderTexture(Renderer::width, Renderer::height);

    finalscene::CreateRenderTargetView();

    return S_OK;
}

void CleanupDevice() {
    if (Renderer::context) Renderer::context->ClearState();
}

void Render() {
    // First Pass: Render clouds to texture using ray marching
    {
        // Set the ray marching render target and viewport
        Renderer::context->OMSetRenderTargets(1, Raymarch::rtv.GetAddressOf(), nullptr);
        D3D11_VIEWPORT rayMarchingVP = {};
        rayMarchingVP.Width = static_cast<float>(Raymarch::RT_WIDTH);
        rayMarchingVP.Height = static_cast<float>(Raymarch::RT_HEIGHT);
        rayMarchingVP.MinDepth = 0.0f;
        rayMarchingVP.MaxDepth = 1.0f;
        rayMarchingVP.TopLeftX = 0;
        rayMarchingVP.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &rayMarchingVP);

        // Clear render target first
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Renderer::context->ClearRenderTargetView(Raymarch::rtv.Get(), clearColor);

        // Update camera constants
        Camera::UpdateBuffer();
		environment::UpdateBuffer();

        Renderer::context->VSSetConstantBuffers(0, 1, Camera::camera_buffer.GetAddressOf());
        Renderer::context->VSSetConstantBuffers(1, 1, environment::environment_buffer.GetAddressOf());

        Renderer::context->PSSetConstantBuffers(0, 1, Camera::camera_buffer.GetAddressOf());
        Renderer::context->PSSetConstantBuffers(1, 1, environment::environment_buffer.GetAddressOf());

        // Set resources for cloud rendering
        Renderer::context->PSSetShaderResources(1, 1, noise::srv.GetAddressOf());
        Renderer::context->PSSetSamplers(1, 1, noise::sampler.GetAddressOf());

        // Render clouds with ray marching
        Renderer::context->VSSetShader(Raymarch::vertex_shader.Get(), nullptr, 0);
        Renderer::context->PSSetShader(Raymarch::pixel_shader.Get(), nullptr, 0);
        Renderer::context->Draw(4, 0);
    }

    // Second Pass: Stretch raymarch texture to full screen
    {
        // Set the final scene render target and viewport to full window size
        Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), nullptr);
        D3D11_VIEWPORT finalSceneVP = {};
        finalSceneVP.Width = static_cast<float>(Renderer::width);
        finalSceneVP.Height = static_cast<float>(Renderer::height);
        finalSceneVP.MinDepth = 0.0f;
        finalSceneVP.MaxDepth = 1.0f;
        finalSceneVP.TopLeftX = 0;
        finalSceneVP.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &finalSceneVP);

        float clearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
        Renderer::context->ClearRenderTargetView(finalscene::rtv.Get(), clearColor);

        // Set raymarch texture as source for post-process
        Renderer::context->PSSetShaderResources(0, 1, Raymarch::srv.GetAddressOf());
        Renderer::context->PSSetSamplers(0, 1, postprocess::sampler.GetAddressOf());
        
        // Use post-process shaders to stretch the texture
        Renderer::context->VSSetShader(postprocess::vs.Get(), nullptr, 0);
        Renderer::context->PSSetShader(postprocess::ps.Get(), nullptr, 0);
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
        
        // Reset all resources that depend on window size
        finalscene::rtv.Reset();
        postprocess::rtv.Reset();
        postprocess::srv.Reset();
        postprocess::tex.Reset();
        Raymarch::rtv.Reset();
        Raymarch::srv.Reset();
        Raymarch::tex.Reset();

        // Resize swap chain
        Renderer::swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Recreate resources with new size
        CreateFinalSceneRenderTarget();
        postprocess::CreateRenderTexture(width, height);
        Raymarch::CreateRenderTarget(); // Make sure to recreate raymarch target
        
        // Update camera projection for new aspect ratio
        Camera::UpdateProjectionMatrix(width, height);
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

            Camera::azimuth_hdg += dx;
            Camera::elevation_deg += dy;

            mouse::lastPos = currentMousePos;

            Camera::eye_pos = Renderer::PolarToCartesian(Camera::look_at_pos, Camera::distance_meter, Camera::azimuth_hdg, Camera::elevation_deg);
            Camera::UpdateCamera(Camera::eye_pos, Camera::look_at_pos);
        }
        break;
    case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            
            // Adjust radius based on wheel movement
            float scaleSpeed = 0.05f;
            Camera::distance_meter -= zDelta * scaleSpeed;
            
            // Clamp radius to reasonable bounds
            Camera::distance_meter = max(1.0f, min(1000.0f, Camera::distance_meter));
            
            // Update camera position maintaining direction
            Camera::eye_pos = Renderer::PolarToCartesian(Camera::look_at_pos, Camera::distance_meter, Camera::azimuth_hdg, Camera::elevation_deg);
            
            // Update view matrix and constant buffer
            Camera::UpdateCamera(Camera::eye_pos, Camera::look_at_pos);
        }
        break;
    case WM_SIZE:
        if (Renderer::context) {
            Renderer::width = static_cast<float>(LOWORD(lParam));
            Renderer::height = static_cast<float>(HIWORD(lParam));
            Camera::UpdateProjectionMatrix(Renderer::width, Renderer::height);
            Camera::UpdateCamera(Camera::eye_pos, Camera::look_at_pos);
			OnResize(Renderer::width, Renderer::height);
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


void postprocess::CreateRenderTexture(UINT width, UINT height) {
    // Create the render target texture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    HRESULT hr = Renderer::device->CreateTexture2D(&textureDesc, nullptr, &postprocess::tex);
    if (FAILED(hr)) {
        LogToFile("Failed to create render texture");
        return;
    }

    // Create the render target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = textureDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    hr = Renderer::device->CreateRenderTargetView(postprocess::tex.Get(), &rtvDesc, &postprocess::rtv);
    if (FAILED(hr)) {
        LogToFile("Failed to create render target view");
        return;
    }

    // Create the shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = Renderer::device->CreateShaderResourceView(postprocess::tex.Get(), &srvDesc, &postprocess::srv);
    if (FAILED(hr)) {
        LogToFile("Failed to create shader resource view");
        return;
    }

    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Renderer::context->RSSetViewports(1, &vp);
}

void postprocess::CreatePostProcessResources() {
    // Create sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Renderer::device->CreateSamplerState(&sampDesc, &postprocess::sampler);

    // Compile shaders
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    Renderer::CompileShaderFromFile(L"PostProcess.hlsl", "VS", "vs_5_0", vsBlob);
    Renderer::CompileShaderFromFile(L"PostProcess.hlsl", "PS", "ps_5_0", psBlob);

    // Create shader objects
    Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &postprocess::vs);
    Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &postprocess::ps);
}

void Raymarch::CreateSamplerState() {
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = Renderer::device->CreateSamplerState(&sampDesc, &noise::sampler);
    if (FAILED(hr)) {
        LogToFile("Failed to create sampler state");
    }
}

void noise::CreateNoiseShaders() {
    // Compile shaders
    ComPtr<ID3DBlob> vsBlob;
    Renderer::CompileShaderFromFile(L"FBMTex.hlsl", "VS", "vs_5_0", vsBlob);

    // Create vertex shader
    Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &noise::vs);

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }  // Changed to R32G32B32_FLOAT
    };
    Renderer::device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &noise::layout);

    // Create pixel shader
    ComPtr<ID3DBlob> psBlob;
    Renderer::CompileShaderFromFile(L"FBMTex.hlsl", "PS", "ps_5_0", psBlob);
    Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &noise::ps);
}

void noise::CreateNoiseTexture3D() {

    // Create 3D texture
    D3D11_TEXTURE3D_DESC texDesc = {};
    texDesc.Width = noise::NOISE_TEX_SIZE;
    texDesc.Height = noise::NOISE_TEX_SIZE;
    texDesc.Depth = noise::NOISE_TEX_SIZE;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Changed from UNORDERED_ACCESS to RENDER_TARGET
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    // Create initial texture data with a mid-gray value
    std::vector<float> initialTexData(noise::NOISE_TEX_SIZE * noise::NOISE_TEX_SIZE * noise::NOISE_TEX_SIZE, 0.5f);
    D3D11_SUBRESOURCE_DATA texInitData = {};
    texInitData.pSysMem = initialTexData.data();
    texInitData.SysMemPitch = noise::NOISE_TEX_SIZE * sizeof(float);
    texInitData.SysMemSlicePitch = noise::NOISE_TEX_SIZE * noise::NOISE_TEX_SIZE * sizeof(float);

    HRESULT hr = Renderer::device->CreateTexture3D(&texDesc, &texInitData, &noise::tex);
    if (FAILED(hr)) {
        LogToFile("Failed to create 3D texture");
        return;
    }

    // Create vertex buffer for full-screen quad with correct UVW coordinates
    noise::Vertex3D noiseVertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },  // Bottom-left
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },  // Top-left
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) },  // Bottom-right
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }   // Top-right
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(noiseVertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbInitData = {};
    vbInitData.pSysMem = noiseVertices;

    ComPtr<ID3D11Buffer> noiseVertexBuffer;
    Renderer::device->CreateBuffer(&vbDesc, &vbInitData, &noiseVertexBuffer);

    // Set up the pipeline for noise generation
    UINT stride = sizeof(Vertex3D); // Fixed: Use correct stride
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, noiseVertexBuffer.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetInputLayout(noise::layout.Get());
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Save current render targets
    ComPtr<ID3D11RenderTargetView> oldRTV;
    Renderer::context->OMGetRenderTargets(1, &oldRTV, nullptr);

    // Create SRV for the 3D texture
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;

    hr = Renderer::device->CreateShaderResourceView(noise::tex.Get(), &srvDesc, &noise::srv);
    if (FAILED(hr)) {
        LogToFile("Failed to create noise texture SRV");
        return;
    }

    // Clear background to a mid-gray
    float clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

    // Set up viewport specifically for noise texture
    D3D11_VIEWPORT noiseVP;
    noiseVP.Width = (FLOAT)noise::NOISE_TEX_SIZE;
    noiseVP.Height = (FLOAT)noise::NOISE_TEX_SIZE;
    noiseVP.MinDepth = 0.0f;
    noiseVP.MaxDepth = 1.0f;
    noiseVP.TopLeftX = 0;
    noiseVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &noiseVP);

    // For each Z-slice of the 3D texture
    for (UINT slice = 0; slice < noise::NOISE_TEX_SIZE; slice++) {
        // Create RTV for this slice
        D3D11_RENDER_TARGET_VIEW_DESC sliceRTVDesc = {};
        sliceRTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        sliceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        sliceRTVDesc.Texture3D.FirstWSlice = slice;
        sliceRTVDesc.Texture3D.WSize = 1;
        sliceRTVDesc.Texture3D.MipSlice = 0;

        ComPtr<ID3D11RenderTargetView> sliceRTV;
        Renderer::device->CreateRenderTargetView(noise::tex.Get(), &sliceRTVDesc, &sliceRTV);

        // Set and clear the render target
        Renderer::context->OMSetRenderTargets(1, sliceRTV.GetAddressOf(), nullptr);
        Renderer::context->ClearRenderTargetView(sliceRTV.Get(), clearColor);

        // Set shaders and draw
        Renderer::context->VSSetShader(noise::vs.Get(), nullptr, 0);
        Renderer::context->PSSetShader(noise::ps.Get(), nullptr, 0);

        // Update noise parameters for this slice
        struct NoiseParams {
            float currentSlice;
            float time;
            float scale;
            float persistence;
        };

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(NoiseParams);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        NoiseParams params = {
            static_cast<float>(slice) / (noise::NOISE_TEX_SIZE - 1),  // currentSlice
            // for now needed for padding even if not used
            0.0f,                                // time
            4.0f,                                // scale
            0.5f                                 // persistence
        };
        D3D11_SUBRESOURCE_DATA cbData = { &params };

        ComPtr<ID3D11Buffer> noiseParamsCB;
        Renderer::device->CreateBuffer(&cbDesc, &cbData, &noiseParamsCB);
        Renderer::context->PSSetConstantBuffers(2, 1, noiseParamsCB.GetAddressOf());

        // Set viewport again for each slice to ensure correct dimensions
        Renderer::context->RSSetViewports(1, &noiseVP);

        // Draw the quad
        Renderer::context->Draw(4, 0);
    }

    // Restore original render target
    Renderer::context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), nullptr);
}