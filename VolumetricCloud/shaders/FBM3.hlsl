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
    In the context of the fbmd_8 function, 
    derivatives refer to the partial derivatives of the noise function 
    with respect to the input coordinates. 
    These derivatives provide information about 
    the rate of change of the noise value in different directions, 
    which can be useful for various applications such as normal mapping, 
    procedural texture generation, and more.
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
        // n.x is the noise value.
        // n.yzw are the partial derivatives of the noise with respect to x, y, and z.
        float4 n = noised(x);
        a += b*n.x;          // accumulate values		
        // The derivatives are accumulated in the d variable.
        // The mul(m, n.yzw) operation transforms the derivatives using the matrix m.
        if( i<4 )
            d += mul(m,n.yzw)*b;      // accumulate derivatives
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