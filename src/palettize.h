#ifndef PALETTIZE_H
#define PALETTIZE_H

#include <float.h>
#include <stdint.h>

typedef int32_t s32;
typedef s32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef uintptr_t umm;

typedef float f32;

#define F32Max FLT_MAX

#if PALETTIZE_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int*)0 = 0;}
#else
#define Assert(Expression)
#endif
#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define ArrayCount(Array) (sizeof((Array)) / sizeof((Array)[0]))

#define Maximum(A, B) ((A) > (B) ? (A) : (B))
#define Minimum(A, B) ((A) < (B) ? (A) : (B))

inline char
FlipCase(char C)
{
    char Result = '\0';
    if(('a' <= C) && (C <= 'z'))
    {
        Result = ((C - 'a') + 'A');
    }
    else if(('A' <= C) && (C <= 'Z'))
    {
        Result = ((C - 'A') + 'a');
    }
    return(Result);
}

inline b32
StringsAreEqualCaseInsensitive(char* A, char* B)
{
    while(*A &&
          *B)
    {
        if((*A == *B) ||
           (*A == FlipCase(*B)))
        {
            A++;
            B++;
        }
        else
        {
            break;
        }
    }
    
    b32 Result = (*A == *B);
    
    return(Result);
}

#include "palettize_math.h"
#include "palettize_random.h"

enum sort_type
{
    SortType_Weight,
    SortType_Red,
    SortType_Green,
    SortType_Blue,
};

#define GetBitmapPtr(Bitmap, X, Y) ((u8*)(Bitmap).Address + (sizeof(u32)*(X)) + ((Y)*(Bitmap).Pitch))
struct bitmap
{
    void* Address;
    int Width;
    int Height;
    umm Pitch;
};

struct cluster
{
    v3 Centroid;
    
    v3 ObservationSum;
    int ObservationCount;
    
    int TotalObservationCount;
};
struct cluster_group
{
    random_series Entropy;
    
    bitmap Bitmap;
    
    int ClusterCount;
    cluster* Clusters;
    
    int TotalObservationCount;
};

#pragma pack(push, 1)
#define BI_RGB 0x0000
struct bitmap_header
{
    u16 Type;
    u32 FileSize;
    u16 Reserved1;
    u16 Reserved2;
    u32 OffBits;
    
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitCount;
    u32 Compression;
    u32 SizeImage;
    s32 XPelsPerMeter;
    s32 YPelsPerMeter;
    u32 ClrUsed;
    u32 ClrImportant;
};

#pragma pack(pop)

#endif
