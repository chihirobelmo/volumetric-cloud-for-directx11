// references:
// - https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
// - https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/
// - https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/
// - https://qiita.com/edo_m18/items/876f2857e67e26a053d6
// - https://wallisc.github.io/rendering/2020/05/02/Volumetric-Rendering-Part-1.html
// - https://www.shadertoy.com/view/wssBR8
// - https://www.shadertoy.com/view/Xttcz2
// - https://www.shadertoy.com/view/WdsSzr

SamplerState depthSampler : register(s0);
SamplerState noiseSampler : register(s1);
SamplerState cloudMapSampler : register(s2);
SamplerState skySampler : register(s3);
SamplerState linearSampler : register(s4);

TextureCube skyTexture : register(t0);
Texture2D previousTexture : register(t1);
Texture2D depthTexture : register(t2);
Texture3D noiseTexture : register(t3);
Texture3D noiseSmallTexture : register(t4);
Texture2D cloudMapTexture : register(t5);
Texture2D<float4> fMapTexture : register(t6);

#define MAX_LENGTH 422440.0f
#define LIGHT_MARCH_SIZE 400.0f

#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"
#include "FBM.hlsl"
#include "SDF.hlsl"

cbuffer TransformBuffer : register(b3) {
    matrix cScaleMatrix_;
    matrix cRotationMatrix_;
    matrix cTranslationMatrix_;
    matrix cSRTMatrix_;
};

cbuffer InputData : register(b3) {
    float4 cPixelSize_;
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

    // Extract FOV from cProjection_ matrix
    float verticalFOV = 2.0 * atan(1.0 / projectionMatrix[1][1]);
    float horizontalFOV = 2.0 * atan(1.0 / projectionMatrix[0][0]);
    float2 fov = float2(horizontalFOV, verticalFOV);

    // Extract forward, right, and up vectors from the view matrix
    float3 right = normalize(float3(cView_._11, cView_._21, cView_._31));
    float3 up = normalize(float3(cView_._12, cView_._22, cView_._32));
    float3 forward = normalize(float3(cView_._13, cView_._23, cView_._33));

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
    worldPos = mul(worldPos, cSRTMatrix_);
    // worldPos.xyz -= cCameraPosition_.xyz;
    
    // consider camera position is always 0
    // camera is placed inside the box, always.
    output.Pos = mul(mul(worldPos, cView_), cProjection_);
	output.TexCoord = input.TexCoord;
    output.Worldpos = worldPos;
    
    // Get ray direction in world space
    // GetRayDir___NotUsed(input.TexCoord * 2.0 - 1.0, cProjection_);

    return output;
}

float3 Pos2UVW(float3 pos, float3 boxPos, float3 boxSize) {
    // Normalize the world position to the box dimensions
    float3 boxMin = boxPos - boxSize * 0.5;
    float3 boxMax = boxPos + boxSize * 0.5;
    float3 uvw = (pos - boxMin) / (boxMax - boxMin);
    return uvw;
}

float MipCurve(float3 pos) {
    float dist = length(cCameraPosition_.xyz - pos);
    float t = dist / (MAX_LENGTH * 0.05) - 1.0;
    return t * 4.0;
}

float4 Noise3DTex(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within 0 to +1 by normalize
    return noiseTexture.SampleLevel(noiseSampler, pos, mip);
}

float4 Noise3DSmallTex(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within 0 to +1 by normalize
    return noiseSmallTexture.SampleLevel(noiseSampler, pos, mip);
}

float4 CloudMapTex(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within 0 to +1 by normalize
    return cloudMapTexture.SampleLevel(cloudMapSampler, pos.xz, 0.0);
}

float4 FmapTex(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within 0 to +1 by normalize
    return fMapTexture.Load(int3(pos.xz * 59, 0));
}

float UnsignedDensity(float density) {
    return max(density, 0.0);
}

float BeerLambertFunciton(float density, float stepSize) {
    return exp(-density * stepSize);
}

// from https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
float Energy(float density, float stepSize, float HG) {
    return exp( - density * stepSize );// * (1.0 - exp( - 2.0 * density * stepSize )) * HG;
}

float Powder(float density, float stepSize) {
    return 1.0 - exp( - 2.0 * density * stepSize );
}

// from https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
float HenyeyGreenstein(float cos_angle, float eccentricity)
{
    return ((1.0 - eccentricity * eccentricity) / pow((1.0 + eccentricity * eccentricity - 2.0 * eccentricity * cos_angle), 3.0 / 2.0)) / 4.0 * 3.1415;
}

// dl is the density sampled along the light ray for the given sample position.
// ds_lodded is the low lod sample of density at the given sample position.

#define PRIMARY_INTENSITY_CURVE 0.5
#define SECONDARY_INTENSITY_CURVE 0.25

// get light energy
float GetLightEnergy( float3 p, float height_fraction, float dl, float ds_loded, float phase_probability, float cos_angle, float step_size, float brightness)
{
    // attenuation â€? difference from slides â€? reduce the secondary component when we look toward the sun.
    float primary_attenuation = exp( - dl );
    float secondary_attenuation = exp(-dl * 0.25) * 0.7;
    float attenuation_probability = max( Remap( cos_angle, 0.7, 1.0, SECONDARY_INTENSITY_CURVE, SECONDARY_INTENSITY_CURVE * 0.25) , PRIMARY_INTENSITY_CURVE);
     
    // in-scattering â€? one difference from presentation slides â€? we also reduce this effect once light has attenuated to make it directional.
    float depth_probability = lerp( 0.05 + pow( ds_loded, Remap( height_fraction, 0.3, 0.85, 0.5, 2.0 )), 1.0, saturate( dl / step_size));
    float vertical_probability = pow( Remap( height_fraction, 0.07, 0.14, 0.1, 1.0 ), 0.8 );
    float in_scatter_probability = depth_probability * vertical_probability;

    float light_energy = attenuation_probability * in_scatter_probability * phase_probability * brightness;

    return light_energy;
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

// Function to adjust for Earth's curvature
float3 AdjustForEarthCurvature(float3 raypos, float3 raystart) {

    const float EARTH_RADIUS = 6371e3;
    
    // Calculate the direction from the camera to the position
    const float3 DIR = normalize(raypos - raystart);

    // Calculate the distance from the camera to the position
    const float DISTANCE = length(raypos - raystart);

    // Calculate the angle subtended by the distance on the sphere
    const float ANGLE = DISTANCE / EARTH_RADIUS;

    // Calculate the new position considering Earth's curvature
    float3 NEWPOS = raystart + EARTH_RADIUS * sin(ANGLE) * DIR - float3(0, EARTH_RADIUS * (1 - cos(ANGLE)), 0);

    return NEWPOS;
}

float CloudDensity(float3 pos, out float distance, out float3 normal) {

    const float rayHeightMeter = -pos.y;
    normal = float3(0, 1, 0); // Placeholder normal
    distance = 5.0; // Placeholder distance

    float4 fmap = fMapTexture.SampleLevel(linearSampler, Pos2UVW(pos, 0.0, 1000*16*64).xz, 0.0);     
    float poor = RemapClamp( fmap.r, 0.0, 1.0, 0.0, 1.0 );
    float finaldense = 1.0;

    // first later: cumulus and stratocumulus
    {
        // the narrower UV you use, the more noise but performance worse
        // the wider UV you use, the less noise but performance better
        float4 largeNoiseValue = Noise3DTex(pos * (1.0) / (1000*16), 0); // Large scale noise
        float dense = 1.0;
        dense = RemapClamp( (largeNoiseValue.r * 0.5 + 0.5), 1.0 - poor * 0.75, 1.0, 0.0, 1.0); // perlinWorley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.g * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.b * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.a * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        const float cumulusThickness = 500 + 2000 * fmap.g;
        const float cumulusBottomAltMeter = fmap.b * 0.3048;
        const float height = (rayHeightMeter - cumulusBottomAltMeter) / cumulusThickness;
        distance = DISTANCE_CLOUD(rayHeightMeter, cumulusBottomAltMeter, cumulusThickness);
        const float cumulusLayer = RemapClamp(height, 0.00, 0.20, 0.0, 1.0) * RemapClamp(height, 0.20, 1.00, 1.0, 0.0);
        dense = RemapClamp( dense, 1.0 - cumulusLayer, 1.0, 0.0, 1.0);
        // cumulus anvil
        const float anvil = 1.0;
        const float slope = 0.2;
        const float bottomWide = 0.8;
        dense = pow(dense, RemapClamp( 1.0 - height, slope, bottomWide, 1.0, lerp(1.0, 0.5, anvil)));
        finaldense = dense;
    }

    // second later: cirrus
    {        
        // the narrower UV you use, the more noise but performance worse
        // the wider UV you use, the less noise but performance better
        float4 largeNoiseValue = Noise3DTex(pos * (1.0) / (1000*16) + 0.5, 0); // Large scale noise
        float dense = 1.0;
        dense = RemapClamp( (largeNoiseValue.r * 0.5 + 0.5), 1.0 - poor * 0.5, 1.0, 0.0, 1.0); // perlinWorley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.g * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.b * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        dense = RemapClamp( dense, 1.0 - (largeNoiseValue.a * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
        const float cumulusThickness = 10 + 500 * fmap.g;
        const float cumulusBottomAltMeter = (fmap.b + 5000) * 0.3048;
        const float height = (rayHeightMeter - cumulusBottomAltMeter) / cumulusThickness;
        distance = min(distance, DISTANCE_CLOUD(rayHeightMeter, cumulusBottomAltMeter, cumulusThickness));
        const float cumulusLayer = RemapClamp(height, 0.00, 0.20, 0.0, 1.0) * RemapClamp(height, 0.20, 1.00, 1.0, 0.0);
        dense = RemapClamp( dense, 1.0 - cumulusLayer, 1.0, 0.0, 1.0);
        // cumulus anvil
        const float anvil = 1.0;
        const float slope = 0.2;
        const float bottomWide = 0.8;
        dense = pow(dense, RemapClamp( 1.0 - height, slope, bottomWide, 1.0, lerp(1.0, 0.5, anvil)));
        finaldense = max(dense, finaldense);
    }

    // apply noise detail
    // the narrower UV you use, the more noise but performance worse
    // the wider UV you use, the less noise but performance better
    float4 smallNoiseValue = Noise3DSmallTex(pos * 1.5 / (1.0 * NM_TO_M), 0); // small scale noise   
    finaldense = RemapClamp(finaldense, 1.0 - (smallNoiseValue.r * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
    finaldense = RemapClamp(finaldense, 1.0 - (smallNoiseValue.g * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
    finaldense = RemapClamp(finaldense, 1.0 - (smallNoiseValue.b * 0.5 + 0.5), 1.0, 0.0, 1.0); // worley
    return finaldense / 64.0;
}

// For Heat Map Strategy
float4 RayMarch(float3 rayStart, float3 rayDir, int sunSteps, float in_start, float in_end, float2 screenPosPx, float primDepthMeter, out float output_cloud_depth) {

    // initialize
    output_cloud_depth = 0;

    // light direction fix.
    const float3 SUNDIR = -(cLightDir_.xyz * float3(-1,1,-1));
    const float3 SUNCOLOR = CalculateSunlightColor(SUNDIR);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);

    // sun light scatter
    float cos_angle = dot(normalize(SUNDIR), normalize(rayDir));
    float lightScatter = HenyeyGreenstein(dot(normalize(SUNDIR), normalize(rayDir)), 0.05);

    // float lightScatter = max(0.50, dot(normalize(SUNDIR), rayDir));
    // lightScatter *= phaseFunction(0.01, lightScatter);
    
    float rayDistance = in_start;

    //rayStart = rayStart + rayDir * 500.0 * noiseTexture.Sample(noiseSampler, rayDir * cTime_.x).a;

    bool hit = false;

    [fastopt]
    for (int i = 0; i < 512; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * rayDistance;
        rayPos = AdjustForEarthCurvature(rayPos, cCameraPosition_.xyz);

        // Get the density at the current position
        float distance;
        float3 normal;
        const float DENSE = CloudDensity(rayPos, distance, normal);
        
        // for Next Iteration
        const float RAY_ADVANCE_LENGTH = max(25, distance * 1.00);
        rayDistance += RAY_ADVANCE_LENGTH; 

        // primitive depth check
        if (rayDistance > min(primDepthMeter, in_end)) { break; }
        // below deadsea level, or too high
        if (-rayPos.y < -400 || -rayPos.y > 25000) { break; }

        // Skip if density is zero
        if (DENSE <= 0.0) { continue; }
        if (!hit) { 
            // Calculate the depth of the cloud
            const float4 PROJ = mul(mul(float4(rayPos/*revert to camera relative position*/ - cCameraPosition_.xyz, 1.0), cView_), cProjection_);
            output_cloud_depth = PROJ.z / PROJ.w;

            hit = true; 
        }

        // here starts inside cloud !

        // Calculate the scattering and transmission
        const float TRANSMITTANCE = BeerLambertFunciton(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);
                                  //* Powder(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);
                                  //* lightScatter;
        float lightVisibility = 1.0;

        // light ray march
        float previousDensity = DENSE;

        [unroll]
        for (int s = 1; s <= sunSteps; s++)
        {
            const float TO_SUN_RAY_ADVANCED_LENGTH = (LIGHT_MARCH_SIZE / sunSteps);
            const float3 TO_SUN_RAY_POS = rayPos + SUNDIR * TO_SUN_RAY_ADVANCED_LENGTH * s;

            float nd;
            float3 nn;
            const float DENSE_2 = CloudDensity(TO_SUN_RAY_POS, nd, nn);
            
            // Trapezoidal integration
            float averageDensity = (previousDensity + DENSE_2) * 0.5;
            lightVisibility *= Energy(UnsignedDensity(averageDensity), TO_SUN_RAY_ADVANCED_LENGTH, lightScatter);
            previousDensity = DENSE_2;
        }

        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - TRANSMITTANCE);
        intScattTrans.rgb += integScatt * intScattTrans.a * SUNCOLOR;
        intScattTrans.a *= TRANSMITTANCE;

        // MIP DEBUG
        // if (MipCurve(rayPos) <= 4.0) { intScattTrans.rgb = float3(1, 0, 1); }
        // if (MipCurve(rayPos) <= 3.0) { intScattTrans.rgb = float3(0, 0, 1); }
        // if (MipCurve(rayPos) <= 2.0) { intScattTrans.rgb = float3(0, 1, 0); }
        // if (MipCurve(rayPos) <= 1.0) { intScattTrans.rgb = float3(1, 0, 0); }

        if (intScattTrans.a < 0.03)
        {
            intScattTrans.a = 0.0;
            break;
        }
    }

    // ambient light
    intScattTrans.rgb += skyTexture.Sample(skySampler, -rayDir).rgb * (1.0 - intScattTrans.a);
    // monteCarloAmbient(/*ground*/float3(0,1,0)) * (1.0 - intScattTrans.a);
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

float4 ReprojectPreviousFrame(float4 currentPos) {
    // Transform current position to previous frame's view space
    float4 previousPos = mul(cPreviousViewProjection_, currentPos);
    previousPos /= previousPos.w;

    // Convert to UV coordinates
    float2 previousUV = previousPos.xy * 0.5 + 0.5;

    // Sample the previous frame texture
    return previousTexture.Sample(linearSampler, currentPos.xy / 360);
}

PS_OUTPUT StartRayMarch(PS_INPUT input, int sunSteps, float in_start, float in_end) {
    PS_OUTPUT output;
    
    // TODO : pass cResolution_ some way
    float2 screenPos = input.Pos.xy;
    float2 pixelPos = screenPos.xy / cPixelSize_.xy;

	float3 ro = cCameraPosition_.xyz; // Ray origin

    // consider camera position is always 0
    // no normalize to reduce ring anomaly
    float3 rd = normalize(input.Worldpos.xyz - 0); // Ray direction
    
    // primitive depth in meter.
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(+1.0, 0.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(-1.0, 0.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(0.0, +1.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(0.0, -1.0) / cPixelSize_.xy).r);
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;

    // dither effect to reduce anomaly
    //float dither = frac(screenPos.x * 0.5) + frac(screenPos.y * 0.5);

    // Ray march the cloud
    float4 cloud = RayMarch(ro, rd, sunSteps, in_start, in_end, screenPos, primDepthMeter, cloudDepth);

    // output
    output.Color = cloud;// lerp(cloud, ReprojectPreviousFrame(float4(input.Pos.xy, 0.0, 1.0)), 0.5);
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

    return output;
}

PS_OUTPUT PS(PS_INPUT input) {

    return StartRayMarch(input, 8, 0, MAX_LENGTH * 0.10);
}

PS_OUTPUT PS_FAR(PS_INPUT input) {

    return StartRayMarch(input, 8, MAX_LENGTH * 0.25, MAX_LENGTH * 1.0);
}

PS_OUTPUT PS_SKYBOX(PS_INPUT input) {
    PS_OUTPUT output;

    // consider camera position is always 0
	float3 ro = cCameraPosition_.xyz;
    float3 rd = (input.Worldpos.xyz - 0);
    
    output.Color = skyTexture.Sample(skySampler, rd);
    output.DepthColor = 0;
    output.Depth = 0;

    return output;
}

cbuffer LosBuffer : register(b4) {
    float4 cStartPoint_;
    float4 cEndPoint_;
};

RWStructuredBuffer<float> OutputBuffer : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    
    float3 ro = cStartPoint_.xyz;
    float3 rd = normalize(cEndPoint_ - cStartPoint_).xyz;

    float los = 1.0;
    float rayDistance = 0;
    const float END = MAX_LENGTH * 0.25;
    const float EXP = 0.00004;
    int i = 0;

    [loop]
    while (rayDistance <= END) {
        i++;

        float3 pos = ro + rd * rayDistance;
        float distance;
        float3 normal;
        const float DENSE = CloudDensity(pos, distance, normal);

        // for Next Iteration
        const float RAY_ADVANCE_LENGTH = max(((END - 0) / cPixelSize_.x) * (exp(i * EXP) - 1), distance * 0.25);
        rayDistance += RAY_ADVANCE_LENGTH; 

        if (-pos.y < -400 || -pos.y > 25000) { break; }
        if (DENSE <= 0.0) { continue; }

        const float TRANSMITTANCE = BeerLambertFunciton(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);

        los *= TRANSMITTANCE;
    }

    OutputBuffer[0] = los;
}