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

class CubeMap {
public:
    // Vertex structure
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    struct CameraBuffer {
        XMMATRIX view; // 4 x 4 = 16 floats
        XMMATRIX projection; // 4 x 4 = 16 floats
        XMVECTOR lightDir; // 4 floats
    };

    ComPtr<ID3D11Buffer> buffer_;

    // each face resolution
    int width_ = 512;
    int height_ = 512;

	CubeMap(int width, int height) : width_(width), height_(height) {};
    ~CubeMap() {};

    ComPtr<ID3D11Texture2D> colorTEX_;
    ComPtr<ID3D11RenderTargetView> colorRTV_[6];
    ComPtr<ID3D11ShaderResourceView> colorSRV_;

    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    UINT indexCount_ = 0;

    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11VertexShader> vertexShader_;

    XMMATRIX viewMatrices_[6] = {
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(+1.0f, +0.0f, +0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, +0.0f, 0.0f))), // +X
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(-1.0f, +0.0f, +0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, +0.0f, 0.0f))), // -X
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(+0.0f, +1.0f, +0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f))), // +Y
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(+0.0f, -1.0f, +0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, +1.0f, 0.0f))), // -Y
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(+0.0f, +0.0f, +1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, +0.0f, 0.0f))), // +Z
        XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(+0.0f, +0.0f, -1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, +0.0f, 0.0f)))  // -Z
    };

    XMMATRIX projMatrix_ = XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 100.0f));

    void CreateRenderTarget();
    void CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
    void CreateGeometry();
    void Render(XMVECTOR lightDir);
};