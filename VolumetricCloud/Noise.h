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

    inline static int NOISE_TEX_SIZE = 256;

    // noise
    inline static ComPtr<ID3D11InputLayout> layout;
    inline static ComPtr<ID3D11VertexShader> vs;
    inline static ComPtr<ID3D11PixelShader> ps;
    inline static ComPtr<ID3D11SamplerState> sampler;
    inline static ComPtr<ID3D11ShaderResourceView> srv;
    inline static ComPtr<ID3D11Texture3D> tex;
    inline static ComPtr<ID3D11RenderTargetView> rtv;

    static void CreateNoiseShaders();
    static void CreateNoiseTexture3D();

};