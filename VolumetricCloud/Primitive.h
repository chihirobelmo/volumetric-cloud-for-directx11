#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Primitive {
public:
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;  // Add texcoord
    };

    struct Box {
        XMFLOAT3 center;
        XMFLOAT3 size;
        XMFLOAT3 rotation;
    };

    Primitive() {};
    ~Primitive() { Cleanup(); }

    ComPtr<ID3D11Texture2D> colorTex_;
    ComPtr<ID3D11Texture2D> depthTex_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    ComPtr<ID3D11DepthStencilView> depthStencilView_;
    ComPtr<ID3D11ShaderResourceView> colorSRV_;
    ComPtr<ID3D11ShaderResourceView> depthSRV_;

    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;

    void CreateRenderTargets(int width, int height);
    void CreateShaders();
    void CreateGeometry();
    void Begin(float width, float height);
    void RenderBox(ID3D11Buffer** buffers, UINT bufferCount);
    void End();
    void Cleanup();
};