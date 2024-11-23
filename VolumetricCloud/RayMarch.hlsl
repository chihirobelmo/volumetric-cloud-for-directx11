// references:
// - https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/
// - https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/
// - https://iquilezles.org/articles/distfunctions/
// - https://qiita.com/edo_m18/items/876f2857e67e26a053d6
// - https://wallisc.github.io/rendering/2020/05/02/Volumetric-Rendering-Part-1.html mostly from here
// - https://www.shadertoy.com/view/wssBR8

SamplerState depthSampler : register(s0);
SamplerState noiseSampler : register(s1);

Texture2D depthTexture : register(t0);
Texture3D noiseTexture : register(t1);

// performance tuning
#define MAX_STEPS 512
#define MAX_VOLUME_LIGHT_MARCH_STEPS 4

#include "CommonBuffer.hlsl"

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
// then you canget ray direction with below function.

// Get ray direction in world space
// Based on screen position and camera settings
// Screen position is in [-1,1] range
// Camera position is in world space
// Returns normalized direction
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

float HeightGradient___NotUsed(float height, float cloudBottom, float cloudTop, float3 areaSize) 
{
    // Wider transition zones for smoother gradients
    float bottomFade = smoothstep(cloudBottom, cloudBottom - areaSize.y * 0.3, height);
    float topFade = 1.0 - smoothstep(cloudTop + areaSize.y * 0.3, cloudTop, height);
    return bottomFade * topFade;
}

float ExtinctionFunction___NotUsed(float density, float3 position, float3 areaPos, float3 areaSize) 
{
    // Fix height calculations - bottom to top
    float cloudBottom = areaPos.y + areaSize.y * 0.5;  // Lower boundary
    float cloudTop = areaPos.y - areaSize.y * 0.5;     // Upper boundary
    
    float heightGradient = HeightGradient___NotUsed(position.y, cloudBottom, cloudTop, areaSize);
    
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

float4 fbm(float3 pos) {
    // value input expected within -1 to +1
    return noiseTexture.Sample(noiseSampler, pos);
}

// Ray-box intersection
float2 CloudBoxIntersection(float3 rayStart, float3 rayDir, float3 BoxPos, float3 BoxArea )
{
	float3 invraydir = 1 / rayDir;

	float3 firstintersections = (BoxPos - BoxArea - rayStart) * invraydir;
	float3 secondintersections = (BoxPos + BoxArea - rayStart) * invraydir;
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

float sdBox( float3 p, float3 b )
{
  float3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

// Get the march size for the current step
// As we get closer to the end, the steps get larger
float GetMarchSize(int stepIndex, float maxLength) {
    // Exponential curve parameters
    float x = float(stepIndex) / MAX_STEPS;  // Normalize to [0,1]
    float base = 2.71828;  // e
    float exponent = 1.0;  // Controls curve steepness
    
    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);
    
    // Scale to ensure total distance is covered
    return curve * (maxLength / MAX_STEPS);
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

float CloudDensity(float3 pos) {
    return fbm(pos).r;
}

// to check 3d texture
float4 RayMarch(float3 rayStart, float3 rayDir, float primDepthMeter, out float cloudDepth) {
    
    float3 boxPos = cloudAreaPos.xyz;
    float3 boxSize = cloudAreaSize.xyz;// float3(1000,200,1000);

    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);
    cloudDepth = 0;

    // Check if ray intersects the cloud box
    float2 startEnd = CloudBoxIntersection(rayStart, rayDir, boxPos, boxSize * 0.5);
    if (startEnd.x >= startEnd.y) { return float4(0, 0, 0, 0); } // No intersection

    // Clamp the intersection points, if intersect primitive earlier stop ray there
    startEnd.x = max(0, startEnd.x);
    startEnd.y = min(primDepthMeter, startEnd.y);

    // Calculate the offset of the intersection point from the box
    // start from box intersection point
	float planeoffset = 1 - frac( ( startEnd.x - length(rayDir) ) * MAX_STEPS );
    float rayTrans = startEnd.x + (planeoffset / MAX_STEPS);
    
    // light ray marching setups
    float lightMarchSize = 1.0;
    float marchLength = (startEnd.y - startEnd.x) / MAX_STEPS;

    [loop]
    for (int i = 0; i < MAX_STEPS; i++) {

        // Translate the ray position each iterate
        float3 rayPos = rayStart + rayDir * rayTrans;

        // Get the density at the current position
        float3 uvw = pos_to_uvw(rayPos, 0, boxSize);
        float dense = CloudDensity(uvw);

        // Calculate the scattering and transmission
        if (dense > 0.0) {

            float transmittance = BeerLambertLaw(UnsignedDensity(dense), marchLength);
            float lightVisibility = 1.0f;

            // light ray march
            [loop]
            for (int v = 0; v < MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
            {
                float3 rayPos2 = rayPos + v * lightDir.xyz * lightMarchSize;

                float3 uvw2 = pos_to_uvw(rayPos2, 0, boxSize);
                float dense2 = CloudDensity(uvw2);

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

        rayTrans += marchLength;

        // Break if we're outside the box or intersect the primitive
        if (rayTrans > startEnd.y) { break; }
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
    float2 pixelPos = screenPos / 512/*resolution for raymarch*/;

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

    output.Color = cloud;
    output.DepthColor = cloudDepth;
    output.Depth = cloudDepth;

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