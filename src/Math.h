#pragma once
#include <immintrin.h>

typedef union
{
    struct
    {
        float x;
        float y;
    };

    float data[2];
} Vector2;

typedef union
{
    struct
    {
        float x;
        float y;
        float z;
    };

    float data[3];
} Vector3;

typedef union
{
    struct
    {
        float x;
        float y;
        float z;
        float w;
    };

    float data[4];
} Vector4;

typedef union
{
    Vector4 columns[4];
    f32 data[16];
    struct
    {
        f32 _11, _21, _31, _41;
        f32 _12, _22, _32, _42;
        f32 _13, _23, _33, _43;
        f32 _14, _24, _34, _44;
    };
} Matrix4x4;


inline Vector2 MakeVector2(float x, float y)
{
    Vector2 v;
    v.x = x;
    v.y = y;
    return v;
}

inline Vector2 MakeVector2u(u32 x, u32 y)
{
    Vector2 v;
    v.x = (f32)x;
    v.y = (f32)y;
    return v;
}

inline Vector2 MakeVector2i(i32 x, i32 y)
{
    Vector2 v;
    v.x = (f32)x;
    v.y = (f32)y;
    return v;
}

inline Vector3 MakeVector3(float x, float y, float z)
{
    Vector3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

inline Vector4 MakeVector4(float x, float y, float z, float w)
{
    Vector4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

inline f32 fAbs(f32 a)
{
    return fabsf(a);
}

inline f32 fMin(f32 a, f32 b)
{
    return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b))); 
}

inline f32 fMax(f32 a, f32 b)
{
    return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b))); 
}

inline i32 iMin(i32 a, i32 b)
{
    return a < b ? a : b;
}

inline i32 iMax(i32 a, i32 b)
{
    return a > b ? a : b;
}

inline f32 fClamp(f32 min, f32 v, f32 max)
{
    __m128 r0 = _mm_max_ss(_mm_set_ss(min), _mm_set_ss(v));
    return _mm_cvtss_f32(_mm_min_ss(r0, _mm_set_ss(max))); 
}

inline i32 iClamp(i32 min, i32 v, i32 max)
{
    return iMin(max, iMax(min, v));
}

inline f32 fLerp(f32 a, f32 t, f32 b)
{
    return (1.0f - t) * a + t * b;
}

inline f32 fFloor(f32 a)
{
    return floorf(a);
}

inline f32 fCeil(f32 a)
{
    return ceilf(a);
}

inline f32 fSin(f32 a)
{
    return sinf(a);
}

inline f32 fCos(f32 a)
{
    return cosf(a);
}

inline f64 dFloor(f64 a)
{
    return floor(a);
}

inline Vector2 v2Lerp(Vector2 a, f32 t, Vector2 b)
{
    return MakeVector2((1.0f - t) * a.x + t * b.x, (1.0f - t) * a.y + t * b.y);
}

inline f32 fPow(f32 a, f32 p)
{
    return powf(a, p);
}

inline f32 fRsqrt(f32 x)
{ 
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); 
}

inline Vector2 v2Normalize(Vector2 v)
{
    f32 d = v.x * v.x + v.y * v.y;
    if (d > 0.0f)
    {
        f32 invLen = fRsqrt(d);
        v.x *= invLen;
        v.y *= invLen;
    }

    return v;
}

inline Vector2 v2Add(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline Vector2 v2Sub(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline Vector2 v2Scale(Vector2 a, f32 s)
{
    Vector2 result;
    result.x = a.x * s;
    result.y = a.y * s;
    return result;
}

inline Vector4 v4Scale(Vector4 a, f32 s)
{
    Vector4 result;
    result.x = a.x * s;
    result.y = a.y * s;
    result.z = a.y * s;
    result.w = a.y * s;
    return result;
}

inline Vector2 v2Hadamard(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

inline Matrix4x4 MakeMatrix4x4()
{
    Matrix4x4 m = {0};
    m._11 = 1.0f;
    m._22 = 1.0f;
    m._33 = 1.0f;
    m._44 = 1.0f;
    return m;
}

inline Matrix4x4 OrthoGLRH(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
    Matrix4x4 m = {0};
    m._11 = 2.0f / (right - left);
    m._22 = 2.0f / (top - bottom);
    m._33 = 2.0f / (far - near);
    m._14 = -(right + left) / (right - left);
    m._24 = -(top + bottom) / (top - bottom);
    m._34 = -(far + near) / (far - near);
    m._44 = 1.0f;
    return m;
}

inline Vector2 v2Min(Vector2 a, Vector2 b)
{
    Vector2 result =
    {
        .x = fMin(a.x, b.x),
        .y = fMin(a.y, b.y),
    };

    return result;
}

inline Vector2 v2Max(Vector2 a, Vector2 b)
{
    Vector2 result =
    {
        .x = fMax(a.x, b.x),
        .y = fMax(a.y, b.y),
    };

    return result;
}
