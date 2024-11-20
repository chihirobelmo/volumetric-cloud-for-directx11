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

void Camera::Init() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Camera::CameraBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA cameraInitData = {};
    cameraInitData.pSysMem = &bd;

    HRESULT hr = Renderer::device->CreateBuffer(&bd, &cameraInitData, &buffer);
    if (FAILED(hr))
        return;

    az_ = 270;
	el_ = 0;
	dist_ = 1000;
}

void Camera::Update(UINT width, UINT height) {

    XMVECTOR Forward = XMVector3Normalize(XMVectorSubtract(look_at_pos, eye_pos));
    XMVECTOR WorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Forward, WorldUp));
    XMVECTOR Up = XMVector3Cross(Forward, Right);

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float nearPlane = 0.1f;
    float farPlane = 422440.f;
    float hFov = 2.0f * atanf(tanf(vFov * 0.5f) * aspectRatio) * (180 / XM_PI);

    CameraBuffer bf;
    bf.view = XMMatrixTranspose(XMMatrixLookAtLH(eye_pos, look_at_pos, Up));
    // Inverting near-far on purpose, don't change it
    bf.projection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(vFov * (XM_PI / 180), aspectRatio, farPlane, nearPlane));
    bf.invViewProjMatrix = XMMatrixInverse(nullptr, XMMatrixMultiply(bf.view, bf.projection));
    bf.cameraPosition = eye_pos;
    bf.resolution = XMFLOAT2(width, height);
    bf.hvfov = XMFLOAT2(hFov * (XM_PI / 180), vFov * (XM_PI / 180));

    Renderer::context->UpdateSubresource(buffer.Get(), 0, nullptr, &bf, 0, 0);
}

void Camera::LookAtFrom() {

    float azimuth = az_ * (XM_PI / 180);
    float elevation = el_ * (XM_PI / 180);

    // Calculate Cartesian coordinates
    float x = dist_ * cosf(elevation) * sinf(azimuth);
    float y = dist_ * sinf(elevation);
    float z = dist_ * cosf(elevation) * cosf(azimuth);

    // Create the Cartesian vector
    XMVECTOR cartesian = XMVectorSet(x, y, z, 0.0f);

    // Translate the Cartesian vector by the origin
    eye_pos = XMVectorAdd(cartesian, look_at_pos);
}