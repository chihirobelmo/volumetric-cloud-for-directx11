cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float3 cameraPos;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    float4 worldPos = float4(input.Position, 1.0f);
    output.WorldPos = worldPos.xyz;
    output.Position = mul(mul(worldPos, view), projection);
    output.Normal = input.Normal;
    
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    float depth = input.Position.z / input.Position.w;
    return float4(depth, depth, depth, 1.0);
}