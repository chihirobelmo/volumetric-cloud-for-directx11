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

#include "../includes/Camera.h"
#include "../includes/Renderer.h"

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
}

void Camera::UpdateBuffer(UINT width, UINT height) {

    XMVECTOR Forward = XMVector3Normalize(XMVectorSubtract(lookAtPos_, eyePos_));
    XMVECTOR WorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (XMVector3Equal(Forward, WorldUp) || XMVector3Equal(Forward, -WorldUp)) {
        WorldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Forward, WorldUp));
    XMVECTOR Up = XMVector3Cross(Forward, Right);

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float hFov = 2.0f * atanf(tanf(vFov_ * 0.5f) * aspectRatio) * (180 / XM_PI);

    CameraBuffer bf;
    // consider camera position is always 0
    bf.view = XMMatrixTranspose(XMMatrixLookAtLH(XMVectorSet(0.0, 0.0, 0.0, 1.0), XMVectorSubtract(lookAtPos_, eyePos_), Up));
    // Inverting near-far on purpose, don't change it
    bf.projection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(vFov_ * (XM_PI / 180), aspectRatio, far_, near_));
    bf.invViewProjMatrix = XMMatrixInverse(nullptr, XMMatrixMultiply(bf.view, bf.projection));
    bf.previousViewProjectionMatrix = lastViewProjectionMatrix_;
    bf.cameraPosition = eyePos_;
    bf.resolution = XMFLOAT2(width, height);
	bf.padding1 = XMFLOAT2(0.0f, 0.0f);

    Renderer::context->UpdateSubresource(buffer.Get(), 0, nullptr, &bf, 0, 0);

	lastViewProjectionMatrix_ = bf.view * bf.projection;
}

void Camera::UpdateEyePosition() {

    float azimuth = az_ * (XM_PI / 180);
    float elevation = el_ * (XM_PI / 180);

    // Calculate Cartesian coordinates
    float x = dist_ * cosf(elevation) * sinf(azimuth);
    float y = dist_ * sinf(elevation);
    float z = dist_ * cosf(elevation) * cosf(azimuth);

    // Create the Cartesian vector
    XMVECTOR cartesian = XMVectorSet(x, y, z, 0.0f);

    // Translate the Cartesian vector by the origin
    eyePos_ = XMVectorAdd(cartesian, lookAtPos_);
}