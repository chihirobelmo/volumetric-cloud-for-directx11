cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float3 cameraPos;
};

struct VS_INPUT {
    float3 Position : POSITION;    // Keep original position semantic
    float2 TexCoord : TEXCOORD0;  // Add texcoord semantic
    float3 Normal : NORMAL;       // Keep original normal semantic
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;  // Pass through texcoord
    float3 Normal : NORMAL;        // Use NORMAL for normal
    float4 Worldpos : POSITION;
    
};

// struct PS_OUTPUT {
//     float4 Color : SV_TARGET;
//     float Depth : SV_Depth;
// };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    float4 worldPos = float4(input.Position, 1.0f);
    output.Position = mul(mul(worldPos, view), projection);
    output.Normal = input.Normal;
    output.TexCoord = input.TexCoord;
    output.Worldpos = worldPos;
    
    return output;
}

float4 PS(PS_INPUT input) : SV_Target0 {
    // PS_OUTPUT output;
    // output.Color = float4(1.0, 1.0, 1.0, 1.0);
    // output.Depth = input.Position.z / input.Position.w;
    return float4(1.0, 1.0, 1.0, 1.0);
}