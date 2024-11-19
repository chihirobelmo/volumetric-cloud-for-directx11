#include "CommonBuffer.hlsl"

struct VS_INPUT {
    float3 Position : POSITION;    // Keep original position semantic
    float2 TexCoord : TEXCOORD0;  // Add texcoord semantic
    float3 Normal : NORMAL;  // Add texcoord semantic
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 Worldpos : POSITION;
    float2 TexCoord : TEXCOORD0;  // Pass through texcoord
    float3 Normal : NORMAL;        // Use NORMAL for normal
    
};

struct PS_OUTPUT {
    float4 Color : SV_TARGET;
    float Depth : SV_Depth;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    float4 worldPos = float4(input.Position, 1.0f);
    output.Position = mul(mul(worldPos, view), projection);
    output.TexCoord = input.TexCoord;
    output.Worldpos = worldPos;
    output.Normal = input.Normal;
    
    return output;
}

float LinearizeDepth(float depth)
{
    // These values should match your camera's near and far planes
    float n = 0.1; // near plane
    float f = 1000.0; // far plane
    return (2.0 * n) / (f + n - depth * (f - n));
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    float3 v = normalize(input.Worldpos.xyz - cameraPosition);
    float3 n = normalize(input.Normal);
    float3 l = normalize(lightDir.xyz);
    float3 h = normalize(l + v);
    float col = 10 * max(0.0, dot(n,l)) / 3.1415 + pow(1.0 - max(0.0, dot(n,v)), 5.0);

    float depth = input.Position.z / input.Position.w;
    //depth = LinearizeDepth(depth); // Transform to linear depth

    output.Color = float4(col, col, col, 1.0); // Visualize depth for debugging
    output.Depth = 1.0 - depth;
    return output;
}