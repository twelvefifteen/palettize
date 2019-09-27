#include <stdio.h>
#include <stdlib.h>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma warning(pop)

#include "palettize.h"

static bitmap
LoadBitmap(char* Path)
{
    bitmap Result;
    Result.Address = stbi_load(Path, &Result.Width, &Result.Height, 0, 4);
    Result.Pitch = (sizeof(u32)*Result.Width);
    
    return(Result);
}

static bitmap
MakeEmptyBitmap(int Width, int Height)
{
    bitmap Result;
    Result.Address = malloc(sizeof(u32)*Width*Height);
    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = (sizeof(u32)*Width);
    
    return(Result);
}

static u32
GetNearestNeighborSample(bitmap Bitmap, f32 U, f32 V)
{
    Assert((0.0f <= U) && (U <= 1.0f));
    Assert((0.0f <= V) && (V <= 1.0f));
    
    int SampleX = RoundToInt(U*((f32)Bitmap.Width - 1.0f));
    int SampleY = RoundToInt(V*((f32)Bitmap.Height - 1.0f));
    Assert((0 <= SampleX) && (SampleX < Bitmap.Width));
    Assert((0 <= SampleY) && (SampleY < Bitmap.Height));
    
    u32 Result = *(u32*)GetBitmapPtr(Bitmap, SampleX, SampleY);
    
    return(Result);
}

static void
Resize(bitmap Resized, bitmap Source)
{
    int MinX = 0;
    int MinY = 0;
    int MaxX = Resized.Width;
    int MaxY = Resized.Height;
    
    u8* Row = (u8*)GetBitmapPtr(Resized, MinX, MinY);
    for(int Y = MinY;
        Y < MaxY;
        Y++)
    {
        u32* TexelPtr = (u32*)Row;
        for(int X = MinX;
            X < MaxX;
            X++)
        {
            f32 U = ((f32)X / ((f32)MaxX - 1.0f));
            f32 V = ((f32)Y / ((f32)MaxY - 1.0f));
            u32 Sample = GetNearestNeighborSample(Source, U, V);
            *TexelPtr++ = Sample;
        }
        
        Row += Resized.Pitch;
    }
}

static void
SeedCluster(cluster_group* ClusterGroup, cluster* Cluster)
{
    u32 RandomSampleX =
        RandomU32Between(&ClusterGroup->Entropy, 0, (u32)(ClusterGroup->Bitmap.Width - 1));
    u32 RandomSampleY =
        RandomU32Between(&ClusterGroup->Entropy, 0, (u32)(ClusterGroup->Bitmap.Height - 1));
    u32 RandomSample = *(u32*)GetBitmapPtr(ClusterGroup->Bitmap, RandomSampleX, RandomSampleY);
#if PALETTIZE_CIELAB
    v3 RandomSampleV3 = UnpackRGBAToCIELAB(RandomSample);
#else
    v3 RandomSampleV3 = UnpackRGBA(RandomSample);
#endif
    Cluster->Centroid = RandomSampleV3;
}

static void
ClearClusterObservations(cluster* Cluster)
{
    Cluster->ObservationSum = V3i(0, 0, 0);
    Cluster->ObservationCount = 0;
}

static cluster_group*
AllocateClusterGroup(bitmap Bitmap, u32 Seed, int ClusterCount)
{
    cluster_group* ClusterGroup = (cluster_group*)malloc(sizeof(cluster_group));
    ClusterGroup->Entropy = SeedSeries(Seed);
    ClusterGroup->Bitmap = Bitmap;
    ClusterGroup->ClusterCount = ClusterCount;
    ClusterGroup->Clusters = (cluster*)malloc(sizeof(cluster)*ClusterCount); 
    ClusterGroup->TotalObservationCount = 0;
    // @Features: Support k-means++ as another cluster seeding option
    for(int ClusterIndex = 0;
        ClusterIndex < ClusterGroup->ClusterCount;
        ClusterIndex++)
    {
        cluster* Cluster = (ClusterGroup->Clusters + ClusterIndex);
        Cluster->TotalObservationCount = 0;
        ClearClusterObservations(Cluster);
        SeedCluster(ClusterGroup, Cluster);
    }
    
    return(ClusterGroup);
}

static u32
AddObservation(cluster_group* ClusterGroup, v3 C)
{
    f32 ClosestDistSquared = F32Max;
    cluster* ClosestCluster = 0;
    for(int ClusterIndex = 0;
        ClusterIndex < ClusterGroup->ClusterCount;
        ClusterIndex++)
    {
        cluster* Cluster = (ClusterGroup->Clusters + ClusterIndex);
        f32 d = LengthSquared(Cluster->Centroid - C);
        if(d < ClosestDistSquared)
        {
            ClosestDistSquared = d;
            ClosestCluster = Cluster;
        }
    }
    Assert(ClosestCluster);
    
    ClosestCluster->ObservationSum += C;
    ClosestCluster->ObservationCount++;
    ClosestCluster->TotalObservationCount++;
    
    ClusterGroup->TotalObservationCount++;
    
    // Returning the index of the closest cluster for our early out
    // condition in the loop where this function's called
    u32 Result = (u32)(ClosestCluster - ClusterGroup->Clusters);
    Assert(Result < (u32)ClusterGroup->ClusterCount);
    
    return(Result);
}

static void
CommitObservations(cluster_group* ClusterGroup)
{
    for(int ClusterIndex = 0;
        ClusterIndex < ClusterGroup->ClusterCount;
        ClusterIndex++)
    {
        cluster* Cluster = (ClusterGroup->Clusters + ClusterIndex);
        
        if(Cluster->ObservationCount)
        {
            v3 NewCentroid = (Cluster->Centroid + Cluster->ObservationSum);
            f32 InvOnePastObservationCount = (1.0f / ((f32)Cluster->ObservationCount + 1.0f));
            NewCentroid *= InvOnePastObservationCount;
            Cluster->Centroid = NewCentroid;
        }
        else
        {
            SeedCluster(ClusterGroup, Cluster);
        }
        
        ClearClusterObservations(Cluster);
    }
}

static void
ExportBMP(bitmap Bitmap, char* Path)
{
    FILE* File = fopen(Path, "wb");
    if(File)
    {
        // Swizzling the red and blue components of each texel because
        // that's how BMPs expect channels to be ordered
        u8* Row = (u8*)Bitmap.Address;
        for(int Y = 0;
            Y < Bitmap.Height;
            Y++)
        {
            u32* TexelPtr = (u32*)Row;
            for(int X = 0;
                X < Bitmap.Width;
                X++)
            {
                u32 Texel = *TexelPtr;
                u32 Swizzled = ((((Texel >> 0) & 0xFF) << 16) |
                                (((Texel >> 8) & 0xFF) << 8) |
                                (((Texel >> 16) & 0xFF) << 0) |
                                (((Texel >> 24) & 0xFF) << 24));
                *TexelPtr++ = Swizzled;
            }
            
            Row += Bitmap.Pitch;
        }
        
        u32 BitmapSize = (u32)(Bitmap.Pitch*Bitmap.Height);
        
        bitmap_header BitmapHeader = {};
        BitmapHeader.Type = 0x4D42;
        BitmapHeader.FileSize = (sizeof(BitmapHeader) + BitmapSize);
        BitmapHeader.OffBits = sizeof(BitmapHeader);
        BitmapHeader.Size = 40; // sizeof(BITMAPINFOHEADER)
        BitmapHeader.Width = Bitmap.Width;
        BitmapHeader.Height = Bitmap.Height;
        BitmapHeader.Planes = 1;
        BitmapHeader.BitCount = 32;
        BitmapHeader.Compression = BI_RGB;
        
        fwrite(&BitmapHeader, sizeof(BitmapHeader), 1,  File);
        fwrite(Bitmap.Address, BitmapSize, 1, File);
        
        fclose(File);
    }
}

int
main(int ArgCount, char** Args)
{
    if(ArgCount > 1)
    {
        char* SourcePath = Args[1];
        int ClusterCount = 5;
        if(ArgCount > 2)
        {
            int RequestedClusterCount = atoi(Args[2]);
            ClusterCount = Clampi(1, RequestedClusterCount, 64);
        }
        u32 Seed = 2019;
        if(ArgCount > 3)
        {
            Seed = (u32)atoi(Args[3]);
        }
        sort_type SortType = SortType_Weight;
        if(ArgCount > 4)
        {
            char* SortTypeArg = Args[4];
            if(StringsAreEqualCaseInsensitive(SortTypeArg, "red"))
            {
                SortType = SortType_Red;
            }
            else if(StringsAreEqualCaseInsensitive(SortTypeArg, "green"))
            {
                SortType = SortType_Green;
            }
            else if(StringsAreEqualCaseInsensitive(SortTypeArg, "blue"))
            {
                SortType = SortType_Blue;
            }
        }
        
        bitmap Source = LoadBitmap(SourcePath);
        if(Source.Address)
        {
            int MaxDim = Maximum(Source.Width, Source.Height);
            f32 DimFactor = (100.0f / (f32)MaxDim);
            int ResizedWidth = RoundToInt(Source.Width*DimFactor);
            int ResizedHeight = RoundToInt(Source.Height*DimFactor);
            // @Refactor: Since this program isn't sharing its code,
            // perhaps Resize() can simply return an allocated bitmap instead
            // of accepting one
            bitmap Resized = MakeEmptyBitmap(ResizedWidth, ResizedHeight);
            Resize(Resized, Source);
            
            cluster_group* ClusterGroup =
                AllocateClusterGroup(Resized, Seed, ClusterCount);
            
            bitmap LastClusterIndexBuffer =
                MakeEmptyBitmap(ResizedWidth, ResizedHeight);
            for(int Iteration = 0;
                Iteration < 300;
                Iteration++)
            {
                int MinX = 0;
                int MinY = 0;
                int MaxX = Resized.Width;
                int MaxY = Resized.Height;
                
                b32 IndexChangeOccurred = false;
                
                u8* Row = (u8*)GetBitmapPtr(Resized, MinX, MinY);
                for(int Y = MinY;
                    Y < MaxY;
                    Y++)
                {
                    u32* TexelPtr = (u32*)Row;
                    for(int X = MinX;
                        X < MaxX;
                        X++)
                    {
                        u32 Texel = *TexelPtr;
                        
#if PALETTIZE_CIELAB
                        v3 TexelV3 = UnpackRGBAToCIELAB(Texel);
#else
                        v3 TexelV3 = UnpackRGBA(Texel);
#endif
                        u32 ClusterIndex = AddObservation(ClusterGroup,
                                                          TexelV3);
                        u32* PrevClusterIndexPtr =
                            (u32*)GetBitmapPtr(LastClusterIndexBuffer, X, Y);
                        if(Iteration > 0)
                        {
                            if(*PrevClusterIndexPtr != ClusterIndex)
                            {
                                IndexChangeOccurred = true;
                            }
                        }
                        *PrevClusterIndexPtr = ClusterIndex;
                        
                        TexelPtr++;
                    }
                    
                    Row += Resized.Pitch;
                }
                
                CommitObservations(ClusterGroup);
                if((Iteration > 0) &&
                   !IndexChangeOccurred)
                {
                    break;
                }
            }
            
            v3 FocalColor = V3i(0, 0, 0);
            switch(SortType)
            {
#if PALETTIZE_CIELAB
                case SortType_Red:
                {
                    FocalColor =
                        V3(53.23288178584245f, 80.10930952982204f, 67.22006831026425f);
                } break;
                
                case SortType_Green:
                {
                    FocalColor =
                        V3(87.73703347354422f, -86.18463649762525f, 83.18116474777854);
                } break;
                
                case SortType_Blue:
                {
                    FocalColor =
                        V3(32.302586667249486, 79.19666178930935, -107.86368104495168);
                } break;
#else
                case SortType_Red:
                {
                    FocalColor = V3i(1, 0, 0);
                } break;
                
                case SortType_Green:
                {
                    FocalColor = V3i(0, 1, 0);
                } break;
                
                case SortType_Blue:
                {
                    FocalColor = V3i(0, 0, 1);
                } break;
#endif
            }
            for(int Outer = 0;
                Outer < ClusterGroup->ClusterCount;
                Outer++)
            {
                b32 SwapOccurred = false;
                
                for(int Inner = 0;
                    Inner < (ClusterGroup->ClusterCount - 1);
                    Inner++)
                {
                    cluster* ClusterA = (ClusterGroup->Clusters + Inner);
                    cluster* ClusterB = (ClusterGroup->Clusters + (Inner + 1));
                    
                    if(SortType == SortType_Weight)
                    {
                        Assert((FocalColor.x == 0.0f) && (FocalColor.y == 0.0f) && (FocalColor.z == 0.0f));
                        
                        if(ClusterB->TotalObservationCount > ClusterA->TotalObservationCount)
                        {
                            cluster Swap = *ClusterA;
                            *ClusterA = *ClusterB;
                            *ClusterB = Swap;
                            
                            SwapOccurred = true;
                        }
                    }
                    else
                    {
                        Assert((SortType == SortType_Red) || (SortType == SortType_Green) || (SortType == SortType_Blue));
                        
                        f32 DistSquaredToColorA =
                            LengthSquared(FocalColor - ClusterA->Centroid);
                        f32 DistSquaredToColorB =
                            LengthSquared(FocalColor - ClusterB->Centroid);
                        if(DistSquaredToColorB < DistSquaredToColorA)
                        {
                            cluster Swap = *ClusterA;
                            *ClusterA = *ClusterB;
                            *ClusterB = Swap;
                            
                            SwapOccurred = true;
                        }
                    }
                }
                
                if(!SwapOccurred)
                {
                    break;
                }
            }
            
            int PaletteWidth = 512;
            int PaletteHeight = 64;
            
            u8* ScanLine = (u8*)malloc(sizeof(u32)*PaletteWidth);
            
            // @Features: Sort colors as best you can before generating
            // the scan line?
            u32* Row = (u32*)ScanLine;
            for(int ClusterIndex = 0;
                ClusterIndex < ClusterGroup->ClusterCount;
                ClusterIndex++)
            {
                cluster* Cluster = (ClusterGroup->Clusters + ClusterIndex);
                f32 Weight = SafeRatio0((f32)Cluster->TotalObservationCount,
                                        (f32)ClusterGroup->TotalObservationCount);
                int ClusterPixelWidth = RoundToInt(Weight*PaletteWidth);
#if PALETTIZE_CIELAB
                u32 CentroidColor = PackCIELABToRGBA(Cluster->Centroid);
#else
                u32 CentroidColor = PackRGBA(Cluster->Centroid);
#endif
                while(ClusterPixelWidth--)
                {
                    // @Robustness: A counter of how many pixels we've
                    // written out (and breaking out if we've gone too far)
                    // could prevent heap corruption bugs
                    *Row++ = CentroidColor;
                }
            }
            
            bitmap Palette = MakeEmptyBitmap(PaletteWidth, PaletteHeight);
            u8* DestRow = (u8*)Palette.Address;
            for(int Y = 0;
                Y < PaletteHeight;
                Y++)
            {
                u32* SourceTexelPtr = (u32*)ScanLine;
                u32* DestTexelPtr = (u32*)DestRow;
                for(int X = 0;
                    X < PaletteWidth;
                    X++)
                {
                    *DestTexelPtr++ = *SourceTexelPtr++;
                }
                
                DestRow += Palette.Pitch;
            }
            ExportBMP(Palette, "palette.bmp");
        }
        else
        {
            fprintf(stderr, "stb_image failed: %s\n", stbi_failure_reason());
        }
    }
    else
    {
        fprintf(stderr, "%s [image path] [cluster count] [seed] [sort type]\n", Args[0]);
    }
    
    return(0);
}
