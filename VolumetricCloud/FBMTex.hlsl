#include "FBM.hlsl"

cbuffer NoiseParams : register(b2) {
    float currentSlice;
    float time;
    float scale;
    float persistence;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float3 TexCoord : TEXCOORD0;  // Now using float3 for 3D coordinates
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float3 TexCoord : TEXCOORD0;  // Passing through all 3 coordinates
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = float3(input.TexCoord.xy, currentSlice);  // Z coordinate from currentSlice
    return output;
}

// from https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine

float remapNubis(float value, float original_min, float original_max, float new_min, float new_max)
{
    return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float perlinWorley(float3 uvw, float freq, float octaves)
{
    float worley = worleyFbm(uvw, freq);
    float perlin = perlinFbm(uvw, freq, octaves);
    return remap(perlin, 1.0 - worley, 1.0, 0.0, 1.0);
}

float cloudWithCoverage(float3 uvw, float freq, float cloud_coverage)
{
    return remapNubis(gradientNoise(uvw, freq), cloud_coverage, 1.0, 0.0, 1.0);
}

float baseCloud(float3 uvw, float loFreq, float hiFreq)
{
    return remapNubis(perlinFbm(uvw,loFreq,8), perlinFbm(uvw,hiFreq,8), 1.0, 0.0, 1.0);
}

float stratusHeight(float height)
{
    return remap(height, 0.0, 0.1, 0.0, 1.0) * remap(height, 0.2, 0.3, 1.0, 0.0);
}

float normalize01(float value)
{
    return value * 0.5 + 0.5;
}

float normalize11(float value)
{
    return value * 2.0 *- 1.0;
}

float sdSphere( float3 p, float s )
{
  return length(p)-s;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float3 uvw = float3(input.TexCoord.x, currentSlice, input.TexCoord.y);

    // Use texCoord directly as 3D position for noise

    // R: coverage
    float r = 0;
    float freq_r = 6;
    for (int i = 0; i < freq_r; i++)
    {
        r += perlinFbm(uvw, pow(2, i), 8) / freq_r;
    }
    // r *= stratusHeight(uvw.y);
    // r = -sdSphere(uvw - 0.5, 0.5);

    // maybe some other info later
    float g = 0;
    float b = 0;
    float a = hash33(uvw * /*resolution*/256.0);

    // value output expected within -1 to +1
    return float4(r, g, b, a);
}