#include "CommonBuffer.hlsl"

Texture2D skyBoxTexture : register(t0);
Texture2D primitiveTexture : register(t1);
Texture2D primitiveDepthTexture : register(t2);
Texture2D farCloudTexture : register(t3);
Texture2D cloudTexture : register(t4);
Texture2D cloudDepthTexture : register(t5);

SamplerState linearSampler : register(s0);
SamplerState pixelSampler : register(s1);

/* constants */
static const float2 g_kernel[4] = {
    float2(+0.0f, +1.0f),
    float2(+1.0f, +0.0f),
    float2(-1.0f, +0.0f),
    float2(+0.0f, -1.0f)
};

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
    float4 primitiveColor = primitiveTexture.Sample(linearSampler, input.Tex);
    float primitiveDepthValue = primitiveDepthTexture.Sample(linearSampler, input.Tex).r;
    float4 farCloudColor = farCloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudColor = cloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudDepthValue = cloudDepthTexture.Sample(linearSampler, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(linearSampler, input.Tex);

    float4 finalColor = skyBoxColor * (1.0 - primitiveColor.a) + primitiveColor;
    finalColor = finalColor * (1.0 - farCloudColor.a) + farCloudColor;
    if (cloudDepthValue.r > primitiveDepthValue.r) {
        finalColor = finalColor * (1.0 - cloudColor.a) + cloudColor;
    }

    return finalColor;
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}