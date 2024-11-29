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

float4 GaussianBlur(float2 texCoord, float2 texelSize) {
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    // Gaussian weights
    float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

    // Sample the texture at multiple positions
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            float weight = weights[abs(i)] * weights[abs(j)] * 4.0;
            color += cloudTexture.Sample(samplerState, texCoord + float2(i, j) * texelSize) * weight;
            totalWeight += weight;
        }
    }

    // Normalize the color
    return color / totalWeight;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float4 primitiveColor = primitiveTexture.Sample(samplerState, input.Tex);
    float4 cloudColor = cloudTexture.Sample(samplerState, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(samplerState, input.Tex);
    float primitiveDepthValue = primitiveDepth.Sample(samplerState, input.Tex).r;
    float cloudDepthValue = cloudDepth.Sample(samplerState, input.Tex).r;

    float4 finalColor = skyBoxColor * (1.0 - primitiveColor.a) + primitiveColor;

	uint dx, dy;
	cloudTexture.GetDimensions(dx, dy);
	float2 rcpro = rcp(float2(dx, dy));
    float4 cloudColorSmoothed = GaussianBlur(input.Tex, rcpro);

    finalColor = finalColor * (1.0 - cloudColorSmoothed.a) + cloudColorSmoothed;

    return finalColor;
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}