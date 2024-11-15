#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DepthBoxRender {
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

    int RT_WIDTH;
    int RT_HEIGHT;

    DepthBoxRender(int x, int y) : RT_WIDTH(x), RT_HEIGHT(y) {};
    ~DepthBoxRender() { Cleanup(); }

    ComPtr<ID3D11Texture2D> colorTex;
    ComPtr<ID3D11Texture2D> depthTex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> colorSRV;
    ComPtr<ID3D11ShaderResourceView> depthSRV;

    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;

    void Initialize();
    void CreateRenderTargets();
    void CreateShaders();
    void CreateBoxVertices();
    void CreateGeometry();
    void Begin();
    void RenderBox();
    void End();
    void Cleanup();
};