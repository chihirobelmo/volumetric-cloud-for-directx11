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

#include <chrono>
#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>
#include <string>
#include <format>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

#include "../includes/GPUTimer.h"
#include "../includes/CubeMap.h"
#include "../includes/Camera.h"
#include "../includes/postProcess.h"
#include "../includes/Renderer.h"
#include "../includes/Raymarching.h"
#include "../includes/Noise.h"
#include "../includes/Primitive.h"
#include "../includes/Fmap.h"
#include "../includes/FinalScene.h"
#include "../includes/DDSLoader.h"

#pragma comment(lib, "dxgi.lib")

#ifdef USE_IMGUI
    #include "imgui.h"
    #include "backends/imgui_impl_win32.h"
    #include "backends/imgui_impl_dx11.h"
#endif
#include "../includes/TimeCounter.h"

void LogToFile(const char* message) {
    std::ofstream logFile("d3d_debug.log", std::ios::app);
    time_t now = time(nullptr);
    logFile << ": " << message << std::endl;
    logFile.close();
}

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace environment {

    float total_distance_meter = 30/*nautical mile*/ * 1852/*nm to meters*/;
    float cloud_height_range = 200.0f;

    float lightAz_, lightEl_;
    XMVECTOR lightColor_;

    XMVECTOR GetLightDir() {
        float azimuth = lightAz_ * (XM_PI / 180);
        float elevation = lightEl_ * (XM_PI / 180);

        // Calculate Cartesian coordinates
        float x = cosf(elevation) * sinf(azimuth);
        float y = sinf(elevation);
        float z = cosf(elevation) * cosf(azimuth);

        return XMVectorSet(x, y, z, 1.0f);
    }

    struct EnvironmentBuffer {
        XMVECTOR lightDir; // 3 floats
        XMVECTOR lightColor; // 3 floats
        XMVECTOR time;
    };

    ComPtr<ID3D11Buffer> environment_buffer;

    void InitBuffer();
    void UpdateBuffer();

    // clouds SDF
    const int MAX_CLOUDS = 48;

    struct CumulusBuffer {
        XMFLOAT4 cumulusPos[MAX_CLOUDS];
    };

    ComPtr<ID3D11Buffer> cumulus_buffer;

    void CreateCumulusBuffer();

} // namespace environment

namespace mouse {

    POINT lastPos;
    bool is_dragging = false;

} // namespace mouse

namespace {

    GPUTimer gpuTimer;
    TimeCounter timer;

    // weather map
    Fmap fmap("resources/WeatherSample.fmap");
	DDSLoader cloudMapTest;

    // for rendering
    Camera camera(80.0f, 0.1f, 422440.f, 270, -20, 50000.0f);
    Noise fbm(128, 128, 128);
    CubeMap skyMap(1024, 1024);
    CubeMap skyMapIrradiance(32, 32);
    Raymarch skyBox(2048, 2048);
    Primitive monolith;
    Raymarch cloud(512, 512);

    PostProcess cloudMapGenerate;
    PostProcess fxaa;
    PostProcess manualMerger;

    // for debug
    PostProcess monolithDepthDebug;
    PostProcess cloudDepthDebug;
    PostProcess fbmDebugR;
    PostProcess fbmDebugG;
    PostProcess fbmDebugB;
    PostProcess fbmDebugA;
    PostProcess heightRemapTest;

    ComPtr<ID3DUserDefinedAnnotation> annotation;

} // namepace

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
HRESULT PreRender();
HRESULT Setup();
void CleanupDevice();
void Render();

float GetDPIScalingFactor() {
    HDC screen = GetDC(NULL);
    int dpiX = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);
    return dpiX / 96.0f; // 96 DPI is the default DPI value
}

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

        ImGui::GetIO().FontGlobalScale = GetDPIScalingFactor();
    }
#endif

    Setup();
    PreRender();

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
    
    // Get screen dimensions
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    Renderer::width = desktop.right;
    Renderer::height = desktop.bottom;

    // Create window
    Renderer::hWnd = CreateWindow(L"DirectXExample", L"DirectX Example", WS_POPUP/*WS_OVERLAPPEDWINDOW*/,
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

    Renderer::context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&annotation);

    LogToFile("Device created successfully");
}

HRESULT InitDevice() {
    CreateDeviceAndSwapChain(Renderer::width, Renderer::height);

    return S_OK;
}

HRESULT PreRender() {

    // noise makes its own viewport so we need to reset it later.
    fbm.CreateNoiseShaders(L"shaders/FBMTex.hlsl", "VS", "PS");
    fbm.CreateNoiseTexture3DResource();
	fbm.RenderNoiseTexture3D();

	skyMap.CreateGeometry();
    skyMap.CreateRenderTarget();
	skyMap.CompileShader(L"shaders/SkyMap.hlsl", "VS", "PS");

    skyMapIrradiance.CreateGeometry();
    skyMapIrradiance.CreateRenderTarget();
    skyMapIrradiance.CompileShader(L"shaders/SkyMapIrradiance.hlsl", "VS", "PS");

    return S_OK;
}

HRESULT Setup() {

    gpuTimer.Init(Renderer::device.Get(), Renderer::context.Get());
	timer.Start();

    fmap.CreateTexture2DFromData();
	cloudMapTest.Load(L"resources/WeatherMap.dds");

    camera.Init();
    camera.LookAt(XMVectorSet(0,0,0,0));
	camera.UpdateEyePosition();

	skyBox.CreateRenderTarget();
	skyBox.CompileShader(L"shaders/RayMarch.hlsl", "VS", "PS_SKYBOX");
	skyBox.CreateGeometry();

	monolith.CreateRenderTargets(Renderer::width, Renderer::height);
	monolith.CreateShaders(L"shaders/Primitive.hlsl", "VS", "PS");
	monolith.CreateGeometry(Primitive::CreateTopologyHealthMonolith);
    // or try CreateTopologyIssueMonolith for your study ...

    cloud.CreateRenderTarget();
    cloud.CompileShader(L"shaders/RayMarch.hlsl", "VS", "PS");
    cloud.CreateGeometry();

	cloudMapGenerate.CreatePostProcessResources(L"shaders/CloudMapGenerate.hlsl", "VS", "PS");
	cloudMapGenerate.CreateRenderTexture(1024, 1024);

    fxaa.CreatePostProcessResources(L"shaders/PostAA.hlsl", "VS", "PS");
    fxaa.CreateRenderTexture(Renderer::width, Renderer::height);

    manualMerger.CreatePostProcessResources(L"shaders/PostProcess.hlsl", "VS", "PS");
    manualMerger.CreateRenderTexture(Renderer::width, Renderer::height);

    environment::InitBuffer();
    environment::UpdateBuffer();
    environment::CreateCumulusBuffer();

    finalscene::CreateRenderTargetView();

    // for debug

    monolithDepthDebug.CreatePostProcessResources(L"shaders/DepthDebug.hlsl", "VS", "PS");
    monolithDepthDebug.CreateRenderTexture(Renderer::width, Renderer::height);

	cloudDepthDebug.CreatePostProcessResources(L"shaders/DepthDebug.hlsl", "VS", "PS");
	cloudDepthDebug.CreateRenderTexture(cloud.width_, cloud.height_);

	fbmDebugR.CreatePostProcessResources(L"shaders/NoiseTextureDebug.hlsl", "VS", "PSR");
	fbmDebugR.CreateRenderTexture(fbm.widthPx_, fbm.heightPx_);
    fbmDebugG.CreatePostProcessResources(L"shaders/NoiseTextureDebug.hlsl", "VS", "PSG");
    fbmDebugG.CreateRenderTexture(fbm.widthPx_, fbm.heightPx_);
    fbmDebugB.CreatePostProcessResources(L"shaders/NoiseTextureDebug.hlsl", "VS", "PSB");
    fbmDebugB.CreateRenderTexture(fbm.widthPx_, fbm.heightPx_);
    fbmDebugA.CreatePostProcessResources(L"shaders/NoiseTextureDebug.hlsl", "VS", "PSA");
    fbmDebugA.CreateRenderTexture(fbm.widthPx_, fbm.heightPx_);

	heightRemapTest.CreatePostProcessResources(L"shaders/HeightRemapTest.hlsl", "VS", "PS");
	heightRemapTest.CreateRenderTexture(cloud.width_, cloud.height_);

    return S_OK;
}

void CleanupDevice() {
    if (Renderer::context) Renderer::context->ClearState();
}

namespace imgui_info {

std::vector<float> frameTimes;
const int maxFrames = 100; // Number of frames to store
float texPreviewScale = 1.0;
bool demoMode = false;
bool flyThroughMode = false;
float flyThroughSpeedMach = 0.9;

} // namespace imgui_info


void DispImguiInfo() {
#ifdef USE_IMGUI
    ImGui::Begin("INFO");

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        mouse::is_dragging = false;
    }

    //imgui_info::frameTimes.push_back(1000.0 / ImGui::GetIO().Framerate);
    //if (imgui_info::frameTimes.size() > imgui_info::maxFrames) {
    //    imgui_info::frameTimes.erase(imgui_info::frameTimes.begin());
    //}

    float sum = 0.0f;
    for (float time : imgui_info::frameTimes) {
        sum += time;
    }
	float avg = sum / imgui_info::frameTimes.size();

    ImGui::PlotLines("for Ray Marching Cloud",
        imgui_info::frameTimes.data(), static_cast<int>(imgui_info::frameTimes.size()), 0,
        std::format("Frame Time: {:.1f} ms", avg).c_str(), 0.0f, 4.0f, ImVec2(0, 80));

	ImGui::Checkbox("Demo Mode", &imgui_info::demoMode);
    if (imgui_info::demoMode) {
        camera.az_ += 10.0f * 1.0f / ImGui::GetIO().Framerate;
        camera.el_ += 5.0f * 1.0f / ImGui::GetIO().Framerate;

        camera.UpdateEyePosition();
        camera.UpdateBuffer(Renderer::width, Renderer::width);
    }

    ImGui::Checkbox("Fly Through Mode", &imgui_info::flyThroughMode);
    if (imgui_info::flyThroughMode) {

        ImGui::SliderFloat("Fly Speed (Mach)", &imgui_info::flyThroughSpeedMach, 0.6f, 1.2f, "%.1f");

        float azimuth = camera.az_ * (XM_PI / 180);
        float elevation = camera.el_ * (XM_PI / 180);

        // Calculate Cartesian coordinates
        float x = 343 * imgui_info::flyThroughSpeedMach * (1.0 / ImGui::GetIO().Framerate) * cosf(elevation) * sinf(azimuth);
        float y = 343 * imgui_info::flyThroughSpeedMach * (1.0 / ImGui::GetIO().Framerate) * sinf(elevation);
        float z = 343 * imgui_info::flyThroughSpeedMach * (1.0 / ImGui::GetIO().Framerate) * cosf(elevation) * cosf(azimuth);

        // Create the Cartesian vector
        XMVECTOR cartesian = XMVectorSet(x, y, z, 0.0f);

        // Translate the Cartesian vector by the origin
        XMVECTOR Forward = XMVector3Normalize(XMVectorSubtract(camera.lookAtPos_, camera.eyePos_));
		camera.dist_ = XMVector3Length(Forward).m128_f32[0];
		camera.lookAtPos_ = XMVectorAdd(camera.eyePos_, Forward);
        camera.lookAtPos_ = XMVectorSubtract(camera.lookAtPos_, XMVectorSet(x, y, z, 0.0));

        camera.UpdateEyePosition();
        camera.UpdateBuffer(Renderer::width, Renderer::width);
    }

    ImGui::NewLine();

    if (ImGui::Button("Re-Compile Shaders")) {
		monolith.RecompileShader();
        cloud.RecompileShader();
		cloudMapGenerate.RecompileShader();
		manualMerger.RecompileShader();
		heightRemapTest.RecompileShader();
    }

    if (ImGui::Button("Re-Load Weather Map")) {
        cloudMapTest.LoadAgain();
    }

    if (ImGui::Button("Re-Render Noise Texture")) {
        fbm.RecompileShader();
		fbm.RenderNoiseTexture3D();
    }

    ImGui::NewLine();

    if (ImGui::CollapsingHeader("Camera Settings")) {
		ImGui::Text("Camera Position: (%.1fnm, %.0fft, %.1fnm)", camera.eyePos_.m128_f32[0] / 1852.0, camera.eyePos_.m128_f32[1] * 3.28, camera.eyePos_.m128_f32[2] / 1852.0);
        ImGui::SliderFloat("Camera Distance", &camera.dist_, 1.0f, 100 * 1852.0f, "%.1f");
        ImGui::SliderFloat("Camera Vertical FOV", &camera.vFov_, 10.0f, 80.0f, "%.f");
        ImGui::SliderFloat3("Camera Look At", reinterpret_cast<float*>(&camera.lookAtPos_), -environment::total_distance_meter, environment::total_distance_meter, "%.f");
        float lightDir[2] = { environment::lightAz_, environment::lightEl_ };
        if (ImGui::SliderFloat2("Light Direction", lightDir, 0.0f, 360.0f, "%.f")) {
            environment::lightAz_ = lightDir[0];
            environment::lightEl_ = lightDir[1];
        }
        ImGui::SliderFloat("HEATMAP: Cloud Height Range", &environment::cloud_height_range, 100.0f, 1000.0f, "%.f");
        ImGui::SliderFloat("HEATMAP: Cloud Distance Meter", &environment::total_distance_meter, 100.0f, 200.0f * 1852.0f, "%.f");
    }

    float aspect = Renderer::width / (float)Renderer::height;
    ImVec2 texPreviewSize(256 * aspect, 256);
    ImVec2 texPreviewSizeSquare(256 * aspect, 256 * aspect);

    // Create a table
    if (ImGui::CollapsingHeader("Rendering Pipeline")) {

        monolithDepthDebug.Draw(1, monolith.depthSRV_.GetAddressOf(), 0, nullptr);
        cloudDepthDebug.Draw(1, cloud.debugSRV_.GetAddressOf(), 0, nullptr);

        if (ImGui::BeginTable("Texture Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

            ImGui::TableSetupColumn("Color");
            ImGui::TableSetupColumn("Depth");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)monolith.colorSRV_.Get(), texPreviewSize);
            ImGui::TableSetColumnIndex(1);
            ImGui::Image((ImTextureID)(intptr_t)monolithDepthDebug.shaderResourceView_.Get(), texPreviewSize);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)cloud.colorSRV_.Get(), texPreviewSize);
            ImGui::TableSetColumnIndex(1);
            ImGui::Image((ImTextureID)(intptr_t)cloudDepthDebug.shaderResourceView_.Get(), texPreviewSize);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Rendering Resource")) {

        fbmDebugR.Draw(1, fbm.colorSRV_.GetAddressOf(), 0, nullptr);
        fbmDebugG.Draw(1, fbm.colorSRV_.GetAddressOf(), 0, nullptr);
        fbmDebugB.Draw(1, fbm.colorSRV_.GetAddressOf(), 0, nullptr);
        fbmDebugA.Draw(1, fbm.colorSRV_.GetAddressOf(), 0, nullptr);

        if (ImGui::BeginTable("Weather Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

            ImGui::TableSetupColumn("FMAP");
            ImGui::TableSetupColumn("Cloud Map");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)cloudMapTest.colorSRV_.Get(), texPreviewSizeSquare);
            ImGui::TableSetColumnIndex(1);
            ImGui::Image((ImTextureID)(intptr_t)cloudMapGenerate.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::EndTable();
        }

        if (ImGui::BeginTable("Noise Table 1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Noise R");
            ImGui::TableSetupColumn("Noise G");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)fbmDebugR.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::TableSetColumnIndex(1);
            ImGui::Image((ImTextureID)(intptr_t)fbmDebugG.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::EndTable();
        }

        if (ImGui::BeginTable("Noise Table 2", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Noise B");
            ImGui::TableSetupColumn("Noise A");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)fbmDebugB.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::TableSetColumnIndex(1);
            ImGui::Image((ImTextureID)(intptr_t)fbmDebugA.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Rendering Test")) {
        if (ImGui::BeginTable("Rendering Test 1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

			heightRemapTest.Draw(0, nullptr, 0, nullptr);

            ImGui::TableSetupColumn("Rendering Test 1-1");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Image((ImTextureID)(intptr_t)heightRemapTest.shaderResourceView_.Get(), texPreviewSizeSquare);
            ImGui::EndTable();
        }
    }

    ImGui::End();
#endif
}


void AnnotateRendering(LPCWSTR Name, std::function<void()> func) {
    annotation->BeginEvent(Name);
	func();
    annotation->EndEvent();
}


void CalculateFrameTime(std::function<void()> func) {
    gpuTimer.Start();
    func();
    gpuTimer.End();
    float gpuTime = gpuTimer.GetGPUTimeInMS();

    imgui_info::frameTimes.push_back(gpuTime);
    if (imgui_info::frameTimes.size() > imgui_info::maxFrames) {
        imgui_info::frameTimes.erase(imgui_info::frameTimes.begin());
    }
}


void Render() {

    camera.UpdateEyePosition();
    camera.UpdateBuffer(Renderer::width, Renderer::height);
    environment::UpdateBuffer();

	ID3D11Buffer* buffers[] = { 
        camera.buffer.Get(), 
        environment::environment_buffer.Get(), 
        environment::cumulus_buffer.Get() 
    };
    UINT bufferCount = sizeof(buffers) / sizeof(ID3D11Buffer*);

	auto renderSkyMap = [&]() {
		skyMap.Render(environment::GetLightDir());
	};

    auto renderSkyMapIrradiance = [&]() {
        ID3D11ShaderResourceView* srvs[] = {
            skyMap.colorSRV_.Get(), // 0
        };
        skyMapIrradiance.Render(environment::GetLightDir(), _countof(srvs), srvs);
    };

    auto renderSkyBox = [&]() {
        skyBox.UpdateTransform(camera);
        ID3D11ShaderResourceView* srvs[] = {
            nullptr, nullptr, nullptr,
            skyMap.colorSRV_.Get() // 3
        };
        skyBox.Render(_countof(srvs), srvs, bufferCount, buffers);
    };

    auto renderMonolith = [&]() {
		monolith.UpdateTransform(XMFLOAT3(10,10,10), XMFLOAT3(45,0,0), XMFLOAT3(0,0,0));
        monolith.Render(static_cast<float>(Renderer::width), static_cast<float>(Renderer::height), buffers, bufferCount);
    };

	auto renderCloud = [&]() {
		cloudMapGenerate.Draw(1, fmap.colorSRV_.GetAddressOf(), bufferCount, buffers);
		cloud.UpdateTransform(camera);
        ID3D11ShaderResourceView* srvs[] = { 
            monolith.depthSRV_.Get(), // 0
            fbm.colorSRV_.Get(), // 1 
            cloudMapGenerate.shaderResourceView_.Get(), // 2
            skyMapIrradiance.colorSRV_.Get() // 3
        };
		cloud.Render(_countof(srvs), srvs, bufferCount, buffers);
	};

	auto renderSmoothCloud = [&]() {
        //ditheringRevert.Draw(1, cloud.colorSRV_.GetAddressOf(), bufferCount, buffers);
        //smoothCloud.Draw(1, cloud.colorSRV_.GetAddressOf(), bufferCount, buffers);
	};

	auto renderManualMerger = [&]() {
		ID3D11ShaderResourceView* srvs[] = {
			monolith.colorSRV_.Get(),
            // give up FXAA because of black noise appears:
            // WE NEED "Bilateral Upsampling" for "Mixed resolution rendering"
            // REMEMBER THESE KEY WORDS
            cloud.colorSRV_.Get(), // smoothCloud.colorSRV_.Get(),
			monolith.depthSRV_.Get(),
			cloud.depthSRV_.Get(),
			skyBox.colorSRV_.Get(),
            monolith.normalSRV_.Get(),
		};
		UINT srvCout = sizeof(srvs) / sizeof(ID3D11ShaderResourceView*);
		manualMerger.Draw(srvCout, srvs, bufferCount, buffers);
	};

	auto renderFXAA = [&]() {
		fxaa.Draw(finalscene::colorRTV_.Get(), finalscene::colorRTV_.GetAddressOf(), 
            nullptr, 1, manualMerger.shaderResourceView_.GetAddressOf(), 
            bufferCount, buffers);
	};

    AnnotateRendering(L"Sky Map", renderSkyMap);
    AnnotateRendering(L"Sky Map Irradiance", renderSkyMapIrradiance);
    AnnotateRendering(L"Sky Box", renderSkyBox);
    AnnotateRendering(L"Render monolith as primitive", renderMonolith);
    AnnotateRendering(L"Render clouds using ray marching", [&]() { CalculateFrameTime(renderCloud); });
    AnnotateRendering(L"Stretch raymarch to full screen and merge with primitive", renderManualMerger);
    AnnotateRendering(L"FXAA to final image", renderFXAA);

#ifdef USE_IMGUI

    auto renderImgui = [&] {

        // Start the ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

		DispImguiInfo();

        // Rendering
        ImGui::Render();
        Renderer::context->OMSetRenderTargets(1, finalscene::colorRTV_.GetAddressOf(), nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    };

	AnnotateRendering(L"ImGui", renderImgui);

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

    hr = Renderer::device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &finalscene::colorRTV_);
    if (FAILED(hr)) return;

    Renderer::context->OMSetRenderTargets(1, finalscene::colorRTV_.GetAddressOf(), nullptr);

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
        camera.UpdateBuffer(width, height);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &viewport);
        
        // Reset all resources that depend on window size
        finalscene::colorRTV_.Reset();
        manualMerger.renderTargetView_.Reset();
        manualMerger.shaderResourceView_.Reset();
        manualMerger.texture_.Reset();
        cloud.colorRTV_.Reset();
        cloud.colorSRV_.Reset();
        cloud.colorTEX_.Reset();
		monolith.colorRTV_.Reset();
		monolith.depthSV_.Reset();
		monolith.colorSRV_.Reset();
		monolith.depthSRV_.Reset();
		monolith.colorTEX_.Reset();
		monolith.depthTEX_.Reset();

        // Resize swap chain
        Renderer::swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Recreate resources with new size
        monolith.CreateRenderTargets(width, height);
        cloud.CreateRenderTarget();
        manualMerger.CreateRenderTexture(width, height);
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

			// Update azimuth and elevation angles
            camera.az_ += dx;
            camera.el_ += dy;

            camera.UpdateEyePosition();
            camera.UpdateBuffer(Renderer::width, Renderer::width);

            mouse::lastPos = currentMousePos;
        }
        break;
    case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            float scaleSpeed = 1.0f;
            
            // Adjust radius based on wheel movement
            camera.dist_ -= zDelta * scaleSpeed;
            
            // Clamp radius to reasonable bounds
            camera.dist_ = max(1.0f, min(100 * 1852.0f, camera.dist_));
            
            // Update camera position maintaining direction
            camera.UpdateEyePosition();
            camera.UpdateBuffer(Renderer::width, Renderer::height);
        }
        break;
    case WM_SIZE:
        if (Renderer::context) {
            Renderer::width = static_cast<UINT>(LOWORD(lParam));
            Renderer::height = static_cast<UINT>(HIWORD(lParam));
			OnResize(Renderer::width, Renderer::height);
            camera.UpdateBuffer(Renderer::width, Renderer::height);
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

void environment::InitBuffer() {

    lightAz_ = 90.0f;
	lightEl_ = 45.0f;
    lightColor_ = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);

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
	bf.lightDir = GetLightDir();
	bf.lightColor = lightColor_;
	bf.time = XMVectorSet(timer.GetElapsedTime(), 0.0f, 0.0f, 0.0f);

    Renderer::context->UpdateSubresource(environment::environment_buffer.Get(), 0, nullptr, &bf, 0, 0);
}

float RandomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

namespace {

    // Add noise functions
    float Noise2D(float x, float y) {
        int xi = (int)floor(x);
        int yi = (int)floor(y);
        float xf = x - xi;
        float yf = y - yi;

        // Smooth interpolation
        float u = xf * xf * (3.0f - 2.0f * xf);
        float v = yf * yf * (3.0f - 2.0f * yf);

        return RandomFloat(0.0f, 1.0f); // Simplified for example
    }

    float FBM(float x, float y, int octaves) {
        float value = 0.0f;
        float amplitude = 0.5f;
        float frequency = 1.0f;

        for (int i = 0; i < octaves; i++) {
            value += amplitude * Noise2D(x * frequency, y * frequency);
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        return value;
    }

} // namespace

void environment::CreateCumulusBuffer() {

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(CumulusBuffer);
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = Renderer::device->CreateBuffer(&bufferDesc, nullptr, &cumulus_buffer);
    if (FAILED(hr)) {
        LogToFile("Failed to create cumulus buffer.");
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = Renderer::context->Map(cumulus_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        LogToFile("Failed to map cumulus buffer.");
        return;
    }

    CumulusBuffer* clouds = reinterpret_cast<CumulusBuffer*>(mappedResource.pData);

    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    const float SPACE_SCALE = total_distance_meter;
    const float HEIGHT_SCALE = 500.0f;
    const float SIZE_SCALE = 1000.0f;

    // Generate fractal-based clouds
    for (int i = 0; i < MAX_CLOUDS; i++) {
        float angle = (float)i / MAX_CLOUDS * XM_2PI;
        float radius = SPACE_SCALE * FBM(cos(angle), sin(angle), 4);

        float x = radius * cos(angle);
        float z = radius * sin(angle);
        float y = HEIGHT_SCALE * FBM(x * 0.1f, z * 0.1f, 3);
        float size = SIZE_SCALE * (0.5f + 0.5f * FBM(x * 0.2f, z * 0.2f, 2));

        clouds->cumulusPos[i] = XMFLOAT4(x, y, z, size);
    }

    clouds->cumulusPos[0] = XMFLOAT4(0, 0, 0, 1);

    Renderer::context->Unmap(cumulus_buffer.Get(), 0);

    // Set to pixel shader
    // Renderer::context->PSSetConstantBuffers(2, 1, cumulus_buffer.GetAddressOf());
}