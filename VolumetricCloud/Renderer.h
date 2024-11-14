#pragma once

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

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Renderer {
public:
    // screen resolution state
    inline static UINT width = 1024;
    inline static UINT height = 1024;

    // Global variables
    inline static HWND hWnd;

    inline static ComPtr<ID3D11Device> device;
    inline static ComPtr<ID3D11DeviceContext> context;
    inline static ComPtr<IDXGISwapChain> swapchain;

    static void SetupViewport() {
        // Setup the viewport
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)Renderer::width;
        vp.Height = (FLOAT)Renderer::height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        Renderer::context->RSSetViewports(1, &vp);
    }

};