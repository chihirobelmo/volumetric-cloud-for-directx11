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

class Raymarch {
public:
    // Vertex structure
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    // ray march resolution
    int width = 512;
    int height = 512;

    Raymarch(int width, int height) : width(width), height(height) {};
	~Raymarch() {};

    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11Texture2D> tex2;
    ComPtr<ID3D11Texture2D> dtex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11RenderTargetView> rtv2;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11ShaderResourceView> srv2;
    ComPtr<ID3D11ShaderResourceView> dsrv;

    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    ComPtr<ID3D11InputLayout> vertex_layout;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11VertexShader> vertex_shader;

    ComPtr<ID3D11SamplerState> depthSampler;
    ComPtr<ID3D11SamplerState> noiseSampler;

    void SetupViewport();
    void CreateRenderTarget();
    void CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
    void CreateVertex();
    void SetVertexBuffer();
    void CreateSamplerState();

};