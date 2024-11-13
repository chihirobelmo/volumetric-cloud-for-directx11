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

struct PS_OUTPUT {
    float4 color : SV_Target;
    float depth : SV_Depth;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    // Transform to get projection space position
    float4 worldPos = float4(input.position, 1.0f);
    float4 viewPos = mul(worldPos, view);
    float4 projPos = mul(viewPos, projection);
    // After projection matrix, coordinates are in clip space (-w to +w)
    // Hardware performs perspective division automatically:
    // NDC.x = projPos.x / projPos.w  (-1 to +1)
    // NDC.y = projPos.y / projPos.w  (-1 to +1)
    // NDC.z = projPos.z / projPos.w  (0 to 1 for depth)
    output.position = projPos;
    output.texcoord = input.texcoord;
    return output;
}

PS_OUTPUT PS(VS_OUTPUT input) {
    PS_OUTPUT output;
    output.color = float4(1.0f, 0.0f, 0.0f, 1.0f); // Red color
    output.depth = input.position.z / input.position.w;
    return output;
}