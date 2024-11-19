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

float3 pos_to_uvw(float3 pos, float3 size) {
    return pos * (1.0 / (cloudAreaSize.x * size));
}

float fbm_from_tex(float3 pos) {
    // value input expected within -1 to +1
    return noiseTexture.Sample(noiseSampler, pos_to_uvw(pos.xzy, 0.25)).r * 0.5
         + noiseTexture.Sample(noiseSampler, pos_to_uvw(pos.xzy, 0.10)).r * 0.5;
        //  + noiseTexture.Sample(noiseSampler, pos_to_uvw(pos.xzy, 0.50)).g * 0.25
        //  + noiseTexture.Sample(noiseSampler, pos_to_uvw(pos.xzy, 0.25)).b * 0.25
        //  + noiseTexture.Sample(noiseSampler, pos_to_uvw(pos.xzy, 0.12)).a * 0.25;
}

struct VS_INPUT {
    float3 Pos : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 RayDir : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

struct PS_OUTPUT {
    float4 Color : SV_TARGET;
    float Depth : SV_Depth;
};

// Function to extract FOV from projection matrix
float2 ExtractFOVFromProjectionMatrix(float4x4 projectionMatrix) {
    float verticalFOV = 2.0 * atan(1.0 / projectionMatrix[1][1]);
    float horizontalFOV = 2.0 * atan(1.0 / projectionMatrix[0][0]);
    return float2(horizontalFOV, verticalFOV);
}

float3 GetRightFromView(matrix view) {
    return normalize(float3(view._11, view._21, view._31));
}

float3 GetUpFromView(matrix view) {
    return normalize(float3(view._12, view._22, view._32));
}

float3 GetForwardFromView(matrix view) {
    return normalize(float3(view._13, view._23, view._33));
}

// Get ray direction in world space
// Based on screen position and camera settings
// Screen position is in [-1,1] range
// Camera position is in world space
// Returns normalized direction
float3 GetRayDir_Frame(float2 screenPos, float4x4 projectionMatrix) {

    // Extract FOV from projection matrix
    float2 fov = ExtractFOVFromProjectionMatrix(projectionMatrix);

    // Extract forward, right, and up vectors from the view matrix
    float3 forward = GetForwardFromView(view);
    float3 right = GetRightFromView(view);
    float3 up = GetUpFromView(view);

    // Apply to screen position
    float horizontalAngle = -screenPos.x * fov.x * 0.5;
    float verticalAngle = -screenPos.y * fov.y * 0.5;
    
    // Create direction using trigonometry
    float3 direction = forward;
    direction += -right * tan(horizontalAngle);
    direction += up * tan(verticalAngle);
    
    return normalize(direction);
}

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;

    // Transform to get projection space position
    float4 worldPos = float4(input.Pos, 1.0f);
    float4 viewPos = mul(worldPos, view);
    float4 projPos = mul(viewPos, projection);
    
    // Keep position for raster
    output.Pos = float4(input.Pos, 1.0f); // do not use proj position here. because its ray marching
    output.TexCoord = input.TexCoord;
    
    // Get ray direction in world space
    output.RayDir = GetRayDir_Frame(input.TexCoord * 2.0 - 1.0, projection);

    return output;
}

float HeightGradient(float height, float cloudBottom, float cloudTop) 
{
    // Wider transition zones for smoother gradients
    float bottomFade = smoothstep(cloudBottom, cloudBottom - cloudAreaSize.y * 0.3, height);
    float topFade = 1.0 - smoothstep(cloudTop + cloudAreaSize.y * 0.3, cloudTop, height);
    return bottomFade * topFade;
}

float DensityFunction(float noise, float3 position) 
{
    // Fix height calculations - bottom to top
    float cloudBottom = cloudAreaPos.y + cloudAreaSize.y * 0.5;  // Lower boundary
    float cloudTop = cloudAreaPos.y - cloudAreaSize.y * 0.5;     // Upper boundary
    
    float heightGradient = HeightGradient(position.y, cloudBottom, cloudTop);
    
    // Increase base density and use noise directly
    float density = max(-noise, 0.0) * heightGradient;

    // Add variation with larger scale influence
    // density *= (1.0 + 0.2 * fbm_from_tex(position * 0.005));
    
    // Ensure positive density
    return max(density, 0.0);
}

// Ray-box intersection
float2 CloudBoxIntersection(float3 rayStart, float3 rayDir, float3 BoxArea )
{
	float3 invraydir = 1 / rayDir;

	float3 firstintersections = (-BoxArea - rayStart) * invraydir;
	float3 secondintersections = (+BoxArea - rayStart) * invraydir;
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
    // Exponential curve parameters
    float x = float(stepIndex) / MAX_STEPS;  // Normalize to [0,1]
    float base = 2.71828;  // e
    float exponent = 1.0;  // Controls curve steepness
    
    // Exponential curve: smaller steps at start, larger at end
    float curve = (pow(base, x * exponent) - 1.0) / (base - 1.0);
    
    // Scale to ensure total distance is covered
    return curve * (cloudAreaSize.x / MAX_STEPS);
}

float LinearizeDepth(float depth)
{
    // These values should match your camera's near and far planes
    float n = 0.1; // near plane
    float f = 1000.0; // far plane
    return (2.0 * n) / (f + n - depth * (f - n));
}

// Ray march through the volume
float4 RayMarch(float3 rayStart, float3 rayDir, float depth, out float rayDepth)
{
    // Scattering in RGB, transmission in A
    float4 intScattTrans = float4(0, 0, 0, 1);

    // Check if ray intersects the cloud box
    float2 boxint = CloudBoxIntersection(rayStart, rayDir, cloudAreaPos.xyz + cloudAreaSize.xyz * 0.5);
    if (boxint.x >= boxint.y) { return float4(0, 0, 0, 1); }
    boxint.x = max(0, boxint.x);

    // Calculate the offset of the intersection point from the box
	float planeoffset = 1-frac( ( boxint.x - length(rayDir) ) * MAX_STEPS );
    float t = boxint.x + (planeoffset / MAX_STEPS);
    
    // Ray march size
    float rayMarchSize = 1.00;
    bool hit = false;
    rayDepth = 0.0;

    // Ray march
    [loop]
    for (int u = 0; u < MAX_STEPS; u++)
    {   
        // Get the march size for the current step
        float marchSize = GetMarchSize(u);

        // Current ray position
        float3 rayPos = rayStart + rayDir * t;

        // Evaluate our signed distance field at the current ray position
        float sdf = fbm_from_tex(rayPos);

        // Check if we're inside the volume
        if (sdf < 0.0) {

            if (!hit) {
                hit = true;
                float4 viewPos = mul(float4(rayPos, 1.0), view); // Transform to view space
                float4 projPos = mul(viewPos, projection); // Transform to clip space
                rayDepth = projPos.z / projPos.w; // Perspective divide to get NDC z-value
                //rayDepth = LinearizeDepth(rayDepth); // Transform to linear depth
            }

            // transmittance
            half extinction = DensityFunction(sdf, rayPos);// max(-sdf, 0);
            half transmittance = exp(-extinction * rayMarchSize);  // Beer Lambert Law  

            // light ray marching setups
            float t2 = 0.0f;
            float lightVisibility = 1.0f;
            float sdf2 = 0.0;

            // light ray march
            for (int v = 0; v < MAX_VOLUME_LIGHT_MARCH_STEPS; v++) 
            {
                t2 += rayMarchSize;
                float3 rayPos2 = rayPos + t2 * lightDir.xyz;
                sdf2 = fbm_from_tex(rayPos2);

                // Check if we're inside the volume
                if(sdf2 < 0.0)
                {
                    float fogDensity = max(-sdf2,0.0);
                    // Beer Lambert Law
                    lightVisibility *= exp(-fogDensity * rayMarchSize);
                }
            }

            // Get the luminance for the current ray position
            half3 luminance = lightVisibility;

            // Integrate scattering
            half3 integScatt = luminance - luminance * transmittance;
            intScattTrans.rgb += integScatt * intScattTrans.a;
            intScattTrans.a *= transmittance;
        }

        // Opaque check
        if (intScattTrans.a < 0.03)
        {
            intScattTrans.a = 0.0;
            break;
        }

        // March forward; step size depends on if we're inside the volume or not
        t += marchSize;

        // Check if we've reached the end of the box
        if (t >= boxint.y) {
            break;
        }
    }

    // Return the accumulated scattering and transmission
    return float4(intScattTrans.rgb, 1-intScattTrans.a);
}

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    float3 ro = cameraPosition; // Ray origin
    float3 rd = normalize(input.RayDir); // Ray direction
    
    float depth = depthTexture.Sample(depthSampler, input.TexCoord).r;
    float rayDepth = 0.0;
    float4 cloud = RayMarch(ro, normalize(rd), depth, rayDepth);

    output.Color = cloud;
    output.Depth = rayDepth;

    return output;
}