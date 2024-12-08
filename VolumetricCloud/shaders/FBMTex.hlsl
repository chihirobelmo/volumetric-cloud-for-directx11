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

    // R: perlin
    float r = 0;
    float freq_r = 6;
    [loop]
    for (int i = 0; i < freq_r; i++)
    {
        r += perlinFbm(uvw, pow(2, i), 8) / freq_r;
    }

    // G: worley
    float g = worleyFbm(uvw, 8);

    // B: perly
    float b = perlinWorley(uvw, 4, 8);

    // A: blue noise
    float a = blueNoise(uvw * float3(128, 128, 128), 1);

    // R16G16B16A16_FLOAT: Returns raw float values (can be outside 0-1 range)
    // R8G8B8A8_UNORM: Values are automatically normalized to 0-1 range

    // value expected within -1 to +1
    // normalize to 0-1 when R8G8B8A8_UNORM
    return float4(r, g, b, a) * 0.5 + 0.5;
}