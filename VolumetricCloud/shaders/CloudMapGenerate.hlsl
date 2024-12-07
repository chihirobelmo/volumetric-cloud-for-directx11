#include "CommonBuffer.hlsl"

Texture2D fmapTexture : register(t0);

SamplerState linearSampler : register(s0);
SamplerState pixelSampler : register(s1);

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

VS_OUTPUT VS(float4 Pos : POSITION, float2 Tex : TEXCOORD0) {
    VS_OUTPUT output;
    output.Pos = Pos;
    output.Tex = Tex;
    return output;
}

#include "FBM.hlsl"

float perlinWorley(float3 uvw, float freq, float octaves)
{
    float worley = worleyFbm(uvw, freq);
    float perlin = perlinFbm(uvw, freq, octaves);
    return remap(perlin, 1.0 - worley, 1.0, 0.0, 1.0);
}

float normalize01(float value)
{
    return value * 0.5 + 0.5;
}

float normalize11(float value)
{
    return value * 2.0 *- 1.0;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {

    float3 uvw = float3(input.Tex, 0.0);

    // float perlin = 0;
    // float freq_perlin = 6;
    // [loop]
    // for (int i = 0; i < freq_perlin; i++)
    // {
    //     perlin += perlinFbm(uvw, pow(2, i), 8) / freq_perlin;
    // }

    float r = 1;

    // G: worley
    float g = perlinFbm(uvw, 16, 8);

    // B: perly
    float b = 0.0;//perlinWorley(uvw, 4, 8);

    // A: blue noise
    float a = 1;//blueNoise(uvw * float3(1024, 1024, 1024), 1);

    // R16G16B16A16_FLOAT: Returns raw float values (can be outside 0-1 range)
    // R8G8B8A8_UNORM: Values are automatically normalized to 0-1 range

    // value expected within -1 to +1
    // normalize to 0-1 when R8G8B8A8_UNORM
    return float4(r, g, b, a);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}