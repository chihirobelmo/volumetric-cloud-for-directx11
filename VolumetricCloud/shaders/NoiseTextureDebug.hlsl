#include "CommonBuffer.hlsl"
#include "SDF.hlsl"

Texture3D noiseTexture : register(t0);
SamplerState linearSampler : register(s0);
SamplerState pixelSampler : register(s1);

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float3 Dir : TEXCOORD1;
};

float3 GetRayDir(float2 screenPos) {

    // Extract forward, right, and up vectors from the cView_ matrix
    float3 right = normalize(float3(cView_._11, cView_._21, cView_._31));
    float3 up = normalize(float3(cView_._12, cView_._22, cView_._32));
    float3 forward = normalize(float3(cView_._13, cView_._23, cView_._33));

    // Apply to screen position
    float horizontalAngle = -screenPos.x * 45 * 0.5;
    float verticalAngle = -screenPos.y * 45 * 0.5;
    
    // Create direction using trigonometry
    float3 direction = forward;
    direction += -right * tan(horizontalAngle);
    direction += +up * tan(verticalAngle);
    
    return normalize(direction);
}

VS_OUTPUT VS(float4 Pos : POSITION, float2 Tex : TEXCOORD0) {
    VS_OUTPUT output;
    output.Pos = Pos;
    output.Tex = Tex;
    output.Dir = GetRayDir(Tex * 2.0 - 1.0);
    return output;
}

float3 pos_to_uvw(float3 pos, float3 boxPos, float3 boxSize) {
    // Normalize the world position to the box dimensions
    float3 boxMin = boxPos - boxSize * 0.5;
    float3 boxMax = boxPos + boxSize * 0.5;
    float3 uvw = (pos - boxMin) / (boxMax - boxMin);
    return uvw;
}

float2 CloudBoxIntersection(float3 rayStart, float3 rayDir, float3 BoxPos, float3 BoxArea )
{
	float3 invraydir = 1 / rayDir;

	float3 firstintersections = (BoxPos - BoxArea * 0.5 - rayStart) * invraydir;
	float3 secondintersections = (BoxPos + BoxArea * 0.5 - rayStart) * invraydir;
	float3 closest = min(firstintersections, secondintersections);
	float3 furthest = max(firstintersections, secondintersections);

    // Get the intersection points
	float entryPos = max(closest.x, max(closest.y, closest.z));
	float exitPos = min(furthest.x, min(furthest.y, furthest.z));

    // Clamp the values
	entryPos = max(0, entryPos);
	exitPos = max(0, exitPos);
	entryPos = min(entryPos, exitPos);

	return float2(entryPos, exitPos);
}

float4 NoiseBox(float3 dir) {
    float3 forward = normalize(float3(cView_._13, cView_._23, cView_._33));
    float3 ro = -forward * 30.0;
    float3 rd = dir;
    float3 boxpos = 0;
    float3 boxsize = 20;
    float2 startEnd = CloudBoxIntersection(ro, rd, boxpos, boxsize);
    float4 col = float4(0.0, 0.0, 0.0, 0.0);
    float3 p = ro + rd * startEnd.x;
    if (sdBox(p - 0, boxsize * 0.5) <= 0.001) {
        float3 uvw = pos_to_uvw(p, boxpos, boxsize);
        col = noiseTexture.SampleLevel(pixelSampler, uvw, 0.0);
        //col.gba = normalize(col.gba);
        return col;
    }
    return 0.0;
}

float4 PSR(VS_OUTPUT input) : SV_TARGET {
    return noiseTexture.SampleLevel(pixelSampler, float3(input.Tex, 0.0), 0.0).r;// NoiseBox(normalize(input.Dir)).r;
}
float4 PSG(VS_OUTPUT input) : SV_TARGET {
    return noiseTexture.SampleLevel(pixelSampler, float3(input.Tex, 0.0), 0.0).g; // NoiseBox(normalize(input.Dir)).g;
}
float4 PSB(VS_OUTPUT input) : SV_TARGET {
    return noiseTexture.SampleLevel(pixelSampler, float3(input.Tex, 0.0), 0.0).b; // NoiseBox(normalize(input.Dir)).b;
}
float4 PSA(VS_OUTPUT input) : SV_TARGET {
    return noiseTexture.SampleLevel(pixelSampler, float3(input.Tex, 0.0), 0.0).a; // NoiseBox(normalize(input.Dir)).a;
}