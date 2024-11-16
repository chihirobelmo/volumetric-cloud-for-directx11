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
        XMVECTOR cameraPosition; // 4 floats
        float aspectRatio; // 1 float
        float cameraFov; // 1 float
    };

	Camera(float az, float el, float dist, float fov) : 
        azimuth_hdg(az), 
        elevation_deg(el), 
        distance_meter(dist), 
        fov(fov),
		view(XMMatrixIdentity()),
		projection(XMMatrixIdentity()),
		eye_pos(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		look_at_pos(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		aspect_ratio(1.0f) {}
    ~Camera() {};

    inline static ComPtr<ID3D11Buffer> camera_buffer;

    // mouse controlled
    float azimuth_hdg;
    float elevation_deg;
    float distance_meter;
    float fov;

    // camera
    XMMATRIX view;
    XMMATRIX projection;
    XMVECTOR eye_pos;
    XMVECTOR look_at_pos;
    float aspect_ratio;

    void UpdateProjectionMatrix(int windowWidth, int windowHeight);
    void UpdateView(XMVECTOR Eye, XMVECTOR At);

    void InitBuffer();
    void UpdateBuffer();
    void InitializeCamera();

};