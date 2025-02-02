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
#include "../includes/Camera.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Raymarch::UpdateTransform(Camera& camera) {
    transform_.SetScale(1.0f, 1.0f, 1.0f);
    transform_.SetRotation(-camera.el_ * (XM_PI / 180), camera.az_ * (XM_PI / 180), 0.0f);
    transform_.SetTranslation(0.0f, 0.0f, 0.0f);
    transform_.UpdateBuffer();
}

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

        Renderer::device->CreateTexture2D(&textureDesc, nullptr, &prevTEX_);
        Renderer::device->CreateRenderTargetView(prevTEX_.Get(), nullptr, &prevRTV_);
        Renderer::device->CreateShaderResourceView(prevTEX_.Get(), nullptr, &prevSRV_);
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
    
    XMFLOAT3 top_left_behind =     XMFLOAT3(+10.0, -10.0, +1.0);
    XMFLOAT3 top_right_behind =    XMFLOAT3(-10.0, -10.0, +1.0);
    XMFLOAT3 bottom_left_behind =  XMFLOAT3(+10.0, +10.0, +1.0);
    XMFLOAT3 bottom_right_behind = XMFLOAT3(-10.0, +10.0, +1.0);
    XMFLOAT3 top_left_front =      XMFLOAT3(+10.0, -10.0, -1.0);
    XMFLOAT3 top_right_front =     XMFLOAT3(-10.0, -10.0, -1.0);
    XMFLOAT3 bottom_left_front =   XMFLOAT3(+10.0, +10.0, -1.0);
    XMFLOAT3 bottom_right_front =  XMFLOAT3(-10.0, +10.0, -1.0);
    
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

    transform_.CreateBuffer();
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
        fmapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        fmapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        fmapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        fmapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        fmapDesc.MinLOD = 0;
        fmapDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Renderer::device->CreateSamplerState(&fmapDesc, &fmapSampler_);

        // cubemap sampler
		D3D11_SAMPLER_DESC cubeDesc = {};
		cubeDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		cubeDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		cubeDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		cubeDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		cubeDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		cubeDesc.MinLOD = 0;
		cubeDesc.MaxLOD = D3D11_FLOAT32_MAX;

		Renderer::device->CreateSamplerState(&cubeDesc, &cubeSampler_);

        // linear sampler
        D3D11_SAMPLER_DESC linDesc = {};
        linDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        linDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        linDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        linDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        linDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        linDesc.MinLOD = 0;
        linDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Renderer::device->CreateSamplerState(&linDesc, &linearSampler_);
    }
    
    // Create the constant buffer
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.ByteWidth = sizeof(InputData);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA dsd = {};
        dsd.pSysMem = &cbDesc;

        Renderer::device->CreateBuffer(&cbDesc, &dsd, &inputData_);

        InputData bf;
        bf.pixelsize = XMFLOAT4(width_, height_, 0.0f, 0.0f);

        Renderer::context->UpdateSubresource(inputData_.Get(), 0, nullptr, &bf, 0, 0);
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
    Renderer::context->VSSetConstantBuffers(3, 1, transform_.buffer_.GetAddressOf());
    Renderer::context->PSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->PSSetConstantBuffers(4, 1, inputData_.GetAddressOf());

    // Set resources for cloud rendering
    Renderer::context->PSSetShaderResources(0, NumViews, ppShaderResourceViews);
    Renderer::context->PSSetSamplers(0, 1, depthSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(1, 1, noiseSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(2, 1, fmapSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(3, 1, cubeSampler_.GetAddressOf());
    Renderer::context->PSSetSamplers(4, 1, linearSampler_.GetAddressOf());

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

namespace {

bool CompileComputeShaderFromSource(const std::wstring& source, const std::string& entryPoint, ID3D11Device* device, ID3D11ComputeShader** computeShader) {
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        source.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Shader Compilation Error", MB_OK | MB_ICONERROR);
        }
        return false;
    }

    hr = device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, computeShader);
    return SUCCEEDED(hr);
}

} // namespace

bool Raymarch::ComputeShaderFromPointToPoint(DirectX::XMVECTOR startPoint, DirectX::XMVECTOR endPoint, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, std::vector<float>& result) {
    // Create and set up the compute shader
    // Assume computeShader_ is already created and set up
    // Ensure the compute shader is initialized
    if (!computeShader_) {
        if (!(CompileComputeShaderFromSource(L"shaders/RayMarch.hlsl", "CSMain", Renderer::device.Get(), &computeShader_))) {
            return false;
        }
    }

    // Create input buffer
    struct InputData {
        DirectX::XMFLOAT4 startPoint;
        DirectX::XMFLOAT4 endPoint;
    };

    InputData inputData = {
        DirectX::XMFLOAT4(startPoint.m128_f32[0], startPoint.m128_f32[1], startPoint.m128_f32[2], startPoint.m128_f32[3]),
        DirectX::XMFLOAT4(endPoint.m128_f32[0], endPoint.m128_f32[1], endPoint.m128_f32[2], endPoint.m128_f32[3])
    };

    D3D11_BUFFER_DESC inputBufferDesc = {};
    inputBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    inputBufferDesc.ByteWidth = sizeof(InputData);
    inputBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    inputBufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA inputDataInit = {};
    inputDataInit.pSysMem = &inputData;

    Microsoft::WRL::ComPtr<ID3D11Buffer> inputBuffer;
    HRESULT hr = Renderer::device->CreateBuffer(&inputBufferDesc, &inputDataInit, &inputBuffer);
    if (FAILED(hr)) return false;

    // Create output buffer
    D3D11_BUFFER_DESC outputBufferDesc = {};
    outputBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    outputBufferDesc.ByteWidth = sizeof(float);
    outputBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    outputBufferDesc.CPUAccessFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Buffer> outputBuffer;
    hr = Renderer::device->CreateBuffer(&outputBufferDesc, nullptr, &outputBuffer);
    if (FAILED(hr)) return false;

    // Create UAV for output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 1;

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> outputUAV;
    hr = Renderer::device->CreateUnorderedAccessView(outputBuffer.Get(), &uavDesc, &outputUAV);
    if (FAILED(hr)) return false;

    // Set the compute shader and UAV
    Renderer::context->CSSetShader(computeShader_.Get(), nullptr, 0);
    Renderer::context->CSSetShaderResources(0, NumViews, ppShaderResourceViews);
    Renderer::context->CSSetSamplers(0, 1, depthSampler_.GetAddressOf());
    Renderer::context->CSSetSamplers(1, 1, noiseSampler_.GetAddressOf());
    Renderer::context->CSSetSamplers(2, 1, fmapSampler_.GetAddressOf());
    Renderer::context->CSSetSamplers(3, 1, cubeSampler_.GetAddressOf());
    Renderer::context->CSSetConstantBuffers(3, 1, inputBuffer.GetAddressOf());
    Renderer::context->CSSetConstantBuffers(4, 1, inputData_.GetAddressOf());
    Renderer::context->CSSetUnorderedAccessViews(0, 1, outputUAV.GetAddressOf(), nullptr);

    // Dispatch the compute shader
    Renderer::context->Dispatch(1, 1, 1);

    // Copy the result from the GPU to the CPU
    D3D11_BUFFER_DESC readbackDesc = {};
    readbackDesc.Usage = D3D11_USAGE_STAGING;
    readbackDesc.ByteWidth = sizeof(float);
    readbackDesc.BindFlags = 0;
    readbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Buffer> readbackBuffer;
    hr = Renderer::device->CreateBuffer(&readbackDesc, nullptr, &readbackBuffer);
    if (FAILED(hr)) return false;

    Renderer::context->CopyResource(readbackBuffer.Get(), outputBuffer.Get());

    // Map the readback buffer to access the result
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = Renderer::context->Map(readbackBuffer.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) return false;

    float* data = reinterpret_cast<float*>(mappedResource.pData);
    result.assign(data, data + 1);

    Renderer::context->Unmap(readbackBuffer.Get(), 0);

    return true;
}