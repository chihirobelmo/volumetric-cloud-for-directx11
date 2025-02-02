SamplerState skySampler : register(s0);
TextureCube skyTexture : register(t0);

cbuffer ConstantBuffer : register(b0) {
    matrix cView_;
    matrix worldViewProj;
    float4 cLightDir_;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldpos : POSITION;
    float2 texcoord : TEXCOORD;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), cView_), worldViewProj);
    output.worldpos = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Importance sampling function (simplified for demonstration)
float3 ImportanceSampleHemisphere(float3 normal, uint sampleIndex, uint sampleCount) {
    // Generate a random direction in the hemisphere
    float phi = 2.0 * 3.14159265359 * (sampleIndex / float(sampleCount));
    float cosTheta = sqrt(1.0 - (sampleIndex / float(sampleCount)));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 sampleDir = float3(
        cos(phi) * sinTheta,
        cosTheta, // Y direction is up
        sin(phi) * sinTheta
    );

    // Transform the sample direction to align with the normal
    float3 up = abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    return normalize(tangent * sampleDir.x + bitangent * sampleDir.z + normal * sampleDir.y);
}

float3 SampleDiffuseIrradiance(float3 normal) {
    const uint sampleCount = 64;
    float3 irradiance = float3(0.0, 0.0, 0.0);

    for (uint i = 0; i < sampleCount; ++i) {
        // Generate a random sample direction in the hemisphere
        float3 sampleDir = ImportanceSampleHemisphere(normal, i, sampleCount);

        // Sample the environment map
        float3 envColor = skyTexture.Sample(skySampler, sampleDir).rgb;

        // Accumulate the irradiance
        irradiance += envColor * dot(normal, sampleDir);
    }

    // Average the accumulated irradiance
    irradiance /= sampleCount;

    return irradiance;
}

float4 PS(PS_INPUT input) : SV_TARGET {
    
    float3 ro = float3(0,0,0); // Ray origin
    float3 rd = normalize(input.worldpos.xyz - ro); // Ray direction
    // rd.y = -rd.y; // Invert Y axis

    // Sample the diffuse irradiance from the environment map
    float3 irradiance = SampleDiffuseIrradiance(rd);

    return float4(irradiance, 1.0);
}