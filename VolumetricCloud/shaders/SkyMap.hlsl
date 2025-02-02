cbuffer ConstantBuffer : register(b0) {
    matrix cView_;
    matrix worldViewProj;
    float4 cLightDir_;
    float4 cameraPos;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldpos : POSITION;
    float2 texcoord : TEXCOORD;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), cView_), worldViewProj);
    output.worldpos = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

#include "SkyRay.hlsl"

float4 PS(PS_INPUT input) : SV_TARGET {
    
    float3 ro = cameraPos; // Ray origin
    float3 rd = normalize(input.worldpos.xyz - 0); // Ray direction

    float3 col = SkyRay(ro, rd, cLightDir_.xyz);

    return float4(col, 1.0);
}