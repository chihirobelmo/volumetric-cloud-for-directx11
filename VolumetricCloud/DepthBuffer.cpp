#include "DepthBuffer.h"
#include "Renderer.h"

ComPtr<ID3D11Texture2D> DepthBuffer::colorTex;
ComPtr<ID3D11Texture2D> DepthBuffer::depthTex;
ComPtr<ID3D11RenderTargetView> DepthBuffer::rtv;
ComPtr<ID3D11DepthStencilView> DepthBuffer::dsv;
ComPtr<ID3D11ShaderResourceView> DepthBuffer::colorSRV;
ComPtr<ID3D11ShaderResourceView> DepthBuffer::depthSRV;

void DepthBuffer::CreateRenderTargets() {
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

    // Create depth texture
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

    // Create depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    Renderer::device->CreateDepthStencilView(depthTex.Get(), &dsvDesc, &dsv);

    // Create depth SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    Renderer::device->CreateShaderResourceView(depthTex.Get(), &srvDesc, &depthSRV);
}

void DepthBuffer::SetupViewport() {
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(RT_WIDTH);
    vp.Height = static_cast<float>(RT_HEIGHT);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    Renderer::context->RSSetViewports(1, &vp);
}

void DepthBuffer::Clear() {
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(rtv.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);
}

void DepthBuffer::Begin() {
    Renderer::context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
    SetupViewport();
}

void DepthBuffer::End() {
    ID3D11RenderTargetView* nullRTV = nullptr;
    Renderer::context->OMSetRenderTargets(1, &nullRTV, nullptr);
}