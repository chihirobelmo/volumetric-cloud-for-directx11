// PostProcess.hlsl

Texture2D primitiveTexture : register(t0);
Texture2D cloudTexture : register(t1);
Texture2D primitiveDepthTexture : register(t2);
Texture2D cloudDepthTexture : register(t3);
Texture2D skyBoxTexture : register(t4);
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

float4 GaussianBlur(float2 texCoord, float2 texelSize) {
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    // Gaussian weights
    float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

    // Sample the texture at multiple positions
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            float weight = weights[abs(i)] * weights[abs(j)] * 4.0;
            color += cloudTexture.Sample(linearSampler, texCoord + float2(i, j) * texelSize) * weight;
            totalWeight += weight;
        }
    }

    // Normalize the color
    return color / totalWeight;
}

// is it?
float4 BilateralUpsample(float2 texCoord, float2 texelSize, float primitiveDepth, float sigmaSpatial, float sigmaDepth) {
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    // Define the kernel size
    int kernelRadius = 1;

    // Iterate over the kernel
    for (int i = -kernelRadius; i <= kernelRadius; ++i) {
        for (int j = -kernelRadius; j <= kernelRadius; ++j) {
            float2 offset = float2(i, j) * texelSize;
            float2 sampleCoord = texCoord + offset;

            // Sample the color and depth
            float4 sampleColor = cloudTexture.Sample(linearSampler, sampleCoord);
            float sampleDepth = primitiveDepthTexture.Sample(linearSampler, sampleCoord).r;

            // Calculate the spatial weight
            float spatialWeight = exp(-dot(offset, offset) / (2.0 * sigmaSpatial * sigmaSpatial));

            // Calculate the depth weight
            float depthWeight = exp(-pow(sampleDepth - primitiveDepth, 2.0) / (2.0 * sigmaDepth * sigmaDepth));

            // Combine the weights
            float weight = spatialWeight * depthWeight;

            // Accumulate the color and weight
            color += sampleColor * weight;
            totalWeight += weight;
        }
    }

    // Normalize the color
    return color / totalWeight;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float4 primitiveColor = primitiveTexture.Sample(linearSampler, input.Tex);
    float4 cloudColor = cloudTexture.Sample(linearSampler, input.Tex);
    float4 skyBoxColor = skyBoxTexture.Sample(linearSampler, input.Tex);
    float primitiveDepthValue = primitiveDepthTexture.Sample(pixelSampler, input.Tex).r;
    float cloudDepthValue = cloudDepthTexture.Sample(pixelSampler, input.Tex).r;

    float4 finalColor = skyBoxColor * (1.0 - primitiveColor.a) + primitiveColor;

    uint dx, dy;
    cloudTexture.GetDimensions(dx, dy);
    // The texelSize typically represents the size of a single texel in texture coordinate space, 
    // which is usually a small fraction of the texture's dimensions. 
    // It is calculated as the reciprocal of the texture's width and height, 
    // resulting in values that are much smaller than 1.0.
    float2 texelSize = float2(1.0 / dx, 1.0 / dy);

    // Parameters for bilateral upsampling
    float sigmaSpatial = 0.1;
    float sigmaDepth = 1.0;
    float4 cloudColorSmoothed = BilateralUpsample(input.Tex, texelSize, primitiveDepthValue, sigmaSpatial, sigmaDepth);//GaussianBlur(input.Tex, rcpro);
    
    //finalColor = finalColor * (1.0 - cloudColorSmoothed.a) + cloudColorSmoothed;
    finalColor = finalColor * (1.0 - cloudColor.a) + cloudColor;

    return finalColor;
}

technique10 Render {
    pass P0 {
        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}