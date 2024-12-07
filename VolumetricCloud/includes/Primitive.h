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
#include <functional>

#include "Transform.h"
#include "Camera.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Primitive {
public:
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;  // Add texcoord
        XMFLOAT3 normal;
        XMFLOAT4 color;
    };

    struct Box {
        XMFLOAT3 center;
        XMFLOAT3 size;
        XMFLOAT3 rotation;
    };

    Primitive() {};
    ~Primitive() { Cleanup(); }

    Transform transform_;

    ComPtr<ID3D11Texture2D> colorTEX_;
    ComPtr<ID3D11Texture2D> depthTEX_;
    ComPtr<ID3D11RenderTargetView> colorRTV_;
    ComPtr<ID3D11DepthStencilView> depthSV_;
    ComPtr<ID3D11ShaderResourceView> colorSRV_;
    ComPtr<ID3D11ShaderResourceView> depthSRV_;

    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;

	UINT indexCount_ = 0;

    std::wstring shaderFilePath_ = L"";
    std::string entryPointVS_ = "";
    std::string entryPointPS_ = "";

    void UpdateTransform(Camera& camera);
    void CreateRenderTargets(int width, int height);
    void RecompileShader();
    void CreateShaders(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
    void CreateGeometry(std::function<void(std::vector<Primitive::Vertex>& vtx, std::vector<UINT>& idx)> vertexFunc);
    void Render(float width, float height, ID3D11Buffer** buffers, UINT bufferCount);
    void Cleanup();

	static void CreateTopologyIssueMonolith(std::vector<Vertex>& vertices, std::vector<UINT>& indices);
	static void CreateTopologyHealthMonolith(std::vector<Vertex>& vertices, std::vector<UINT>& indices);
};