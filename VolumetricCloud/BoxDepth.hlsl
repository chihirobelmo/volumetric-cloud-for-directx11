cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float3 cameraPos;
};

struct VS_INPUT {
    float3 Position : POSITION;    // Keep original position semantic
    float3 Normal : NORMAL;       // Keep original normal semantic
    float2 TexCoord : TEXCOORD0;  // Add texcoord semantic
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;    // Use POSITION for world pos
    float3 Normal : NORMAL;        // Use NORMAL for normal
    float2 TexCoord : TEXCOORD0;  // Pass through texcoord
};

struct PS_OUTPUT {
    float4 Color : SV_TARGET;
    float Depth : SV_Depth;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    float4 worldPos = float4(input.Position, 1.0f);
    output.WorldPos = worldPos.xyz;
    output.Position = mul(mul(worldPos, view), projection);
    output.Normal = input.Normal;
    
    return output;
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;
    output.Color = float4(1.0, 1.0, 1.0, 1.0);
    output.Depth = input.Position.z / input.Position.w;
    return output;
}