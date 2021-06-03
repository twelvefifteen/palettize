#ifndef PALETTIZE_MATH_H
#define PALETTIZE_MATH_H

#include <float.h>
#include <math.h>

const f32 F32_MAX = FLT_MAX;

struct Vector3 {
    f32 x, y, z;
};

struct Matrix3 {
    Vector3 x_axis;
    Vector3 y_axis;
    Vector3 z_axis;
};

inline Vector3 V3(f32 x, f32 y, f32 z) {
    Vector3 result;
    result.x = x;
    result.y = y;
    result.z = z;
    
    return(result);
}

inline Vector3 V3i(int x, int y, int z) {
    Vector3 result = V3((f32)x, (f32)y, (f32)z);
    
    return(result);
}

// 
// Scalar operations
// 

inline f32 clamp(f32 min, f32 s, f32 max) {
    f32 result = s;

    if (result < min) {
        result = min;
    } else if(result > max) {
        result = max;
    }

    return(result);
}

inline int clampi(int min, int s, int max) {
    int result = (int)clamp((f32)min, (f32)s, (f32)max);
    
    return(result);
}

inline f32 clamp01(f32 s) {
    f32 result = clamp(0.0f, s, 1.0f);
    
    return(result);
}

inline f32 cube(f32 s) {
    f32 result = s*s*s;

    return(result);
}

inline f32 cube_root(f32 s) {
    f32 result = cbrtf(s);
    
    return(result);
}

inline f32 pow(f32 x, f32 y) {
    f32 result = powf(x, y);
    
    return(result);
}

inline f32 round(f32 s) {
    f32 result = roundf(s);

    return(result);
}

inline int round_to_int(f32 s) {
    int result = (int)round(s);
    
    return(result);
}

inline u32 round_to_u32(f32 s) {
    u32 result = (u32)round(s);
    
    return(result);
}

inline f32 safe_ratio_n(f32 dividend, f32 divisor, f32 n) {
    f32 result = n;

    if (divisor != 0.0f) {
        result = (dividend / divisor);
    }

    return(result);
}

inline f32 safe_ratio_0(f32 dividend, f32 divisor) {
    f32 result = safe_ratio_n(dividend, divisor, 0.0f);
    
    return(result);
}

inline f32 square(f32 s) {
    f32 result = s*s;
    
    return(result);
}

inline f32 square_root(f32 s) {
    f32 result = sqrtf(s);
    
    return(result);
}

// 
// Vector3 operations
// 

inline Vector3 operator+(Vector3 a, Vector3 b) {
    Vector3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    
    return(result);
}

inline Vector3 operator-(Vector3 a, Vector3 b) {
    Vector3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    
    return(result);
}

inline Vector3 operator*(Vector3 a, f32 s) {
    Vector3 result;
    result.x = a.x*s;
    result.y = a.y*s;
    result.z = a.z*s;
    
    return(result);
}

inline void operator+=(Vector3 &a, Vector3 b) {
    a = a + b;
}

inline void operator*=(Vector3 &a, f32 s) {
    a = a*s;
}

inline f32 dot(Vector3 a, Vector3 b) {
    f32 result = ((a.x*b.x) +
                  (a.y*b.y) +
                  (a.z*b.z));
    return(result);
}

inline f32 length_squared(Vector3 V) {
    f32 result = dot(V, V);
    
    return(result);
}

inline f32 linear_rgb_to_srgb(f32 a);
inline Vector3 linear_rgb_to_srgb(Vector3 a) {
    Vector3 result;
    result.x = linear_rgb_to_srgb(a.x);
    result.y = linear_rgb_to_srgb(a.y);
    result.z = linear_rgb_to_srgb(a.z);
    
    return(result);
}

inline u32 pack_rgba(Vector3 a) {
    u32 result = ((round_to_u32(a.x*255.0f) << 0) |
                  (round_to_u32(a.y*255.0f) << 8) |
                  (round_to_u32(a.z*255.0f) << 16) |
                  (255 << 24));
    return(result);
}

inline f32 srgb_to_linear_rgb(f32 channel);
inline Vector3 srgb_to_linear_rgb(Vector3 a) {
    Vector3 result;
    result.x = srgb_to_linear_rgb(a.x);
    result.y = srgb_to_linear_rgb(a.y);
    result.z = srgb_to_linear_rgb(a.z);
    
    return(result);
}

inline Vector3 unpack_rgba(u32 c) {
    f32 inv_255 = (1.0f / 255.0f);

    Vector3 result;
    result.x = ((c >> 0) & 0xFF)*inv_255;
    result.y = ((c >> 8) & 0xFF)*inv_255;
    result.z = ((c >> 16) & 0xFF)*inv_255;
    
    return(result);
}

// 
// Matrix3 operations
// 

inline Vector3 operator*(Matrix3 m, Vector3 v) {
    Vector3 result;
    result.x = dot(V3(m.x_axis.x, m.y_axis.x, m.z_axis.x), v);
    result.y = dot(V3(m.x_axis.y, m.y_axis.y, m.z_axis.y), v);
    result.z = dot(V3(m.x_axis.z, m.y_axis.z, m.z_axis.z), v);
    
    return(result);
}

// 
// Color space operations
// 

// White point coords for Illuminant D65
const f32 COORD_Xn = 0.950470f;
const f32 COORD_Yn = 1.0f;
const f32 COORD_Zn = 1.088830f;

inline f32 inv_ft(f32 t) {
    f32 result;

    f32 sigma = (6.0f / 29.0f);
    if (t > sigma) {
        result = cube(t);
    } else {
        result = ((3.0f*square(sigma))*(t - (4.0f / 29.0f)));
    }
    
    return(result);
}

inline Vector3 cielab_to_ciexyz(Vector3 a) {
    Vector3 result;
    result.x = COORD_Xn*inv_ft(((a.x + 16.0f) / 116.0f) + (a.y / 500.0f));
    result.y = COORD_Yn*inv_ft((a.x + 16.0f) / 116.0f);
    result.z = COORD_Zn*inv_ft(((a.x + 16.0f) / 116.0f) - (a.z / 200.0f));
    
    return(result);
}

inline f32 ft(f32 t) {
    f32 result;

    f32 sigma = 6.0f / 29.0f;
    if (t > cube(sigma)) {
        result = cube_root(t);
    } else {
        result = ((t / (3.0f*square(sigma))) + (4.0f / 29.0f));
    }
    
    return(result);
}

inline Vector3 ciexyz_to_cielab(Vector3 a) {
    Vector3 result;
    result.x = (116.0f*ft(a.y / COORD_Yn) - 16.0f);
    result.y = (500.0f*(ft(a.x / COORD_Xn) - ft(a.y / COORD_Yn)));
    result.z = (200.0f*(ft(a.y / COORD_Yn) - ft(a.z / COORD_Zn)));
    
    return(result);
}

inline Vector3 ciexyz_to_linear_rgb(Vector3 a) {
    Matrix3 matrix;
    matrix.x_axis = V3(3.2404542f, -0.9692660f, 0.0556434f);
    matrix.y_axis = V3(-1.5371385f, 1.8760108f, -0.2040259f);
    matrix.z_axis = V3(-0.4985314f, 0.0415560f, 1.0572252f);
    Vector3 result = matrix*a;
    
    return(result);
}

inline Vector3 linear_rgb_to_ciexyz(Vector3 a) {
    Matrix3 matrix;
    matrix.x_axis = V3(0.4124564f, 0.2126729f, 0.0193339f);
    matrix.y_axis = V3(0.3575761f, 0.7151522f, 0.1191920f);
    matrix.z_axis = V3(0.1804375f, 0.0721750f, 0.9503041f);
    Vector3 result = matrix*a;
    
    return(result);
}

inline f32 linear_rgb_to_srgb(f32 a) {
    a = clamp01(a);
    
    f32 result;
    if (a <= 0.0031308f) {
        result = 12.92f*a;
    } else {
        result = ((1.055f*pow(a, (1.0f / 2.4f))) - 0.055f);
    }
    
    return(result);
}

inline u32 pack_cielab_to_rgba(Vector3 a) {
    Vector3 cielab = a;
    Vector3 ciexyz = cielab_to_ciexyz(cielab);
    Vector3 linear_rgb = ciexyz_to_linear_rgb(ciexyz);
    Vector3 srgb = linear_rgb_to_srgb(linear_rgb);
    u32 result = pack_rgba(srgb);
    
    return(result);
}

inline f32 srgb_to_linear_rgb(f32 a) {
    a = clamp01(a);
    
    f32 result;
    if (a <= 0.04045f) {
        result = (a / 12.92f);
    } else {
        result = pow(((a + 0.055f) / 1.055f), 2.4f);
    }
    
    Assert((0.0f <= result) && (result <= 1.0f));
    
    return(result);
}

inline Vector3 unpack_rgba_to_cielab(u32 a) {
    Vector3 result_rbga = unpack_rgba(a);
    Vector3 result_linear_rgb = srgb_to_linear_rgb(result_rbga);
    Vector3 result_ciexyz = linear_rgb_to_ciexyz(result_linear_rgb);
    Vector3 result_cielab = ciexyz_to_cielab(result_ciexyz);
    
    return(result_cielab);
}

#endif
