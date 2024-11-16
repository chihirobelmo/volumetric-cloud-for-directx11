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

#include "Camera.h"
#include "Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Camera::UpdateProjectionMatrix(int windowWidth, int windowHeight) {

    float nearPlane = 0.01f;
    float farPlane = 10000000.0f;
    float fov = Camera::fov * (XM_PI / 180);

    Camera::aspect_ratio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    Camera::projection = XMMatrixPerspectiveFovLH(fov, Camera::aspect_ratio, nearPlane, farPlane);
}

void Camera::UpdateView(XMVECTOR Eye, XMVECTOR At) {

    // Calculate view basis vectors
    XMVECTOR Forward = XMVector3Normalize(XMVectorSubtract(At, Eye));
    XMVECTOR WorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Forward, WorldUp));
    XMVECTOR Up = XMVector3Cross(Forward, Right);

    Camera::view = XMMatrixLookAtLH(Eye, At, Up);
}

void Camera::InitBuffer() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Camera::CameraBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA cameraInitData = {};
    cameraInitData.pSysMem = &bd;

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &cameraInitData, &Camera::camera_buffer);
    if (FAILED(hr))
        return;
}

void Camera::UpdateBuffer() {
    CameraBuffer bf;
    bf.view = XMMatrixTranspose(Camera::view);
    bf.projection = XMMatrixTranspose(Camera::projection);
    bf.cameraPosition = Camera::eye_pos;
    bf.aspectRatio = Camera::aspect_ratio;
    bf.cameraFov = Camera::fov;

    Renderer::context->UpdateSubresource(Camera::camera_buffer.Get(), 0, nullptr, &bf, 0, 0);
}

void Camera::InitializeCamera() {
    // camera initialization
    Camera::eye_pos = Renderer::PolarToCartesian(Camera::look_at_pos, Camera::distance_meter, Camera::azimuth_hdg, Camera::elevation_deg);
}