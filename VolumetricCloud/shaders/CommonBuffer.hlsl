#ifndef COMMON_BUFFERS_HLSL
#define COMMON_BUFFERS_HLSL
cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    matrix invViewProjMatrix; 
    matrix previousViewProjectionMatrix;
    float4 cameraPosition; 
    float2 resolution;
    float2 padding1;
};

cbuffer EnvironmentBuffer : register(b1) {
    float4 lightDir;
    float4 lightColor;
    float4 cloudStatus;
    float4 time;
};

cbuffer CloudBuffer : register(b2) {
    float4 cloudPositions[128];
};

#endif // COMMON_BUFFERS_HLSL