cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float4 cameraPosition;
    float aspectRatio;
    float cameraFov;
};

cbuffer EnvironmentBuffer : register(b1) {
    float4 lightDir;
    float4 lightColor;
    float4 cloudAreaPos;
    float4 cloudAreaSize;
};