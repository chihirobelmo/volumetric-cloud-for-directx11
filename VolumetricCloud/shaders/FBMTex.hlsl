#include "FBM.hlsl"

cbuffer NoiseParams : register(b2) {
    float currentSlice;
    float time;
    float scale;
    float persistence;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float3 TexCoord : TEXCOORD0;  // Now using float3 for 3D coordinates
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float3 TexCoord : TEXCOORD0;  // Passing through all 3 coordinates
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = float3(input.TexCoord.xy, currentSlice);  // Z coordinate from currentSlice
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float3 uvw = float3(input.TexCoord.x, currentSlice, input.TexCoord.y);

    // Use texCoord directly as 3D position for noise

    // // R: perlin
    // float r = perlinFbm(uvw, 4, 4) * 0.5;

    // // G: worley
    // float g = worleyFbm(uvw, 4, true);

    // // B: perly
    // float b = 1.0 - perlinWorley(uvw, 4, 4) * 0.5;

    // // A: blue noise
    // float a = blueNoise(uvw * float3(128, 128, 128), 1);

    // R16G16B16A16_FLOAT: Returns raw float values (can be outside 0-1 range)
    // R8G8B8A8_UNORM: Values are automatically normalized to 0-1 range

    // value expected within -1 to +1
    // normalize to 0-1 when R8G8B8A8_UNORM

    float r = perlinFbm(uvw, 4, 4) * 0.5;
    float g = perlinFbm(uvw + float3(0.1,0,0), 4, 4) * 0.5 - r;
    float b = perlinFbm(uvw + float3(0,0.1,0), 4, 4) * 0.5 - r;
    float a = perlinFbm(uvw + float3(0,0,0.1), 4, 4) * 0.5 - r;

    float3 d = -normalize(float3(g,b,a));

    return float4(r, d);
}