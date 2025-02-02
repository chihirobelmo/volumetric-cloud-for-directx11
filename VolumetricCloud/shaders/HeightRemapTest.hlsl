#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"

Texture3D noiseTexture : register(t0);
Texture3D noiseSmallTexture : register(t1);
Texture2D<uint4> fMapTexture : register(t2);
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

#include "FBM.hlsl"

float4 PS(VS_OUTPUT input) : SV_TARGET {

    float2 uv = input.Tex;
    float height = 1.0 - uv.y;

    float4 noise = noiseTexture.Sample(linearSampler, float3(uv, 0));

    float result = RemapClamp(noise.r * 0.5 + 0.5, 1.0 - cCloudStatus_.r, 1.0, 0.0, 1.0);


    float cumulus = RemapClamp(height, 0.0, 0.1, 0.0, 1.0)
                  * RemapClamp(height, 0.2, 0.5, 1.0, 0.0);
    result = RemapClamp(result, 1.0 - cumulus, 1.0, 0.0, 1.0);
    
    // cumulus anvil
    const float ANVIL_BIAS = 1.0;
    const float SLOPE = 0.1;
    const float BOTTOM_WIDE = 0.5;
    result = pow(result, RemapClamp( 1.0 - height, SLOPE, BOTTOM_WIDE, 1.0, lerp(1.0, 0.5, ANVIL_BIAS)));
    
    result = RemapClamp(result, 1.0 - (noise.g * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley

    const float4 FMAP = FetchAndInterpolateFMapTexture(fMapTexture, uv, int2(59, 59));

    return float4(FMAP.r / 65535.0, FMAP.g / 65535.0, FMAP.b / 65535.0, 1);
}