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

/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/
float4 BilateralUpsampling(VS_OUTPUT input, Texture2D t_depth_hires, Texture2D t_depth, Texture2D t_tex)
{
    float c_texelsize = 1.0 / 512.0f;

    int i;
    float2 coords[4];
    [unroll]
    for (i = 0; i < 4; i++)
        coords[i] = input.Tex + c_texelsize*g_kernel[i];

    /* depth weights */
    float depth_weights[4];
    float depth_hires = DepthToMeter(t_depth_hires.SampleLevel(linearSampler, input.Tex, 0).r);
    [unroll]
    for (i = 0; i < 4; i++) {
        float depth_coarse = DepthToMeter(t_depth.SampleLevel(linearSampler, coords[i], 0).r);
        depth_weights[i] = (0.0001 + abs(depth_hires - depth_coarse));
    }

    /* we have the weights, final color evaluation */
    float4 color_t = 0;
    float weight_sum = 0;
    
    [unroll]        
    for (i = 0; i < 4; i++) {
        float weight = depth_weights[i];
        color_t += t_tex.SampleLevel(linearSampler, coords[i], 0)*weight;
        weight_sum += weight;
    }
    color_t /= weight_sum;
    return color_t;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float4 primitiveColor = primitiveTexture.Sample(linearSampler, input.Tex);
    float primitiveDepthValue = primitiveDepthTexture.Sample(linearSampler, input.Tex).r;
    float4 farCloudColor = farCloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudColor = cloudTexture.Sample(linearSampler, input.Tex);
    float4 cloudDepthValue = cloudDepthTexture.Sample(linearSampler, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(linearSampler, input.Tex);

    float4 upscaledCloud = BilateralUpsampling(input, primitiveDepthTexture, cloudDepthTexture, cloudTexture);

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