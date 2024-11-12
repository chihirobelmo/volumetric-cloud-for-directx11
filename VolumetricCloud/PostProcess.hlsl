struct VS_INPUT {
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D renderTexture : register(t0);
SamplerState samplerState : register(s0);

float4 PS(PS_INPUT input) : SV_Target {
    return renderTexture.Sample(samplerState, input.TexCoord);
}