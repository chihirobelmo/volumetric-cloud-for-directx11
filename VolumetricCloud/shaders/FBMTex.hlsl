#include "FBM.hlsl"

cbuffer NoiseParams : register(b2) {
    float cCurrentSlice_;
    float cTimePadding_;
    float cScalePadding_;
    float cPersistencePadding_;
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
    output.TexCoord = float3(input.TexCoord.x, input.TexCoord.y, cCurrentSlice_);  // Z coordinate from cCurrentSlice_
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float3 uvw = float3(input.TexCoord.xyz);

    float worley = worleyFbm(uvw, 8, true);
    float perlin = perlinFbm(uvw, 8, 4);
    float perlinWorley = Remap(perlin * 0.25 + 0.5, 1.0 - worley, 1.0, 0.0, 1.0);

    float r = perlinWorley;
    float g = worleyFbm(uvw, 6, true);
    float b = worleyFbm(uvw, 12, true);
    float a = worleyFbm(uvw, 24, true);

    // R16G16B16A16_FLOAT: Returns raw float values (can be outside 0-1 range)
    // R8G8B8A8_UNORM: Values are automatically normalized to 0-1 range

    // value expected within -1 to +1
    // normalize to 0-1 when R8G8B8A8_UNORM

    return float4(r, g, b, a);
}

float4 PS_SMALL(PS_INPUT input) : SV_Target
{
    float3 uvw = float3(input.TexCoord.xyz);

    float r = worleyFbm(uvw, 3, true);
    float g = worleyFbm(uvw, 6, true);
    float b = worleyFbm(uvw, 9, true);
    float a = worleyFbm(uvw, 12, true);

    // R16G16B16A16_FLOAT: Returns raw float values (can be outside 0-1 range)
    // R8G8B8A8_UNORM: Values are automatically normalized to 0-1 range

    // value expected within -1 to +1
    // normalize to 0-1 when R8G8B8A8_UNORM

    return float4(r, g, b, a);
}