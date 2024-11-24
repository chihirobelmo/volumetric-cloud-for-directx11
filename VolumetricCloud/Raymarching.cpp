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
    vp.Width = (FLOAT)width_;
    vp.Height = (FLOAT)height_;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void Raymarch::CreateRenderTarget() {
    // Create the render target texture matching window size
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width_;
    textureDesc.Height = height_;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTEX_);
    hr = Renderer::device->CreateRenderTargetView(colorTEX_.Get(), nullptr, &colorRTV_);
    hr = Renderer::device->CreateShaderResourceView(colorTEX_.Get(), nullptr, &colorSRV_);

    // for dept hdebug but do not want to output actual depth
    D3D11_TEXTURE2D_DESC texture2Desc = {};
    texture2Desc.Width = width_;
    texture2Desc.Height = height_;
    texture2Desc.MipLevels = 1;
    texture2Desc.ArraySize = 1;
    texture2Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture2Desc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    texture2Desc.Usage = D3D11_USAGE_DEFAULT;
    texture2Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = Renderer::device->CreateTexture2D(&texture2Desc, nullptr, &debugTEX_);
    hr = Renderer::device->CreateRenderTargetView(debugTEX_.Get(), nullptr, &debugRTV_);
    hr = Renderer::device->CreateShaderResourceView(debugTEX_.Get(), nullptr, &debugSRV_);

    // Create depth texture with R32_FLOAT format for reading in shader
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width_;
    depthDesc.Height = height_;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Renderer::device->CreateTexture2D(&depthDesc, nullptr, &depthTEX_);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    Renderer::device->CreateDepthStencilView(depthTEX_.Get(), &dsvDesc, &depthSV_);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    Renderer::device->CreateShaderResourceView(depthTEX_.Get(), &srvDesc, &depthSRV_);

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;

    ComPtr<ID3D11DepthStencilState> depthStencilState;
    Renderer::device->CreateDepthStencilState(&dsDesc, &depthStencilState);
    Renderer::context->OMSetDepthStencilState(depthStencilState.Get(), 1);
}

void Raymarch::CreateVertex() {
    
    XMFLOAT3 top_left_behind =     XMFLOAT3(+1.0, -1.0, +1.0);
    XMFLOAT3 top_right_behind =    XMFLOAT3(-1.0, -1.0, +1.0);
    XMFLOAT3 bottom_left_behind =  XMFLOAT3(+1.0, +1.0, +1.0);
    XMFLOAT3 bottom_right_behind = XMFLOAT3(-1.0, +1.0, +1.0);
    XMFLOAT3 top_left_front =      XMFLOAT3(+1.0, -1.0, -1.0);
    XMFLOAT3 top_right_front =     XMFLOAT3(-1.0, -1.0, -1.0);
    XMFLOAT3 bottom_left_front =   XMFLOAT3(+1.0, +1.0, -1.0);
    XMFLOAT3 bottom_right_front =  XMFLOAT3(-1.0, +1.0, -1.0);
    

	// in DiretX, the front face is counter-clockwise. makes culling to front.
    Vertex verticesBox[] = {
        // front face
        { bottom_left_front,   XMFLOAT2(0.0f, 1.0f) },
        { top_left_front,      XMFLOAT2(0.0f, 0.0f) },
        { bottom_right_front,  XMFLOAT2(1.0f, 1.0f) },
        { top_right_front,     XMFLOAT2(1.0f, 0.0f) },
        // back face                                
        { bottom_right_behind, XMFLOAT2(0.0f, 1.0f) },
        { top_right_behind,    XMFLOAT2(0.0f, 0.0f) },
        { bottom_left_behind,  XMFLOAT2(1.0f, 1.0f) },
        { top_left_behind,     XMFLOAT2(1.0f, 0.0f) },
        // left face                                
        { bottom_left_behind,  XMFLOAT2(0.0f, 1.0f) },
        { top_left_behind,     XMFLOAT2(0.0f, 0.0f) },
        { bottom_left_front,   XMFLOAT2(1.0f, 1.0f) },
        { top_left_front,      XMFLOAT2(1.0f, 0.0f) },
        // right face                               
        { bottom_right_front,  XMFLOAT2(0.0f, 1.0f) },
        { top_right_front,     XMFLOAT2(0.0f, 0.0f) },
        { bottom_right_behind, XMFLOAT2(1.0f, 1.0f) },
        { top_right_behind,    XMFLOAT2(1.0f, 0.0f) },
        // top face                                 
        { top_left_front,      XMFLOAT2(0.0f, 1.0f) },
        { top_left_behind,     XMFLOAT2(0.0f, 0.0f) },
        { top_right_front,     XMFLOAT2(1.0f, 1.0f) },
        { top_right_behind,    XMFLOAT2(1.0f, 0.0f) },
        // bottom face                              
        { bottom_left_behind,  XMFLOAT2(1.0f, 1.0f) },
        { bottom_left_front,   XMFLOAT2(1.0f, 0.0f) },
        { bottom_right_behind, XMFLOAT2(0.0f, 1.0f) },
        { bottom_right_front,  XMFLOAT2(0.0f, 0.0f) }
    };

    // to inside
    UINT indicesBox[] = {
        // front face
        0, 2, 1, 2, 3, 1,
        // back face
        4, 6, 5, 6, 7, 5,
        // left face
        8, 10, 9, 10, 11, 9,
        // right face
        12, 14, 13, 14, 15, 13,
        // top face
        16, 18, 17, 18, 19, 17,
        // bottom face
        20, 22, 21, 22, 23, 21
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(verticesBox);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = verticesBox;
    Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(indicesBox);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indicesBox;
    Renderer::device->CreateBuffer(&bd, &initData, &indexBuffer_);
}

void Raymarch::CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {
    ComPtr<ID3DBlob> pVSBlob;
    Renderer::CompileShaderFromFile(fileName, entryPointVS, "vs_5_0", pVSBlob);
    Renderer::device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &vertexShader_);

    // Define input inputLayout_ description
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Create input inputLayout_ 
    HRESULT hr = Renderer::device->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        &inputLayout_
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to CreateInputLayout." << std::endl;
        return;
    }

    Renderer::context->IASetInputLayout(inputLayout_.Get());

    ComPtr<ID3DBlob> pPSBlob;
    Renderer::CompileShaderFromFile(fileName, entryPointPS, "ps_5_0", pPSBlob);
    Renderer::device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pixelShader_);
}

void Raymarch::SetVertexBuffer() {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Raymarch::CreateSamplerState() {

    HRESULT hr;

    D3D11_SAMPLER_DESC depthDesc = {};
    depthDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    depthDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    depthDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    depthDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    depthDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // Common for depth comparisons
    depthDesc.MinLOD = 0;
    depthDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Renderer::device->CreateSamplerState(&depthDesc, &depthSampler_);

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Renderer::device->CreateSamplerState(&sampDesc, &noiseSampler_);
}