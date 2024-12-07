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

    float timeFreqMSec = 3 * 60 * 60 * 1000; 
    float3 uvwt = float3(input.Tex - (time.x % timeFreqMSec) / timeFreqMSec, 0);
    float3 uvw = float3(input.Tex, 0);

    // R: cloud height
    float r = perlinFbm(uvwt, 16,  8);

    // G: cloud base alt
    float g = 0.25;

    // B: cloud bottom
    float b = perlinFbm(uvwt, 16,  8);

    // A: cloud scatter
    float a = perlinFbm(uvw, 16,  8) * 0.5 + 0.5;

    // clamped and normalized to 0-1 as R8G8B8A8_UNORM
    return float4(r, g, b, a);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}