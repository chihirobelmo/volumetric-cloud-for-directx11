cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    matrix invViewProjMatrix; 
    float4 cameraPosition; 
    float2 resolution;
    float2 hvFov;
    float2 nearFar;
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