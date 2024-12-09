// from https://www.shadertoy.com/view/ttcSD8
// 
// converted to HLSL

/**
This tab contains all the necessary noise functions required to model a cloud shape.
*/

//-------------------------------------------------------------------------------------
//  Hash Functions
//-------------------------------------------------------------------------------------
    
// Hash functions by Dave_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uint2(UI0, UI1)
#define UI3 uint3(UI0, UI1, 2798796415U)
#define UIF (1. / float(0xffffffffU))

float3 hash33(float3 p)
{
	uint3 q = uint3(int3(p)) * UI3;
	q = (q.x ^ q.y ^ q.z)*UI3;
	return -1. + 2. * float3(q) * UIF;
}

float hash13(float3 p)
{
	uint3 q = uint3(int3(p)) * UI3;
	q *= UI3;
	uint n = (q.x ^ q.y ^ q.z) * UI0;
	return float(n) * UIF;
}

float hash12(float2 p)
{
	uint2 q = uint2(int2(p)) * UI2;
	uint n = (q.x ^ q.y) * UI0;
	return float(n) * UIF;
}

//-------------------------------------------------------------------------------------
// Noise generation
//-------------------------------------------------------------------------------------


float valueNoise(float3 x, float freq)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3. - 2. * f);
	
    return lerp(lerp(lerp(hash13(fmod(i + float3(0, 0, 0), freq)),  
                          hash13(fmod(i + float3(1, 0, 0), freq)), f.x),
                     lerp(hash13(fmod(i + float3(0, 1, 0), freq)),  
                          hash13(fmod(i + float3(1, 1, 0), freq)), f.x), f.y),
                lerp(lerp(hash13(fmod(i + float3(0, 0, 1), freq)),  
                          hash13(fmod(i + float3(1, 0, 1), freq)), f.x),
                     lerp(hash13(fmod(i + float3(0, 1, 1), freq)),  
                          hash13(fmod(i + float3(1, 1, 1), freq)), f.x), f.y), f.z);
}

// Tileable 3D worley noise
float worleyNoise(float3 uv, float freq, bool tileable)
{    
    float3 id = floor(uv);
    float3 p = frac(uv);
    float minDist = 10000.;
    
    for (float x = -1.; x <= 1.; ++x)
    {
        for(float y = -1.; y <= 1.; ++y)
        {
            for(float z = -1.; z <= 1.; ++z)
            {
                float3 offset = float3(x, y, z);
                float3 h = float3(0., 0., 0.);
                if (tileable)
                    h = hash33(fmod(id + offset, float3(freq, freq, freq))) * .4 + .3; // [.3, .7]
				else
                    h = hash33(id + offset) * .4 + .3; // [.3, .7]
    			h += offset;
            	float3 d = p - h;
           		minDist = min(minDist, dot(d, d));
            }
        }
    }
    
    // inverted worley noise
    return 1. - minDist;
}

// Fbm for Perlin noise based on iq's blog
float perlinFbm(float3 p, float freq, int octaves)
{
    float G = exp2(-.85);
    float amp = 1.;
    float noise = 0.;
    for (int i = 0; i < octaves; ++i)
    {
        noise += amp * valueNoise(p * freq, freq);
        freq *= 2.;
        amp *= G;
    }
    
    return noise;
}

// Tileable Worley fbm inspired by Andrew Schneider's Real-Time Volumetric Cloudscapes
// chapter in GPU Pro 7.
float worleyFbm(float3 p, float freq, bool tileable)
{
    float fbm = worleyNoise(p * freq, freq, tileable) * .625 +
        	 	worleyNoise(p * freq * 2., freq * 2., tileable) * .25 +
        	 	worleyNoise(p * freq * 4., freq * 4., tileable) * .125;
    return max(0., fbm * 1.1 - .1);
}

float remap2(float value, float original_min, float original_max, float new_min, float new_max)
{
    return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float perlinWorley(float3 uvw, float freq, float octaves)
{
    float worley = worleyFbm(uvw, freq, true);
    float perlin = perlinFbm(uvw, freq, octaves);
    return remap2(perlin, 1.0 - worley, 1.0, 0.0, 1.0);
}

// Blue noise generation using a simplified void-and-cluster approach
float blueNoise(float3 p, float freq) {
    float3 ip = floor(p * freq);
    float3 fp = frac(p * freq);
    
    // Random offset based on position
    float3 offset = hash33(ip);
    
    // Void and cluster distribution
    float noise = 0.0;
    float w = 1.0;
    
    [unroll]
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            for (int k = -1; k <= 1; k++) {
                float3 pos = float3(i, j, k) - fp;
                float3 cellOffset = hash33(ip + float3(i, j, k));
                
                // Distance-based weighting
                float dist = length(pos + (cellOffset - offset));
                float weight = exp(-4.0 * dist * dist);
                
                noise += weight;
                w += weight;
            }
        }
    }
    
    // Normalize and invert for blue noise characteristic
    return 1.0 - (noise / w);
}

// for 2d noise with smooth freq transition
// use this for 2d cloud map
//
// from https://www.shadertoy.com/view/3dSBRh
//
// converted to HLSL

float hash(float2 p, float t)
{
    float3 p3 = float3(p, t);
    p3  = frac(p3*0.1031);
    p3 += dot(p3, p3.zyx+31.32);
    return frac((p3.x+p3.y)*p3.z);
}

// manu210404's Improved Version
float noise(float2 p, float t)
{
    float4 b = float4(floor(p), ceil(p));
    float2 f = smoothstep(0.0, 1.0, frac(p));
    return lerp(lerp(hash(b.xy, t), hash(b.zy, t), f.x), lerp(hash(b.xw, t), hash(b.zw, t), f.x), f.y);
}

float2 rotate(float2 vec, float rot)
{
    float s = sin(rot), c = cos(rot);
    return float2(vec.x*c-vec.y*s, vec.x*s+vec.y*c);
}

// Fractal Brownian Motion Noise
float fbm(float2 pos, float scale, float num_octaves)
{
    float value = 0.0;
    float atten = 0.5;
    float t = 0.0;
    for(int i = 0; i < num_octaves; i++)
    {
        t += atten;
        value += noise(pos*scale, float(i))*atten;
        scale *= 2.0;
        atten *= 0.5;
        pos = rotate(pos, 0.125*3.1415);
    }
    return value/t * 2.0 - 1.0;
}