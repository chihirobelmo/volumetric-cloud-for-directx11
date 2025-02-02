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
    const float2 g_ScreenParam = float2(1024.0, 1024.0); // TBD
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
    float4 hiResND = primitiveDepthTexture.Sample(linearSampler, input.Tex);

    float2 lowResUV = (input.Tex * kScreenHalfSize.xy + float2(0.1, 0.1));
    float4 lowResND[4];
    float4 lowResCol[4];
    switch (hiResIndex)
    {
    case 0:
        lowResND[0] = cloudDepthTexture.Sample(linearSampler, input.Tex);
        lowResND[1] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResND[2] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResND[3] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        lowResCol[0] = cloudTexture.Sample(linearSampler, input.Tex);
        lowResCol[1] = cloudTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResCol[2] = cloudTexture.Sample(linearSampler, input.Tex + float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResCol[3] = cloudTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        break;
    case 1:
        lowResND[0] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResND[1] = cloudDepthTexture.Sample(linearSampler, input.Tex);
        lowResND[2] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        lowResND[3] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResCol[0] = cloudTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResCol[1] = cloudTexture.Sample(linearSampler, input.Tex);
        lowResCol[2] = cloudTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        lowResCol[3] = cloudTexture.Sample(linearSampler, input.Tex + float2(0.0, 1.0 / kScreenHalfSize.y));
        break;
    case 2:
        lowResND[0] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResND[1] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, -1.0 / kScreenHalfSize.y));
        lowResND[2] = cloudDepthTexture.Sample(linearSampler, input.Tex);
        lowResND[3] = cloudDepthTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResCol[0] = cloudTexture.Sample(linearSampler, input.Tex - float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResCol[1] = cloudTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, -1.0 / kScreenHalfSize.y));
        lowResCol[2] = cloudTexture.Sample(linearSampler, input.Tex);
        lowResCol[3] = cloudTexture.Sample(linearSampler, input.Tex + float2(1.0 / kScreenHalfSize.x, 0.0));
        break;
    case 3:
        lowResND[0] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        lowResND[1] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResND[2] = cloudDepthTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResND[3] = cloudDepthTexture.Sample(linearSampler, input.Tex);
        lowResCol[0] = cloudTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 1.0 / kScreenHalfSize.y));
        lowResCol[1] = cloudTexture.Sample(linearSampler, input.Tex - float2(0.0, 1.0 / kScreenHalfSize.y));
        lowResCol[2] = cloudTexture.Sample(linearSampler, input.Tex - float2(1.0 / kScreenHalfSize.x, 0.0));
        lowResCol[3] = cloudTexture.Sample(linearSampler, input.Tex);
        break;
    }

    float totalWeight = 0.0;
    float4 color = 0.0;
    for( int i = 0; i < 4; ++i )
    {
        float depthDiff = hiResND.r - lowResND[i].r;
        float depthWeight = 1.0 / (0.0001 + abs(depthDiff)); // Add epsilon to avoid division by zero

		// inside cloud it gets all black so we need min value
        float weight = 0.0001 + depthWeight * kBilinearWeights[hiResIndex][i];
        totalWeight += weight;
        color += lowResCol[i] * weight;
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