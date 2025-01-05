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

#include "Transform.h"
#include "../includes/Camera.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Raymarch {
public:
    // Vertex structure
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    Transform transform_;

    // ray march resolution
    int width_ = 512;
    int height_ = 512;

    Raymarch(int width, int height) : width_(width), height_(height) {};
	~Raymarch() {};

    ComPtr<ID3D11Texture2D> colorTEX_;
    ComPtr<ID3D11Texture2D> prevTEX_;
    ComPtr<ID3D11Texture2D> debugTEX_;
    ComPtr<ID3D11Texture2D> depthTEX_;

    ComPtr<ID3D11RenderTargetView> colorRTV_;
    ComPtr<ID3D11RenderTargetView> prevRTV_;
    ComPtr<ID3D11RenderTargetView> debugRTV_;
    ComPtr<ID3D11DepthStencilView> depthSV_;

    ComPtr<ID3D11ShaderResourceView> colorSRV_;
    ComPtr<ID3D11ShaderResourceView> prevSRV_;
    ComPtr<ID3D11ShaderResourceView> debugSRV_;
    ComPtr<ID3D11ShaderResourceView> depthSRV_;

    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    UINT indexCount_ = 0;

    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11VertexShader> vertexShader_;

    ComPtr<ID3D11ComputeShader> computeShader_;
    ComPtr<ID3D11Buffer> resultBuffer_;
    ComPtr<ID3D11UnorderedAccessView> resultUAV_;

    ComPtr<ID3D11SamplerState> depthSampler_;
    ComPtr<ID3D11SamplerState> noiseSampler_;
    ComPtr<ID3D11SamplerState> fmapSampler_;
    ComPtr<ID3D11SamplerState> cubeSampler_;
    ComPtr<ID3D11SamplerState> linearSampler_;

    std::wstring shaderFilePath_ = L"";
    std::string entryPointVS_ = "";
	std::string entryPointPS_ = "";

    void UpdateTransform(Camera& camera);
    void CreateRenderTarget();
	void RecompileShader();
    void CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
    void CreateGeometry();
    void Render(UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, UINT bufferCount, ID3D11Buffer** buffers);

    bool ComputeShaderFromPointToPoint(DirectX::XMVECTOR startPoint, DirectX::XMVECTOR endPoint, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, std::vector<float>& result);
};