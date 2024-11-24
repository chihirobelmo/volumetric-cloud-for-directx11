#pragma once

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

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Camera {
public:

    struct CameraBuffer {
        XMMATRIX view; // 4 x 4 = 16 floats
        XMMATRIX projection; // 4 x 4 = 16 floats
        XMMATRIX invViewProjMatrix; // 4 floats
        XMVECTOR cameraPosition; // 4 floats
        XMFLOAT2 resolution; // 2 float
        XMFLOAT2 hvfov; // 2 float
		XMFLOAT2 nearFar; // 2 float near far
		XMFLOAT2 padding1; // 2 float
    };

	Camera(float fov, float nearZ, float farZ, float al, float ez, float dist) : 
		eyePos_(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		lookAtPos_(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		near_(nearZ), 
        far_(farZ),
        vFov_(fov),
		az_(al),
		el_(ez),
		dist_(dist) {}
    ~Camera() {};

    ComPtr<ID3D11Buffer> buffer;

    // camera position
    XMVECTOR eyePos_;

	// look at position, camera should always look at this position
    XMVECTOR lookAtPos_;

    // vertical FOV
    float vFov_;

	// azimuth, elevation, distance when camera is rotating around lookat position
	float az_, el_, dist_;

    // near-far Z clipping
    float near_, far_;

    void Init();
    void UpdateBuffer(UINT width, UINT height);
    void UpdateEyePosition();

    void LookAt(const XMVECTOR& origin) { lookAtPos_ = origin; }
    void MoveTo(const XMVECTOR& origin) { eyePos_ = origin; }

};