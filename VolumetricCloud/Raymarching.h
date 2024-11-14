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

class Raymarch {
public:
    // Vertex structure
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    // ray march resolution
    static const int RT_WIDTH = 512;
    static const int RT_HEIGHT = 512;

    inline static ComPtr<ID3D11Texture2D> tex;
    inline static ComPtr<ID3D11RenderTargetView> rtv;
    inline static ComPtr<ID3D11ShaderResourceView> srv;

    inline static ComPtr<ID3D11Buffer> vertex_buffer;
    inline static ComPtr<ID3D11InputLayout> vertex_layout;
    inline static ComPtr<ID3D11PixelShader> pixel_shader;
    inline static ComPtr<ID3D11VertexShader> vertex_shader;

    static void SetupViewport();
    static void CreateRenderTarget();
    static void CompileTheVertexShader();
    static void CompileThePixelShader();
    static void CreateVertex();
    static void SetVertexBuffer();
    static void CreateSamplerState();

};