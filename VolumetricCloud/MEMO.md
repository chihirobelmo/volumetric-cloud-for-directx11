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