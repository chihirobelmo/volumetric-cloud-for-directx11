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

#define MESSAGEBOX(hr, title, message) if ( FAILED(hr) ) MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR)

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

    static HRESULT CompileShaderFromFile(const std::wstring& fileName, const std::string& entryPoint, const std::string& shaderModel, ComPtr<ID3DBlob>& outBlob) {
        DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
        shaderFlags |= D3DCOMPILE_DEBUG;
#endif

        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompileFromFile(fileName.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), shaderModel.c_str(), shaderFlags, 0, &outBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                std::string errorMessage = static_cast<const char*>(errorBlob->GetBufferPointer());
                MessageBoxA(nullptr, errorMessage.c_str(), "Shader Compilation Error", MB_OK | MB_ICONERROR);
            }
            else {
                std::cerr << "Unknown shader compilation error." << std::endl;
            }
        }

        return hr;
    }

    static XMVECTOR PolarToCartesian(const XMVECTOR& origin, float radius, float azimuth_deg, float elevation_deg) {

        float azimuth = azimuth_deg * (XM_PI / 180);
        float elevation = elevation_deg * (XM_PI / 180);

        // Calculate Cartesian coordinates
        float x = radius * cosf(elevation) * cosf(azimuth);
        float y = radius * sinf(elevation);
        float z = radius * cosf(elevation) * sinf(azimuth);

        // Create the Cartesian vector
        XMVECTOR cartesian = XMVectorSet(x, y, z, 0.0f);

        // Translate the Cartesian vector by the origin
        cartesian = XMVectorAdd(cartesian, origin);

        return cartesian;
    }
};