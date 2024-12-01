// references:
// - https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/
// - https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/
// - https://qiita.com/edo_m18/items/876f2857e67e26a053d6
// - https://wallisc.github.io/rendering/2020/05/02/Volumetric-Rendering-Part-1.html mostly from here
// - https://www.shadertoy.com/view/wssBR8
// - https://www.shadertoy.com/view/Xttcz2
// https://www.shadertoy.com/view/WdsSzr

SamplerState depthSampler : register(s0);
SamplerState noiseSampler : register(s1);
SamplerState weatherMapSampler : register(s2);
SamplerState skySampler : register(s3);

Texture2D depthTexture : register(t0);
Texture3D noiseTexture : register(t1);
Texture2D weatherMapTexture : register(t2);
TextureCube skyTexture : register(t3);

//#define SDF
#ifdef SDF

    #define MAX_STEPS_SDF 128
    #define SDF_CLOUD_DENSE 0.015
    #define MAX_CLOUDS 48 // set same to environment::MAX_CLOUDS
    #define MU 1.0/100.0

#else

    #define MAX_STEPS_HEATMAP 512

#endif

#define MIP_MIN_METER 5*1852
#define MIP_MAX_METER 50*1852
#define MAX_VOLUME_LIGHT_MARCH_STEPS 2
#define LIGHT_MARCH_SIZE 600.0f

#include "CommonBuffer.hlsl"
#include "SDF.hlsl"

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

    // camera is placed inside the box, always.
    float4 worldPos = float4(input.Pos + cameraPosition.xyz, 1.0f);
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
    return t * 3.0;
    
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

float4 fbm_m(float3 pos, float mip) {
    // value input expected within 0 to 1 when R8G8B8A8_UNORM
    // value output expected within -1 to +1
    return noiseTexture.SampleLevel(noiseSampler, pos, mip);
}

float4 WeatherMap(float3 pos) {
    float4 weather = weatherMapTexture.Sample(weatherMapSampler, pos.xz);
    weather.g *= 255.0 * 50.0; // height in meter 200m precision 0-12750m height from base height
    weather.b *= 255.0 * 200.0; // height in meter 200m precision 0-51000m height from mean sea level
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
        if (dot(sampleDir, normal) < 0.0) {
            sampleDir = -sampleDir;
        }
        
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

#ifdef SDF

float CloudSDF(float3 pos) {

    float sdf = 1e20;
    float3 cloudAreaSize = float3(3300, 1000, 3300);

    [unroll(MAX_CLOUDS)]
    for (int i = 0; i < MAX_CLOUDS; i++) {
        float newSDF = sdEllipsoid(pos - cloudPositions[i].xyz, cloudAreaSize);
        sdf = opSmoothUnion(sdf, newSDF, cloudAreaSize.x * 0.98);
        //if (sdf < 0.0) { break; } // this kind of break makes sphere void inside cloud
    }

    // 0.1 at cloud position - bottom to top at 1.0
    float cloudBottom = cloudAreaPos.y + cloudAreaSize.y * 0.5;
    float bottomFade = smoothstep(cloudBottom, cloudBottom - cloudAreaSize.y * 0.2, pos.y);
    float heightGradient = 1.0; //0.5 + bottomFade;

    // Add noise to the SDF
    float cloudSpread = cloudAreaSize.x * 2.00;
    float cloudDense = 0.2 / cloudAreaSize.x;
    sdf += cloudSpread * fbm(pos * cloudDense).r * heightGradient;

    // Normalize the SDF [-cloudAreaSize.x, 0] to the range [-1, 0]
    float normalized = SmoothNormalize(sdf, -cloudAreaSize.x, 0.0);
    sdf = lerp(sdf, normalized, /*if sdf < 0.0*/step(sdf, 0.0));

    return sdf;
}

// For SDF Strategy but not gonna main because iterating SDFs are slow
float4 RayMarch(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    cloudDepth = 0;

    float integRayTranslate = 0;
    rayStart += rayDir * fbm(rayStart + rayDir).a * 25.0;

    bool hit = false;
    float3 hitPos = rayStart + rayDir * 1e20;

    [loop]
    for (int i = 0; i < MAX_STEPS_SDF; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * integRayTranslate;

        // Get the density at the current position
        float sdf = CloudSDF(rayPos);

        // for Next Iteration
        float deltaRayTranslate = max(sdf * 0.25, 100.0); 

        integRayTranslate += deltaRayTranslate; 
        if (integRayTranslate > primDepthMeter) { break; }
        if (integRayTranslate > 422440.f) { break; }

        // Skip if density is zero
        if (sdf > 0.0) { continue; }
        if (!hit) { hit = true; hitPos = rayPos; }
        // here starts inside cloud !
        float dense = -sdf * (fbm(rayPos * 0.002).r * 0.5 + 0.5);

        // Calculate the scattering and transmission
        float transmittance = BeerLambertFunciton(UnsignedDensity(dense) * MU, deltaRayTranslate);
        float lightVisibility = 1.0f;

        // light ray march
        [loop]
        for (int v = 1; v <= MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
        {
            float3 fixedLightDir = lightDir.xyz * float3(-1,1,-1);
            float3 sunRayPos = rayPos + v * -fixedLightDir.xyz * 100.0;
            float sdf2 = CloudSDF(sunRayPos);

            float sunDense = -sdf2 * (fbm(rayPos * 0.002).r * 0.5 + 0.5);

            lightVisibility *= BeerLambertFunciton(UnsignedDensity(sunDense) * MU, 100.0);
        }
            
        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - transmittance);
        intScattTrans.rgb += integScatt * intScattTrans.a;
        intScattTrans.a *= transmittance;

        // Opaque check
        if (intScattTrans.a < 0.003)
        {
            intScattTrans.a = 0.0;

            // Calculate the depth of the cloud
            float4 proj = mul(mul(float4(rayPos, 1.0), view), projection);
            cloudDepth = proj.z / proj.w;

            break;
        }
    }

    // Sample ambient light from the environment map
    float3 normal = normalize(rayDir + lightDir.xyz);
    float3 ambientLight = SampleAmbientLight(normal);

    // Calculate ambient occlusion // Assuming rayDir is the normal for simplicity
    float ao = 1;//CalculateAmbientOcclusion(hitPos, normal, 10.0, 16); // Adjust sample radius and count as needed

    // ambient light
    intScattTrans.rgb *= 0.8;
    intScattTrans.rgb += ambientLight * (1.0 - intScattTrans.a) * ao;
    // intScattTrans.rgb += skyTexture.Sample(skySampler, rayDir).rgb * intScattTrans.a;
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

#else // HEATMAP

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
    // Exponential curve parameters
    float x = float(stepIndex) / MAX_STEPS_HEATMAP;  // Normalize to [0,1]
    float base = 2.71828;  // e
    float exponent = 1.0;  // Controls curve steepness
    
    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);
    
    // Scale to ensure total distance is covered
    return curve * (maxLength / MAX_STEPS_HEATMAP);
}

float MergeDense(float dense1, float dense2) {
    return (dense1 + dense2) * 0.5;
}

float CloudDensity(float3 pos, float3 boxPos, float3 boxSize) {

    // get the uvw within cloud zone
    float3 uvw = pos_to_uvw(pos, boxPos, boxSize);
    float noiseRepeatNM = 2.5 + 7.5 * WeatherMap(uvw).a;
    float noiseSampleFactor = 1.0 / (noiseRepeatNM * 1852);
    
    // cloud dense control
    float dense = pow( WeatherMap(uvw).r, 2.2);

    // cloud height control
    float heightMeter = WeatherMap(uvw).g;
    float cloudBottom = WeatherMap(uvw).b - 25000 * 0.3048;
    // note that y minus is up
    float cloudTop = cloudBottom - heightMeter; // Upper boundary
    
    float bottomFade = smoothstep(cloudBottom, cloudBottom - 1000 * 0.1, pos.y) * fbm_m(pos * noiseSampleFactor, MipCurve(pos)).r;
    float topFade = 1.0 - smoothstep(cloudTop + heightMeter * 0.5, cloudTop, pos.y);
    float heightGradient = bottomFade * topFade;

    dense = dense * heightGradient;

    dense *= fbm_b(pos * noiseSampleFactor, MipCurve(pos)).r;

    return dense;
}

// For Heat Map Strategy
float4 RayMarch(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {
    
    float3 boxPos = cloudAreaPos.xyz;
    float3 boxSize = float3(200/*nm*/ * 1852/*to meter*/, 51000/*feet*/ * 0.3048/*to meter*/, 200/*nm*/ * 1852/*to meter*/); //cloudAreaSize.xyz;// float3(1000,200,1000);
    float3 fixedLightDir = lightDir.xyz * float3(-1,1,-1);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    float lightScatter = max(0.5, dot(normalize(-fixedLightDir), rayDir));
    lightScatter *= phaseFunction(0.01, lightScatter);
    cloudDepth = 0;

    // Check if ray intersects the cloud box
    float2 startEnd = CloudBoxIntersection(rayStart, rayDir, boxPos, boxSize);
    if (startEnd.x >= startEnd.y) { return float4(0, 0, 0, 0); } // No intersection

    // Clamp the intersection points, if intersect primitive earlier stop ray there
    startEnd.x += fbm_m(rayStart + rayDir, MipCurve(rayStart + rayDir)).a * 100.0;
    startEnd.x = max(0, startEnd.x);
    startEnd.y = min(primDepthMeter, startEnd.y);

    // Calculate the offset of the intersection point from the box
    // start from box intersection point
	float planeoffset = 1 - frac( ( startEnd.x - length(rayDir) ) * MAX_STEPS_HEATMAP );
    float integRayTranslate = startEnd.x + (planeoffset / MAX_STEPS_HEATMAP);

    // SDF from dense is -1 to 1 so if we advance ray with SDF we might need to multiply it
    float sdfMultiplier = 10.0f;

    [loop]
    for (int i = 0; i < MAX_STEPS_HEATMAP; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * integRayTranslate;

        // Get the density at the current position
        float dense = CloudDensity(rayPos, boxPos, boxSize);
        float sdf = -dense;

        // for Next Iteration
        float deltaRayTranslate = GetMarchSize(i, 422440.0f);

        integRayTranslate += deltaRayTranslate; 
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

            float dense2 = CloudDensity(sunRayPos, boxPos, boxSize);

            lightVisibility *= BeerLambertFunciton(UnsignedDensity(dense2), LIGHT_MARCH_SIZE);
        }

        // Integrate scattering
        float3 integScatt = lightVisibility * (1.0 - transmittance) * lightScatter;
        intScattTrans.rgb += integScatt * intScattTrans.a;
        intScattTrans.a *= transmittance;

        // Opaque check
        if (intScattTrans.a < 0.003)
        {
            intScattTrans.a = 0.0;

            // Calculate the depth of the cloud
            float4 proj = mul(mul(float4(rayPos, 1.0), view), projection);
            cloudDepth = proj.z / proj.w;

            break;
        }
    }
    
    // ambient light
    intScattTrans.rgb += monteCarloAmbient(/*ground*/float3(0,1,0)) * (1.0 - intScattTrans.a);
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

#endif

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    // For Debugging
    // output.Color = noiseTexture.SampleLevel(noiseSampler, float3(input.TexCoord, 0.0), 0);
    // output.Color = float4(normalize(input.Worldpos.xyz - cameraPosition.xyz), 1);
    // output.Depth = 1;
    // return output;
    
    float2 screenPos = input.Pos.xy;
    // TODO : pass resolution some way
    
    float2 pixelPos = screenPos / 512/*resolution for raymarch*/;
	float2 rcpro = rcp(float2(512, 512));

    // // dithering, only draw up left of 4 pixels
    // if (frac(screenPos.x * 0.5) + frac(screenPos.y * 0.5) > 1.0) {
    //     output.Color = float4(0, 0, 0, 0);
    //     output.DepthColor = 0;
    //     output.Depth = 0;
    //     return output;
    // }

    float3 ro = cameraPosition; // Ray origin
    float3 rd = normalize(input.Worldpos.xyz - cameraPosition.xyz); // Ray direction
    
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;

    float4 cloud = RayMarch(ro, rd, primDepthMeter, cloudDepth);

    // For Debugging
    // for (int i = 0; i < 512; i++) {
    //     float3 p = ro + rd * i * 10;
    //     float d = sdBox(p - float3(0,0,0), float3(400, 900, 100) * 0.50);
    //     if (d < 0.01) {
    //         cloud = float4(1, 0, 0, 1);
    //         break;
    //     }
    // }

    // cloud = SkyRay(ro, rd, lightDir.xyz) * (1.0 - cloud.a) + cloud;
    // cloud = skyTexture.Sample(skySampler, rd) * (1.0 - cloud.a) + cloud;

    output.Color = cloud;
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

    return output;
}

PS_OUTPUT PS_SKYBOX(PS_INPUT input) {
    PS_OUTPUT output;

    float3 ro = cameraPosition;
    float3 rd = normalize(input.Worldpos.xyz - cameraPosition.xyz);
    
    output.Color = skyTexture.Sample(skySampler, rd);
    output.DepthColor = 0;
    output.Depth = 0;

    return output;
}