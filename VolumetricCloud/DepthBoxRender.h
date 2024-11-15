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
        XMFLOAT3 normal;
    };

    struct Box {
        XMFLOAT3 center;
        XMFLOAT3 size;
        XMFLOAT3 rotation;
    };

    static const int RT_WIDTH = 512;
    static const int RT_HEIGHT = 512;

    static ComPtr<ID3D11Texture2D> colorTex;
    static ComPtr<ID3D11Texture2D> depthTex;
    static ComPtr<ID3D11RenderTargetView> rtv;
    static ComPtr<ID3D11DepthStencilView> dsv;
    static ComPtr<ID3D11ShaderResourceView> colorSRV;
    static ComPtr<ID3D11ShaderResourceView> depthSRV;

    static ComPtr<ID3D11Buffer> vertexBuffer;
    static ComPtr<ID3D11Buffer> indexBuffer;
    static ComPtr<ID3D11InputLayout> layout;
    static ComPtr<ID3D11VertexShader> vs;
    static ComPtr<ID3D11PixelShader> ps;

    static void Initialize();
    static void CreateRenderTargets();
    static void CreateShaders();
    static void CreateGeometry();
    static void Begin();
    static void RenderBox(const Box& box);
    static void End();
    static void Cleanup();

private:
    static std::vector<Vertex> CreateBoxVertices(const Box& box);
    static std::vector<uint32_t> CreateBoxIndices();
};