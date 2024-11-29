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
#include "CubeMap.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void CubeMap::CreateRenderTarget() {

    // Create the render target texture matching window size
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width_; // Width of each face
        textureDesc.Height = height_; // Height of each face
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 6; // 6 faces
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTEX_);

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = textureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.ArraySize = 1;

        for (int i = 0; i < 6; ++i) {
            rtvDesc.Texture2DArray.FirstArraySlice = i;
            Renderer::device->CreateRenderTargetView(colorTEX_.Get(), &rtvDesc, &colorRTV_[i]);
        }

        // Create the Shader Resource View (SRV) for the cubemap texture
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;

        Renderer::device->CreateShaderResourceView(colorTEX_.Get(), &srvDesc, &colorSRV_);
    }

    // Init buffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(CameraBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA cameraInitData = {};
        cameraInitData.pSysMem = &bd;

        HRESULT hr = Renderer::device->CreateBuffer(&bd, &cameraInitData, &buffer_);
    }
}

void CubeMap::CreateGeometry() {

    XMFLOAT3 top_left_behind = XMFLOAT3(+1.0, -1.0, +1.0);
    XMFLOAT3 top_right_behind = XMFLOAT3(-1.0, -1.0, +1.0);
    XMFLOAT3 bottom_left_behind = XMFLOAT3(+1.0, +1.0, +1.0);
    XMFLOAT3 bottom_right_behind = XMFLOAT3(-1.0, +1.0, +1.0);
    XMFLOAT3 top_left_front = XMFLOAT3(+1.0, -1.0, -1.0);
    XMFLOAT3 top_right_front = XMFLOAT3(-1.0, -1.0, -1.0);
    XMFLOAT3 bottom_left_front = XMFLOAT3(+1.0, +1.0, -1.0);
    XMFLOAT3 bottom_right_front = XMFLOAT3(-1.0, +1.0, -1.0);

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

    indexCount_ = sizeof(indicesBox) / sizeof(UINT);
}

void CubeMap::CompileShader(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {

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
}

void CubeMap::Render(XMVECTOR lightDir) {

    // Set view port
    D3D11_VIEWPORT rayMarchingVP = {};
    rayMarchingVP.Width = static_cast<float>(width_);
    rayMarchingVP.Height = static_cast<float>(height_);
    rayMarchingVP.MinDepth = 0.0f;
    rayMarchingVP.MaxDepth = 1.0f;
    rayMarchingVP.TopLeftX = 0;
    rayMarchingVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &rayMarchingVP);

    for (int i = 0; i < 6; ++i) {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Renderer::context->ClearRenderTargetView(colorRTV_[i].Get(), clearColor);
        Renderer::context->OMSetRenderTargets(1, colorRTV_[i].GetAddressOf(), nullptr);

        CameraBuffer bf;
        bf.view = viewMatrices_[i];
        bf.projection = projMatrix_;
		bf.lightDir = lightDir;
        Renderer::context->UpdateSubresource(buffer_.Get(), 0, nullptr, &bf, 0, 0);

        // Render the scene

        Renderer::context->VSSetConstantBuffers(0, 1, buffer_.GetAddressOf());
        Renderer::context->PSSetConstantBuffers(0, 1, buffer_.GetAddressOf());

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
}

void CubeMap::Render(XMVECTOR lightDir, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) {

    // Set view port
    D3D11_VIEWPORT rayMarchingVP = {};
    rayMarchingVP.Width = static_cast<float>(width_);
    rayMarchingVP.Height = static_cast<float>(height_);
    rayMarchingVP.MinDepth = 0.0f;
    rayMarchingVP.MaxDepth = 1.0f;
    rayMarchingVP.TopLeftX = 0;
    rayMarchingVP.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &rayMarchingVP);

    for (int i = 0; i < 6; ++i) {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Renderer::context->ClearRenderTargetView(colorRTV_[i].Get(), clearColor);
        Renderer::context->OMSetRenderTargets(1, colorRTV_[i].GetAddressOf(), nullptr);

        CameraBuffer bf;
        bf.view = viewMatrices_[i];
        bf.projection = projMatrix_;
        bf.lightDir = lightDir;
        Renderer::context->UpdateSubresource(buffer_.Get(), 0, nullptr, &bf, 0, 0);

        // Render the scene

        Renderer::context->VSSetConstantBuffers(0, 1, buffer_.GetAddressOf());
        Renderer::context->PSSetConstantBuffers(0, 1, buffer_.GetAddressOf());

        Renderer::context->PSSetShaderResources(0, NumViews, ppShaderResourceViews);

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
}