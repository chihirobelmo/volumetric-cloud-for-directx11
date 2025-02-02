#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"

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

float4 BU(VS_OUTPUT input)
{
    const float2 g_ScreenParam = float2(3840.0, 2160.0); // TBD
	const float2 kScreenSize = g_ScreenParam.xy * 2.0;
	const float2 kScreenHalfSize = float2(512.0, 512.0); // TBD
	const float4 kBilinearWeights[4] =
	{
		float4( 9.0/16.0, 3.0/16.0, 3.0/16.0, 1.0/16.0 ),
		float4( 3.0/16.0, 9.0/16.0, 1.0/16.0, 3.0/16.0 ),
		float4( 3.0/16.0, 1.0/16.0, 9.0/16.0, 3.0/16.0 ),
		float4( 1.0/16.0, 3.0/16.0, 3.0/16.0, 9.0/16.0 )
	};

	int2 hiResUV = (int2)(input.Tex * kScreenSize + float2(0.1, 0.1));
	int hiResIndex = (1 - (hiResUV.y & 0x01)) * 2 + (1 - (hiResUV.x & 0x01));
	float4 hiResND = primitiveDepthTexture.Load( int3(hiResUV, 0), int2(0, 0) );

	int2 lowResUV = (int2)(input.Tex * kScreenHalfSize.xy + float2(0.1, 0.1));
	float4 lowResND[4];
	float4 lowResAO[4];
	switch (hiResIndex)
	{
	case 0:
		lowResND[0] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResND[1] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(1, 0) );
		lowResND[2] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 1) );
		lowResND[3] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(1, 1) );
		lowResAO[0] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResAO[1] = cloudTexture.Load( int3(lowResUV, 0), int2(1, 0) );
		lowResAO[2] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 1) );
		lowResAO[3] = cloudTexture.Load( int3(lowResUV, 0), int2(1, 1) );
		break;
	case 1:
		lowResND[0] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(-1, 0) );
		lowResND[1] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResND[2] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(-1, 1) );
		lowResND[3] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 1) );
		lowResAO[0] = cloudTexture.Load( int3(lowResUV, 0), int2(-1, 0) );
		lowResAO[1] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResAO[2] = cloudTexture.Load( int3(lowResUV, 0), int2(-1, 1) );
		lowResAO[3] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 1) );
		break;
	case 2:
		lowResND[0] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, -1) );
		lowResND[1] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(1, -1) );
		lowResND[2] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResND[3] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(1, 0) );
		lowResAO[0] = cloudTexture.Load( int3(lowResUV, 0), int2(0, -1) );
		lowResAO[1] = cloudTexture.Load( int3(lowResUV, 0), int2(1, -1) );
		lowResAO[2] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResAO[3] = cloudTexture.Load( int3(lowResUV, 0), int2(1, 0) );
		break;
	case 3:
		lowResND[0] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(-1, -1) );
		lowResND[1] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, -1) );
		lowResND[2] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(-1, 0) );
		lowResND[3] = cloudDepthTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		lowResAO[0] = cloudTexture.Load( int3(lowResUV, 0), int2(-1, -1) );
		lowResAO[1] = cloudTexture.Load( int3(lowResUV, 0), int2(0, -1) );
		lowResAO[2] = cloudTexture.Load( int3(lowResUV, 0), int2(-1, 0) );
		lowResAO[3] = cloudTexture.Load( int3(lowResUV, 0), int2(0, 0) );
		break;
	}

	float totalWeight = 0.0;
	float4 color = 0.0;
	for( int i = 0; i < 4; ++i )
	{
		// float normalWeight = dot( lowResND[i].xyz, hiResND.xyz );
		// normalWeight = pow( saturate(normalWeight), 32.0 );

		float depthDiff = hiResND.w - lowResND[i].w;
		float depthWeight = 1.0 / (1.0 + abs(depthDiff));

		float weight = /*normalWeight * */depthWeight * kBilinearWeights[hiResIndex][i];
		totalWeight += weight;
		color += lowResAO[i] * weight;
	}

	color /= totalWeight;

	return color;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float4 primitiveColor = primitiveTexture.Sample(linearSampler, input.Tex);
    float primitiveDepthValue = primitiveDepthTexture.Sample(linearSampler, input.Tex).r;
    float4 farCloudColor = farCloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudColor = cloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudDepthValue = cloudDepthTexture.Sample(linearSampler, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(linearSampler, input.Tex);

    float4 upscaledCloud = BU(input);

    float4 finalColor = skyBoxColor * (1.0 - primitiveColor.a) + primitiveColor;
    finalColor = finalColor * (1.0 - farCloudColor.a) + farCloudColor;
    if (cloudDepthValue.r > primitiveDepthValue.r) {
        finalColor = finalColor * (1.0 - upscaledCloud.a) + upscaledCloud;
    }

    return finalColor;
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}