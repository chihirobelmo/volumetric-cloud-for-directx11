// PostProcess.hlsl

Texture2D depthBoxTexture : register(t0);
Texture2D cloudTexture : register(t1);
Texture2D depthBoxDepth : register(t2);
Texture2D cloudDepth : register(t3);
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
    float depthBoxDepthValue = depthBoxDepth.Sample(samplerState, input.Tex).r;
    float cloudDepthValue = cloudDepth.Sample(samplerState, input.Tex).r;

    return cloudColor;

    if (depthBoxDepthValue > cloudDepthValue) {
        return depthBoxColor;
    }

    return depthBoxColor + float4(cloudColor.xyz * cloudColor.a, 0.0);
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}