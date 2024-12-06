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

#include "../includes/Renderer.h"
#include "../includes/Raymarching.h"
#include "../includes/Noise.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Raymarch::CreateRenderTarget() {

    // Create the render target texture matching window size
    {
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

        Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTEX_);
        Renderer::device->CreateRenderTargetView(colorTEX_.Get(), nullptr, &colorRTV_);
        Renderer::device->CreateShaderResourceView(colorTEX_.Get(), nullptr, &colorSRV_);
    }

    // for dept hdebug but do not want to output actual depth
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width_;
        textureDesc.Height = height_;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        Renderer::device->CreateTexture2D(&textureDesc, nullptr, &debugTEX_);
        Renderer::device->CreateRenderTargetView(debugTEX_.Get(), nullptr, &debugRTV_);
        Renderer::device->CreateShaderResourceView(debugTEX_.Get(), nullptr, &debugSRV_);
    }

    // Create depth texture with R32_FLOAT format for reading in shader
    {
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
}

/// <summary>
/// We create a box that covers the entire screen as all triangles are facing to inside.
/// then it is easy to get ray direction by 
/// 
/// `normalize(worldPos - cameraPos)`
/// 
/// while shader will change worldPos to chase camera position always by 
/// 
/// `float4 worldPos = float4(input.Pos + cameraPosition.xyz, 1.0f);`
/// 
/// </summary>
void Raymarch::CreateGeometry() {
    
    XMFLOAT3 top_left_behind =     XMFLOAT3(+1.0, -1.0, +1.0);
    XMFLOAT3 top_right_behind =    XMFLOAT3(-1.0, -1.0, +1.0);
    XMFLOAT3 bottom_left_behind =  XMFLOAT3(+1.0, +1.0, +1.0);
    XMFLOAT3 bottom_right_behind = XMFLOAT3(-1.0, +1.0, +1.0);
    XMFLOAT3 top_left_front =      XMFLOAT3(+1.0, -1.0, -1.0);
    XMFLOAT3 top_right_front =     XMFLOAT3(-1.0, -1.0, -1.0);
    XMFLOAT3 bottom_left_front =   XMFLOAT3(+1.0, +1.0, -1.0);
    XMFLOAT3 bottom_right_front =  XMFLOAT3(-1.0, +1.0, -1.0);
    
    verticesBox_ = {
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

	// all triangles are facing to inside.
	// in DiretX, the front face is counter-clockwise. makes culling to front.
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
    bd.ByteWidth = static_cast<UINT>(verticesBox_.size() * sizeof(Vertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = verticesBox_.data();
    Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(indicesBox);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indicesBox;
    Renderer::device->CreateBuffer(&bd, &initData, &indexBuffer_);

    indexCount_ = sizeof(indicesBox) / sizeof(UINT);
}

void Raymarch::RecompileShader() {
	CompileShader(shaderFilePath_, entryPointVS_, entryPointPS_);
}

void Raymarch::CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {

	shaderFilePath_ = fileName;
	entryPointVS_ = entryPointVS;
	entryPointPS_ = entryPointPS;

    // shader
    {
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

    // sampler
    {
        D3D11_SAMPLER_DESC depthDesc = {};
        depthDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        depthDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        depthDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        depthDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        depthDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // Common for depth comparisons
        depthDesc.MinLOD = 0;
        depthDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Renderer::device->CreateSamplerState(&depthDesc, &depthSampler_);

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Renderer::device->CreateSamplerState(&sampDesc, &noiseSampler_);

        D3D11_SAMPLER_DESC fmapDesc = {};
        fmapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        fmapDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        fmapDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        fmapDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        fmapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        fmapDesc.MinLOD = 0;
        fmapDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Renderer::device->CreateSamplerState(&sampDesc, &fmapSampler_);

        // cubemap sampler
		D3D11_SAMPLER_DESC cubeDesc = {};
		cubeDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		cubeDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		cubeDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		cubeDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		cubeDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		cubeDesc.MinLOD = 0;
		cubeDesc.MaxLOD = D3D11_FLOAT32_MAX;

		Renderer::device->CreateSamplerState(&cubeDesc, &cubeSampler_);
    }
}

void Raymarch::Render(UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, UINT bufferCount, ID3D11Buffer** buffers) {

    // Clear render target first
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Renderer::context->ClearRenderTargetView(colorRTV_.Get(), clearColor);
    Renderer::context->ClearRenderTargetView(debugRTV_.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(depthSV_.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);

	// Set render target
    ID3D11RenderTargetView* rtvs[] = { colorRTV_.Get(), debugRTV_.Get() };
    Renderer::context->OMSetRenderTargets(2, rtvs, nullptr/*cloud.depthSV_.Get()*/);

	// Set view port
    D3D11_VIEWPORT rayMarchingVP = {};
    rayMarchingVP.Width = static_cast<float>(width_);
    rayMarchingVP.Height = static_cast<float>(height_);
    rayMarchingVP.MinDepth = 0.0f;
    rayMarchingVP.MaxDepth = 1.0f;
    rayMarchingVP.TopLeftX = 0;
    rayMarchingVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &rayMarchingVP);

    // Update camera constants
    Renderer::context->VSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->PSSetConstantBuffers(0, bufferCount, buffers);

    // Set resources for cloud rendering
    Renderer::context->PSSetShaderResources(0, NumViews, ppShaderResourceViews);
    Renderer::context->PSSetSamplers(0, 1, depthSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(1, 1, noiseSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(2, 1, fmapSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(3, 1, cubeSampler_.GetAddressOf());

    // Render clouds with ray marching
    Renderer::context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    Renderer::context->PSSetShader(pixelShader_.Get(), nullptr, 0);

	// Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw
    Renderer::context->DrawIndexed(indexCount_, 0, 0);
}