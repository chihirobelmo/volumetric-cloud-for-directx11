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

float normalize01(float value)
{
    return value * 0.5 + 0.5;
}

float normalize11(float value)
{
    return value * 2.0 *- 1.0;
}

// Custom smoothstep function with adjustable parameters
float customSmoothstep(float edge0, float edge1, float x, float exponent) {
    // Normalize x to [0, 1]
    x = saturate((x - edge0) / (edge1 - edge0));
    // Apply exponent to adjust the curve
    return pow(x, exponent) * (3.0 - 2.0 * pow(x, exponent));
}

float4 PS(VS_OUTPUT input) : SV_TARGET {

    float timeFreqMSec = 60 * 60 * 1000 * 1000; 
    float timeFreqNom = time.x / timeFreqMSec;
    float3 uvwt = float3(input.Tex * 2.0 - 1.0 - timeFreqNom, 0);
    float3 uvw = float3(input.Tex * 2.0 - 1.0, 0);

    // R: cloud density
    float r = fbm( uvwt.xy, cloudStatus.w,  8);
    // cloud morphing, it increase coverage of plus value.
    r = pow(r * 0.5 + 0.5, 1.0 / (0.0001 + cloudStatus.x * 2.2) ) * 2.0 - 1.0;

    // G: cloud thickness
    float g = (fbm( uvwt.xy * 2.0, cloudStatus.w,  8) * 0.5 + 0.5) * cloudStatus.y;

    // B: cloud base alt
    float b = cloudStatus.z;

    // A: cloud scatter
    float a = r * 0.5 + 0.5;

    // smoothly cut teacup effect
    //r *= customSmoothstep(0.1, 0.3, r, 0.5);

    // clamped and normalized to 0-1 as R8G8B8A8_UNORM
    return float4(r, g, b, a);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}