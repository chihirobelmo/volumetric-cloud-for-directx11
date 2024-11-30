#pragma once

#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>
#include <string>
#include <format>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace finalscene {

    ComPtr<ID3D11RenderTargetView> colorRTV_;

    void CreateRenderTargetView();

} // namespace finalscene

void finalscene::CreateRenderTargetView() {
    // Create a render target view
    ComPtr<ID3D11Texture2D> pBackBuffer;
    Renderer::swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    Renderer::device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &finalscene::colorRTV_);

    Renderer::context->OMSetRenderTargets(1, finalscene::colorRTV_.GetAddressOf(), nullptr);
}