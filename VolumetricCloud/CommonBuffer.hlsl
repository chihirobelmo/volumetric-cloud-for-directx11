cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    matrix invViewProjMatrix; 
    float4 cameraPosition; 
    float2 resolution;
    float2 hvFov;
};

cbuffer EnvironmentBuffer : register(b1) {
    float4 lightDir;
    float4 lightColor;
    float4 cloudAreaPos;
    float4 cloudAreaSize;
};