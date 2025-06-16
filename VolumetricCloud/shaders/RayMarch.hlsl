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
float Energy(float density, float stepSize) {
                    // beer lambert * beer powder
    return exp( - density * stepSize );
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
    // attenuation �? difference from slides �? reduce the secondary component when we look toward the sun.
    float primary_attenuation = exp( - dl );
    float secondary_attenuation = exp(-dl * 0.25) * 0.7;
    float attenuation_probability = max( Remap( cos_angle, 0.7, 1.0, SECONDARY_INTENSITY_CURVE, SECONDARY_INTENSITY_CURVE * 0.25) , PRIMARY_INTENSITY_CURVE);
     
    // in-scattering �? one difference from presentation slides �? we also reduce this effect once light has attenuated to make it directional.
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

#define NUM_SAMPLES_MONTE_CARLO 8

// Monte Carlo
float3 monteCarloAmbient(float3 normal) {
    float3 ambientColor = 0.0;
    float3 tangent, bitangent;
    
    // Create a coordinate system based on the normal
    if (abs(normal.y) > 0.99) {
        tangent = float3(1, 0, 0);
    } else {
        tangent = normalize(cross(float3(0, 1, 0), normal));
    }
    bitangent = cross(normal, tangent);
    
    // Use stratified sampling - divide the hemisphere into cells
    int sqrtSamples = 2; // sqrt(NUM_SAMPLES_MONTE_CARLO) rounded down
    for (int i = 0; i < sqrtSamples; i++) {
        for (int j = 0; j < sqrtSamples; j++) {
            // Use Hammersley sequence for better distribution
            float u = (float(i) + 0.5) / float(sqrtSamples);
            float v = (float(j) + 0.5) / float(sqrtSamples);
            
            // Add randomization to reduce banding
            float2 jitter = float2(hash12(float2(u, v)), hash12(float2(v, u))) * 0.9 / sqrtSamples;
            u += jitter.x;
            v += jitter.y;
            
            // Convert uniform distribution to cosine-weighted hemisphere distribution
            float phi = 2.0 * 3.14159 * u;
            float cosTheta = sqrt(1.0 - v);
            float sinTheta = sqrt(v);
            
            // Create the sample direction vector
            float3 sampleDir = normalize(
                tangent * (sinTheta * cos(phi)) +
                bitangent * (sinTheta * sin(phi)) +
                normal * cosTheta
            );
            
            // Importance sampling - weight by cosine factor
            float weight = cosTheta;
            float3 envColor = skyTexture.Sample(skySampler, sampleDir).rgb;
            
            ambientColor += envColor * weight;
        }
    }
    
    // Additional boosted ambient from high altitude
    float3 upColor = skyTexture.Sample(skySampler, float3(0, 1, 0)).rgb;
    
    // Return average sample plus boosted up-vector sample
    return (ambientColor / (sqrtSamples * sqrtSamples)) * 1.5 + upColor * 0.15;
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

// Forward declarations
float CloudDensity(float3 pos, out float distance, out float3 normal);
float CloudDensityWithoutNormal(float3 pos);

// Level of detail selection based on distance and performance mode
float GetMipLevel(float distance, bool highQuality) {
    // Base distance threshold where we start switching to lower detail
    float baseThreshold = MAX_LENGTH * 0.025;
    float mipOffset = highQuality ? 0.0 : 1.0; // Lower starting mip in low quality mode
    
    if (distance < baseThreshold) return 0.0 + mipOffset;
    else if (distance < baseThreshold * 2.0) return 1.0 + mipOffset;
    else if (distance < baseThreshold * 4.0) return 2.0 + mipOffset;
    else if (distance < baseThreshold * 8.0) return 3.0 + mipOffset;
    else return 4.0 + mipOffset;
}

// Helper function to calculate density without normals to avoid recursion
float CloudDensityWithoutNormal(float3 pos) {
    // Simplified version of CloudDensity that doesn't call itself
    float RAYHEIGHT_METER = -pos.y;
    float RAY_DIST = length(cCameraPosition_.xyz - pos);
    
    // Use simplified texture sampling for this function
    float4 FMAP = fMapTexture.SampleLevel(linearSampler, Pos2UVW(pos, 0.0, 1000*16*64).xz, 0.0);
    float4 LARGE_NOISE = CUTOFF(Noise3DTex(pos * (1.0) / (1000*16*2), 0.0), 0.0);
    
    // Fast height check
    const float CUMULUS_THICKNESS_METER = 500 + 7000 * FMAP.g;
    const float CUMULUS_BOTTOM_ALT_METER = FMAP.b * 0.3048;
    const float HEIGHT = (RAYHEIGHT_METER - CUMULUS_BOTTOM_ALT_METER) / CUMULUS_THICKNESS_METER;
    
    if (HEIGHT < 0.0 || HEIGHT > 1.0) return 0.0;
    
    // Quick density calculation
    float density = LARGE_NOISE.r * 0.5 + 0.5;
    density *= RemapClamp(HEIGHT, 0.00, 0.20, 0.0, 1.0) * RemapClamp(HEIGHT, 0.20, 1.00, 1.0, 0.0);
    
    return density * (1.0 / 64.0);
}

// Calculate cloud normal using central differences 
// for better lighting and ambient occlusion
float3 CalculateCloudNormal(float3 pos, float sampleDist) {
    const float h = max(10.0, sampleDist * 0.1); // Adaptive sampling based on distance
    
    float dx = CloudDensityWithoutNormal(pos + float3(h, 0, 0)) - CloudDensityWithoutNormal(pos - float3(h, 0, 0));
    float dy = CloudDensityWithoutNormal(pos + float3(0, h, 0)) - CloudDensityWithoutNormal(pos - float3(0, h, 0));
    float dz = CloudDensityWithoutNormal(pos + float3(0, 0, h)) - CloudDensityWithoutNormal(pos - float3(0, 0, h));
    
    return normalize(float3(dx, dy, dz) / (2.0 * h));
}

float CloudDensity(float3 pos, out float distance, out float3 normal) {
    // note that y minus is up
    const float RAYHEIGHT_METER = -pos.y;
    float dense = 0; // linear to gamma
    distance = 0;
    normal = float3(0, 1, 0); // Default normal points up
    
    // cloud dense control
    float RAY_DIST = length(cCameraPosition_.xyz - pos);
    
    // Calculate efficient LOD for texturing
    float mipLevel = GetMipLevel(RAY_DIST, true); // true = high quality
    
    // Cache texture samples to avoid redundant lookups
    const float4 FMAP = fMapTexture.SampleLevel(linearSampler, Pos2UVW(pos, 0.0, 1000*16*64).xz, 0.0);
    
    // Only sample large noise if needed for optimization
    float4 LARGE_NOISE = float4(0, 0, 0, 0);
    if (RAY_DIST < MAX_LENGTH * 0.25) {
        LARGE_NOISE = CUTOFF(Noise3DTex(pos * (1.0) / (1000*16*2), mipLevel), 0.0);
    } else {
        // Use a cheaper approximation for distant clouds
        LARGE_NOISE = CUTOFF(Noise3DTex(pos * (1.0) / (1000*16*4), mipLevel + 1.0), 0.0);
    }
    
    // Only sample detail noise for closer clouds
    float4 NOISE = float4(0, 0, 0, 0);
    if (RAY_DIST < MAX_LENGTH * 0.15) {
        NOISE = CUTOFF(Noise3DSmallTex(pos * 1.0 / (1.0 * NM_TO_M), mipLevel), 0.0);
    }

    const float POOR_WEATHER_PARAM = RemapClamp(FMAP.r, 0.0, 1.0, 0.0, 1.0);
    const float CUMULUS_THICKNESS_PARAM = cCloudStatus_.g;
    const float CUMULUS_BOTTOM_ALT_PARAM = cCloudStatus_.b;

    // first layer: cumulus(WIP) and stratocumulus(TBD)
    {
        const float INITIAL_DENSE = 1.0 / 64.0;
        
        // cloud height parameter
        const float CUMULUS_THICKNESS_METER = 500 + 7000 * FMAP.g;
        const float CUMULUS_BOTTOM_ALT_METER = FMAP.b * 0.3048;
        const float HEIGHT = (RAYHEIGHT_METER - CUMULUS_BOTTOM_ALT_METER) / CUMULUS_THICKNESS_METER;

        // calculate distance and normal
        distance = DISTANCE_CLOUD(RAYHEIGHT_METER, CUMULUS_BOTTOM_ALT_METER, CUMULUS_THICKNESS_METER);
        
        // Early exit if outside cloud layer - significant performance boost
        if (HEIGHT < 0.0 || HEIGHT > 1.0) {
            return 0.0;
        }
        
        // If we're in the cloud layer, continue with shape calculations
        float first_layer_dense = 1.0;
        first_layer_dense = RemapClamp((LARGE_NOISE.r * 0.5 + 0.5), 1.0 - POOR_WEATHER_PARAM, 1.0, 0.0, 1.0); // perlinWorley
        
        // Progressive detail addition based on distance for better performance
        if (first_layer_dense > 0.05) {
            first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.g * 0.5 + 0.5), 1.0, 0.0, 1.0);
            
            if (RAY_DIST < MAX_LENGTH * 0.15) {
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.b * 0.5 + 0.5), 1.0, 0.0, 1.0);
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.a * 0.5 + 0.5), 1.0, 0.0, 1.0);
            }
        } else {
            return 0.0; // Early exit if first shape test is negative
        }

        // shape cumulus coverage smaller on top, to create cumulus shape
        const float CUMULUS_LAYER = RemapClamp(HEIGHT, 0.00, 0.20, 0.0, 1.0) * RemapClamp(HEIGHT, 0.20, 1.00, 1.0, 0.0);
        first_layer_dense = RemapClamp(first_layer_dense, 1.0 - CUMULUS_LAYER, 1.0, 0.0, 1.0);
        
        // Skip further calculations if density is zero
        if (first_layer_dense <= 0.01) {
            return 0.0;
        }
        
        // cumulus anvil with improved shape
        const float ANVIL_BIAS = 1.0;
        const float SLOPE = 0.2;
        const float BOTTOM_WIDE = 0.8;
        first_layer_dense = pow(first_layer_dense, RemapClamp(1.0 - HEIGHT, SLOPE, BOTTOM_WIDE, 1.0, lerp(1.0, 0.5, ANVIL_BIAS)));

        // Progressive detail addition - add detail only when needed
        if (RAY_DIST < MAX_LENGTH * 0.10) {
            first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.r * 0.5 + 0.5), 1.0, 0.0, 1.0);
            
            if (RAY_DIST < MAX_LENGTH * 0.10 * 0.50) {
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.g * 0.5 + 0.5), 1.0, 0.0, 1.0);
                
                if (RAY_DIST < MAX_LENGTH * 0.10 * 0.25) {
                    first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.b * 0.5 + 0.5), 1.0, 0.0, 1.0);
                    
                    if (RAY_DIST < MAX_LENGTH * 0.10 * 0.125) {
                        first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.a * 0.5 + 0.5), 1.0, 0.0, 1.0);
                    }
                }
            }
        }
        
        first_layer_dense *= INITIAL_DENSE;
        dense += first_layer_dense;
    }

    // second layer: cirrus (TBD)
    if (dense < 0.01 && RAY_DIST < MAX_LENGTH * 0.2) {
        const float INITIAL_DENSE = 1.0 / 64.0;
        
        // cloud height parameter
        const float CUMULUS_THICKNESS_METER = 500 + 500 * FMAP.g;
        const float CUMULUS_BOTTOM_ALT_METER = FMAP.b * 0.3048 + 2500;
        const float HEIGHT = (RAYHEIGHT_METER - CUMULUS_BOTTOM_ALT_METER) / CUMULUS_THICKNESS_METER;

        // Early exit for second layer if outside height range
        if (HEIGHT < 0.0 || HEIGHT > 1.0) {
            return dense;
        }
        
        // calculate distance and normal
        distance = min(distance, DISTANCE_CLOUD(RAYHEIGHT_METER, CUMULUS_BOTTOM_ALT_METER, CUMULUS_THICKNESS_METER));
        
        // create coverage shape
        float first_layer_dense = 1.0;
        first_layer_dense = RemapClamp((LARGE_NOISE.r * 0.7 + 0.3), 1.0 - POOR_WEATHER_PARAM, 1.0, 0.0, 1.0);
        
        if (first_layer_dense > 0.05) {
            first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.g * 0.5 + 0.5), 1.0, 0.0, 1.0);
            
            if (RAY_DIST < MAX_LENGTH * 0.15) {
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.b * 0.5 + 0.5), 1.0, 0.0, 1.0);
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (LARGE_NOISE.a * 0.5 + 0.5), 1.0, 0.0, 1.0);
            }
        } else {
            return dense;
        }

        // shape cirrus coverage
        const float CUMULUS_LAYER = RemapClamp(HEIGHT, 0.00, 0.20, 0.0, 1.0) * RemapClamp(HEIGHT, 0.20, 1.00, 1.0, 0.0);
        first_layer_dense = RemapClamp(first_layer_dense, 1.0 - CUMULUS_LAYER, 1.0, 0.0, 1.0);

        // Progressive detail addition for cirrus
        if (RAY_DIST < MAX_LENGTH * 0.08) {
            first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.r * 0.5 + 0.5), 1.0, 0.0, 1.0);
            
            if (RAY_DIST < MAX_LENGTH * 0.08 * 0.50) {
                first_layer_dense = RemapClamp(first_layer_dense, 1.0 - (NOISE.g * 0.5 + 0.5), 1.0, 0.0, 1.0);
            }
        }
        
        first_layer_dense *= INITIAL_DENSE; // Make cirrus more transparent
        dense += first_layer_dense;
    }

    // Calculate normals only for close clouds or when density is significant
    if (dense > 0.005 && RAY_DIST < MAX_LENGTH * 0.1) {
        normal = CalculateCloudNormal(pos, RAY_DIST * 0.001);
    }

    return dense;
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
    // Improved Henyey-Greenstein with forward and backward scattering components
    float g = 0.05; // forward scattering factor
    float g2 = -0.2; // backward scattering factor
    float lightScatter = HenyeyGreenstein(cos_angle, g) * 0.8 + HenyeyGreenstein(cos_angle, g2) * 0.2;
    
    float rayDistance = in_start;

    // Early ray termination variables
    bool hit = false;
    float previousDensity = 0.0;
    float densitySum = 0.0;

    [fastopt]
    for (int i = 0; i < 2048; i++) { // Reduced max iterations from 2048 to 128 for better performance

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * rayDistance;
        rayPos = AdjustForEarthCurvature(rayPos, cCameraPosition_.xyz);

        // Get the density at the current position
        float distance;
        float3 normal;
        const float DENSE = CloudDensity(rayPos, distance, normal);
        
        // Adaptive step size based on density and distance
        float adaptiveStepFactor = 1.0;
        if (DENSE > 0) {
            adaptiveStepFactor = max(0.2, 1.0 - DENSE * 0.8); // Smaller steps in dense areas
        }
        
        // For Next Iteration with adaptive step size
        const float RAY_ADVANCE_LENGTH = max((in_end - in_start) * (exp(i * 0.000005) - 1.0), distance * adaptiveStepFactor);
        rayDistance += RAY_ADVANCE_LENGTH; 

        // primitive depth check
        if (rayDistance > min(primDepthMeter, in_end)) { break; }
        // below deadsea level, or too high
        if (-rayPos.y < -400 || -rayPos.y > 25000) { break; }

        // Skip if density is zero
        if (DENSE <= 0.0) { continue; }
        
        // Early exit if we've already accumulated enough density
        if (intScattTrans.a < 0.01) {
            intScattTrans.a = 0.0;
            break;
        }

        if (!hit) { 
            // Calculate the depth of the cloud
            const float4 PROJ = mul(mul(float4(rayPos/*revert to camera relative position*/ - cCameraPosition_.xyz, 1.0), cView_), cProjection_);
            output_cloud_depth = PROJ.z / PROJ.w;
            hit = true; 
        }

        // here starts inside cloud !

        // Calculate the scattering and transmission
        const float TRANSMITTANCE = BeerLambertFunciton(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);
        float lightVisibility = 1.0f;

        // Adaptive light sampling - use more samples when density is higher
        int adaptiveSunSteps = max(2, int(sunSteps * min(1.0, DENSE * 2.0)));
        
        // light ray march - only do extensive light sampling for dense cloud areas
        float previousLightDensity = DENSE;

        [faseopt]
        for (int s = 1; s <= adaptiveSunSteps; s++)
        {
            const float TO_SUN_RAY_ADVANCED_LENGTH = (LIGHT_MARCH_SIZE / adaptiveSunSteps);
            const float3 TO_SUN_RAY_POS = rayPos + SUNDIR * TO_SUN_RAY_ADVANCED_LENGTH * s;

            float nd;
            float3 nn;
            const float DENSE_2 = CloudDensity(TO_SUN_RAY_POS, nd, nn);
            
            // Improved integration with cone sampling approximation
            float weight = 1.0 - float(s) / float(adaptiveSunSteps + 1);
            float averageDensity = lerp(previousLightDensity, DENSE_2, 0.5);
            lightVisibility *= Energy(UnsignedDensity(averageDensity) * weight, TO_SUN_RAY_ADVANCED_LENGTH);
            previousLightDensity = DENSE_2;
        }

        // Silver lining effect - enhance edges that face the sun
        float edgeFactor = 1.0;
        if (DENSE < 0.1 && previousDensity > 0.0) {
            float sunAlignment = max(0, dot(normalize(normal), SUNDIR));
            edgeFactor = 1.0 + sunAlignment * 2.0;
        }
        previousDensity = DENSE;

        // Integrate scattering with improved lighting model
        float3 integScatt = lightVisibility * (1.0 - TRANSMITTANCE) * edgeFactor;
        intScattTrans.rgb += integScatt * intScattTrans.a * SUNCOLOR * lightScatter;
        intScattTrans.a *= TRANSMITTANCE;

        if (intScattTrans.a < 0.03)
        {
            intScattTrans.a = 0.0;
            break;
        }
    }

    // Improved ambient light with less Monte Carlo samples but better stratification
    intScattTrans.rgb += monteCarloAmbient(float3(0,1,0)) * (1.0 - intScattTrans.a) * 0.5;
    
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

// Apply temporal reprojection with jitter to reduce aliasing artifacts
float4 TemporalReproject(float4 currentColor, float2 pixelPos, float2 screenPos, float cloudDepth) {
    // Generate temporal offset based on frame number
    float frameIndex = fmod(cTime_.x * 30.0, 16.0);
    float2 jitter = float2(
        frac(sin(frameIndex * 12.9898 + pixelPos.y * 78.233) * 43758.5453),
        frac(cos(frameIndex * 39.7468 + pixelPos.x * 97.145) * 97531.4532)
    ) * 2.0 - 1.0;
    
    // Calculate reprojection vectors
    float4 currentPos = float4(screenPos.xy, cloudDepth, 1.0);
    float4 previousPos = mul(cPreviousViewProjection_, currentPos);
    previousPos /= previousPos.w;
    
    // Convert to UV coordinates with jitter for temporal supersampling
    float2 previousUV = (previousPos.xy * 0.5 + 0.5) + jitter * (0.5 / cPixelSize_.xy);
    
    // Sample previous frame with bilinear filtering
    float4 previousColor = previousTexture.SampleLevel(linearSampler, previousUV, 0);
    
    // Check if reprojection is valid (within screen and reasonable depth difference)
    bool validReprojection = 
        previousUV.x >= 0.0 && previousUV.x <= 1.0 &&
        previousUV.y >= 0.0 && previousUV.y <= 1.0 &&
        abs(previousColor.w - currentColor.w) < 0.1;
    
    // Blend between current and previous frame (temporal anti-aliasing)
    float blendFactor = validReprojection ? 0.05 : 1.0; // Use more current frame data when reprojection fails
    return lerp(previousColor, currentColor, blendFactor);
}

// Blue noise dithering pattern to break up banding artifacts
float BlueNoiseDither(float2 screenPos) {
    // Blue noise hash function (this could be replaced with a texture lookup for better quality)
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(screenPos, magic.xy))) * 2.0 - 1.0;
}

PS_OUTPUT StartRayMarch(PS_INPUT input, int sunSteps, float in_start, float in_end) {
    PS_OUTPUT output;
    
    float2 screenPos = input.Pos.xy;
    float2 pixelPos = screenPos.xy / cPixelSize_.xy;

    float3 ro = cCameraPosition_.xyz; // Ray origin

    // Ray direction with blue noise dithering to break up banding
    float3 rd = normalize(input.Worldpos.xyz); // Ray direction
    
    // Apply dithering to starting position to break up banding artifacts
    //float dither = BlueNoiseDither(screenPos);
    //ro += rd * dither * 10.0; // Small offset based on dither pattern
    
    // Gather primitive depth with a 5-tap pattern for more accurate depth bounds
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(+1.0, 0.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(-1.0, 0.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(0.0, +1.0) / cPixelSize_.xy).r);
    primDepth = min(primDepth, depthTexture.Sample(depthSampler, pixelPos + float2(0.0, -1.0) / cPixelSize_.xy).r);
    float primDepthMeter = DepthToMeter(primDepth);
    float cloudDepth = 0;

    // Adaptive quality control based on frame rate or performance setting
    float performanceScale = 1.0; // This could be controlled by app settings
    int adaptiveSunSteps = max(1, int(sunSteps * performanceScale));
    
    // Ray march the cloud with adaptive quality
    float4 cloud = RayMarch(ro, rd, adaptiveSunSteps, in_start, in_end, screenPos, primDepthMeter, cloudDepth);
    
    // Apply temporal reprojection to improve stability and reduce noise
    //cloud = TemporalReproject(cloud, pixelPos, screenPos, cloudDepth);
    
    // Apply subtle post-processing for visual enhancement
    //cloud.rgb = cloud.rgb * (1.0 + dither * 0.02); // Subtle dithering in final color
    
    // Tone mapping adjustment to enhance contrast in the clouds
    //cloud.rgb = cloud.rgb / (1.0 + cloud.rgb); // Simple Reinhard tone mapping
    //cloud.rgb = pow(cloud.rgb, 0.95); // Subtle gamma adjustment for nicer clouds
    
    // Output with improved temporal stability
    output.Color = cloud;
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
    float totalDistance = length(cEndPoint_ - cStartPoint_);

    float los = 1.0;
    float rayDistance = 0;
    const float END = min(MAX_LENGTH * 0.25, totalDistance);
    const float EXP = 0.00004;
    int i = 0;
    
    // Early optimization - check if path is likely to intersect clouds
    bool mayHitClouds = false;
    {
        // Test at a few strategic points along the ray
        for (int test = 0; test < 4; test++) {
            float sampleDist = END * (test / 3.0);
            float3 testPos = ro + rd * sampleDist;
            
            // Quick cloud layer test
            float height = -testPos.y;
            if (height >= 0 && height <= 25000) {
                // Sample weather map at a very low resolution
                float4 weatherMap = fMapTexture.SampleLevel(linearSampler, Pos2UVW(testPos, 0.0, 1000*16*64).xz, 0.0);
                if (weatherMap.r > 0.1) {
                    // Possible cloud intersection
                    mayHitClouds = true;
                    break;
                }
            }
        }
    }
    
    // Skip detailed marching if no clouds likely in path
    if (!mayHitClouds) {
        OutputBuffer[0] = 1.0; // Fully transparent
        return;
    }
    
    // Adaptive step size control
    float stepSize = END / 64.0; // Start with reasonable step size
    float stepMultiplier = 1.0;  // Will be adjusted based on cloud density
    
    [fastopt]
    while (rayDistance <= END) {
        i++;

        float3 pos = ro + rd * rayDistance;
        float distance;
        float3 normal;
        const float DENSE = CloudDensity(pos, distance, normal);

        // Adjust step size based on density and previous results
        if (DENSE > 0.0) {
            // Smaller steps in dense areas for accuracy
            stepMultiplier = max(0.5, 1.0 - DENSE * 2.0);
        } else {
            // Larger steps in empty areas for speed
            stepMultiplier = min(2.0, stepMultiplier + 0.1);
        }
        
        // Adaptive stepping with minimum step size
        const float RAY_ADVANCE_LENGTH = max(stepSize * stepMultiplier, distance * 0.25);
        rayDistance += RAY_ADVANCE_LENGTH;

        // Early exit conditions
        if (-pos.y < -400 || -pos.y > 25000) { break; }
        if (DENSE <= 0.0) { continue; }

        const float TRANSMITTANCE = BeerLambertFunciton(UnsignedDensity(DENSE), RAY_ADVANCE_LENGTH);
        los *= TRANSMITTANCE;
        
        // Early exit if already very opaque (optimization)
        if (los < 0.01) {
            los = 0.0;
            break;
        }
    }

    OutputBuffer[0] = los;
}