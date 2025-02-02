#ifndef COMMON_BUFFERS_HLSL
#define COMMON_BUFFERS_HLSL
cbuffer CameraBuffer : register(b0) {
    matrix cView_;
    matrix cProjection_;
    matrix cInvViewProjection_; 
    matrix cPreviousViewProjection_;
    float4 cCameraPosition_; 
    float2 cResolution_;
    float2 cPadding_;
};

cbuffer EnvironmentBuffer : register(b1) {
    float4 cLightDir_;
    float4 cLightColor_;
    float4 cCloudStatus_;
    float4 cTime_;
};

cbuffer CloudBuffer : register(b2) {
    float4 cCloudPos_[128];
};

#endif // COMMON_BUFFERS_HLSL