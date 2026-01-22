#include "commonFunctions.hlsl"

#ifndef FBM_HLSL
#define FBM_HLSL

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

float PerlinWorleyPeriodic(float3 p, int frequency)
{
    float pNoise = PerlinPeriodic(p) * 1.5;
    float wNoise = WorleyPeriodic(p, frequency);

    return Remap(pNoise, 1.0 - wNoise, 1.0, 0.0, 1.0);
}

float WorleyF2MinusF1(float3 p, int frequency)
{
    p *= frequency;

    int3 cell = (int3)floor(p);
    float3 f = frac(p);

    float d1 = 1e6;
    float d2 = 1e6;

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
        float dist = dot(d,d);

        if (dist < d1)
        {
            d2 = d1;
            d1 = dist;
        }
        else if (dist < d2)
        {
            d2 = dist;
        }
    }

    return sqrt(d2) - sqrt(d1);
}

float AlligatorPeriodic(float3 p, int frequency)
{
    // -----------------------------
    // 1. Domain Warp (Perlin)
    // -----------------------------
    float warpFreq = 1.0;
    float warpAmp  = 0.35;

    float3 warp;
    warp.x = PerlinPeriodic(p + float3(12.3, 45.1, 78.9), frequency);
    warp.y = PerlinPeriodic(p + float3(98.2, 11.7,  3.4), frequency);
    warp.z = PerlinPeriodic(p + float3( 7.1, 63.5, 29.8), frequency);

    warp = warp * 2.0 - 1.0;   // [-1,1]
    float3 pw = p + warp * warpAmp;

    // -----------------------------
    // 2. Low Frequency Cells
    // -----------------------------
    float f1 = WorleyPeriodic(pw, frequency);
    float edge = WorleyF2MinusF1(pw, frequency);

    // 塊（セル内部）
    float cells = saturate(f1);
    cells = smoothstep(0.25, 0.85, cells);

    // 割れ目（エッジ）
    edge = saturate(edge * 2.0);
    edge = pow(edge, 1.4);

    float base = lerp(cells, edge, 0.65);

    // -----------------------------
    // 3. High Frequency Detail
    // -----------------------------
    float detailFreq = frequency * 3;
    float d1 = WorleyPeriodic(pw, detailFreq);
    float d2 = WorleyF2MinusF1(pw, detailFreq);

    float detail = saturate(lerp(d1, d2, 0.5));
    detail = smoothstep(0.3, 0.7, detail);

    // -----------------------------
    // 4. Final Composition
    // -----------------------------
    float result = base * (0.7 + 0.3 * detail);

    return saturate(result);
}

#endif // FBM_HLSL