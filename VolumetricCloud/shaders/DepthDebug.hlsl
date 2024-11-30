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

// // for inverse
// float LinearizeDepth(float depth, float nearZ, float farZ) {
//     float meter = nearZ * farZ / (nearZ - depth * (farZ - nearZ));
//     float linearDepth = 1.0 - (meter - nearZ) / (farZ - nearZ); // Normalize to [0, 1]
//     return linearDepth;
// }

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float depth = tex.Sample(sam, input.Tex).r;
    return float4(depth * 500, depth * 2000, depth * 1000, 1.0);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}