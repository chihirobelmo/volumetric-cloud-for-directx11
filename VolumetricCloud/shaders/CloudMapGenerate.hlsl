#include "CommonBuffer.hlsl"
#include "Commonfunctions.hlsl"
#include "FBM.hlsl"

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

float4 PS(VS_OUTPUT input) : SV_TARGET {

    float timeFreqMSec = 60 * 60 * 1000 * 1000; 
    float timeFreqNom = cTime_.x / timeFreqMSec;
    float3 uvwt = float3(input.Tex * 2.0 - 1.0 - timeFreqNom, 0);
    float3 uvw = float3(input.Tex * 2.0 - 1.0, 0);

    // R: cloud coverage
    float pw = perlinWorley(uvw, 8, 4) * 0.25;
    float w = worleyFbm(uvw, 8, true);

    float r = RemapClamp((pw * 0.5 + 0.5), 1.0 - cCloudStatus_.r, 1.0, 0.0, 1.0);
    float g = RemapClamp((w * 0.5 + 0.5), 1.0 - cCloudStatus_.r, 1.0, 0.0, 1.0);

    // smoothly cut teacup effect
    // r *= customSmoothstep(0.1, 0.3, r, 0.5);

    // clamped and normalized to 0-1 as R8G8B8A8_UNORM
    return float4(r, g, 0, 1);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}