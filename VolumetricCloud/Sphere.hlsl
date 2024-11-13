// SphereVS.hlsl
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

cbuffer WorldBuffer : register(b2) {
    matrix world;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    // Transform to get projection space position
    float4 worldPos = float4(input.position, 1.0f);
    float4 viewPos = mul(worldPos, view);
    float4 projPos = mul(viewPos, projection);
    output.position = projPos;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Target {
    return float4(1.0f, 1.0f, 1.0f, 1.0f); // Red sphere
}