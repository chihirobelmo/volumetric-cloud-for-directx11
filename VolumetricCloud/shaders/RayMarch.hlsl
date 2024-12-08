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
SamplerState weatherMapSampler : register(s2);
SamplerState skySampler : register(s3);

Texture2D depthTexture : register(t0);
Texture3D noiseTexture : register(t1);
Texture2D weatherMapTexture : register(t2);
TextureCube skyTexture : register(t3);

#define MAX_STEPS_HEATMAP 512
#define MIP_MIN_METER 20 * 1852
#define MIP_MAX_METER 80 * 1852
#define MAX_VOLUME_LIGHT_MARCH_STEPS 3
#define LIGHT_MARCH_SIZE 640.0f / MAX_VOLUME_LIGHT_MARCH_STEPS

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

#define ALT_MAX 51000*FT_TO_M

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
    
    // Stay at 0 until 10000m, then smoothly increase to 2 at 30000m
    float t = saturate((dist - MIP_MIN_METER) / (MIP_MAX_METER - MIP_MIN_METER));
    
    // Add curve to keep low values longer
    t = smoothstep(0.0, 1.0, t);
    
    // Output range 0 to 2
    return t * 4.0;
    
    // Alternative with even longer low values:
    // return pow(t, 2.0) * 2.0;
}

float4 fbm2d(float3 pos, float slice, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within -1 to +1
    return noiseTexture.SampleLevel(noiseSampler, float3(pos.x, slice, pos.z), mip) * 2.0 - 1.0;
}

float4 fbm_b(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within -1 to +1
    return noiseTexture.SampleLevel(noiseSampler, pos, mip) * 2.0 - 1.0;
}

float4 fbm_c(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within -1 to +1
    return max(0.0, noiseTexture.SampleLevel(noiseSampler, pos, mip) * 2.0 - 1.0);
}

float4 fbm_m(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within -1 to +1
    return noiseTexture.SampleLevel(noiseSampler, pos, mip);
}

// WEATHER MAP has to be BC7 Linear
float4 CloudMap(float3 pos) {
    float4 weather = weatherMapTexture.Sample(weatherMapSampler, pos.xz);
    // weather.r = pow(weather.r, 2.2);
    // weather.g = pow(weather.g, 2.2);
    // weather.b = pow(weather.b, 2.2);
    // weather.r *= ALT_MAX;
    // weather.g *= ALT_MAX;
    // weather.b *= ALT_MAX;
    // weather.r = max(0.0, weather.r);
    // weather.g = max(0.0, weather.g);
    // weather.b = max(0.0, weather.b);
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
float GetMarchSize(int stepIndex, float maxLength) {
    // Normalize step index to [0,1]
    float x = float(stepIndex) / MAX_STEPS_HEATMAP;

    // Exponential curve parameters
    float base = 2.71828;  // e
    float exponent = 0.75;  // Controls curve steepness

    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);

    // Scale to ensure total distance is covered
    return curve * (maxLength / MAX_STEPS_HEATMAP);
}

float MergeDense(float dense1, float dense2) {
    return (dense1 + dense2) * 0.5;
}

// Custom smoothstep function with adjustable parameters
float customSmoothstep(float edge0, float edge1, float x, float exponent) {
    // Normalize x to [0, 1]
    x = saturate((x - edge0) / (edge1 - edge0));
    // Apply exponent to adjust the curve
    return pow(x, exponent) * (3.0 - 2.0 * pow(x, exponent));
}

// from https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine

float remap(float value, float original_min, float original_max, float new_min, float new_max)
{
    return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float CloudDensity(float3 pos, float3 boxPos, float3 boxSize, out float distance, out float3 normal) {

    // note that y minus is up
    float height = -pos.y;
    distance = 0;
    normal = 0;

    // get the uvw within cloud zone
    float3 uvw = pos_to_uvw(pos, boxPos, boxSize);
    float3 uvwc = uvw * 2.0 - 1.0;
    float4 cloudMap = CloudMap( uvw );
    float noiseRepeatNM = 6.0 + 4.0 * cloudMap.a;
    float noiseSampleFactor= 1.0 / (noiseRepeatNM * NM_TO_M);
    
    float mip = MipCurve(pos);
    float noise = fbm_m(pos * noiseSampleFactor, MipCurve(pos)).r;
    
    // cloud dense control
    float dense = 1.0 / 32.0; // linear to gamma

    // smoothly cut teacup effect
    //cloudMap.r *= customSmoothstep(0.05, 0.1, cloudMap.r, 0.50);

    // add noise to height
    // make parameter to actual alt value
    cloudMap.r = pow(cloudMap.r, 1.0 + 1.0 * noise);

    float cloudeDenseMultiplier = cloudMap.r;

    // cloud height control
    float thicknessMeter = cloudeDenseMultiplier * ALT_MAX;
    float cloudBaseMeter = cloudMap.g * ALT_MAX;

    // cloud top has to be above cloud bottom
    float cloudTop = cloudBaseMeter + thicknessMeter;
    float cloudBottom = cloudBaseMeter - thicknessMeter * 0.25;

    // cloudDenseTop has to be above cloudDenseBottom, below cloudTop
    float cloudDenseTop = cloudBaseMeter + thicknessMeter * 0.75;
    
    // cloudDenseBottom has to be below cloudDenseTop, above cloudBottom
    float cloudDenseBottom = cloudBaseMeter - thicknessMeter * 0.10;
    
    // remove below bottom and over top, also gradient them when it reaches bottom/top
    float cumulus = remap(height, cloudBottom, cloudDenseTop, 0.0, 1.0)
                  * remap(height, cloudDenseBottom, cloudTop, 1.0, 0.0);

    distance = abs(height - cloudBaseMeter) - thicknessMeter;
    normal = normalize( float3(0.0, sign(height - cloudBaseMeter), 0.0) );

    dense *= cumulus;
    dense *= cloudeDenseMultiplier; // pow(cloudeDenseMultiplier, 1.0); // for less tea-cup edge
    dense *= (noise * 0.5 + 0.5); // apply normalized normal

    return dense;
}

// For Heat Map Strategy
float4 RayMarch(float3 rayStart, float3 rayDir, float dither, float primDepthMeter, out float cloudDepth) {
    
    float3 boxPos = float3(0, -ALT_MAX, 0);
    float3 boxSize = float3(400 * NM_TO_M, ALT_MAX * 2.0, 400 * NM_TO_M);
    float3 fixedLightDir = lightDir.xyz * float3(-1,1,-1);
    float3 lightColor = CalculateSunlightColor(-fixedLightDir);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    float lightScatter = max(0.5, dot(normalize(-fixedLightDir), rayDir));
    lightScatter *= phaseFunction(0.01, lightScatter);
    cloudDepth = 0;

    // Check if ray intersects the cloud box
    float2 startEnd = CloudBoxIntersection(rayStart, rayDir, boxPos, boxSize);
    if (startEnd.x >= startEnd.y) { return float4(0, 0, 0, 0); } // No intersection

    // Clamp the intersection points, if intersect primitive earlier stop ray there
    startEnd.x += dither * 0.125 * 422440.0f / MAX_STEPS_HEATMAP;
    startEnd.x = max(0, startEnd.x);
    startEnd.y = min(primDepthMeter, startEnd.y);

    // Calculate the offset of the intersection point from the box
    // start from box intersection point
	float planeoffset = 1 - frac( ( startEnd.x - length(rayDir) ) * MAX_STEPS_HEATMAP );
    float integRayTranslate = startEnd.x;// + (planeoffset / MAX_STEPS_HEATMAP);

    // SDF from dense is -1 to 1 so if we advance ray with SDF we might need to multiply it
    float sdfMultiplier = 10.0f;

    [loop]
    for (int i = 0; i < MAX_STEPS_HEATMAP; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * integRayTranslate;

        // Get the density at the current position
        float distance;
        float3 normal;
        float dense = CloudDensity(rayPos, boxPos, boxSize, distance, normal);
        float sdf = -dense;

        // for Next Iteration
        float deltaRayTranslate = GetMarchSize(i, 422440.0f);

        integRayTranslate += max(deltaRayTranslate, distance * 0.5); 
        if (integRayTranslate > startEnd.y) { break; }

        // Skip if density is zero
        if (dense <= 0.0) { continue; }
        // here starts inside cloud !

        // Calculate the scattering and transmission
        float transmittance = BeerLambertFunciton(UnsignedDensity(dense), deltaRayTranslate);
        float lightVisibility = 1.0f;

        // light ray march
        [loop]
        for (int v = 1; v <= MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
        {
            float3 sunRayPos = rayPos + v * -fixedLightDir.xyz * LIGHT_MARCH_SIZE;

            float nd;
            float3 nn;
            float dense2 = CloudDensity(sunRayPos, boxPos, boxSize, nd, nn);

            lightVisibility *= BeerLambertFunciton(UnsignedDensity(dense2), LIGHT_MARCH_SIZE);
        }

        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - transmittance) * lightScatter;
        intScattTrans.rgb += integScatt * intScattTrans.a * lightColor;
        intScattTrans.a *= transmittance;

        // MIP DEBUG
        // if (MipCurve(rayPos) <= 4.0) { intScattTrans.rgb = float3(1, 0, 1); }
        // if (MipCurve(rayPos) <= 3.0) { intScattTrans.rgb = float3(0, 0, 1); }
        // if (MipCurve(rayPos) <= 2.0) { intScattTrans.rgb = float3(0, 1, 0); }
        // if (MipCurve(rayPos) <= 1.0) { intScattTrans.rgb = float3(1, 0, 0); }

        // Opaque check
        if (intScattTrans.a < 0.03)
        {
            intScattTrans.a = 0.0;

            // Calculate the depth of the cloud
            float4 proj = mul(mul(float4(rayPos/*revert to camera relative position*/ - cameraPosition.xyz, 1.0), view), projection);
            cloudDepth = proj.z / proj.w;

            break;
        }
        //intScattTrans.rgb += skyTexture.Sample(skySampler, normal) * (1.0 - intScattTrans.a) * (1.0 / 32.0);
    }

    // ambient light
    intScattTrans.rgb += monteCarloAmbient(/*ground*/float3(0,1,0)) * (1.0 - intScattTrans.a);
    
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
    
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;

    float dither = frac(screenPos.x * 0.5) + frac(screenPos.y * 0.5);
    float4 cloud = RayMarch(ro, rd, dither, primDepthMeter, cloudDepth);

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