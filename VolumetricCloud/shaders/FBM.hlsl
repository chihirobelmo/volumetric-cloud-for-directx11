#include "commonFunctions.hlsl"

#ifndef FBM_HLSL
#define FBM_HLSL

/// BY ChatGPT code generation based on existing noise functions

static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;

uint hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

float hashToFloat(uint x)
{
    return (x & 0x00FFFFFF) / 16777216.0;
}

uint hash3(uint3 p)
{
    return hash(p.x ^ hash(p.y ^ hash(p.z)));
}

uint hash4(uint4 p)
{
    return hash(p.x ^ hash(p.y ^ hash(p.z ^ hash(p.w))));
}

float hash01(uint x)
{
    return (x & 0x00FFFFFF) / 16777216.0;
}

float fade(float t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float4 fade(float4 t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float3 grad(uint3 p)
{
    float h = hashToFloat(hash3(p)) * 6.2831853;
    return float3(cos(h), sin(h), cos(h * 0.37));
}

float4 grad4(uint4 p)
{
    float h1 = hash01(hash4(p));
    float h2 = hash01(hash4(p + 11));
    float h3 = hash01(hash4(p + 37));

    float a = h1 * TWO_PI;
    float b = h2 * TWO_PI;
    float c = h3 * TWO_PI;

    float4 g;
    g.x = cos(a);
    g.y = sin(a);
    g.z = cos(b);
    g.w = sin(b);

    return normalize(g);
}

float Perlin4D(float4 p)
{
    int4 i0 = (int4)floor(p);
    int4 i1 = i0 + 1;

    float4 f = frac(p);
    float4 u = fade(f);

    float n0000 = dot(grad4(i0), f);
    float n1000 = dot(grad4(int4(i1.x,i0.y,i0.z,i0.w)), f - float4(1,0,0,0));
    float n0100 = dot(grad4(int4(i0.x,i1.y,i0.z,i0.w)), f - float4(0,1,0,0));
    float n1100 = dot(grad4(int4(i1.x,i1.y,i0.z,i0.w)), f - float4(1,1,0,0));

    float n0010 = dot(grad4(int4(i0.x,i0.y,i1.z,i0.w)), f - float4(0,0,1,0));
    float n1010 = dot(grad4(int4(i1.x,i0.y,i1.z,i0.w)), f - float4(1,0,1,0));
    float n0110 = dot(grad4(int4(i0.x,i1.y,i1.z,i0.w)), f - float4(0,1,1,0));
    float n1110 = dot(grad4(int4(i1.x,i1.y,i1.z,i0.w)), f - float4(1,1,1,0));

    float n0001 = dot(grad4(int4(i0.x,i0.y,i0.z,i1.w)), f - float4(0,0,0,1));
    float n1001 = dot(grad4(int4(i1.x,i0.y,i0.z,i1.w)), f - float4(1,0,0,1));
    float n0101 = dot(grad4(int4(i0.x,i1.y,i0.z,i1.w)), f - float4(0,1,0,1));
    float n1101 = dot(grad4(int4(i1.x,i1.y,i0.z,i1.w)), f - float4(1,1,0,1));

    float n0011 = dot(grad4(int4(i0.x,i0.y,i1.z,i1.w)), f - float4(0,0,1,1));
    float n1011 = dot(grad4(int4(i1.x,i0.y,i1.z,i1.w)), f - float4(1,0,1,1));
    float n0111 = dot(grad4(int4(i0.x,i1.y,i1.z,i1.w)), f - float4(0,1,1,1));
    float n1111 = dot(grad4(i1), f - 1.0);

    float nx000 = lerp(n0000, n1000, u.x);
    float nx100 = lerp(n0100, n1100, u.x);
    float nx010 = lerp(n0010, n1010, u.x);
    float nx110 = lerp(n0110, n1110, u.x);

    float nx001 = lerp(n0001, n1001, u.x);
    float nx101 = lerp(n0101, n1101, u.x);
    float nx011 = lerp(n0011, n1011, u.x);
    float nx111 = lerp(n0111, n1111, u.x);

    float nxy00 = lerp(nx000, nx100, u.y);
    float nxy10 = lerp(nx010, nx110, u.y);
    float nxy01 = lerp(nx001, nx101, u.y);
    float nxy11 = lerp(nx011, nx111, u.y);

    float nxyz0 = lerp(nxy00, nxy10, u.z);
    float nxyz1 = lerp(nxy01, nxy11, u.z);

    return lerp(nxyz0, nxyz1, u.w) * 0.5 + 0.5;
}

float PerlinPeriodic(float3 p, int frequency = 1)
{
    float3 a = p * frequency * TWO_PI;

    float4 p4;
    p4.x = cos(a.x);
    p4.y = sin(a.x);
    p4.z = cos(a.y);
    p4.w = sin(a.y);

    float n1 = Perlin4D(p4 + float4(0,0, cos(a.z), sin(a.z)));
    float n2 = Perlin4D(p4 + float4(0,0, cos(a.z + 1.7), sin(a.z + 1.7)));

    return 1.0 - (n1 + n2) * 0.5;
}

float WorleyPeriodic(float3 p, int frequency)
{
    p *= frequency;

    int3 cell = (int3)floor(p);
    float3 f = frac(p);

    float minDist = 1e6;

    [fastopt]
    for (int z = -1; z <= 1; z++)
    [fastopt]
    for (int y = -1; y <= 1; y++)
    [fastopt]
    for (int x = -1; x <= 1; x++)
    {
        int3 c = (cell + int3(x,y,z) + frequency) % frequency;
        uint3 cu = (uint3)c;

        float3 rand = float3(
            hashToFloat(hash3(cu + 1)),
            hashToFloat(hash3(cu + 2)),
            hashToFloat(hash3(cu + 3))
        );

        float3 d = float3(x,y,z) + rand - f;
        minDist = min(minDist, dot(d,d));
    }

    return 1.0 - sqrt(minDist);
}

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

// High-Resolution Noise Function
float3 hash33h(float3 p) {
    p = fmod(p, 289.0);
    p = p * 0.1031;
    p = p - floor(p);
    p = p * (p + 33.33);
    p = p - floor(p);
    return -1.0 + 2.0 * p;
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

float gradientNoise(float3 p) {
    float3 i = floor(p);
    float3 f = frac(p);

    float3 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(lerp(dot(hash33h(i + float3(0, 0, 0)), f - float3(0, 0, 0)),
                          dot(hash33h(i + float3(1, 0, 0)), f - float3(1, 0, 0)), u.x),
                     lerp(dot(hash33h(i + float3(0, 1, 0)), f - float3(0, 1, 0)),
                          dot(hash33h(i + float3(1, 1, 0)), f - float3(1, 1, 0)), u.x), u.y),
                lerp(lerp(dot(hash33h(i + float3(0, 0, 1)), f - float3(0, 0, 1)),
                          dot(hash33h(i + float3(1, 0, 1)), f - float3(1, 0, 1)), u.x),
                     lerp(dot(hash33h(i + float3(0, 1, 1)), f - float3(0, 1, 1)),
                          dot(hash33h(i + float3(1, 1, 1)), f - float3(1, 1, 1)), u.x), u.y), u.z);
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

// https://www.shadertoy.com/view/3d3fWN
float worley(float3 p, float scale){

    float3 id = floor(p*scale);
    float3 fd = frac(p*scale);

    float n = 0.;

    float minimalDist = 1.;


    for(float x = -1.; x <=1.; x++){
        for(float y = -1.; y <=1.; y++){
            for(float z = -1.; z <=1.; z++){

                float3 coord = float3(x,y,z);
                float3 rId = hash33(fmod(id+coord,scale))*0.5+0.5;

                float3 r = coord + rId - fd; 

                float d = dot(r,r);

                if(d < minimalDist){
                    minimalDist = d;
                }

            }//z
        }//y
    }//x
    
    return 1.0-minimalDist;
}

// Fbm for Perlin noise based on iq's blog
float perlinFbm(float3 p, float freq, int octaves)
{
    float G = .5;
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

float4 perlinFbmWithDerivatives(float3 uvw, float frequency, int octaves, float delta) {
    float r = perlinFbm(uvw, frequency, octaves);

    // Calculate derivatives using central differences
    float drdx = (perlinFbm(uvw + float3(delta, 0, 0), frequency, octaves) - perlinFbm(uvw - float3(delta, 0, 0), frequency, octaves)) / (2.0 * delta);
    float drdy = (perlinFbm(uvw + float3(0, delta, 0), frequency, octaves) - perlinFbm(uvw - float3(0, delta, 0), frequency, octaves)) / (2.0 * delta);
    float drdz = (perlinFbm(uvw + float3(0, 0, delta), frequency, octaves) - perlinFbm(uvw - float3(0, 0, delta), frequency, octaves)) / (2.0 * delta);

    return float4(r, normalize(float3(drdx, drdy, drdz)));
}

// Tileable Worley fbm inspired by Andrew Schneider's Real-Time Volumetric Cloudscapes
// chapter in GPU Pro 7.
float worleyFbm(float3 p, float freq, bool tileable)
{
    float fbm = WorleyPeriodic(p, freq) * .75 +
        	 	WorleyPeriodic(p * 2., freq) * .25 +
        	 	WorleyPeriodic(p * 4., freq) * .125;
    return max(0., fbm) * 1.5;
    // float fbm = worleyNoise(p * freq, freq, tileable) * .75 +
    //     	 	worleyNoise(p * freq * 2., freq * 2., tileable) * .25 +
    //     	 	worleyNoise(p * freq * 4., freq * 4., tileable) * .125;
    // return max(0., fbm);
}

float4 worleyFbmWithDerivatives(float3 uvw, float freq, int octaves, float delta) {
    float r = worleyFbm(uvw, freq, true);

    // Calculate derivatives using central differences
    float drdx = (worleyFbm(uvw + float3(delta, 0, 0), freq, true) - worleyFbm(uvw - float3(delta, 0, 0), freq, true)) / (2.0 * delta);
    float drdy = (worleyFbm(uvw + float3(0, delta, 0), freq, true) - worleyFbm(uvw - float3(0, delta, 0), freq, true)) / (2.0 * delta);
    float drdz = (worleyFbm(uvw + float3(0, 0, delta), freq, true) - worleyFbm(uvw - float3(0, 0, delta), freq, true)) / (2.0 * delta);

    return float4(r, normalize(float3(drdx, drdy, drdz)));
}

float perlinWorley(float3 uvw, float freq, float octaves)
{
    float worley = worleyFbm(uvw, freq, true);
    float perlin = perlinFbm(uvw, freq, octaves);
    return Remap(perlin, 1.0 - worley, 1.0, 0.0, 1.0);
}

float4 perlinWorleyWithDerivatives(float3 uvw, float freq, int octaves, float delta) {
    float r = perlinWorley(uvw, freq, octaves) * .33;

    // Calculate derivatives using central differences
    float drdx = (perlinWorley(uvw + float3(delta, 0, 0), freq, octaves) - perlinWorley(uvw - float3(delta, 0, 0), freq, octaves)) / (2.0 * delta);
    float drdy = (perlinWorley(uvw + float3(0, delta, 0), freq, octaves) - perlinWorley(uvw - float3(0, delta, 0), freq, octaves)) / (2.0 * delta);
    float drdz = (perlinWorley(uvw + float3(0, 0, delta), freq, octaves) - perlinWorley(uvw - float3(0, 0, delta), freq, octaves)) / (2.0 * delta);

    return float4(r, normalize(float3(drdx, drdy, drdz)));
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

float Fbm2dHash(float2 p, float t)
{
    float3 p3 = float3(p, t);
    p3  = frac(p3*0.1031);
    p3 += dot(p3, p3.zyx+31.32);
    return frac((p3.x+p3.y)*p3.z);
}

// manu210404's Improved Version
float Fbm2dNoise(float2 p, float t)
{
    float4 b = float4(floor(p), ceil(p));
    float2 f = smoothstep(0.0, 1.0, frac(p));
    return lerp(lerp(Fbm2dHash(b.xy, t), Fbm2dHash(b.zy, t), f.x), lerp(Fbm2dHash(b.xw, t), Fbm2dHash(b.zw, t), f.x), f.y);
}

float2 FbmRotate(float2 vec, float rot)
{
    float s = sin(rot), c = cos(rot);
    return float2(vec.x*c-vec.y*s, vec.x*s+vec.y*c);
}

// Fractal Brownian Motion Noise
float Fbm2d(float2 pos, float scale, float num_octaves)
{
    float value = 0.0;
    float atten = 0.5;
    float t = 0.0;
    for(int i = 0; i < num_octaves; i++)
    {
        t += atten;
        value += Fbm2dNoise(pos*scale, float(i))*atten;
        scale *= 2.0;
        atten *= 0.5;
        pos = FbmRotate(pos, 0.125*3.1415);
    }
    return value/t * 2.0 - 1.0;
}


//////////////////////////////////////////////////////////////////////////
// blue noise from https://www.shadertoy.com/view/ssBBW1

uint HilbertIndex(uint2 p) {
    uint i = 0u;
    for(uint l = 0x4000u; l > 0u; l >>= 1u) {
        uint2 r = min(p & l, 1u);
        
        i = (i << 2u) | ((r.x * 3u) ^ r.y);       
        p = r.y == 0u ? (0x7FFFu * r.x) ^ p.yx : p;
    }
    return i;
}

uint ReverseBits(uint x) {
    x = ((x & 0xaaaaaaaau) >> 1) | ((x & 0x55555555u) << 1);
    x = ((x & 0xccccccccu) >> 2) | ((x & 0x33333333u) << 2);
    x = ((x & 0xf0f0f0f0u) >> 4) | ((x & 0x0f0f0f0fu) << 4);
    x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
    return (x >> 16) | (x << 16);
}

uint OwenHash(uint x, uint seed) { // seed is any random number
    x ^= x * 0x3d20adeau;
    x += seed;
    x *= (seed >> 16) | 1u;
    x ^= x * 0x05526c56u;
    x ^= x * 0x53a22864u;
    return x;
}

float blueNoise(uint2 fragCoord, float seed) {
    uint m = HilbertIndex(fragCoord);     // map pixel coords to hilbert curve index
    m = OwenHash(ReverseBits(m), 0xe7843fbfu + seed);   // owen-scramble hilbert index
    m = OwenHash(ReverseBits(m), 0x8d8fb1e0u + seed);   // map hilbert index to sobol sequence and owen-scramble
    return float(ReverseBits(m)) / 4294967296.0; // convert to float
}
//////////////////////////////////////////////////////////////////////////

#endif // FBM_HLSL