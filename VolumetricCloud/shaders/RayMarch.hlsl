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
Texture3D noiseSmallTexture : register(t4);

#define MAX_LENGTH 422440.0f
#define LIGHT_MARCH_SIZE 1000.0f

#include "CommonBuffer.hlsl"
#include "CommonFunctions.hlsl"
#include "FBM.hlsl"
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

float3 Pos2UVW(float3 pos, float3 boxPos, float3 boxSize) {
    // Normalize the world position to the box dimensions
    float3 boxMin = boxPos - boxSize * 0.5;
    float3 boxMax = boxPos + boxSize * 0.5;
    float3 uvw = (pos - boxMin) / (boxMax - boxMin);
    return uvw;
}

float MipCurve(float3 pos) {
    float dist = length(cameraPosition.xyz - pos);
    float t = dist / (MAX_LENGTH * 0.1) - 1.0;
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

// from https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
float Energy(float density, float stepSize) {
                    // beer lambert * beer powder
    return max( exp( - density * stepSize ) * (1.0 - exp( - density * stepSize * 2.0 )), 
                exp( - density * stepSize * 0.75) * 0.95 );
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
    // attenuation – difference from slides – reduce the secondary component when we look toward the sun.
    float primary_attenuation = exp( - dl );
    float secondary_attenuation = exp(-dl * 0.25) * 0.7;
    float attenuation_probability = max( Remap( cos_angle, 0.7, 1.0, SECONDARY_INTENSITY_CURVE, SECONDARY_INTENSITY_CURVE * 0.25) , PRIMARY_INTENSITY_CURVE);
     
    // in-scattering – one difference from presentation slides – we also reduce this effect once light has attenuated to make it directional.
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

    // note that y minus is up
    const float RAYHEIGHT = -pos.y;
    float dense = 0; // linear to gamma
    distance = 0;
    normal = 0;
    
    // cloud dense control
    float4 noise = CUTOFF( Noise3DSmallTex(pos * 1.0 / (0.5 * NM_TO_M), MipCurve(pos)), 0.0 );
    float4 largeNoise = CUTOFF( Noise3DTex(pos * (1.0) / (5.0 * NM_TO_M), MipCurve(pos)), 0.0 );
    float4 theaterNoise = CUTOFF( Noise3DSmallTex(pos * (1.0) / (30.0 * NM_TO_M), MipCurve(pos)), 0.0 );

    const float POOR_WEATHER_PARAM = cloudStatus.r;
    const float CUMULUS_THICKNESS_PARAM = cloudStatus.g;
    const float CUMULUS_BOTTOM_ALT_PARAM = cloudStatus.b;

    // when pre-calculating derivative for 3d noise.
    normal = normalize(largeNoise.yzw);

    const float THEATER_COVERALE = RemapClamp( theaterNoise.r * 0.5 + 0.5, 1.0 - POOR_WEATHER_PARAM, 1.0, 0.0, 1.0);
    const float CLOUD_COVERAGE = RemapClamp( largeNoise.r * 0.5 + 0.5, 1.0 - THEATER_COVERALE, 1.0, 0.0, 1.0);

    // first layer: cumulus(WIP) and stratocumulus(TBD)
    {
        const float INITIAL_DENSE = 1.0 / 256.0;
        
        // cloud height parameter
        const float CUMULUS_THICKNESS_METER = CUTOFF( CUMULUS_THICKNESS_PARAM * ALT_MAX, 0.0 ) * largeNoise.a;
        const float CUMULUS_BOTTOM_ALT_METER = CUTOFF( CUMULUS_BOTTOM_ALT_PARAM * ALT_MAX, 0.0 );
        const float HEIGHT = (RAYHEIGHT - CUMULUS_BOTTOM_ALT_METER) / CUMULUS_THICKNESS_METER;

        const float ANVIL_BIAS = 1.0;
        const float SLOPE = 0.2;
        const float BOTTOM_WIDE = 0.8;
        const float CUMULUS_SHAPE = pow(CLOUD_COVERAGE, RemapClamp( 1.0 - HEIGHT, SLOPE, BOTTOM_WIDE, 1.0, lerp(1.0, 0.5, ANVIL_BIAS)));
        const float TOP = RemapClamp(HEIGHT, 0.50, 1.00, 1.0, 0.0);
        
        // remove below bottom and over top, also gradient them when it reaches bottom/top
        const float CUMULUS_LAYER = RemapClamp(HEIGHT, 0.00, 0.50, 0.0, 1.0) * RemapClamp(HEIGHT, 0.50, 1.00, 1.0, 0.0);

        // calculate distance and normal
        distance = DISTANCE_CLOUD(RAYHEIGHT, CUMULUS_BOTTOM_ALT_METER, CUMULUS_THICKNESS_METER);
        //normal = normalize( float3(0.0, sign(rayHeight - cloudBaseMeter), 0.0) );

        // apply dense
        float first_layer_dense = RemapClamp(noise.r * 0.3 + 0.7, 1.0 - CUMULUS_SHAPE, 1.0, 0.0, 1.0) * TOP * INITIAL_DENSE * CUMULUS_LAYER;

        // cutoff so edge not become fluffy
        //first_layer_dense = SMOOTH_CUTOFF(first_layer_dense, 0.0005);

        // apply to final dense
        dense += first_layer_dense;
    }

    return dense;
}

// For Heat Map Strategy
float4 RayMarch(float3 rayStart, float3 rayDir, int steps, int sunSteps, float in_start, float in_end, float2 screenPosPx, float primDepthMeter, out float output_cloud_depth) {

    // initialize
    output_cloud_depth = 0;

    // light direction fix.
    const float3 SUNDIR = -(lightDir.xyz * float3(-1,1,-1));
    const float3 SUNCOLOR = CalculateSunlightColor(SUNDIR);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);

    // sun light scatter
    float cos_angle = dot(normalize(SUNDIR), normalize(rayDir));
    float lightScatter = HenyeyGreenstein(dot(normalize(SUNDIR), normalize(rayDir)), 0.10);

    // float lightScatter = max(0.50, dot(normalize(SUNDIR), rayDir));
    // lightScatter *= phaseFunction(0.01, lightScatter);
    
    float rayDistance = in_start;

    //rayStart = rayStart + rayDir * 500.0 * noiseTexture.Sample(noiseSampler, rayDir * time.x).a;

    int i = 0;

    [loop]
    while (rayDistance <= in_end) {
        i++;

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * rayDistance;
        rayPos = AdjustForEarthCurvature(rayPos, cameraPosition.xyz);

        // Get the density at the current position
        float distance;
        float3 normal;
        const float DENSE = CloudDensity(rayPos, distance, normal);
        
        // for Next Iteration
        const float RAY_ADVANCE_LENGTH = max((MAX_LENGTH / steps) * (exp(i * 0.005) - 1), distance * 0.25);
        rayDistance += RAY_ADVANCE_LENGTH; 

        // primitive depth check
        if (rayDistance > min(primDepthMeter, MAX_LENGTH)) { break; }
        // below deadsea level, or too high
        if (-rayPos.y < -400 || -rayPos.y > 25000) { break; }

        // Skip if density is zero
        if (DENSE <= 0.0) { continue; }

        // here starts inside cloud !

        // Calculate the scattering and transmission
        const float TRANSMITTANCE = BeerLambertFunciton(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);
        float lightVisibility = 1.0f;

        // light ray march
        for (int s = 1; s <= sunSteps; s++)
        {
            const float TO_SUN_RAY_ADVANCED_LENGTH = (LIGHT_MARCH_SIZE / sunSteps);
            const float3 TO_SUN_RAY_POS = rayPos + SUNDIR * TO_SUN_RAY_ADVANCED_LENGTH * s;

            float nd;
            float3 nn;
            const float DENSE_2 = CloudDensity(TO_SUN_RAY_POS, nd, nn);
            
            lightVisibility *= Energy(UnsignedDensity(DENSE_2), TO_SUN_RAY_ADVANCED_LENGTH);
        }

        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - TRANSMITTANCE);
        intScattTrans.rgb += integScatt * intScattTrans.a * SUNCOLOR * lightScatter;
        intScattTrans.a *= TRANSMITTANCE;

        // MIP DEBUG
        // if (MipCurve(rayPos) <= 4.0) { intScattTrans.rgb = float3(1, 0, 1); }
        // if (MipCurve(rayPos) <= 3.0) { intScattTrans.rgb = float3(0, 0, 1); }
        // if (MipCurve(rayPos) <= 2.0) { intScattTrans.rgb = float3(0, 1, 0); }
        // if (MipCurve(rayPos) <= 1.0) { intScattTrans.rgb = float3(1, 0, 0); }

        if (intScattTrans.a < 0.03)
        {
            // Calculate the depth of the cloud
            const float4 PROJ = mul(mul(float4(rayPos/*revert to camera relative position*/ - cameraPosition.xyz, 1.0), view), projection);
            output_cloud_depth = PROJ.z / PROJ.w;

            intScattTrans.a = 0.0;
            break;
        }
    }

    // ambient light
    intScattTrans.rgb += monteCarloAmbient(/*ground*/float3(0,1,0)) * (1.0 - intScattTrans.a);
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

PS_OUTPUT StartRayMarch(PS_INPUT input, int steps, int sunSteps, float in_start, float in_end, float px) {
    PS_OUTPUT output;
    
    // TODO : pass resolution some way
    float2 screenPos = input.Pos.xy;
    float2 pixelPos = screenPos / px /*resolution for raymarch*/;

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
    float4 cloud = RayMarch(ro, rd, steps, sunSteps, in_start, in_end, screenPos, primDepthMeter, cloudDepth);

    // output
    output.Color = cloud;
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

    return output;
}

PS_OUTPUT PS(PS_INPUT input) {

    return StartRayMarch(input, 5000, 4, 0, MAX_LENGTH * 0.033, 512);
}

PS_OUTPUT PS_FAR(PS_INPUT input) {

    return StartRayMarch(input, 1500, 4, MAX_LENGTH * 0.033, MAX_LENGTH * 1.0, 256);
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