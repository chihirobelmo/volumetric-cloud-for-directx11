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

    Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTex);
    Renderer::device->CreateRenderTargetView(colorTex.Get(), nullptr, &rtv);
    Renderer::device->CreateShaderResourceView(colorTex.Get(), nullptr, &colorSRV);

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

    Renderer::device->CreateTexture2D(&depthDesc, nullptr, &depthTex);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    Renderer::device->CreateDepthStencilView(depthTex.Get(), &dsvDesc, &dsv);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    Renderer::device->CreateShaderResourceView(depthTex.Get(), &srvDesc, &depthSRV);
}

void Primitive::Begin(float width, float height) {
    
    float clearColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(rtv.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    Renderer::context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());

    D3D11_VIEWPORT vp = {};
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void Primitive::End() {
    ID3D11RenderTargetView* nullRTV = nullptr;
    Renderer::context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void Primitive::CreateShaders() {
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr;

    // Compile vertex shader with error checking
    hr = Renderer::CompileShaderFromFile(L"BoxDepth.hlsl", "VS", "vs_5_0", vsBlob);
    hr = Renderer::CompileShaderFromFile(L"BoxDepth.hlsl", "PS", "ps_5_0", psBlob);

    // Create shader objects
    hr = Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    hr = Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    // Update input layout to match VS_INPUT
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Create input layout 
    hr = Renderer::device->CreateInputLayout(
        layoutDesc,
        ARRAYSIZE(layoutDesc),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &layout
    );
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

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer);
    if (FAILED(hr)) {
        // Handle error
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
}

void Primitive::RenderBox(ID3D11Buffer** buffers, UINT bufferCount) {
    // Set shaders and input layout
    Renderer::context->VSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->PSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->VSSetShader(vs.Get(), nullptr, 0);
    Renderer::context->PSSetShader(ps.Get(), nullptr, 0);
    Renderer::context->IASetInputLayout(layout.Get());

    // Draw
    Renderer::context->Draw(4, 0); // 36 indices for a box
}

void Primitive::Cleanup() {
    colorTex.Reset();
    depthTex.Reset();
    rtv.Reset();
    dsv.Reset();
    colorSRV.Reset();
    depthSRV.Reset();
    vertexBuffer.Reset();
    indexBuffer.Reset();
    layout.Reset();
    vs.Reset();
    ps.Reset();
}