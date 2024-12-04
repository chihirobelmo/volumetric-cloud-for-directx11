#include "CommonFunctions.hlsl"
#include "CommonBuffer.hlsl"

struct VS_INPUT {
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 Worldpos : POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float depth : DEPTH;
};

struct PS_OUTPUT {
    float4 Color : SV_TARGET;
    float Depth : SV_Depth;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    // consider camera position is always 0
    float4 worldPos = float4(input.Position - cameraPosition.xyz, 1.0f);
    output.Position = mul(mul(worldPos, view), projection);
	output.TexCoord = input.TexCoord;
    // consider camera position is always 0
    output.Worldpos = worldPos;
    output.Normal = input.Normal;
    output.depth = output.Position.z / output.Position.w;
    output.Color = input.Color;
    
    return output;
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    float4 albedo = input.Color;
    
    // consider camera position is always 0
    float3 v = normalize(input.Worldpos.xyz - 0);
    float3 n = normalize(input.Normal);
    float3 l = normalize(lightDir.xyz);
    float3 h = normalize(l + v);
    float col = 0.02 + 0.98 * pow(1.0 - max(0.0, dot(n,v)), 5.0);

    float3 fixedLightDir = lightDir.xyz * float3(-1,1,-1);
    float3 lightColor = CalculateSunlightColor(-fixedLightDir);
    lightColor *= col;

    output.Color = float4(lightColor, 1.0);
    output.Depth = input.depth;
    return output;
}