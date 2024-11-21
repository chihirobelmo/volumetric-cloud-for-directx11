#include "FXAA.hlsl"

Texture2D targetTexture : register(t0);
SamplerState linearSampler : register(s0);

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
	uint dx, dy;
	targetTexture.GetDimensions(dx, dy);
	float2 rcpro = rcp(float2(dx, dy));

	FxaaTex t;
	t.smpl = linearSampler;
	t.tex = targetTexture;

	return FxaaPixelShader(input.Tex, 0, t, t, t, rcpro, 0, 0, 0, 1.0, 0.166, 0.0312, 0, 0, 0, 0);
}