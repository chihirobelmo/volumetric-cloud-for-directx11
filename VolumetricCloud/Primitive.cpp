#include <vector>
#include <DirectXMath.h>
using namespace DirectX;

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

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;

    ComPtr<ID3D11DepthStencilState> depthStencilState;
    Renderer::device->CreateDepthStencilState(&dsDesc, &depthStencilState);
    Renderer::context->OMSetDepthStencilState(depthStencilState.Get(), 1);
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
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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

void Primitive::CreateGeometry() {

    std::vector<Vertex> vtx;
    std::vector<uint32_t> idc;

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

	// front face
    for (int v = 0; v <= 900; v += 100) {
        for (int u = 0; u <= 400; u += 100) {
            vtx.push_back({ XMFLOAT3(u, v, 0.0f), XMFLOAT2(u / 400.0, v / 900.0), XMFLOAT3(0.0f, 0.0f, 1.0f) });
        }
    }
    // in DiretX, the front face is counter-clockwise. makes culling to front.
    for (int v = 0; v < 45; v += 5) {
        for (int u = 0; u < 4; u++) {
			idc.push_back(v + u);
			idc.push_back(v + u + 5);
			idc.push_back(v + u + 1);
			idc.push_back(v + u + 6);
			idc.push_back(v + u + 1);
			idc.push_back(v + u + 5);
        }
    }

	// back face
	for (int v = 0; v <= 900; v += 100) {
		for (int u = 0; u <= 400; u += 100) {
			vtx.push_back({ XMFLOAT3(u, v, 100.0f), XMFLOAT2(u / 400.0, v / 900.0), XMFLOAT3(0.0f, 0.0f, -1.0f) });
		}
	}
	for (int v = 50; v < 95; v += 5) {
		for (int u = 0; u < 4; u++) {
            idc.push_back(v + u);
            idc.push_back(v + u + 1);
            idc.push_back(v + u + 5);
            idc.push_back(v + u + 6);
            idc.push_back(v + u + 5);
            idc.push_back(v + u + 1);
		}
	}

	// right face
	for (int v = 0; v <= 900; v += 100) {
		for (int u = 0; u <= 100; u += 100) {
			vtx.push_back({ XMFLOAT3(0.0, v, u), XMFLOAT2(u / 100.0, v / 900.0), XMFLOAT3(1.0f, 0.0f, 0.0f) });
		}
	}
	for (int v = 100; v < 118; v += 2) {
        idc.push_back(v);
        idc.push_back(v + 1);
        idc.push_back(v + 2);
        idc.push_back(v + 3);
        idc.push_back(v + 2);
        idc.push_back(v + 1);
	}

	// left face
	for (int v = 0; v <= 900; v += 100) {
		for (int u = 0; u <= 100; u += 100) {
			vtx.push_back({ XMFLOAT3(400.0, v, u), XMFLOAT2(u / 100.0, v / 900.0), XMFLOAT3(-1.0f, 0.0f, 0.0f) });
		}
	}
	for (int v = 120; v < 138; v += 2) {
		idc.push_back(v);
		idc.push_back(v + 2);
		idc.push_back(v + 1);
		idc.push_back(v + 3);
		idc.push_back(v + 1);
		idc.push_back(v + 2);
	}

	// top face
	for (int v = 0; v <= 100; v += 100) {
		for (int u = 0; u <= 400; u += 100) {
			vtx.push_back({ XMFLOAT3(u, 0.0, v), XMFLOAT2(u / 400.0, v / 100.0), XMFLOAT3(0.0f, 1.0f, 0.0f) });
		}
	}
	for (int v = 140; v < 145; v += 5) {
        for (int u = 0; u < 4; u++) {
            idc.push_back(v + u);
            idc.push_back(v + u + 1);
            idc.push_back(v + u + 5);
            idc.push_back(v + u + 6);
            idc.push_back(v + u + 5);
            idc.push_back(v + u + 1);
        }
	}

	// bottom face
	for (int v = 0; v <= 100; v += 100) {
		for (int u = 0; u <= 400; u += 100) {
			vtx.push_back({ XMFLOAT3(u, 900.0, v), XMFLOAT2(u / 400.0, v / 100.0), XMFLOAT3(0.0f, -1.0f, 0.0f) });
		}
	}
	for (int v = 150; v < 155; v += 5) {
        for (int u = 0; u < 4; u++) {
            idc.push_back(v + u);
            idc.push_back(v + u + 5);
            idc.push_back(v + u + 1);
            idc.push_back(v + u + 6);
            idc.push_back(v + u + 1);
            idc.push_back(v + u + 5);
        }
	}

	TranslateVertices(vtx, XMFLOAT3(-400 * 0.5, -900 * 0.5, -100 * 0.5));

 //   vtx = {
 //       // front face
 //       { bottom_left_front,   XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
 //       { top_left_front,      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
 //       { bottom_right_front,  XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
 //       { top_right_front,     XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
 //       // back face
 //       { bottom_right_behind, XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
 //       { top_right_behind,    XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
 //       { bottom_left_behind,  XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
 //       { top_left_behind,     XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
 //       // left face
 //       { bottom_left_behind,  XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
 //       { top_left_behind,     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
 //       { bottom_left_front,   XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
 //       { top_left_front,      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
 //       // right face
 //       { bottom_right_front,  XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
 //       { top_right_front,     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
 //       { bottom_right_behind, XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
 //       { top_right_behind,    XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
 //       // top face
 //       { top_left_front,      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
 //       { top_left_behind,     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
 //       { top_right_front,     XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
 //       { top_right_behind,    XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
 //       // bottom face
 //       { bottom_left_behind,  XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
 //       { bottom_left_front,   XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
 //       { bottom_right_behind, XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
 //       { bottom_right_front,  XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) }
 //   };

 //   idc = {
 //       // front face
 //       0, 1, 2, 2, 1, 3,
 //       // back face
 //       4, 5, 6, 6, 5, 7,
 //       // left face
 //       8, 9, 10, 10, 9, 11,
 //       // right face
 //       12, 13, 14, 14, 13, 15,
 //       // top face
 //       16, 17, 18, 18, 17, 19,
 //       // bottom face
 //       20, 21, 22, 22, 21, 23
 //   };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(vtx.size() * sizeof(Vertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vtx.data();
    Renderer::device->CreateBuffer(&bd, &initData, &vertexBuffer_);

    bd.Usage = D3D11_USAGE_DYNAMIC; // D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(idc.size() * sizeof(uint32_t));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// 0;

    initData.pSysMem = idc.data();
    Renderer::device->CreateBuffer(&bd, &initData, &indexBuffer_);

	indexCount_ = idc.size();
}

void Primitive::Begin(float width, float height) {

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Renderer::context->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);

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

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Renderer::context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Renderer::context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    Renderer::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    Renderer::context->DrawIndexed(indexCount_, 0, 0); // 36 indices for a box
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