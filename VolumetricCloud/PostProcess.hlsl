// PostProcess.hlsl

Texture2D primitiveTexture : register(t0);
Texture2D cloudTexture : register(t1);
Texture2D primitiveDepth : register(t2);
Texture2D cloudDepth : register(t3);
Texture2D skyBoxTexture : register(t4);
SamplerState samplerState : register(s0);

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
    float4 primitiveColor = primitiveTexture.Sample(samplerState, input.Tex);
    float4 cloudColor = cloudTexture.Sample(samplerState, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(samplerState, input.Tex);
    float primitiveDepthValue = primitiveDepth.Sample(samplerState, input.Tex).r;
    float cloudDepthValue = cloudDepth.Sample(samplerState, input.Tex).r;

    float4 finalColor = skyBoxColor * (1.0 - primitiveColor.a) + primitiveColor;
    finalColor = finalColor * (1.0 - cloudColor.a) + cloudColor;

    return finalColor;
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}