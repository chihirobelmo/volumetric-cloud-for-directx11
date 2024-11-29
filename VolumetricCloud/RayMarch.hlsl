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
SamplerState fmapSampler : register(s2);
SamplerState skySampler : register(s3);

Texture2D depthTexture : register(t0);
Texture3D noiseTexture : register(t1);
Texture2D fmapTexture : register(t2);
TextureCube skyTexture : register(t3);

// performance tuning
#define MAX_STEPS_SDF 128
#define MAX_STEPS_HEATMAP 512
#define MAX_VOLUME_LIGHT_MARCH_STEPS 2

#define SDF_CLOUD_DENSE 0.015
#define MAX_CLOUDS 48 // set same to environment::MAX_CLOUDS

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

float ExtinctionFunction(float density, float3 position, float3 areaPos, float3 areaSize) 
{
    // Fix height calculations - bottom to top
    float cloudBottom = areaPos.y + areaSize.y * 0.5;  // Lower boundary
    float cloudTop = areaPos.y - areaSize.y * 0.5;     // Upper boundary
    
    float heightGradient = HeightGradient(position.y, cloudBottom, cloudTop, areaSize);
    
    // Increase base density and use noise directly
    float newDensity = max(density, 0.0) * heightGradient;

    // Ensure positive density
    return max(newDensity, 0.0);
}

float3 pos_to_uvw(float3 pos, float3 boxPos, float3 boxSize) {
    // Normalize the world position to the box dimensions
    float3 boxMin = boxPos - boxSize * 0.5;
    float3 boxMax = boxPos + boxSize * 0.5;
    float3 uvw = (pos - boxMin) / (boxMax - boxMin);
    return uvw;
}

float4 fbm2d(float3 pos, float slice) {
    // value input expected within -1 to +1
    return noiseTexture.Sample(noiseSampler, float3(pos.x, slice, pos.z));
}

float4 fbm(float3 pos) {
    // value input expected within -1 to +1
    return noiseTexture.Sample(noiseSampler, pos);
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
    // Exponential curve parameters
    float x = float(stepIndex) / MAX_STEPS_HEATMAP;  // Normalize to [0,1]
    float base = 2.71828;  // e
    float exponent = 1.0;  // Controls curve steepness
    
    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);
    
    // Scale to ensure total distance is covered
    return curve * (maxLength / MAX_STEPS_HEATMAP);
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

float BeerLambertLaw(float density, float stepSize) {
    return exp(-density * stepSize);
}

float VisibleAreaCloudCoverage(float3 pos) {
    return fbm2d(pos, /*placeholder*/0.5).r;
}

float MergeDense(float dense1, float dense2) {
    return (dense1 + dense2) * 0.5;
}

float CloudDensity(float3 pos, float3 boxPos, float3 boxSize) {
    float3 uvw = pos_to_uvw(pos, boxPos, boxSize);
    // return fbm(uvw).r;
    float dense = VisibleAreaCloudCoverage(uvw);

    dense = MergeDense(dense, fbm(pos * 0.0005).r);

    dense = ExtinctionFunction(dense, pos, boxPos, boxSize);

    return dense;
}

float CloudSDF(float3 pos) {

    float sdf = 1e20;
    float3 cloudAreaSize = float3(3300, 1000, 3300);

    [loop]
    for (int i = 0; i < MAX_CLOUDS; i++) {
        float newSDF = sdEllipsoid(pos - cloudPositions[i].xyz, cloudAreaSize);
        sdf = opSmoothUnion(sdf, newSDF, cloudAreaSize.x * 0.98);
        if (sdf < 0.0) { break; }
    }

    // 0.1 at cloud position - bottom to top at 1.0
    float cloudBottom = cloudAreaPos.y + cloudAreaSize.y * 0.5;
    float bottomFade = smoothstep(cloudBottom, cloudBottom - cloudAreaSize.y * 0.2, pos.y);
    float heightGradient = 1.0; //0.5 + bottomFade;

    sdf += (cloudAreaSize.x * 2.00) * fbm(pos * (0.1 / cloudAreaSize.x)).r * heightGradient;
    if (sdf <= 0.0) { 
        // normalize -500 -> 0 value to -1 -> 0
        sdf = max(-cloudAreaSize.x, sdf) * (1.0 / cloudAreaSize.x);
    }

    return sdf;
}

#define MU 1.0/100.0

// to check 3d texture
// somehow shadow does not work for this...
float4 RayMarch___SDF(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    cloudDepth = 0;

    float integRayTranslate = 0;
    rayStart += rayDir * fbm(rayStart + rayDir).a * 25.0;

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
        if (sdf > 50.0) { continue; }
        // here starts inside cloud !
        float dense = -sdf * (fbm(rayPos * 0.0005).r * 0.5 + 0.5);

        // Calculate the scattering and transmission
        float transmittance = BeerLambertLaw(UnsignedDensity(dense) * MU, deltaRayTranslate);
        float lightVisibility = 1.0f;

        // light ray march
        [loop]
        for (int v = 1; v <= MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
        {
            float3 sunRayPos = rayPos + v * -lightDir.xyz * 100.0;
            float sdf2 = CloudSDF(sunRayPos);

            float sunDense = -sdf2 * (fbm(rayPos * 0.0005).r * 0.5 + 0.5);
            if (sdf2 >= 0.0) { sunDense = 0.0f; }

            lightVisibility *= BeerLambertLaw(UnsignedDensity(sunDense) * MU, 100.0);
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

    // ambient light
    intScattTrans.rgb *= 0.8;
    intScattTrans.rgb += skyTexture.Sample(skySampler, -rayDir).rgb * (1.0 - intScattTrans.a);
    // intScattTrans.rgb += skyTexture.Sample(skySampler, rayDir).rgb * intScattTrans.a;
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

// to check 3d texture
float4 RayMarch___HeatMap(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {
    
    float3 boxPos = cloudAreaPos.xyz;
    float3 boxSize = cloudAreaSize.xyz;// float3(1000,200,1000);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    cloudDepth = 0;

    // Check if ray intersects the cloud box
    float2 startEnd = CloudBoxIntersection(rayStart, rayDir, boxPos, boxSize);
    if (startEnd.x >= startEnd.y) { return float4(0, 0, 0, 0); } // No intersection

    // Clamp the intersection points, if intersect primitive earlier stop ray there
    startEnd.x = max(0, startEnd.x);
    startEnd.y = min(primDepthMeter, startEnd.y);

    // Calculate the offset of the intersection point from the box
    // start from box intersection point
	float planeoffset = 1 - frac( ( startEnd.x - length(rayDir) ) * MAX_STEPS_HEATMAP );
    float integRayTranslate = startEnd.x + (planeoffset / MAX_STEPS_HEATMAP);
    
    // light ray marching setups
    float lightMarchSize = 10.0f;

    // SDF from dense is -1 to 1 so if we advance ray with SDF we might need to multiply it
    float sdfMultiplier = 10.0f;

    rayStart += rayDir * fbm(rayStart + rayDir).a * 1.0;

    [loop]
    for (int i = 0; i < MAX_STEPS_HEATMAP; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * integRayTranslate;

        // Get the density at the current position
        float dense = CloudDensity(rayPos, boxPos, boxSize);
        float sdf = -dense;

        // for Next Iteration
        float deltaRayTranslate = max(sdf, 5.0); 

        integRayTranslate += deltaRayTranslate; 
        if (integRayTranslate > startEnd.y) { break; }

        // Skip if density is zero
        if (dense <= 0.0) { continue; }
        // here starts inside cloud !

        // Calculate the scattering and transmission
        float transmittance = BeerLambertLaw(UnsignedDensity(dense), deltaRayTranslate);
        float lightVisibility = 1.0f;

        // light ray march
        [loop]
        for (int v = 1; v <= MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
        {
            float3 sunRayPos = rayPos + v * -lightDir.xyz * lightMarchSize;
            if (sdBox(sunRayPos - boxPos, boxSize) > 0.0) { break; }

            float dense2 = CloudDensity(sunRayPos, boxPos, boxSize);

            lightVisibility *= BeerLambertLaw(UnsignedDensity(dense2), lightMarchSize);
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
    
    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1 - intScattTrans.a);
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    // For Debugging
    // output.Color = float4(normalize(input.Worldpos.xyz - cameraPosition.xyz), 1);
    // output.Depth = 1;
    // return output;
    
    float2 screenPos = input.Pos.xy;
    // TODO : pass resolution some way
    float2 pixelPos = screenPos / 360/*resolution for raymarch*/;

    float3 ro = cameraPosition; // Ray origin
    float3 rd = normalize(input.Worldpos.xyz - cameraPosition.xyz); // Ray direction
    
    float primDepth = depthTexture.Sample(depthSampler, pixelPos).r;
    float primDepthMeter = DepthToMeter( primDepth );
    float cloudDepth = 0;
    float4 cloud = RayMarch___SDF(ro, rd, primDepthMeter, cloudDepth);

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







// to check 3d texture
float4 RayMarchTest(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {
    // draw 3D box from 3d texture
    float3 boxsize = float3(1000,200,1000);
    float2 startEnd = CloudBoxIntersection(rayStart, rayDir, 0, boxsize * 0.5);
    if (startEnd.x >= startEnd.y) { return float4(0, 0, 0, 1); }
    float4 col = float4(0, 0, 0, 1);
    float s = 512;
    float t = startEnd.x;
    float m = (startEnd.y - startEnd.x) / s;
    float4 proj = mul(mul(float4(rayStart + rayDir * t, 1.0), view), projection);
    cloudDepth = proj.z / proj.w;
    [loop]
    for (int i = 0; i < s; i++) {
        float3 p = rayStart + rayDir * t;
        float d = sdBox(p, boxsize * 0.50);
        if (d <= 0.0) {
            float r = max(0.0, fbm(pos_to_uvw(p, 0, boxsize)).r);
            float g = max(0.0, fbm(pos_to_uvw(p, 0, boxsize)).r);
            float b = max(0.0, fbm(pos_to_uvw(p, 0, boxsize)).r);
            if (r > 0.0) { col.r += r / s; }
            if (g > 0.0) { col.g += g / s; }
            if (b > 0.0) { col.b += b / s; }
        }
        t += m;
    }
    return float4(col.rgb, 1);
}