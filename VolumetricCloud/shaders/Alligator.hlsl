#ifndef ALLIGATOR_NOISE_HLSL
#define ALLIGATOR_NOISE_HLSL

// ------------------------------------------------------------
// Alligator Noise HLSL port
// Suitable for volumetric cloud detail noise.
// ------------------------------------------------------------

static const float ALLIGATOR_LACUNARITY  = 2.0;
static const float ALLIGATOR_PERSISTENCE = 0.5;

// 0-1 smoothstep
float SmoothValue(float x)
{
    x = saturate(x);
    return x * x * (3.0 - 2.0 * x);
}

// Simple deterministic hash: float3 -> float
float Hash13(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

// Simple deterministic hash: float3 -> float3
float3 Hash33(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

// Single octave Alligator Noise
float AlligatorNoiseSingle(
    float3 position,
    uint gridsize,
    uint3 seed,
    bool tiling
)
{
    float fGridSize = (float)gridsize;

    // Scale into grid space
    position *= fGridSize;

    float3 id   = floor(position);
    float3 grid = position - id;

    float densest       = 0.0;
    float secondDensest = 0.0;

    // Compare with 3x3x3 neighbor cells
    [unroll]
    for (int ix = -1; ix <= 1; ++ix)
    {
        [unroll]
        for (int iy = -1; iy <= 1; ++iy)
        {
            [unroll]
            for (int iz = -1; iz <= 1; ++iz)
            {
                float3 offset = float3(ix, iy, iz);
                float3 cell = id + offset;

                if (tiling)
                {
                    // Repeat in 0-1 domain
                    cell = fmod(cell, fGridSize);

                    // HLSL fmod can be negative, so fix wrap
                    cell = (cell < 0.0) ? cell + fGridSize : cell;
                }

                // Hash dislikes zero-ish coordinates, so add seed
                cell += float3(seed);

                // Random feature point inside neighbor cell
                float3 center = Hash33(cell) + offset;

                float dist = distance(grid, center);

                float density = Hash13(cell) * SmoothValue(1.0 - dist);

                if (density > densest)
                {
                    secondDensest = densest;
                    densest = density;
                }
                else if (density > secondDensest)
                {
                    secondDensest = density;
                }
            }
        }
    }

    return densest - secondDensest;
}

// Fractal/octaved Alligator Noise
float AlligatorNoise(
    float3 position,
    float gridsize,
    int octaves,
    float lacunarity,
    float persistence,
    bool tiling
)
{
    float amplitude = 1.0;
    float amplitudeSum = 0.0;
    float result = 0.0;

    uint3 seed = uint3(421, 421, 421);

    [loop]
    for (int i = 0; i < octaves; ++i)
    {
        uint gridU = max(1, (uint)gridsize);

        result += AlligatorNoiseSingle(position, gridU, seed, tiling) * amplitude;

        amplitudeSum += amplitude;

        gridsize *= lacunarity;
        amplitude *= persistence;

        seed += uint3((uint)gridsize, (uint)gridsize, (uint)gridsize);
    }

    return result / max(amplitudeSum, 1e-5);
}

// Convenience version
float AlligatorNoiseDefault(float3 position)
{
    return AlligatorNoise(
        position,
        8.0,                    // gridsize
        5,                      // octaves
        ALLIGATOR_LACUNARITY,
        ALLIGATOR_PERSISTENCE,
        true                    // tiling
    );
}

#endif