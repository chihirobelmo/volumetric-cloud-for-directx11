// PostProcess.hlsl

Texture2D depthBoxTexture : register(t0);
Texture2D cloudTexture : register(t1);
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
    float4 depthBoxColor = depthBoxTexture.Sample(samplerState, input.Tex);
    float4 cloudColor = cloudTexture.Sample(samplerState, input.Tex);
    return depthBoxColor + cloudColor; // Combine the textures
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}