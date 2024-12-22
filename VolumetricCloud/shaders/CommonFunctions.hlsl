#ifndef COMMON_FUNCTIONS_HLSL
#define COMMON_FUNCTIONS_HLSL

float2 RaySphereIntersectForSunColor(
    float3 start, // starting position of the ray
    float3 dir, // the direction of the ray
    float radius // and the sphere radius
) {
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, start);
    float c = dot(start, start) - (radius * radius);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return float2(1e5,-1e5);
    return float2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

float3 CalculateSunlightColor(float3 sunDir) {
    sunDir.y *= -1.0; // Invert the Y-axis

    // Constants for atmospheric scattering
    const float3 rayleighCoeff = float3(0.0058, 0.0135, 0.0331); // Rayleigh scattering coefficients (R, G, B)
    const float3 mieCoeff = float3(0.0030, 0.0030, 0.0030);       // Mie scattering coefficients (R, G, B)
    const float rayleighScaleDepth = 0.25; // Rayleigh scattering scale
    const float mieScaleDepth = 0.1;       // Mie scattering scale

    // Sun altitude angle
    float sunAltitude = asin(sunDir.y); // Sun's altitude angle
    float sunZenithAngle = max(0.0, 1.0 - sunDir.y); // Zenith angle (0: sun overhead, 1: sun at the horizon)

    // Avoid division by zero near zenith
    float safeSunDirY = max(abs(sunDir.y), 0.01);

    // Approximation of air mass (amount of atmosphere sunlight passes through)
    float rayleighAirMass = exp(-safeSunDirY / rayleighScaleDepth) / (safeSunDirY + 0.15 * pow(93.885 - sunZenithAngle * 180.0 / 3.14159, -1.253));
    float mieAirMass = exp(-safeSunDirY / mieScaleDepth) / (safeSunDirY + 0.15 * pow(93.885 - sunZenithAngle * 180.0 / 3.14159, -1.253));

    // Attenuation of sunlight due to scattering
    float3 rayleighAttenuation = exp(-rayleighCoeff * rayleighAirMass);
    float3 mieAttenuation = exp(-mieCoeff * mieAirMass);

    // Sunlight color (applying scattering attenuation)
    float3 sunlightColor = float3(1.0, 1.0, 1.0) * rayleighAttenuation * mieAttenuation;

    float3 pos = float3(0, 5000 + 6371e3, 0); // Ray origin
    float2 planet_intersect = RaySphereIntersectForSunColor(pos - /*earth position*/0.0, sunDir, /*earth radius*/6371e3); 

    float4 color = float4(sunlightColor, 1e12);
    // if the ray hit the planet, set the max distance to that ray
    if (0.0 < planet_intersect.y) {
    	color.w = max(planet_intersect.x, 0.0);
        
        // sample position, where the pixel is
        float3 sample_pos = pos + (sunDir * planet_intersect.x) - 6371e3;
        
        // and the surface normal
        float3 surface_normal = normalize(sample_pos);
        
        // get the color of the sphere
        color.xyz = float3(0.5, 0.5, 0.5); 
        
        // get wether this point is shadowed, + how much light scatters towards the camera according to the lommel-seelinger law
        float3 N = surface_normal;
        float3 L = sunDir;
        float dotNL = max(1e-6, dot(N, L));
        float shadow = dotNL;
        
        // apply the shadow
        color.xyz *= shadow;
    }

    return color;
}

// from https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine

float Remap(float value, float original_min, float original_max, float new_min, float new_max)
{
    return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float RemapClamp(float value, float original_min, float original_max, float new_min, float new_max)
{        
    // completly set out range value to 0
    return min(max(new_max, new_min), max(min(new_max, new_min), Remap(value, original_min, original_max, new_min, new_max) ) );
}

float RemapNormalize(float value, float original_min, float original_max, float new_min, float new_max)
{        
    // completly set out range value to 0
    return min(1.0, max(0.0, Remap(value, original_min, original_max, new_min, new_max) ) );
}


#define REMAP(a,b) Remap(a, 0.0, 1.0, 0.0, b)
// minus value becomes 0
#define UFLOAT(a) max(a, 0.0)
// cutoff below threashould 0.0
#define CUTOFF(a,threshold) step(threshold, a) * a
// cutoff below threashould1 and beyond threashould2 0.0
#define CUTOFF_BOTH(a,threshold1,threshould2) step(threshold1, a) * step(a, threshould2)
#define SMOOTH_CUTOFF(a,threshold) smoothstep(0, threshold, a) * a
#define DISTANCE_CLOUD(pos,botoom,thickness) min(abs(pos - botoom), abs(pos - botoom) - thickness)
#define HIGH_CONTRAST(a) max(0.0, a * 2.0 - 1.0)

float4 FetchAndInterpolateFMapTexture(Texture2D<uint4> tex, float2 uv, int2 textureSize) {
    // Convert UV coordinates to integer texture coordinates
    float2 texCoords = uv * textureSize;
    int2 texCoordsInt = int2(floor(texCoords));
    float2 fracCoords = frac(texCoords);

    // Fetch the values from the neighboring texels
    uint4 f00 = tex.Load(int3(texCoordsInt, 0));
    uint4 f10 = tex.Load(int3(texCoordsInt + int2(1, 0), 0));
    uint4 f01 = tex.Load(int3(texCoordsInt + int2(0, 1), 0));
    uint4 f11 = tex.Load(int3(texCoordsInt + int2(1, 1), 0));

    // Convert the unsigned integer values to float
    float4 f00f = float4(f00);
    float4 f10f = float4(f10);
    float4 f01f = float4(f01);
    float4 f11f = float4(f11);

    // Perform bilinear interpolation
    float4 result = lerp(lerp(f00f, f10f, fracCoords.x), lerp(f01f, f11f, fracCoords.x), fracCoords.y);

    return result;
}


#endif // COMMON_FUNCTIONS_HLSL