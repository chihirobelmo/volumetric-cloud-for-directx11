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

#include "../includes/Noise.h"
#include "../includes/Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Noise::CreateNoiseShaders(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {
    // Compile shaders
    ComPtr<ID3DBlob> vsBlob;
    Renderer::CompileShaderFromFile(fileName, entryPointVS, "vs_5_0", vsBlob);

    // Create vertex shader
    Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_);

    // Create input inputLayout_
    D3D11_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }  // Changed to R32G32B32_FLOAT
    };
    Renderer::device->CreateInputLayout(inputLayout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_);

    // Create pixel shader
    ComPtr<ID3DBlob> psBlob;
    Renderer::CompileShaderFromFile(fileName, entryPointPS, "ps_5_0", psBlob);
    Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_);
}

void Noise::CreateNoiseTexture3DResource() {

    // Define texture dimensions
    const UINT textureWidth = widthPx_;
    const UINT textureHeight = heightPx_;
    const UINT textureDepth = slicePx_;
    const DXGI_FORMAT textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Calculate the number of mip levels
    const UINT mipLevels = 4;

    // Create 3D texture
    D3D11_TEXTURE3D_DESC texDesc = {};
    texDesc.Width = widthPx_;
    texDesc.Height = heightPx_;
    texDesc.Depth = slicePx_;
    texDesc.MipLevels = mipLevels; // Specify 4 mip levels
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    // Prepare subresource data (example: simple gradient for demonstration)
    std::vector<D3D11_SUBRESOURCE_DATA> initData(mipLevels);
    std::vector<std::vector<float>> textureData(mipLevels);

	float bytePerPixel = 8; // 8 for 16bit, 4 for 8bit

    for (UINT mip = 0; mip < mipLevels; ++mip)
    {
        const float dataSize = widthPx_ * heightPx_ * slicePx_ * bytePerPixel; // 4 bytes per texel (RGBA8)
        textureData[mip].resize(dataSize);

        initData[mip].pSysMem = textureData[mip].data();
        initData[mip].SysMemPitch = widthPx_ * bytePerPixel; // Bytes per row
        initData[mip].SysMemSlicePitch = widthPx_ * heightPx_ * bytePerPixel; // Bytes per slice
    }

    HRESULT hr = Renderer::device->CreateTexture3D(&texDesc, initData.data(), &colorTEX_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create 3D texture." << std::endl;
        return;
    }

    // Create SRV for the 3D texture
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = mipLevels; // Specify 4 mip levels

    hr = Renderer::device->CreateShaderResourceView(colorTEX_.Get(), &srvDesc, &colorSRV_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create Shader Resource View for 3D texture." << std::endl;
        return;
    }
}


void Noise::RenderNoiseTexture3D() {
    // Create vertex buffer for full-screen quad with correct UVW coordinates
    Noise::Vertex3D noiseVertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },  // Bottom-left
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },  // Top-left
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) },  // Bottom-right
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }   // Top-right
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(noiseVertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbInitData = {};
    vbInitData.pSysMem = noiseVertices;

    ComPtr<ID3D11Buffer> noiseVertexBuffer;
    Renderer::device->CreateBuffer(&vbDesc, &vbInitData, &noiseVertexBuffer);

    // Set up the pipeline for noise generation
    UINT stride = sizeof(Vertex3D); // Fixed: Use correct stride
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, noiseVertexBuffer.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetInputLayout(Noise::inputLayout_.Get());
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Save current render targets
    ComPtr<ID3D11RenderTargetView> oldRTV;
    Renderer::context->OMGetRenderTargets(1, &oldRTV, nullptr);

    // Clear background to a mid-gray
    float clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

    // Set up viewport specifically for noise texture
    D3D11_VIEWPORT noiseVP;
    noiseVP.Width = (FLOAT)widthPx_;
    noiseVP.Height = (FLOAT)heightPx_;
    noiseVP.MinDepth = 0.0f;
    noiseVP.MaxDepth = 1.0f;
    noiseVP.TopLeftX = 0;
    noiseVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &noiseVP);

    // For each Z-slice of the 3D texture
    for (UINT slice = 0; slice < slicePx_; slice++) {
        // Create RTV for this slice
        D3D11_RENDER_TARGET_VIEW_DESC sliceRTVDesc = {};
        sliceRTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        sliceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        sliceRTVDesc.Texture3D.FirstWSlice = slice;
        sliceRTVDesc.Texture3D.WSize = 1;
        sliceRTVDesc.Texture3D.MipSlice = 0;

        ComPtr<ID3D11RenderTargetView> sliceRTV;
        Renderer::device->CreateRenderTargetView(Noise::colorTEX_.Get(), &sliceRTVDesc, &sliceRTV);

        // Set and clear the render target
        Renderer::context->OMSetRenderTargets(1, sliceRTV.GetAddressOf(), nullptr);
        Renderer::context->ClearRenderTargetView(sliceRTV.Get(), clearColor);

        // Set shaders and draw
        Renderer::context->VSSetShader(Noise::vertexShader_.Get(), nullptr, 0);
        Renderer::context->PSSetShader(Noise::pixelShader_.Get(), nullptr, 0);

        // Update noise parameters for this slice
        struct NoiseParams {
            float currentSlice;
            float time;
            float scale;
            float persistence;
        };

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(NoiseParams);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        NoiseParams params = {
            static_cast<float>(slice) / (slicePx_ - 1),  // currentSlice
            // for now needed for padding even if not used
            0.0f,                                // time
            4.0f,                                // scale
            0.5f                                 // persistence
        };
        D3D11_SUBRESOURCE_DATA cbData = { &params };

        ComPtr<ID3D11Buffer> noiseParamsCB;
        Renderer::device->CreateBuffer(&cbDesc, &cbData, &noiseParamsCB);
        Renderer::context->PSSetConstantBuffers(2, 1, noiseParamsCB.GetAddressOf());

        // Set viewport again for each slice to ensure correct dimensions
        Renderer::context->RSSetViewports(1, &noiseVP);

        // Draw the quad
        Renderer::context->Draw(4, 0);
        Renderer::context->GenerateMips(colorSRV_.Get());
    }

    // Restore original render target
    Renderer::context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), nullptr);
}