#include <vector>
#include <DirectXMath.h>
using namespace DirectX;

#include "../includes/Primitive.h"
#include "../includes/Renderer.h"

void Primitive::UpdateTransform(XMFLOAT3 scale, XMFLOAT3 rotate, XMFLOAT3 translate) {
	transform_.SetScale(scale.x, scale.y, scale.z);
    transform_.SetRotation(rotate.x, rotate.y, rotate.z);
	transform_.SetTranslation(translate.x, translate.y, translate.z);
	transform_.UpdateBuffer();
}

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

    Renderer::device->CreateTexture2D(&textureDesc, nullptr, &colorTEX_);
    Renderer::device->CreateRenderTargetView(colorTEX_.Get(), nullptr, &colorRTV_);
    Renderer::device->CreateShaderResourceView(colorTEX_.Get(), nullptr, &colorSRV_);

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

void Primitive::RecompileShader() {
    CreateShaders(shaderFilePath_, entryPointVS_, entryPointPS_);
}

void Primitive::CreateShaders(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS) {

    shaderFilePath_ = fileName;
    entryPointVS_ = entryPointVS;
    entryPointPS_ = entryPointPS;

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
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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

void Primitive::CreateGeometry(std::function<void(std::vector<Primitive::Vertex>& vtx, std::vector<UINT>& idx)> vertexFunc) {

    std::vector<Vertex> vertex;
    std::vector<uint32_t> indicies;

    vertexFunc(vertex, indicies);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(vertex.size() * sizeof(Vertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertex.data();
    Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);

    bd.Usage = D3D11_USAGE_DYNAMIC; // D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(indicies.size() * sizeof(uint32_t));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// 0;

    initData.pSysMem = indicies.data();
    Renderer::device->CreateBuffer(&bd, &initData, &indexBuffer_);

	indexCount_ = indicies.size();

    transform_.CreateBuffer();
}

void Primitive::Render(float width, float height, ID3D11Buffer** buffers, UINT bufferCount) {

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Renderer::context->ClearRenderTargetView(colorRTV_.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(depthSV_.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);

    Renderer::context->OMSetRenderTargets(1, colorRTV_.GetAddressOf(), depthSV_.Get());

    D3D11_VIEWPORT vp = {};
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);

    // Set shaders and input inputLayout_
    Renderer::context->VSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->VSSetConstantBuffers(bufferCount, 1, transform_.buffer_.GetAddressOf());
    Renderer::context->PSSetConstantBuffers(0, bufferCount, buffers);
    Renderer::context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    Renderer::context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    Renderer::context->IASetInputLayout(inputLayout_.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    Renderer::context->DrawIndexed(indexCount_, 0, 0); // 36 indices for a box

	// Unbind render target
    ID3D11RenderTargetView* nullRTV = nullptr;
    Renderer::context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void Primitive::Cleanup() {
    colorTEX_.Reset();
    depthTEX_.Reset();
    colorRTV_.Reset();
    depthSV_.Reset();
    colorSRV_.Reset();
    depthSRV_.Reset();
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    inputLayout_.Reset();
    vertexShader_.Reset();
    pixelShader_.Reset();
}

namespace {

void TranslateVertices(std::vector<Primitive::Vertex>& vertices, const XMFLOAT3& translation) {
    for (auto& vertex : vertices) {
        XMVECTOR pos = XMLoadFloat3(&vertex.position);
        XMVECTOR trans = XMLoadFloat3(&translation);
        pos = XMVectorAdd(pos, trans);
        XMStoreFloat3(&vertex.position, pos);
    }
}

} // namespace

/// <summary>
/// This monolith has topology issue as triangle is so long, depth interpolation does not work well, 
/// making cloud intersect changes by camera angle
/// reserving the method for educational purpose.
/// </summary>
void Primitive::CreateTopologyIssueMonolith(std::vector<Primitive::Vertex>& vertices, std::vector<UINT>& indices) {

    const float scale = 100.0f;
    const float depth = scale * 1.0f * 1.0f;
    const float width = scale * 2.0f * 2.0f;
    const float height = scale * 3.0f * 3.0f;

    const XMFLOAT3 A = XMFLOAT3(+width * 0.5, -height * 0.5, -depth * 0.5); // top_left_front
    const XMFLOAT3 B = XMFLOAT3(-width * 0.5, -height * 0.5, -depth * 0.5); // top_right_front
    const XMFLOAT3 C = XMFLOAT3(+width * 0.5, +height * 0.5, -depth * 0.5); // bottom_left_front
    const XMFLOAT3 D = XMFLOAT3(-width * 0.5, +height * 0.5, -depth * 0.5); // bottom_right_front
    const XMFLOAT3 E = XMFLOAT3(+width * 0.5, -height * 0.5, +depth * 0.5); // top_left_behind
    const XMFLOAT3 F = XMFLOAT3(-width * 0.5, -height * 0.5, +depth * 0.5); // top_right_behind
    const XMFLOAT3 G = XMFLOAT3(+width * 0.5, +height * 0.5, +depth * 0.5); // bottom_left_behind
    const XMFLOAT3 H = XMFLOAT3(-width * 0.5, +height * 0.5, +depth * 0.5); // bottom_right_behind

    const XMFLOAT4 black = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);


    /* MONOLITH
    
                TOP
               
              16E ---- 17F
                |      |
	          18A ---- 19B

      LEFT    FRONT           RIGHT     BACK
                                  
	 8E - 9A   0A ---- 1B   12B - 13F  4F ---- 5E 
	  |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
      |   |     |      |      |   |     |      | 
	10G - 11C  2C ---- 3D   14D - 15H  6H ---- 7G 

              20C ---- 21D
                |      |
	          22G ---- 23H

			 BOTTOM

    */

    vertices = {
        // front face
        { A, XMFLOAT2(0.0f, 0.0f), XMFLOAT3( 0.0f, 0.0f, +1.0f), black }, // 0
        { B, XMFLOAT2(0.0f, 1.0f), XMFLOAT3( 0.0f, 0.0f, +1.0f), black }, // 1
        { C, XMFLOAT2(1.0f, 0.0f), XMFLOAT3( 0.0f, 0.0f, +1.0f), black }, // 2
        { D, XMFLOAT2(1.0f, 1.0f), XMFLOAT3( 0.0f, 0.0f, +1.0f), black }, // 3
        // back face
        { F, XMFLOAT2(0.0f, 0.0f), XMFLOAT3( 0.0f, 0.0f, -1.0f), black }, // 4
        { E, XMFLOAT2(0.0f, 1.0f), XMFLOAT3( 0.0f, 0.0f, -1.0f), black }, // 5
        { H, XMFLOAT2(1.0f, 0.0f), XMFLOAT3( 0.0f, 0.0f, -1.0f), black }, // 6
        { G, XMFLOAT2(1.0f, 1.0f), XMFLOAT3( 0.0f, 0.0f, -1.0f), black }, // 7
        // left face
		{ E, XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), black }, // 8
        { A, XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), black }, // 9
        { G, XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), black }, // 10
        { C, XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), black }, // 11
        // right face
        { B, XMFLOAT2(0.0f, 0.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f), black }, // 12
        { F, XMFLOAT2(0.0f, 1.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f), black }, // 13
        { D, XMFLOAT2(1.0f, 0.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f), black }, // 14
        { H, XMFLOAT2(1.0f, 1.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f), black }, // 15
        // top face
        { E, XMFLOAT2(0.0f, 0.0f), XMFLOAT3( 0.0f, +1.0f, 0.0f), black }, // 16
        { F, XMFLOAT2(0.0f, 1.0f), XMFLOAT3( 0.0f, +1.0f, 0.0f), black }, // 17
        { A, XMFLOAT2(1.0f, 0.0f), XMFLOAT3( 0.0f, +1.0f, 0.0f), black }, // 18
        { B, XMFLOAT2(1.0f, 1.0f), XMFLOAT3( 0.0f, +1.0f, 0.0f), black }, // 19
        // bottom face
        { C, XMFLOAT2(0.0f, 0.0f), XMFLOAT3( 0.0f, -1.0f, 0.0f), black }, // 20
        { D, XMFLOAT2(0.0f, 1.0f), XMFLOAT3( 0.0f, -1.0f, 0.0f), black }, // 21
        { G, XMFLOAT2(1.0f, 0.0f), XMFLOAT3( 0.0f, -1.0f, 0.0f), black }, // 22
        { H, XMFLOAT2(1.0f, 1.0f), XMFLOAT3( 0.0f, -1.0f, 0.0f), black }  // 23
    };

    // in DiretX, the front face is counter-clockwise. makes culling to front.
    // but this looks CW and working fine, how?
    indices = {
        // front face
        0, 1, 2, 3, 2, 1,
        // back face
        4, 5, 6, 7, 6, 5,
        // left face
        8, 9, 10, 11, 10, 9,
        // right face
        12, 13, 14, 15, 14, 13,
        // top face
        16, 17, 18, 19, 18, 17,
        // bottom face
        20, 21, 22, 23, 22, 21
    };
}

/// <summary>
/// This monolith consists of isosceles triangles creating squares.
/// Topology has less error for depth calculation.
/// Makes cloud intersect well.
/// </summary>
void Primitive::CreateTopologyHealthMonolith(std::vector<Primitive::Vertex>& vertices, std::vector<UINT>& indices) {

    /* MONOLITH

    we have to create vertices equal length space segments
    otherwise depth changes by camera angle
    makes cloud look intersected weirdly

       TOP

       140 -- 141 -- 142 -- 143 -- 144
        |   /  |   /  |   /  |   /  |
        |  /   |  /   |  /   |  /   |
       145 -- 146 -- 147 -- 148 -- 149

        FRONT                        RIGHT      BACK                         LEFT

        0 --  1 --  2 --  3 --  4   100 -- 101   50 -- 51 -- 52 -- 53 -- 54   120 -- 121
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
        5 --  6 --  7 --  8 --  9   102 -- 103   55 -- 56 -- 57 -- 58 -- 59   122 -- 123
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       10 -- 11 -- 12 -- 13 -- 14   104 -- 105   60 -- 61 -- 62 -- 63 -- 64   124 -- 125
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       15 -- 16 -- 17 -- 18 -- 19   106 -- 107   65 -- 66 -- 67 -- 68 -- 69   126 -- 127
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       20 -- 21 -- 22 -- 23 -- 24   108 -- 109   70 -- 71 -- 72 -- 73 -- 74   128 -- 129
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       25 -- 26 -- 27 -- 28 -- 29   110 -- 111   75 -- 76 -- 77 -- 78 -- 79   130 -- 131
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       30 -- 31 -- 32 -- 33 -- 34   112 -- 113   80 -- 81 -- 82 -- 83 -- 84   132 -- 133
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       35 -- 36 -- 37 -- 38 -- 39   114 -- 115   85 -- 86 -- 87 -- 88 -- 89   134 -- 135
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       40 -- 41 -- 42 -- 43 -- 44   116 -- 117   90 -- 91 -- 92 -- 93 -- 94   136 -- 137
        |  /  |  /  |  /  |  /  |    |   /  |    |  /  |  /  |  /  |  /  |     |   /  |
        | /   | /   | /   | /   |    |  /   |    | /   | /   | /   | /   |     |  /   |
       45 -- 46 -- 47 -- 48 -- 49   118 -- 119   95 -- 96 -- 97 -- 98 -- 99   138 -- 139

       150 -- 151 -- 152 -- 153 -- 154
        |   /  |   /  |   /  |   /  |
        |  /   |  /   |  /   |  /   |
       155 -- 156 -- 157 -- 158 -- 159

        BOTTOM

    */

    const XMFLOAT4 black = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    // front face
    for (int v = 0; v <= 900; v += 100) {
        for (int u = 0; u <= 400; u += 100) {
            vertices.push_back({ XMFLOAT3(u, v, 0.0f), XMFLOAT2(u / 400.0, v / 900.0), XMFLOAT3(0.0f, 0.0f, 1.0f), black });
        }
    }
    // in DiretX, the front face is counter-clockwise. makes culling to front.
    for (int v = 0; v < 45; v += 5) {
        for (int u = 0; u < 4; u++) {
            indices.push_back(v + u);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 6);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 5);
        }
    }

    // back face
    for (int v = 0; v <= 900; v += 100) {
        for (int u = 0; u <= 400; u += 100) {
            vertices.push_back({ XMFLOAT3(u, v, 100.0f), XMFLOAT2(u / 400.0, v / 900.0), XMFLOAT3(0.0f, 0.0f, -1.0f), black });
        }
    }
    for (int v = 50; v < 95; v += 5) {
        for (int u = 0; u < 4; u++) {
            indices.push_back(v + u);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 6);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 1);
        }
    }

    // right face
    for (int v = 0; v <= 900; v += 100) {
        for (int u = 0; u <= 100; u += 100) {
            vertices.push_back({ XMFLOAT3(0.0, v, u), XMFLOAT2(u / 100.0, v / 900.0), XMFLOAT3(1.0f, 0.0f, 0.0f), black });
        }
    }
    for (int v = 100; v < 118; v += 2) {
        indices.push_back(v);
        indices.push_back(v + 1);
        indices.push_back(v + 2);
        indices.push_back(v + 3);
        indices.push_back(v + 2);
        indices.push_back(v + 1);
    }

    // left face
    for (int v = 0; v <= 900; v += 100) {
        for (int u = 0; u <= 100; u += 100) {
            vertices.push_back({ XMFLOAT3(400.0, v, u), XMFLOAT2(u / 100.0, v / 900.0), XMFLOAT3(-1.0f, 0.0f, 0.0f), black });
        }
    }
    for (int v = 120; v < 138; v += 2) {
        indices.push_back(v);
        indices.push_back(v + 2);
        indices.push_back(v + 1);
        indices.push_back(v + 3);
        indices.push_back(v + 1);
        indices.push_back(v + 2);
    }

    // top face
    for (int v = 0; v <= 100; v += 100) {
        for (int u = 0; u <= 400; u += 100) {
            vertices.push_back({ XMFLOAT3(u, 0.0, v), XMFLOAT2(u / 400.0, v / 100.0), XMFLOAT3(0.0f, 1.0f, 0.0f), black });
        }
    }
    for (int v = 140; v < 145; v += 5) {
        for (int u = 0; u < 4; u++) {
            indices.push_back(v + u);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 6);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 1);
        }
    }

    // bottom face
    for (int v = 0; v <= 100; v += 100) {
        for (int u = 0; u <= 400; u += 100) {
            vertices.push_back({ XMFLOAT3(u, 900.0, v), XMFLOAT2(u / 400.0, v / 100.0), XMFLOAT3(0.0f, -1.0f, 0.0f), black });
        }
    }
    for (int v = 150; v < 155; v += 5) {
        for (int u = 0; u < 4; u++) {
            indices.push_back(v + u);
            indices.push_back(v + u + 5);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 6);
            indices.push_back(v + u + 1);
            indices.push_back(v + u + 5);
        }
    }

    TranslateVertices(vertices, XMFLOAT3(-400 * 0.5, -900 * 0.5, -100 * 0.5));
}