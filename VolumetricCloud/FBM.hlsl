// from https://www.shadertoy.com/view/3dVXDc
// converted to HLSL

/**
This tab contains all the necessary noise functions required to model a cloud shape.
*/

// Hash by David_Hoskins
#define UI0 1597334673u
#define UI1 3812015801u
#define UIF (1.0 / float(0xffffffffu))

float3 hash33(float3 p) {
    uint3 q = uint3(int3(p)) * uint3(UI0, UI1, 2798796415u);
    q = (q.x ^ q.y ^ q.z) * uint3(UI0, UI1, 2798796415u);
    return -1.0 + 2.0 * float3(q) * UIF;
}

float remap(float x, float a, float b, float c, float d) {
    return (((x - a) / (b - a)) * (d - c)) + c;
}

// Gradient noise by iq (modified to be tileable)
float gradientNoise(float3 x, float freq) {
    // grid
    float3 p = floor(x);
    float3 w = frac(x);
    
    // quintic interpolant
    float3 u = w * w * w * (w * (w * 6.0 - 15.0) + 10.0);
    
    // gradients
    float3 ga = hash33(fmod(p + float3(0.0, 0.0, 0.0), freq));
    float3 gb = hash33(fmod(p + float3(1.0, 0.0, 0.0), freq));
    float3 gc = hash33(fmod(p + float3(0.0, 1.0, 0.0), freq));
    float3 gd = hash33(fmod(p + float3(1.0, 1.0, 0.0), freq));
    float3 ge = hash33(fmod(p + float3(0.0, 0.0, 1.0), freq));
    float3 gf = hash33(fmod(p + float3(1.0, 0.0, 1.0), freq));
    float3 gg = hash33(fmod(p + float3(0.0, 1.0, 1.0), freq));
    float3 gh = hash33(fmod(p + float3(1.0, 1.0, 1.0), freq));
    
    // projections
    float va = dot(ga, w - float3(0.0, 0.0, 0.0));
    float vb = dot(gb, w - float3(1.0, 0.0, 0.0));
    float vc = dot(gc, w - float3(0.0, 1.0, 0.0));
    float vd = dot(gd, w - float3(1.0, 1.0, 0.0));
    float ve = dot(ge, w - float3(0.0, 0.0, 1.0));
    float vf = dot(gf, w - float3(1.0, 0.0, 1.0));
    float vg = dot(gg, w - float3(0.0, 1.0, 1.0));
    float vh = dot(gh, w - float3(1.0, 1.0, 1.0));
    
    // interpolation
    return va + 
           u.x * (vb - va) + 
           u.y * (vc - va) + 
           u.z * (ve - va) + 
           u.x * u.y * (va - vb - vc + vd) + 
           u.y * u.z * (va - vc - ve + vg) + 
           u.z * u.x * (va - vb - ve + vf) + 
           u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}

// Tileable 3D worley noise
float worleyNoise(float3 uv, float freq) {    
    float3 id = floor(uv);
    float3 p = frac(uv);
    
    float minDist = 10000.0;
    [unroll]
    for (float x = -1.0; x <= 1.0; ++x) {
        [unroll]
        for(float y = -1.0; y <= 1.0; ++y) {
            [unroll]
            for(float z = -1.0; z <= 1.0; ++z) {
                float3 offset = float3(x, y, z);
                float3 h = hash33(fmod(id + offset, float3(freq, freq, freq))) * 0.5 + 0.5;
                h += offset;
                float3 d = p - h;
                minDist = min(minDist, dot(d, d));
            }
        }
    }
    
    // inverted worley noise
    return 1.0 - minDist;
}

// Fbm for Perlin noise based on iq's blog
float perlinFbm(float3 p, float freq, int octaves) {
    float G = exp2(-0.85);
    float amp = 1.0;
    float noise = 0.0;
    
    [loop]
    for (int i = 0; i < octaves; ++i) {
        noise += amp * gradientNoise(p * freq, freq);
        freq *= 2.0;
        amp *= G;
    }
    
    return noise;
}

// Tileable Worley fbm inspired by Andrew Schneider's Real-Time Volumetric Cloudscapes
// chapter in GPU Pro 7.
float worleyFbm(float3 p, float freq) {
    return worleyNoise(p * freq, freq) * 0.625 +
           worleyNoise(p * freq * 2.0, freq * 2.0) * 0.25 +
           worleyNoise(p * freq * 4.0, freq * 4.0) * 0.125;
}