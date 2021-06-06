#if !defined(PALETTIZE_H)

#include <assert.h>
#include <float.h>
#include <stdint.h>

// @Production: Make Assert expand to nothing based on a macro
#define Assert assert
#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define ArrayCount(Array) (sizeof((Array)) / sizeof((Array)[0]))

#define Maximum(A, B) ((A) > (B) ? (A) : (B))
#define Minimum(A, B) ((A) < (B) ? (A) : (B))

#define F32Max FLT_MAX

typedef int32_t s32;
typedef s32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef uintptr_t umm;

typedef float f32;

#include "palettize_math.h"
#include "palettize_random.h"
#include "palettize_string.h"

enum sort_type
{
    SortType_Weight,
    SortType_Red,
    SortType_Green,
    SortType_Blue,
};

struct palettize_config
{
    char *SourcePath;
    int ClusterCount;
    u32 Seed;
    sort_type SortType;
    int IterationCount;
    char *DestPath;
};

#define GetBitmapPtr(Bitmap, X, Y) ((u8 *)(Bitmap).Memory + (sizeof(u32)*(X)) + ((Y)*(Bitmap).Pitch))
struct bitmap
{
    void *Memory;
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
struct kmeans_context
{
    random_series Entropy;
    
    bitmap Bitmap;
    
    int ClusterCount;
    cluster *Clusters;
    
    // @TODO: Remove this, since it can be computed from the
    // TotalObservationCount maintained in each cluster
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

#define PALETTIZE_H
#endif
