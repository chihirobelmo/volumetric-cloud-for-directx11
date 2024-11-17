#include "Primitive.h"
#include "Renderer.h"

void Primitive::CreateRenderTargets(int width, int height) {

    // Create the render target texture matching window size
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

    Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTex_);
    Renderer::device->CreateRenderTargetView(colorTex_.Get(), nullptr, &renderTargetView_);
    Renderer::device->CreateShaderResourceView(colorTex_.Get(), nullptr, &colorSRV_);

    // Create depth texture with R32_FLOAT format for reading in shader
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Renderer::device->CreateTexture2D(&depthDesc, nullptr, &depthTex_);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    Renderer::device->CreateDepthStencilView(depthTex_.Get(), &dsvDesc, &depthStencilView_);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    Renderer::device->CreateShaderResourceView(depthTex_.Get(), &srvDesc, &depthSRV_);
}

void Primitive::CreateShaders(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    HRESULT hr;

    // Compile vertex shader with error checking
    hr = Renderer::CompileShaderFromFile(fileName, entryPointVS, "vs_5_0", vsBlob);
    hr = Renderer::CompileShaderFromFile(fileName, entryPointPS, "ps_5_0", psBlob);

    // Create shader objects
    hr = Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_);
    hr = Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_);

    // Update input inputLayout_ to match VS_INPUT
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Create input inputLayout_ 
    hr = Renderer::device->CreateInputLayout(
        layoutDesc,
        ARRAYSIZE(layoutDesc),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &inputLayout_
    );
	MESSAGEBOX(hr, "Error", "Primitive: Failed to create input layout.");
}

void Primitive::CreateGeometry() {

    float size = 1000;

    Vertex vertices[] = {
        // Front face
        { XMFLOAT3(-size, -size, 0.0f), XMFLOAT2(0.0f, 1.0f)  },
        { XMFLOAT3(-size, +size, 0.0f), XMFLOAT2(0.0f, 0.0f)  },
        { XMFLOAT3(+size, -size, 0.0f), XMFLOAT2(1.0f, 1.0f)  },
        { XMFLOAT3(+size, +size, 0.0f), XMFLOAT2(1.0f, 0.0f)  }
    };

    // Create Index Buffer
    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = { 0 };
    initData.pSysMem = vertices;

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);
    if (FAILED(hr)) {
        // Handle error
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
}

void Primitive::Begin(float width, float height) {

    float clearColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    Renderer::context->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());

    D3D11_VIEWPORT vp = {};
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void Primitive::RenderBox(ID3D11Buffer** buffers, UINT bufferCount) {
    // Set shaders and input inputLayout_
    Renderer::context->VSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->PSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    Renderer::context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    Renderer::context->IASetInputLayout(inputLayout_.Get());

    // Draw
    Renderer::context->Draw(4, 0); // 36 indices for a box
}

void Primitive::End() {
    ID3D11RenderTargetView* nullRTV = nullptr;
    Renderer::context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void Primitive::Cleanup() {
    colorTex_.Reset();
    depthTex_.Reset();
    renderTargetView_.Reset();
    depthStencilView_.Reset();
    colorSRV_.Reset();
    depthSRV_.Reset();
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    inputLayout_.Reset();
    vertexShader_.Reset();
    pixelShader_.Reset();
}