#ifndef PALETTIZE_H
#define PALETTIZE_H

#include <stdint.h>

// @Production: Make this a stub based on a macro or something
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#define Invalid_Code_Path Assert(!"InvalidCodePath")
#define Invalid_Default_Case default: {InvalidCodePath;} break

#define Array_Count(array) (sizeof((array)) / sizeof((array)[0]))

#define Maximum(A, B) ((A) > (B) ? (A) : (B))
#define Minimum(A, B) ((A) < (B) ? (A) : (B))

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

enum Sort_Type {
    SORT_TYPE_WEIGHT,
    SORT_TYPE_RED,
    SORT_TYPE_GREEN,
    SORT_TYPE_BLUE,
};

struct Palettize_Config {
    char *source_path;
    char *dest_path;

    u32 seed;
    
    int num_clusters;
    int iteration_count;
    
    Sort_Type sort_type;
};

#define Get_Bitmap_Ptr(Bitmap, X, Y) ((u8 *)(Bitmap).memory + (sizeof(u32)*(X)) + ((Y)*(Bitmap).pitch))
struct Bitmap {
    void *memory;
    int width;
    int height;
    umm pitch;
};

struct Cluster {
    Vector3 centroid;
    
    Vector3 observation_sum;
    int num_observations;
    
    int total_observation_count;
};
struct Cluster_Group {
    Random_Series entropy;
    
    Bitmap bitmap;
    
    int num_clusters;
    Cluster *clusters;
    
    // @TODO: Remove this, since it can be computed from the
    // total_observation_count maintained in each cluster
    int total_observation_count;
};

#pragma pack(push, 1)
#define BI_RGB 0x0000
struct Bitmap_Header {
    u16 type;
    u32 file_size;
    u16 reserved1;
    u16 reserved2;
    u32 off_bits;
    
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bit_count;
    u32 compression;
    u32 size_image;
    s32 x_pels_per_meter;
    s32 y_pels_per_meter;
    u32 clr_used;
    u32 clr_important;
};
#pragma pack(pop)

#endif
