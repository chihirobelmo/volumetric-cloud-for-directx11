cbuffer ConstantBuffer : register(b0) {
    matrix view;
    matrix worldViewProj;
    float4 lightDir;
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
    output.position = mul(mul(float4(input.position, 1.0f), view), worldViewProj);
    output.worldpos = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

#include "SkyRay.hlsl"

float4 PS(PS_INPUT input) : SV_TARGET {
    
    float3 ro = float3(0,0,0); // Ray origin
    float3 rd = normalize(input.worldpos.xyz - ro); // Ray direction

    float3 col = SkyRay(ro, rd, lightDir.xyz);

    return float4(col, 1.0);
}