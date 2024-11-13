// SphereVS.hlsl
cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float4 cameraPosition;
    float aspectRatio;
    float cameraFov;
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
    float4 worldPosition = float4(input.position, 1.0f);
    output.position = mul(mul(worldPosition, view), projection);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Target {
    return float4(1.0f, 0.0f, 0.0f, 1.0f); // Red sphere
}