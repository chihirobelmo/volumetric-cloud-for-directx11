# create a Texture2D from CPU data in DirectX 11

if we want to get a weather info from in-game weather cell info.

## Plan 1: Direct Creation

```cpp
bool CreateTexture2DFromData(double** data, int width, int height) {
    // Convert double array to RGBA format
    std::vector<uint8_t> pixelData(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            uint8_t value = static_cast<uint8_t>(data[y][x] * 255.0); // Normalize to 0-255
            pixelData[idx + 0] = value; // R
            pixelData[idx + 1] = value; // G
            pixelData[idx + 2] = value; // B
            pixelData[idx + 3] = 255;   // A
        }
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixelData.data();
    initData.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) return false;

    // Create SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, &srv);
    return SUCCEEDED(hr);
}
```

## Plan 2: Dynamic Update

```cpp
bool CreateDynamicTexture2D(int width, int height) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) return false;

    // Create SRV
    hr = device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    return SUCCEEDED(hr);
}

void UpdateTextureData(double** data, int width, int height) {
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    context->Map(texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    
    uint8_t* texPtr = (uint8_t*)mappedResource.pData;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = x * 4;
            uint8_t value = static_cast<uint8_t>(data[y][x] * 255.0);
            texPtr[idx + 0] = value; // R
            texPtr[idx + 1] = value; // G
            texPtr[idx + 2] = value; // B
            texPtr[idx + 3] = 255;   // A
        }
        texPtr += mappedResource.RowPitch;
    }
    
    context->Unmap(texture.Get(), 0);
}
```

# Render into shared texture 

```cpp
void CreateSharedRenderTarget(UINT width, UINT height) {
    // Create color texture
    D3D11_TEXTURE2D_DESC colorDesc = {};
    colorDesc.Width = width;
    colorDesc.Height = height;
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDesc.SampleDesc.Count = 1;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> colorTex;
    Renderer::device->CreateTexture2D(&colorDesc, nullptr, &colorTex);
    Renderer::device->CreateRenderTargetView(colorTex.Get(), nullptr, &finalscene::rtv);
    Renderer::device->CreateShaderResourceView(colorTex.Get(), nullptr, &finalscene::srv);

    // Create depth texture
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTex;
    Renderer::device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
    Renderer::device->CreateDepthStencilView(depthTex.Get(), nullptr, &finalscene::dsv);
}
```

```cpp
void Render() {
    // Clear the render target and depth stencil
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    Renderer::context->ClearRenderTargetView(finalscene::rtv.Get(), clearColor);
    Renderer::context->ClearDepthStencilView(finalscene::dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Set the render target and depth stencil
    Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), finalscene::dsv.Get());

    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(Renderer::width);
    vp.Height = static_cast<float>(Renderer::height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Renderer::context->RSSetViewports(1, &vp);

    // Render the box
    {
        camera.UpdateBuffer();
        Renderer::context->VSSetConstantBuffers(0, 1, camera.buffer.GetAddressOf());
        Renderer::context->PSSetConstantBuffers(0, 1, camera.buffer.GetAddressOf());
        depthBoxRender->RenderBox();
    }

    // Render the clouds
    {
        // Set the same render target and depth stencil
        Renderer::context->OMSetRenderTargets(1, finalscene::rtv.GetAddressOf(), finalscene::dsv.Get());

        // Set viewport
        Renderer::context->RSSetViewports(1, &vp);

        // Render clouds
        cloud.Render();
    }

    // Present the frame
    Renderer::swapchain->Present(1, 0);
}
```