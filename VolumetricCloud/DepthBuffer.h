#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DepthBuffer {
public:
    static const int RT_WIDTH = 512;
    static const int RT_HEIGHT = 512;

    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    static ComPtr<ID3D11Texture2D> colorTex;
    static ComPtr<ID3D11Texture2D> depthTex;
    static ComPtr<ID3D11RenderTargetView> rtv;
    static ComPtr<ID3D11DepthStencilView> dsv;
    static ComPtr<ID3D11ShaderResourceView> colorSRV;
    static ComPtr<ID3D11ShaderResourceView> depthSRV;
    
    static ComPtr<ID3D11Buffer> vertexBuffer;
    static ComPtr<ID3D11InputLayout> layout;
    static ComPtr<ID3D11VertexShader> vs;
    static ComPtr<ID3D11PixelShader> ps;

    static void CreateRenderTargets();
    static void SetupViewport();
    static void Clear();
    static void Begin();
    static void End();
};