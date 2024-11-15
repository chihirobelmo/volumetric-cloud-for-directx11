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

// pre-render noise texture 3d for cloud rendering
class Noise {
public:
    struct Vertex3D {
        XMFLOAT3 position;    // 12 bytes (3 * 4)
        XMFLOAT3 texcoord;    // 12 bytes (3 * 4) - Changed from XMFLOAT2 to XMFLOAT3
        // Total: 24 bytes
    };

    // pixel sizez
    int widthPx = 256;
    int slicePx = 256;
    int heightPx = 256;

    Noise(int widthPx, int slicePx, int heightPx) : widthPx(widthPx), slicePx(slicePx), heightPx(heightPx) {}
    ~Noise() {}

    // noise
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Texture3D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;

    void CreateNoiseShaders();
    void CreateNoiseTexture3D();

};