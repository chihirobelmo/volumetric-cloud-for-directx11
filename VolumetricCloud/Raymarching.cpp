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

#include "Renderer.h"
#include "Raymarching.h"
#include "Noise.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Raymarch::SetupViewport() {
    // Setup the viewport to fixed resolution
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)RT_WIDTH;
    vp.Height = (FLOAT)RT_HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void Raymarch::CreateRenderTarget() {
    // Create the render target texture matching window size
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = RT_WIDTH;
    textureDesc.Height = RT_HEIGHT;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Renderer::device->CreateTexture2D(&textureDesc, nullptr, &Raymarch::tex);
    hr = Renderer::device->CreateRenderTargetView(Raymarch::tex.Get(), nullptr, &Raymarch::rtv);
    hr = Renderer::device->CreateShaderResourceView(Raymarch::tex.Get(), nullptr, &Raymarch::srv);
}

void Raymarch::CreateVertex() {

    // Create vertex data matching layout
    Vertex vertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, +1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(+1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(+1.0f, +1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }
    };

    // Create Index Buffer
    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = { 0 };
    initData.pSysMem = vertices;

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &initData, &Raymarch::vertex_buffer);
    if (FAILED(hr)) {
        // Handle error
    }
}

void Raymarch::CompileTheVertexShader() {
    ComPtr<ID3DBlob> pVSBlob;
    Renderer::CompileShaderFromFile(L"RayMarch.hlsl", "VS", "vs_5_0", pVSBlob);
    Renderer::device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &Raymarch::vertex_shader);

    // Define input layout description
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Create input layout 
    HRESULT hr = Renderer::device->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        &Raymarch::vertex_layout
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to CreateInputLayout." << std::endl;
        return;
    }

    Renderer::context->IASetInputLayout(Raymarch::vertex_layout.Get());
}

void Raymarch::CompileThePixelShader() {
    ComPtr<ID3DBlob> pPSBlob;
    Renderer::CompileShaderFromFile(L"RayMarch.hlsl", "PS", "ps_5_0", pPSBlob);
    Renderer::device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &Raymarch::pixel_shader);
}

void Raymarch::SetVertexBuffer() {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, Raymarch::vertex_buffer.GetAddressOf(), &stride, &offset);
}

void Raymarch::CreateSamplerState() {
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = Renderer::device->CreateSamplerState(&sampDesc, &sampler);
}