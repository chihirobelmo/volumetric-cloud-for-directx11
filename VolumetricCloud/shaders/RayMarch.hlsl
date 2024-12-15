// references:
// - https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/
// - https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/
// - https://qiita.com/edo_m18/items/876f2857e67e26a053d6
// - https://wallisc.github.io/rendering/2020/05/02/Volumetric-Rendering-Part-1.html mostly from here
// - https://www.shadertoy.com/view/wssBR8
// - https://www.shadertoy.com/view/Xttcz2
// - https://www.shadertoy.com/view/WdsSzr

SamplerState depthSampler : register(s0);
SamplerState noiseSampler : register(s1);
SamplerState cloudMapSampler : register(s2);
SamplerState skySampler : register(s3);

Texture2D depthTexture : register(t0);
Texture3D noiseTexture : register(t1);
Texture2D cloudMapTexture : register(t2);
TextureCube skyTexture : register(t3);

#define MAX_STEPS_HEATMAP 256
#define MAX_LENGTH 422440.0f
#define MAX_VOLUME_LIGHT_MARCH_STEPS 1
#define LIGHT_MARCH_SIZE 500.0f

#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"
#include "SDF.hlsl"

cbuffer TransformBuffer : register(b3) {
    matrix scaleMatrix;
    matrix rotationMatrix;
    matrix translationMatrix;
    matrix SRTMatrix;
};

#define NM_TO_M 1852
#define FT_TO_M 0.3048

#define ALT_MAX 20000

struct VS_INPUT {
    float3 Pos : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Worldpos : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT {
    float4 Color : SV_TARGET0;
    float4 DepthColor : SV_TARGET1;
    float Depth : SV_Depth;
};

#if 0
// this is not used but remains for a reference
// you output SV_POSITION worldpos instead of clip space position in VS
// then you can get ray direction with below function.
float3 GetRayDir___NotUsed(float2 screenPos, float4x4 projectionMatrix) {

    // Extract FOV from projection matrix
    float verticalFOV = 2.0 * atan(1.0 / projectionMatrix[1][1]);
    float horizontalFOV = 2.0 * atan(1.0 / projectionMatrix[0][0]);
    float2 fov = float2(horizontalFOV, verticalFOV);

    // Extract forward, right, and up vectors from the view matrix
    float3 right = normalize(float3(view._11, view._21, view._31));
    float3 up = normalize(float3(view._12, view._22, view._32));
    float3 forward = normalize(float3(view._13, view._23, view._33));

    // Apply to screen position
    float horizontalAngle = -screenPos.x * fov.x * 0.5;
    float verticalAngle = -screenPos.y * fov.y * 0.5;
    
    // Create direction using trigonometry
    float3 direction = forward;
    direction += -right * tan(horizontalAngle);
    direction += +up * tan(verticalAngle);
    
    return normalize(direction);
}
#endif

// we now just place camera inside box to get world space to every direction
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;

    float4 worldPos = float4(input.Pos, 1.0f);
    worldPos = mul(worldPos, SRTMatrix);
    // worldPos.xyz -= cameraPosition.xyz;
    
    // consider camera position is always 0
    // camera is placed inside the box, always.
    output.Pos = mul(mul(worldPos, view), projection);
	output.TexCoord = input.TexCoord;
    output.Worldpos = worldPos;
    
    // Get ray direction in world space
    // GetRayDir___NotUsed(input.TexCoord * 2.0 - 1.0, projection);

    return output;
}

//////////////////////////////////////////////////////////////////////////
// blue noise from https://www.shadertoy.com/view/ssBBW1

uint HilbertIndex(uint2 p) {
    uint i = 0u;
    for(uint l = 0x4000u; l > 0u; l >>= 1u) {
        uint2 r = min(p & l, 1u);
        
        i = (i << 2u) | ((r.x * 3u) ^ r.y);       
        p = r.y == 0u ? (0x7FFFu * r.x) ^ p.yx : p;
    }
    return i;
}

uint ReverseBits(uint x) {
    x = ((x & 0xaaaaaaaau) >> 1) | ((x & 0x55555555u) << 1);
    x = ((x & 0xccccccccu) >> 2) | ((x & 0x33333333u) << 2);
    x = ((x & 0xf0f0f0f0u) >> 4) | ((x & 0x0f0f0f0fu) << 4);
    x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
    return (x >> 16) | (x << 16);
}

uint OwenHash(uint x, uint seed) { // seed is any random number
    x ^= x * 0x3d20adeau;
    x += seed;
    x *= (seed >> 16) | 1u;
    x ^= x * 0x05526c56u;
    x ^= x * 0x53a22864u;
    return x;
}

float blueNoise(uint2 fragCoord, float seed) {
    uint m = HilbertIndex(fragCoord);     // map pixel coords to hilbert curve index
    m = OwenHash(ReverseBits(m), 0xe7843fbfu + seed);   // owen-scramble hilbert index
    m = OwenHash(ReverseBits(m), 0x8d8fb1e0u + seed);   // map hilbert index to sobol sequence and owen-scramble
    return float(ReverseBits(m)) / 4294967296.0; // convert to float
}

//////////////////////////////////////////////////////////////////////////

float HeightGradient(float height, float cloudBottom, float cloudTop, float3 areaSize) 
{
    // Wider transition zones for smoother gradients
    float bottomFade = smoothstep(cloudBottom, cloudBottom - areaSize.y * 0.3, height);
    float topFade = 1.0 - smoothstep(cloudTop + areaSize.y * 0.5, cloudTop, height);
    return bottomFade * topFade;
}

float3 pos_to_uvw(float3 pos, float3 boxPos, float3 boxSize) {
    // Normalize the world position to the box dimensions
    float3 boxMin = boxPos - boxSize * 0.5;
    float3 boxMax = boxPos + boxSize * 0.5;
    float3 uvw = (pos - boxMin) / (boxMax - boxMin);
    return uvw;
}

float MipCurve(float3 pos) {
    float dist = length(cameraPosition.xyz - pos);
    float t = dist / (MAX_LENGTH * 0.1);
    return t * 4.0;
}

float4 fbm(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within 0 to +1 by normalize
    return noiseTexture.SampleLevel(noiseSampler, pos, mip);
}

// WEATHER MAP has to be BC7 Linear
float4 CloudMap(float3 pos) {
    float4 weather = cloudMapTexture.Sample(cloudMapSampler, pos.xz);
    return weather;
}

inline float DepthToMeter(float z) {
    // Extract the necessary parameters from the transposed projection matrix
    float c = projection._33;
    float d = projection._43;

    // Calculate linear eye depth with inverted depth values
    return d / (z - c);
}

float UnsignedDensity(float density) {
    return max(density, 0.0);
}

float BeerLambertFunciton(float density, float stepSize) {
    return exp(-density * stepSize);
}

// Henyey-Greenstein
float phaseFunction(float g, float cosTheta) {
    return (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5);
}

float3 SampleAmbientLight(float3 direction) {
    // Sample the cubemap texture using the direction vector
    return skyTexture.Sample(skySampler, direction).rgb;
}

float3 randomDirection(float3 seed) {
    float phi = 2.0 * 3.14159 * frac(sin(dot(seed.xy, float2(12.9898, 78.233))) * 43758.5453);
    float costheta = 2.0 * frac(cos(dot(seed.xy, float2(23.14069, 90.233))) * 12345.6789) - 1.0;
    float sintheta = sqrt(1.0 - costheta * costheta);
    return float3(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

#define NUM_SAMPLES_MONTE_CARLO 16

// Monte Carlo
float3 monteCarloAmbient(float3 normal) {
    float3 ambientColor = 0.0;
    for (int i = 0; i < NUM_SAMPLES_MONTE_CARLO; i++) {
        
        // half sphere
        float3 sampleDir = randomDirection(normal);
        // if (dot(sampleDir, normal) < 0.0) {
        //     sampleDir = -sampleDir;
        // }
        
        float3 envColor = skyTexture.Sample(skySampler, sampleDir).rgb;
        
        ambientColor += envColor;
    }
    return ambientColor / float(NUM_SAMPLES_MONTE_CARLO);
}

float SmoothNormalize(float value, float minVal, float maxVal) {
    // Ensure the value is clamped within the range
    value = clamp(value, minVal, maxVal);

    // Normalize the value to the range [0, 1]
    float normalized = (value - minVal) / (maxVal - minVal);

    // Map the normalized value to the range [-1, 0]
    return lerp(-1.0, 0.0, normalized);
}

// Ray-box intersection
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

// Get the march size for the current step
// As we get closer to the end, the steps get larger
float GetMarchSize(int stepIndex) {
    // Normalize step index to [0,1]
    float x = float(stepIndex) / MAX_STEPS_HEATMAP;

    // Exponential curve parameters
    float base = 2.71828;  // e
    float exponent = 0.1;  // Controls curve steepness

    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);

    // Scale to ensure total distance is covered
    return curve * (MAX_LENGTH / MAX_STEPS_HEATMAP);
}

// Adaptive Raymarching
float GetMarchSizeByDense(float density, float distance) {
    float baseStep = 1.0;
    float adaptiveStep = baseStep / (density + 0.001);
    return min(adaptiveStep, distance);
}

float CloudDensity(float3 pos, out float distance, out float3 normal) {

    // note that y minus is up
    float rayHeight = -pos.y;
    distance = 0;
    normal = 0;
    
    // cloud dense control
    float dense = 0; // linear to gamma
    float4 noise = max(0.0, fbm(pos * 1.0 / (5.0 * NM_TO_M), MipCurve(pos)) );
    float4 detailNoise = max(0.0, fbm(pos * 1.0 / (2.0 * NM_TO_M), MipCurve(pos)) );

    //normal = noise.gba;

    // cloud 3d map
    float fmap = fbm(pos * (1.0 / (cloudStatus.w * 10.0 * NM_TO_M)), 0).r;
    {
        // normalize 0->1
        fmap = fmap * 0.5 + 0.5;
        // cloud coverage bias, fill entire as coveragte increase
        fmap = cloudStatus.x == 0 ? /*clear sky*/0 : pow( fmap, 1.0 / cloudStatus.x);
        // gamma correction
        fmap = fmap * 2.0 - 1.0;
        CUTOFF(fmap,0.01);
    }

    // cloud 3d map
    float c3d = fbm(pos * (1.0 / (cloudStatus.w * NM_TO_M)), 0).r;
    {
        // normalize 0->1
        c3d = c3d * 0.5 + 0.5;
        // cloud coverage bias, fill entire as coveragte increase
        c3d = cloudStatus.x == 0 ? /*clear sky*/0 : pow( c3d, 1.0 / cloudStatus.x);
        // gamma correction
        c3d = c3d * 2.0 - 1.0;
        CUTOFF(c3d,0.01);
    }

    // first layer: cumulus(WIP) and stratocumulus(TBD)
    {
        float layer1 = 16.0 / 256.0;
        
        // cloud height parameter
        float thicknessMeter = cloudStatus.g * ALT_MAX * noise.g;
        float cloudBaseMeter = cloudStatus.b * ALT_MAX;
        float cloudTop = cloudBaseMeter + thicknessMeter;
        
        // remove below bottom and over top, also gradient them when it reaches bottom/top
        float cumulusLayer = remap(rayHeight, cloudBaseMeter + thicknessMeter * 0.00, cloudBaseMeter + thicknessMeter * 0.75, 0.0, 1.0)
                           * remap(rayHeight, cloudBaseMeter + thicknessMeter * 0.25, cloudBaseMeter + thicknessMeter * 1.00, 1.0, 0.0);
        // completly set out range value to 0
        //cumulusLayer *= step(cloudBaseMeter, rayHeight) * step(rayHeight, cloudBaseMeter + thicknessMeter);

        // apply dense
        layer1 *= fmap * c3d * cumulusLayer * noise.b * detailNoise.b;

        // calculate distance and normal
        distance = DISTANCE(rayHeight, cloudBaseMeter, thicknessMeter);
        //normal = normalize( float3(0.0, sign(rayHeight - cloudBaseMeter), 0.0) );

        CUTOFF(layer1,0.0005);
        dense += layer1;
    }

    return dense;
}

// Function to adjust for Earth's curvature
float3 AdjustForEarthCurvature(float3 pos, float3 cameraPos) {

    float earthRadius = 6371e3;
    
    // Calculate the direction from the camera to the position
    float3 dir = normalize(pos - cameraPos);

    // Calculate the distance from the camera to the position
    float distance = length(pos - cameraPos);

    // Calculate the angle subtended by the distance on the sphere
    float angle = distance / earthRadius;

    // Calculate the new position considering Earth's curvature
    float3 newPos = cameraPos + earthRadius * sin(angle) * dir - float3(0, earthRadius * (1 - cos(angle)), 0);

    return newPos;
}

// For Heat Map Strategy
float4 RayMarch(float3 rayStart, float3 rayDir, int steps, float start, float end, float2 screenPosPx, float primDepthMeter, out float cloudDepth) {

    // initialize
    cloudDepth = 0;

    // light direction fix.
    float3 fixedLightDir = lightDir.xyz * float3(-1,1,-1);
    float3 lightColor = CalculateSunlightColor(-fixedLightDir);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);

    // sun light scatter
    float lightScatter = max(0.50, dot(normalize(-fixedLightDir), rayDir));
    lightScatter *= phaseFunction(0.01, lightScatter);
    
    float rayDistance = start;

    rayStart = rayStart + rayDir * 500.0 * noiseTexture.Sample(noiseSampler, rayDir * time.x).a;

    bool hit = false;
    float deltaRayTranslate = 0;

    [loop]
    while (rayDistance <= end) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * rayDistance;
        rayPos = AdjustForEarthCurvature(rayPos, cameraPosition.xyz);

        // Get the density at the current position
        float distance;
        float3 normal;
        float dense = CloudDensity(rayPos, distance, normal);
        
        // for Next Iteration
        float deltaRayTranslate = max(MAX_LENGTH / steps, distance * 0.50);
        rayDistance += deltaRayTranslate; 

        // primitive depth check
        if (rayDistance > min(primDepthMeter, MAX_LENGTH)) { break; }
        // below deadsea level, or too high
        if (-rayPos.y < -400 || -rayPos.y > 25000) { break; }

        // Skip if density is zero
        if (dense <= 0.0) { continue; }

        // here starts inside cloud !

        // Calculate the scattering and transmission
        float transmittance = BeerLambertFunciton(UnsignedDensity(dense), deltaRayTranslate);
        float lightVisibility = 1.0f;

        // light ray march
        {
            float integSunRayTranslate = 0;
            float3 sunRayPos = rayPos + -fixedLightDir.xyz * LIGHT_MARCH_SIZE;
            float nd;
            float3 nn;
            float dense2 = CloudDensity(sunRayPos, nd, nn);
            float deltaSunRayTranslate = max(LIGHT_MARCH_SIZE, 0.10 * nd);
            lightVisibility *= BeerLambertFunciton(UnsignedDensity(dense2), deltaSunRayTranslate);
            integSunRayTranslate += deltaSunRayTranslate;
        }

        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - transmittance) * lightScatter;
        intScattTrans.rgb += integScatt * intScattTrans.a * lightColor;
        intScattTrans.a *= transmittance;

        intScattTrans.rgb += intScattTrans.a * (1.0 - transmittance) * skyTexture.Sample(skySampler, normalize(normal)).rgb;

        // MIP DEBUG
        // if (MipCurve(rayPos) <= 4.0) { intScattTrans.rgb = float3(1, 0, 1); }
        // if (MipCurve(rayPos) <= 3.0) { intScattTrans.rgb = float3(0, 0, 1); }
        // if (MipCurve(rayPos) <= 2.0) { intScattTrans.rgb = float3(0, 1, 0); }
        // if (MipCurve(rayPos) <= 1.0) { intScattTrans.rgb = float3(1, 0, 0); }

        if (intScattTrans.a < 0.03)
        {
            // Calculate the depth of the cloud
            float4 proj = mul(mul(float4(rayPos/*revert to camera relative position*/ - cameraPosition.xyz, 1.0), view), projection);
            cloudDepth = proj.z / proj.w;

            intScattTrans.a = 0.0;
            break;
        }
    }

    // ambient light
    // intScattTrans.rgb += monteCarloAmbient(/*ground*/float3(0,1,0)) * (1.0 - intScattTrans.a);
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;
    
    // TODO : pass resolution some way
    float2 screenPos = input.Pos.xy;
    float2 pixelPos = screenPos / 512/*resolution for raymarch*/;
	float2 rcpro = rcp(float2(512, 512));

	float3 ro = cameraPosition.xyz; // Ray origin
    // consider camera position is always 0
    // no normalize to reduce ring anomaly
    float3 rd = (input.Worldpos.xyz - 0); // Ray direction
    
    // primitive depth in meter.
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;

    // dither effect to reduce anomaly
    //float dither = frac(screenPos.x * 0.5) + frac(screenPos.y * 0.5);

    // Ray march the cloud
    float4 cloud = RayMarch(ro, rd, 4096, 0, MAX_LENGTH * 0.05, screenPos, primDepthMeter, cloudDepth);
    //float4 cloud = RayMarch2(ro, rd, MAX_LENGTH, primDepthMeter, cloudDepth);

    // output
    output.Color = cloud;
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

    return output;
}

PS_OUTPUT PS_FAR(PS_INPUT input) {
    PS_OUTPUT output;
    
    // TODO : pass resolution some way
    float2 screenPos = input.Pos.xy;
    float2 pixelPos = screenPos / 256/*resolution for raymarch*/;
	float2 rcpro = rcp(float2(256, 256));

	float3 ro = cameraPosition.xyz; // Ray origin
    // consider camera position is always 0
    // no normalize to reduce ring anomaly
    float3 rd = (input.Worldpos.xyz - 0); // Ray direction
    
    // primitive depth in meter.
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;

    // Ray march the cloud
    float4 cloud = RayMarch(ro, rd, 2048, MAX_LENGTH * 0.05, MAX_LENGTH * 1.0, screenPos, primDepthMeter, cloudDepth);

    // output
    output.Color = cloud;
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

    return output;
}


PS_OUTPUT PS_SKYBOX(PS_INPUT input) {
    PS_OUTPUT output;

    // consider camera position is always 0
	float3 ro = cameraPosition.xyz;
    float3 rd = (input.Worldpos.xyz - 0);
    
    output.Color = skyTexture.Sample(skySampler, rd);
    output.DepthColor = 0;
    output.Depth = 0;

    return output;
}