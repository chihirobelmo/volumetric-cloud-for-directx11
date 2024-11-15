#include "DepthBoxRender.h"
#include "Renderer.h"

// Define static members
ComPtr<ID3D11Texture2D> DepthBoxRender::colorTex;
ComPtr<ID3D11Texture2D> DepthBoxRender::depthTex;
ComPtr<ID3D11RenderTargetView> DepthBoxRender::rtv;
ComPtr<ID3D11DepthStencilView> DepthBoxRender::dsv;
ComPtr<ID3D11ShaderResourceView> DepthBoxRender::colorSRV;
ComPtr<ID3D11ShaderResourceView> DepthBoxRender::depthSRV;
ComPtr<ID3D11Buffer> DepthBoxRender::vertexBuffer;
ComPtr<ID3D11Buffer> DepthBoxRender::indexBuffer;
ComPtr<ID3D11InputLayout> DepthBoxRender::layout;
ComPtr<ID3D11VertexShader> DepthBoxRender::vs;
ComPtr<ID3D11PixelShader> DepthBoxRender::ps;

void DepthBoxRender::Initialize() {
    CreateRenderTargets();
    CreateShaders();
    CreateGeometry();
}

void DepthBoxRender::CreateRenderTargets() {
    // Create color texture
    D3D11_TEXTURE2D_DESC colorDesc = {};
    colorDesc.Width = RT_WIDTH;
    colorDesc.Height = RT_HEIGHT;
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDesc.SampleDesc.Count = 1;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Renderer::device->CreateTexture2D(&colorDesc, nullptr, &colorTex);
    Renderer::device->CreateRenderTargetView(colorTex.Get(), nullptr, &rtv);
    Renderer::device->CreateShaderResourceView(colorTex.Get(), nullptr, &colorSRV);

    // Create depth texture with R32_FLOAT format for reading in shader
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = RT_WIDTH;
    depthDesc.Height = RT_HEIGHT;
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

void DepthBoxRender::Begin() {
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(rtv.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    
    Renderer::context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
    
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(RT_WIDTH);
    vp.Height = static_cast<float>(RT_HEIGHT);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Renderer::context->RSSetViewports(1, &vp);
}

void DepthBoxRender::End() {
    ID3D11RenderTargetView* nullRTV = nullptr;
    Renderer::context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void DepthBoxRender::CreateShaders() {
    // Compile shaders from BoxDepth.hlsl
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    D3DCompileFromFile(L"BoxDepth.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, nullptr);
    D3DCompileFromFile(L"BoxDepth.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, nullptr);

    // Create shader objects
    Renderer::device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    Renderer::device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    Renderer::device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &DepthBoxRender::layout);
}

void DepthBoxRender::CreateGeometry() {
    Box defaultBox = { XMFLOAT3(0,0,0), XMFLOAT3(1,1,1), XMFLOAT3(0,0,0) };
    auto vertices = CreateBoxVertices(defaultBox);
    auto indices = CreateBoxIndices();

    // Create vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(Vertex) * vertices.size();
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();
    Renderer::device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer);

    // Create index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(uint32_t) * indices.size();
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();
    Renderer::device->CreateBuffer(&ibDesc, &ibData, &indexBuffer);
}

void DepthBoxRender::RenderBox(const Box& box) {
    // Set shaders and input layout
    Renderer::context->VSSetShader(vs.Get(), nullptr, 0);
    Renderer::context->PSSetShader(ps.Get(), nullptr, 0);
    Renderer::context->IASetInputLayout(layout.Get());

    // Set vertex and index buffers
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    Renderer::context->DrawIndexed(36, 0, 0); // 36 indices for a box
}

std::vector<DepthBoxRender::Vertex> DepthBoxRender::CreateBoxVertices(const Box& box) {
    std::vector<Vertex> vertices = {
        // Front face
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        
        // Back face
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
    };

    return vertices;
}

std::vector<uint32_t> DepthBoxRender::CreateBoxIndices() {
    return {
        // Front face
        0, 1, 2, 0, 2, 3,
        // Back face
        4, 5, 6, 4, 6, 7,
        // Left face
        4, 7, 1, 4, 1, 0,
        // Right face
        3, 2, 6, 3, 6, 5,
        // Top face
        1, 7, 6, 1, 6, 2,
        // Bottom face
        4, 0, 3, 4, 3, 5
    };
}

void DepthBoxRender::Cleanup() {
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