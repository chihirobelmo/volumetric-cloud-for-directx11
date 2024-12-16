#include "commonFunctions.hlsl"

//
// from https://www.shadertoy.com/view/4ttSWf
//

//==========================================================================================
// hashes (low quality, do NOT use in production)
//==========================================================================================

float hash1( float2 p )
{
    p  = 50.0*frac( p*0.3183099 );
    return frac( p.x*p.y*(p.x+p.y) );
}

float hash1( float n )
{
    return frac( n*17.0*frac( n*0.3183099 ) );
}

float2 hash2( float2 p ) 
{
    const float2 k = float2( 0.3183099, 0.3678794 );
    float n = 111.0*p.x + 113.0*p.y;
    return frac(n*frac(k*n));
}

//==========================================================================================
// noises
//==========================================================================================

// value noise, and its analytical derivatives
float4 noised( in float3 x )
{
    float3 p = floor(x);
    float3 w = frac(x);
    #if 1
    float3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    float3 du = 30.0*w*w*(w*(w-2.0)+1.0);
    #else
    float3 u = w*w*(3.0-2.0*w);
    float3 du = 6.0*w*(1.0-w);
    #endif

    float n = p.x + 317.0*p.y + 157.0*p.z;
    
    float a = hash1(n+0.0);
    float b = hash1(n+1.0);
    float c = hash1(n+317.0);
    float d = hash1(n+318.0);
    float e = hash1(n+157.0);
	float f = hash1(n+158.0);
    float g = hash1(n+474.0);
    float h = hash1(n+475.0);

    float k0 =   a;
    float k1 =   b - a;
    float k2 =   c - a;
    float k3 =   e - a;
    float k4 =   a - b - c + d;
    float k5 =   a - c - e + g;
    float k6 =   a - b - e + f;
    float k7 = - a + b + c - d + e - f - g + h;

    return float4( -1.0+2.0*(k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z), 
                      2.0* du * float3( k1 + k4*u.y + k6*u.z + k7*u.y*u.z,
                                      k2 + k5*u.z + k4*u.x + k7*u.z*u.x,
                                      k3 + k6*u.x + k5*u.y + k7*u.x*u.y ) );
}

float noise( in float3 x )
{
    float3 p = floor(x);
    float3 w = frac(x);
    
    #if 1
    float3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    #else
    float3 u = w*w*(3.0-2.0*w);
    #endif
    


    float n = p.x + 317.0*p.y + 157.0*p.z;
    
    float a = hash1(n+0.0);
    float b = hash1(n+1.0);
    float c = hash1(n+317.0);
    float d = hash1(n+318.0);
    float e = hash1(n+157.0);
	float f = hash1(n+158.0);
    float g = hash1(n+474.0);
    float h = hash1(n+475.0);

    float k0 =   a;
    float k1 =   b - a;
    float k2 =   c - a;
    float k3 =   e - a;
    float k4 =   a - b - c + d;
    float k5 =   a - c - e + g;
    float k6 =   a - b - e + f;
    float k7 = - a + b + c - d + e - f - g + h;

    return -1.0+2.0*(k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z);
}

float3 noised( in float2 x )
{
    float2 p = floor(x);
    float2 w = frac(x);
    #if 1
    float2 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    float2 du = 30.0*w*w*(w*(w-2.0)+1.0);
    #else
    float2 u = w*w*(3.0-2.0*w);
    float2 du = 6.0*w*(1.0-w);
    #endif
    
    float a = hash1(p+float2(0,0));
    float b = hash1(p+float2(1,0));
    float c = hash1(p+float2(0,1));
    float d = hash1(p+float2(1,1));

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k4 = a - b - c + d;

    return float3( -1.0+2.0*(k0 + k1*u.x + k2*u.y + k4*u.x*u.y), 
                 2.0*du * float2( k1 + k4*u.y,
                            k2 + k4*u.x ) );
}

float noise( in float2 x )
{
    float2 p = floor(x);
    float2 w = frac(x);
    #if 1
    float2 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    #else
    float2 u = w*w*(3.0-2.0*w);
    #endif

    float a = hash1(p+float2(0,0));
    float b = hash1(p+float2(1,0));
    float c = hash1(p+float2(0,1));
    float d = hash1(p+float2(1,1));
    
    return -1.0+2.0*(a + (b-a)*u.x + (c-a)*u.y + (a - b - c + d)*u.x*u.y);
}

//==========================================================================================
// fbm constructions
//==========================================================================================

const float3x3 m3  = float3x3( 0.00,  0.80,  0.60,
                      -0.80,  0.36, -0.48,
                      -0.60, -0.48,  0.64 );
const float3x3 m3i = float3x3( 0.00, -0.80, -0.60,
                       0.80,  0.36, -0.48,
                       0.60, -0.48,  0.64 );
const float2x2 m2 = float2x2(  0.80,  0.60,
                      -0.60,  0.80 );
const float2x2 m2i = float2x2( 0.80, -0.60,
                       0.60,  0.80 );

//------------------------------------------------------------------------------------------

float fbm_4( in float2 x )
{
    float f = 1.9;
    float s = 0.55;
    float a = 0.0;
    float b = 0.5;
    for( int i=0; i<4; i++ )
    {
        float n = noise(x);
        a += b*n;
        b *= s;
        x = mul(m2, x) * f;
    }
	return a;
}

float fbm_4( in float3 x )
{
    float f = 2.0;
    float s = 0.5;
    float a = 0.0;
    float b = 0.5;
    for( int i=0; i<4; i++ )
    {
        float n = noise(x);
        a += b*n;
        b *= s;
        x = mul(m3, x) * f;
    }
	return a;
}

float4 fbmd_7( in float3 x )
{
    float f = 1.92;
    float s = 0.5;
    float a = 0.0;
    float b = 0.5;
    float3  d = 0.0;
    float3x3  m = float3x3(1.0,0.0,0.0,
                   0.0,1.0,0.0,
                   0.0,0.0,1.0);
    for( int i=0; i<7; i++ )
    {
        float4 n = noised(x);
        a += b*n.x;          // accumulate values		
        d += mul(m,n.yzw)*b;      // accumulate derivatives
        b *= s;
        x = mul(m3, x) * f;
        m = f*m3i*m;
    }
	return float4( a, d );
}

/*
In the context of the fbmd_8 function, derivatives refer to the partial derivatives of the noise function with respect to the input coordinates. These derivatives provide information about the rate of change of the noise value in different directions, which can be useful for various applications such as normal mapping, procedural texture generation, and more.

Detailed Explanation:
Noise Function: The noised function returns a float4 where:

n.x is the noise value.
n.yzw are the partial derivatives of the noise with respect to x, y, and z.
Accumulating Derivatives:

The derivatives are accumulated in the d variable.
The mul(m, n.yzw) operation transforms the derivatives using the matrix m.
*/
float4 fbmd_8( in float3 x )
{
    float f = 2.0;
    float s = 0.65;
    float a = 0.0;
    float b = 0.5;
    float3  d = 0.0;
    float3x3  m = float3x3(1.0,0.0,0.0,
                   0.0,1.0,0.0,
                   0.0,0.0,1.0);
    for( int i=0; i<8; i++ )
    {
        float4 n = noised(x);
        a += b*n.x;          // accumulate values		
        if( i<4 ) {
            d += mul(m,n.yzw)*b;      // accumulate derivatives
        }
        b *= s;
        x = mul(m3,x)*f;
        m = f*m3i*m;
    }
	return float4( a, d );
}

float fbm_9( in float2 x )
{
    float f = 1.9;
    float s = 0.55;
    float a = 0.0;
    float b = 0.5;
    for( int i=0; i<9; i++ )
    {
        float n = noise(x);
        a += b*n;
        b *= s;
        x = mul(m2,x)*f;
    }
    
	return a;
}

float3 fbmd_9( in float2 x )
{
    float f = 1.9;
    float s = 0.55;
    float a = 0.0;
    float b = 0.5;
    float2  d = 0.0;
    float2x2  m = float2x2(1.0,0.0,0.0,1.0);
    for( int i=0; i<9; i++ )
    {
        float3 n = noised(x);
        a += b*n.x;          // accumulate values		
        d += b*mul(m,n.yz);       // accumulate derivatives
        b *= s;
        x = f*mul(m2,x);
        m = f*m2i*m;
    }

	return float3( a, d );
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

// Value noise function with derivatives
float4 valueNoise4d(float3 x) {

    float3 p = floor(x);
    float3 w = frac(x);

    float3 u = w * w * w * (w * (w * 6.0 - 15.0) + 10.0);
    float3 du = 30.0 * w * w * (w * (w - 2.0) + 1.0);

    float n = p.x + 317.0 * p.y + 157.0 * p.z;

    float a = hash1(n + 0.0);
    float b = hash1(n + 1.0);
    float c = hash1(n + 317.0);
    float d = hash1(n + 318.0);
    float e = hash1(n + 157.0);
    float f = hash1(n + 158.0);
    float g = hash1(n + 474.0);
    float h = hash1(n + 475.0);

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    float value = -1.0 + 2.0 * (k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z);
    float3 derivatives = 2.0 * du * float3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
                                           k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
                                           k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);

    return float4(value, derivatives);
}

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
float4 perlinFbm4d(float3 p, float freq, int octaves)
{
    float G = .5;
    float amp = 1.;
    float noise = 0.;
    float3  d = 0.0;
    float3x3  m = float3x3(1.0,0.0,0.0,
                   0.0,1.0,0.0,
                   0.0,0.0,1.0);
    for (int i = 0; i < octaves; ++i)
    {
        float4 n = valueNoise4d(p * freq);
        noise += amp * n.x;	
        if( i<4 ) {
            d += mul(m,n.yzw)*G; // accumulate derivatives
        }
        freq *= 2.;
        amp *= G;
    }
    
    return float4(noise, d);
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

float multiPerlin(float3 uvw, float freq_start, bool octaves) {

    float r = 0;
    float freq_r = 8;
    [loop]
    for (int i = freq_start; i < freq_start + freq_r; i++)
    {
        r += perlinFbm(uvw, pow(2, i), octaves) / freq_r;
    }
    return r;
}

float4 perlinFbmWithDerivatives(float3 uvw, float frequency, int octaves, float delta) {
    float r = multiPerlin(uvw, frequency, octaves);

    // Calculate derivatives using central differences
    float drdx = (multiPerlin(uvw + float3(delta, 0, 0), frequency, octaves) - multiPerlin(uvw - float3(delta, 0, 0), frequency, octaves)) / (2.0 * delta);
    float drdy = (multiPerlin(uvw + float3(0, delta, 0), frequency, octaves) - multiPerlin(uvw - float3(0, delta, 0), frequency, octaves)) / (2.0 * delta);
    float drdz = (multiPerlin(uvw + float3(0, 0, delta), frequency, octaves) - multiPerlin(uvw - float3(0, 0, delta), frequency, octaves)) / (2.0 * delta);

    return float4(r, normalize(float3(drdx, drdy, drdz)));
}

// Tileable Worley fbm inspired by Andrew Schneider's Real-Time Volumetric Cloudscapes
// chapter in GPU Pro 7.
float worleyFbm(float3 p, float freq, bool tileable)
{
    float fbm = worleyNoise(p * freq, freq, tileable) * .75 +
        	 	worleyNoise(p * freq * 2., freq * 2., tileable) * .25 +
        	 	worleyNoise(p * freq * 4., freq * 4., tileable) * .125;
    return max(0., fbm);
}

float multiWorley(float3 uvw, float freq_start, bool tileable) {

    float r = 0;
    float freq_r = 2;
    [loop]
    for (int i = freq_start; i < freq_start + freq_r; i++)
    {
        r += worleyFbm(uvw, pow(2, i), tileable) / freq_r;
    }
    return r;
}

float4 worleyFbmWithDerivatives(float3 uvw, float freq, int octaves, float delta) {
    float r = multiWorley(uvw, freq, true);

    // Calculate derivatives using central differences
    float drdx = (multiWorley(uvw + float3(delta, 0, 0), freq, true) - multiWorley(uvw - float3(delta, 0, 0), freq, true)) / (2.0 * delta);
    float drdy = (multiWorley(uvw + float3(0, delta, 0), freq, true) - multiWorley(uvw - float3(0, delta, 0), freq, true)) / (2.0 * delta);
    float drdz = (multiWorley(uvw + float3(0, 0, delta), freq, true) - multiWorley(uvw - float3(0, 0, delta), freq, true)) / (2.0 * delta);

    return float4(r, normalize(float3(drdx, drdy, drdz)));
}

float perlinWorley(float3 uvw, float freq, float octaves)
{
    float worley = worleyFbm(uvw, freq, true);
    float perlin = perlinFbm(uvw, freq, octaves);
    return remap(perlin, 1.0 - worley, 1.0, 0.0, 1.0);
}

float multiPerlinWorley(float3 uvw, float freq_start, float octaves) {

    float r = 0;
    float freq_r = 8;
    [loop]
    for (int i = freq_start; i < freq_start + freq_r; i++)
    {
        r += perlinWorley(uvw, pow(2, i), octaves) / freq_r;
    }
    return r;
}

float4 perlinWorleyWithDerivatives(float3 uvw, float freq, int octaves, float delta) {
    float r = multiPerlinWorley(uvw, freq, octaves);

    // Calculate derivatives using central differences
    float drdx = (multiPerlinWorley(uvw + float3(delta, 0, 0), freq, octaves) - multiPerlinWorley(uvw - float3(delta, 0, 0), freq, octaves)) / (2.0 * delta);
    float drdy = (multiPerlinWorley(uvw + float3(0, delta, 0), freq, octaves) - multiPerlinWorley(uvw - float3(0, delta, 0), freq, octaves)) / (2.0 * delta);
    float drdz = (multiPerlinWorley(uvw + float3(0, 0, delta), freq, octaves) - multiPerlinWorley(uvw - float3(0, 0, delta), freq, octaves)) / (2.0 * delta);

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