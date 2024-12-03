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

    struct NoiseParams {
        float currentSlice;
        // haven't used yet but for padding
        float time;
        float scale;
        float persistence;
    };

    // pixel sizez
    int widthPx_ = 256;
    int slicePx_ = 256;
    int heightPx_ = 256;

    Noise(int widthPx, int slicePx, int heightPx) : widthPx_(widthPx), slicePx_(slicePx), heightPx_(heightPx) {}
    ~Noise() {}

    // noise
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11Texture3D> colorTEX_;
    ComPtr<ID3D11RenderTargetView> colorRTV_;
    ComPtr<ID3D11ShaderResourceView> colorSRV_;

    std::wstring fileName_ = L"";
    std::string entryPointVS_ = "";
    std::string entryPointPS_ = "";

    void RecompileShader();
    void CreateNoiseShaders(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
    void CreateNoiseTexture3DResource();
    void RenderNoiseTexture3D();

};