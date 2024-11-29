Texture2D targetTexture : register(t0);
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
	uint dx, dy;
	targetTexture.GetDimensions(dx, dy);
	float2 rcpro = rcp(float2(dx, dy));

    float2 screenPos = input.Pos.xy;
    float2 gridPos = frac(screenPos * 0.5);
	float4 color = 0;
    // Sample the appropriate neighboring pixel based on the position
    if (gridPos.x > 0.5 && gridPos.y <= 0.5) {
        // Right bottom pixel, sample from left top pixel
        float2 neighborTexCoord = input.Tex - float2(1.0, 1.0) * rcpro;
        color = targetTexture.Sample(pixelSampler, neighborTexCoord);
    } else if (gridPos.x <= 0.5 && gridPos.y > 0.5) {
        // Left bottom pixel, sample from above pixel
        float2 neighborTexCoord = input.Tex - float2(0.0, 1.0) * rcpro;
        color = targetTexture.Sample(pixelSampler, neighborTexCoord);
    } else if (gridPos.x > 0.5 && gridPos.y > 0.5) {
        // Right top pixel, sample from left pixel
        float2 neighborTexCoord = input.Tex - float2(1.0, 0.0) * rcpro;
        color = targetTexture.Sample(pixelSampler, neighborTexCoord);
    } else {
        // Top left pixel, use its own color
        color = targetTexture.Sample(pixelSampler, input.Tex);
    }

	return color;
}