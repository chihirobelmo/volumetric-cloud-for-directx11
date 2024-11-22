#include "CommonBuffer.hlsl"

Texture2D tex : register(t0);
SamplerState sam : register(s0);

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

// for inverse
float LinearizeDepth(float depth, float nearZ, float farZ) {
    float linearDepth = nearZ * farZ / (nearZ - depth * (farZ - nearZ));
    return 1.0 - (linearDepth - nearZ) / (farZ - nearZ); // Normalize to [0, 1]
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float depth = tex.Sample(sam, input.Tex).r;
    float linDepth = LinearizeDepth(depth, nearFar.x, nearFar.y);
    return float4(linDepth, linDepth, linDepth, 1.0);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}