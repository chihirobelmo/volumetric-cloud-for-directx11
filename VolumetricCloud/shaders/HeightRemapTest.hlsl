#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"

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

    float thichness = max(0.0, perlinFbm(float3(uv.x, 0, 0), 8, 8) );

    float cumulus = remap(height, 0.3 - thichness * 0.1, 0.3, 0.0, 1.0)
                  * remap(height, 0.3, 0.3 + thichness * 0.1, 1.0, 0.0);

                    // height 0.5-0.6 becomes 0.0-1.0
    float stratus = remap(height, 0.5, 0.6, 0.0, 1.0)
                    // height 0.6-0.7 becomes 1.0
                    // height 0.7-0.8 becomes 1.0-0.0
                  * remap(height, 0.7, 0.8, 1.0, 0.0);

    return float4(cumulus, stratus, 0, 1);
}