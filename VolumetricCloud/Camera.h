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

	Camera(float fov) : 
		eye_pos(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		look_at_pos(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		near_(0.1f), 
        far_(422440.f),
        vFov(fov) {}
    ~Camera() {};

    ComPtr<ID3D11Buffer> buffer;

    XMVECTOR eye_pos;
    XMVECTOR look_at_pos;
    float vFov;
	float az_, el_, dist_;
    float near_, far_;

    void Init();
    void Update(UINT width, UINT height);
    void LookAtFrom();

    void LookAt(const XMVECTOR& origin) { look_at_pos = origin; }
    void MoveTo(const XMVECTOR& origin) { eye_pos = origin; }

};