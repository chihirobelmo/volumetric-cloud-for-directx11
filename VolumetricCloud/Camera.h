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

    inline static ComPtr<ID3D11Buffer> camera_buffer;

    // mouse
    inline static float azimuth_hdg = 270.0f;
    inline static float elevation_deg = -45.0f;
    inline static float distance_meter = 250.0f;
    inline static float fov = 80;

    // camera
    inline static XMMATRIX view;
    inline static XMMATRIX projection;
    inline static XMVECTOR eye_pos = XMVectorSet(0.0, 0.0, -5.0f, 0.0f);
    inline static XMVECTOR look_at_pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    inline static float aspect_ratio;

    static void UpdateProjectionMatrix(int windowWidth, int windowHeight);
    static void UpdateCamera(XMVECTOR Eye, XMVECTOR At);

    static void InitBuffer();
    static void UpdateBuffer();
    static void InitializeCamera();

};