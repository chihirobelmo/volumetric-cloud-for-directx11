// references:
// - https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/
// - https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/
// - https://iquilezles.org/articles/distfunctions/
// - https://qiita.com/edo_m18/items/876f2857e67e26a053d6
// - https://wallisc.github.io/rendering/2020/05/02/Volumetric-Rendering-Part-1.html mostly from here
// - https://www.shadertoy.com/view/wssBR8

Texture3D noiseTexture : register(t1);
SamplerState noiseSampler : register(s1);

// performance tuning
#define MAX_STEPS 512
#define MAX_VOLUME_LIGHT_MARCH_STEPS 4

cbuffer CameraBuffer : register(b0) {
    matrix view;
    matrix projection;
    float4 cameraPosition;
    float aspectRatio;
    float cameraFov;
};

cbuffer EnvironmentBuffer : register(b1) {
    float4 lightDir;
    float4 lightColor;
    float4 cloudAreaPos;
    float4 cloudAreaSize;
};

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

// Get ray direction in world space
// Based on screen position and camera settings
// Screen position is in [-1,1] range
// Camera position is in world space
// Returns normalized direction
//
// Note: I wanted to use the camera's view matrix to get the direction, but I couldn't figure it out.
//
float3 GetRayDir_Frame(float2 screenPos) {

    // Get camera's right, up, and forward vectors
    //
    // Note: it may gimbal lock if camra is looking straight up or down
    //
    float3 forward = normalize(-cameraPosition);
    float3 right = normalize(cross(forward, float3(0,1,0)));
    float3 up = cross(right, forward);

    // Convert vertical FOV to horizontal using aspect ratio
    //
    // Todo: get fov from camera buffer
    //
    float verticalFOV = radians(cameraFov); // 80 degrees vertical
    float horizontalFOV = 2.0 * atan(tan(verticalFOV * 0.5) * (1.0 / aspectRatio));

    // Apply to screen position
    float horizontalAngle = screenPos.x * horizontalFOV * 0.5;
    float verticalAngle = screenPos.y * verticalFOV * 0.5;
    
    // Create direction using trigonometry
    float3 direction = forward;
    direction += right * tan(horizontalAngle);
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
    output.Pos = float4(input.Pos, 1.0f); // projPos for raster test.
    output.TexCoord = input.TexCoord;
    
    // Get ray direction in world space
    output.RayDir = GetRayDir_Frame(input.TexCoord * 2.0 - 1.0);

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

// Ray march through the volume
float4 RayMarch(float3 rayStart, float3 rayDir)
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

float4 PS(PS_INPUT input) : SV_Target {

    float3 ro = cameraPosition; // Ray origin
    float3 rd = normalize(input.RayDir); // Ray direction
    
    float4 cloud = RayMarch(ro, normalize(rd));

    return /*ambient*/float4(0,0.1,0.2,1.0) + cloud;
}