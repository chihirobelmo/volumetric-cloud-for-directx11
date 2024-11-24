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

#include "PostProcess.h"

void PostProcess::CreateRenderTexture(UINT width, UINT height) {
    // Create the render target texture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    HRESULT hr = Renderer::device->CreateTexture2D(&textureDesc, nullptr, &texture_);

    // Create the render target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = textureDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    hr = Renderer::device->CreateRenderTargetView(texture_.Get(), &rtvDesc, &renderTargetView_);

    // Create the shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = Renderer::device->CreateShaderResourceView(texture_.Get(), &srvDesc, &shaderResourceView_);

    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Renderer::context->RSSetViewports(1, &vp);
}

void PostProcess::CreatePostProcessResources(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {
    // Create samplerState_ state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Renderer::device->CreateSamplerState(&sampDesc, &samplerState_);

    // Compile shaders
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    Renderer::CompileShaderFromFile(fileName, entryPointVS, "vs_5_0", vsBlob);
    Renderer::CompileShaderFromFile(fileName, entryPointPS, "ps_5_0", psBlob);

    // Create shader objects
    Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &vertexShader_);
    Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &pixelShader_);
}

void PostProcess::Draw(UINT width, UINT height, 
    ID3D11ShaderResourceView* const* ppShaderResourceViews,
    UINT numBuffers,
    ID3D11Buffer* const* ppConstantBuffers) {

    // Clear render target first
    float clearColor[4] = { 1.0f, 1.0f, 4.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    Renderer::context->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    D3D11_VIEWPORT rayMarchingVP = {};
    rayMarchingVP.Width = static_cast<float>(width);
    rayMarchingVP.Height = static_cast<float>(height);
    rayMarchingVP.MinDepth = 0.0f;
    rayMarchingVP.MaxDepth = 1.0f;
    rayMarchingVP.TopLeftX = 0;
    rayMarchingVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &rayMarchingVP);

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    Renderer::device->CreateSamplerState(&sampDesc, &sampler);

    Renderer::context->PSSetConstantBuffers(0, numBuffers, ppConstantBuffers);

    Renderer::context->PSSetShaderResources(0, 1, ppShaderResourceViews);
    Renderer::context->PSSetSamplers(0, 1, sampler.GetAddressOf());

    Renderer::context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    Renderer::context->PSSetShader(pixelShader_.Get(), nullptr, 0);

    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

    Vertex vertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, +1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(+1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(+1.0f, +1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }
    };

    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = { 0 };
    initData.pSysMem = vertices;
    ComPtr<ID3D11Buffer> vertexBuffer_;
    Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Renderer::context->Draw(4, 0);
}