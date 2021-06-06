#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma warning(pop)

#include "palettize.h"

static palettize_config
ParseCommandLine(int ArgCount, char **Args)
{
    palettize_config Config = {};

    // @Refactor: Should this be abstracted into a helper?
    time_t Timestamp = time(0);
    tm *Time = gmtime(&Timestamp);
    u32 Seed = (Time->tm_year +
                Time->tm_mon*Time->tm_mday -
                Time->tm_hour*Time->tm_min +
                Time->tm_sec);

    Config.SourcePath = 0;
    Config.ClusterCount = 5;
    Config.Seed = Seed;
    Config.SortType = SortType_Weight;
    Config.IterationCount = 300;
    Config.DestPath = "palette.bmp";

    if(ArgCount > 1)
    {
        Config.SourcePath = Args[1];
    }
    if(ArgCount > 2)
    {
        int RequestedClusterCount = atoi(Args[2]);
        Config.ClusterCount = Clampi(1, RequestedClusterCount, 64);
    }
    if(ArgCount > 3)
    {
        Config.Seed = (u32)atoi(Args[3]);
    }
    if(ArgCount > 4)
    {
        char *SortTypeString = Args[4];
        if(StringsMatch(SortTypeString, "red", false))
        {
            Config.SortType = SortType_Red;
        }
        else if(StringsMatch(SortTypeString, "green", false))
        {
            Config.SortType = SortType_Green;
        }
        else if(StringsMatch(SortTypeString, "blue", false))
        {
            Config.SortType = SortType_Blue;
        }
    }
    if(ArgCount > 5)
    {
        Config.IterationCount = (int)atoi(Args[5]);
    }
    if(ArgCount > 6)
    {
        Config.DestPath = Args[6];
    }

    return(Config);
}

static bitmap
AllocateBitmap(int Width, int Height)
{
    bitmap Result;
    Result.Memory = malloc(sizeof(u32)*Width*Height);
    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = sizeof(u32)*Width;
    
    return(Result);
}

static bitmap
LoadScaledBitmap(char *Path, f32 MaxDim)
{
    bitmap Result = {};

    int SourceWidth;
    int SourceHeight;
    void *SourceMemory = stbi_load(Path, &SourceWidth, &SourceHeight, 0, sizeof(u32));
    if(SourceMemory)
    {
        f32 ScaleFactor = MaxDim / (f32)Maximum(SourceWidth, SourceHeight);
        int ScaledWidth = RoundToInt(SourceWidth*ScaleFactor);
        int ScaledHeight = RoundToInt(SourceHeight*ScaleFactor);

        Result.Width = ScaledWidth;
        Result.Height = ScaledHeight;
        Result.Pitch = sizeof(u32)*ScaledWidth;
        Result.Memory = malloc(sizeof(u32)*ScaledWidth*ScaledHeight);

        stbi_image_free(SourceMemory);
    }
    else
    {
        fprintf(stderr, "stb_image failed: %s\n", stbi_failure_reason());
    }

    return(Result);
}

static u32
SampleNearestNeighbor(bitmap Bitmap, f32 U, f32 V)
{
    Assert((0.0f <= U) && (U <= 1.0f));
    Assert((0.0f <= V) && (V <= 1.0f));
    
    int SampleX = RoundToInt(U*((f32)Bitmap.Width - 1.0f));
    int SampleY = RoundToInt(V*((f32)Bitmap.Height - 1.0f));

    Assert((0 <= SampleX) && (SampleX < Bitmap.Width));
    Assert((0 <= SampleY) && (SampleY < Bitmap.Height));
    
    u32 Sample = *(u32 *)GetBitmapPtr(Bitmap, SampleX, SampleY);
    
    return(Sample);
}

static void
ResizeBitmap(bitmap Source, bitmap Dest)
{
    int MinX = 0;
    int MinY = 0;
    int MaxX = Dest.Width;
    int MaxY = Dest.Height;
    
    u8 *Row = (u8 *)GetBitmapPtr(Dest, MinX, MinY);
    for(int Y = MinY;
        Y < MaxY;
        ++Y)
    {
        u32* TexelPtr = (u32 *)Row;
        for(int X = MinX;
            X < MaxX;
            ++X)
        {
            f32 U = (f32)X / ((f32)MaxX - 1.0f);
            f32 V = (f32)Y / ((f32)MaxY - 1.0f);

            u32 Sample = SampleNearestNeighbor(Source, U, V);
            
            *TexelPtr++ = Sample;
        }
        
        Row += Dest.Pitch;
    }
}

static void
ClearClusterObservations(cluster *Cluster)
{
    Cluster->ObservationSum = V3i(0, 0, 0);
    Cluster->ObservationCount = 0;
}

static void
SeedCluster(cluster_group *ClusterGroup, cluster *Cluster)
{
    u32 RandomSampleX =
        RandomU32Between(&ClusterGroup->Entropy, 0, (u32)(ClusterGroup->Bitmap.Width - 1));
    u32 RandomSampleY =
        RandomU32Between(&ClusterGroup->Entropy, 0, (u32)(ClusterGroup->Bitmap.Height - 1));
    u32 RandomSample = *(u32 *)GetBitmapPtr(ClusterGroup->Bitmap, RandomSampleX, RandomSampleY);
    
    Cluster->Centroid = UnpackRGBAToCIELAB(RandomSample);
}

static cluster_group *
AllocateClusterGroup(bitmap Bitmap, u32 Seed, int ClusterCount)
{
    cluster_group *ClusterGroup = (cluster_group *)malloc(sizeof(cluster_group));
    
    ClusterGroup->Entropy = SeedSeries(Seed);
    ClusterGroup->Bitmap = Bitmap;
    ClusterGroup->ClusterCount = ClusterCount;
    ClusterGroup->Clusters = (cluster *)malloc(sizeof(cluster)*ClusterCount); 
    ClusterGroup->TotalObservationCount = 0;

    for(int ClusterIndex = 0;
        ClusterIndex < ClusterGroup->ClusterCount;
        ++ClusterIndex)
    {
        cluster* Cluster = ClusterGroup->Clusters + ClusterIndex;
        Cluster->TotalObservationCount = 0;
        ClearClusterObservations(Cluster);
        SeedCluster(ClusterGroup, Cluster);
    }
    
    return(ClusterGroup);
}

static u32
AddObservation(cluster_group *ClusterGroup, v3 C)
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
    
    // Returning the index of the closest cluster for our early out in the loop
    // where this function is called
    u32 Result = (u32)(ClosestCluster - ClusterGroup->Clusters);
    Assert(Result < (u32)ClusterGroup->ClusterCount);
    
    return(Result);
}

static void
CommitObservations(cluster_group *ClusterGroup)
{
    for(int ClusterIndex = 0;
        ClusterIndex < ClusterGroup->ClusterCount;
        ClusterIndex++)
    {
        cluster *Cluster = (ClusterGroup->Clusters + ClusterIndex);
        
        if(Cluster->ObservationCount)
        {
            v3 NewCentroid = Cluster->Centroid + Cluster->ObservationSum;
            f32 InvOnePastObservationCount = 1.0f / ((f32)Cluster->ObservationCount + 1.0f);
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
ExportBMP(bitmap Bitmap, char *Path)
{
    FILE *File = fopen(Path, "wb");
    if(File)
    {
        // Swizzling the red and blue components of each texel because
        // that's how BMPs expect channels to be ordered
        u8 *Row = (u8 *)Bitmap.Memory;
        for(int Y = 0;
            Y < Bitmap.Height;
            Y++)
        {
            u32 *TexelPtr = (u32 *)Row;
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
        BitmapHeader.FileSize = sizeof(BitmapHeader) + BitmapSize;
        BitmapHeader.OffBits = sizeof(BitmapHeader);
        BitmapHeader.Size = 40; // sizeof(BITMAPINFOHEADER)
        BitmapHeader.Width = Bitmap.Width;
        BitmapHeader.Height = Bitmap.Height;
        BitmapHeader.Planes = 1;
        BitmapHeader.BitCount = 32;
        BitmapHeader.Compression = BI_RGB;
        
        fwrite(&BitmapHeader, sizeof(BitmapHeader), 1,  File);
        fwrite(Bitmap.Memory, BitmapSize, 1, File);
        
        fclose(File);
    }
}

int
main(int ArgCount, char **Args)
{
    if(ArgCount > 1)
    {
        palettize_config Config = ParseCommandLine(ArgCount, Args);
        
        // To improve performance, the source image is scaled such that its
        // largest dimension has a value of 100 pixels
        bitmap Bitmap = LoadScaledBitmap(Config.SourcePath, 100.0f);
        if(Bitmap.Memory)
        {
            cluster_group *ClusterGroup =
                AllocateClusterGroup(Bitmap, Config.Seed, Config.ClusterCount);
            
            bitmap LastClusterIndexBuffer = AllocateBitmap(Bitmap.Width, Bitmap.Height);
            for(int Iteration = 0;
                Iteration < Config.IterationCount;
                Iteration++)
            {
                b32 IndexChangeOccurred = false;

                int MinX = 0;
                int MinY = 0;
                int MaxX = Bitmap.Width;
                int MaxY = Bitmap.Height;
                
                u8 *Row = (u8 *)GetBitmapPtr(Bitmap, MinX, MinY);
                for(int Y = MinY;
                    Y < MaxY;
                    Y++)
                {
                    u32 *TexelPtr = (u32 *)Row;
                    for(int X = MinX;
                        X < MaxX;
                        X++)
                    {
                        u32 Texel = *TexelPtr;
                        
                        v3 TexelV3 = UnpackRGBAToCIELAB(Texel);
                        u32 ClusterIndex = AddObservation(ClusterGroup,
                                                          TexelV3);

                        u32 *PrevClusterIndexPtr =
                            (u32 *)GetBitmapPtr(LastClusterIndexBuffer, X, Y);
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
                    
                    Row += Bitmap.Pitch;
                }
                
                CommitObservations(ClusterGroup);
                if((Iteration > 0) &&
                   !IndexChangeOccurred)
                {
                    break;
                }
            }
      
            v3 FocalColor = V3i(0, 0, 0);
            switch(Config.SortType)
            {
                case SortType_Red:
                {
                    FocalColor = {53.23288178584245f,
                                  80.10930952982204f,
                                  67.22006831026425f};
                } break;
                
                case SortType_Green:
                {
                    FocalColor = {87.73703347354422f,
                                  -86.18463649762525f,
                                  83.18116474777854};
                } break;
                
                case SortType_Blue:
                {
                    FocalColor = {32.302586667249486,
                                  79.19666178930935,
                                  -107.86368104495168};
                } break;
            }

            for(int Outer = 0;
                Outer < ClusterGroup->ClusterCount;
                ++Outer)
            {
                b32 SwapOccurred = false;
                
                for(int Inner = 0;
                    Inner < (ClusterGroup->ClusterCount - 1);
                    ++Inner)
                {
                    cluster *ClusterA = ClusterGroup->Clusters + Inner;
                    cluster *ClusterB = ClusterGroup->Clusters + Inner + 1;
                    
                    if(Config.SortType == SortType_Weight)
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
                        Assert((Config.SortType == SortType_Red) || (Config.SortType == SortType_Green) || (Config.SortType == SortType_Blue));
                        
                        f32 DistSquaredToColorA = LengthSquared(FocalColor - ClusterA->Centroid);
                        f32 DistSquaredToColorB = LengthSquared(FocalColor - ClusterB->Centroid);
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
            
            u8 *ScanLine = (u8 *)malloc(sizeof(u32)*PaletteWidth);
            
            u32 *Row = (u32 *)ScanLine;
            for(int ClusterIndex = 0;
                ClusterIndex < ClusterGroup->ClusterCount;
                ClusterIndex++)
            {
                cluster *Cluster = ClusterGroup->Clusters + ClusterIndex;

                f32 Weight = SafeRatio0((f32)Cluster->TotalObservationCount,
                                        (f32)ClusterGroup->TotalObservationCount);
                int ClusterPixelWidth = RoundToInt(Weight*PaletteWidth);

                u32 CentroidColor = PackCIELABToRGBA(Cluster->Centroid);
                while(ClusterPixelWidth--)
                {
                    // @Robustness: Add a counter of how many pixels we've
                    // written out (and breaking out if we've gone too far) to
                    // prevent heap corruption bugs
                    *Row++ = CentroidColor;
                }
            }
   
            bitmap Palette = AllocateBitmap(PaletteWidth, PaletteHeight);
            u8 *DestRow = (u8 *)Palette.Memory;
            for(int Y = 0;
                Y < PaletteHeight;
                Y++)
            {
                u32 *SourceTexelPtr = (u32 *)ScanLine;
                u32 *DestTexelPtr = (u32 *)DestRow;
                for(int X = 0;
                    X < PaletteWidth;
                    X++)
                {
                    *DestTexelPtr++ = *SourceTexelPtr++;
                }
                
                DestRow += Palette.Pitch;
            }

            ExportBMP(Palette, Config.DestPath);
        }
        else
        {
            fprintf(stderr, "Error: Memory allocation failed for source bitmap\n");
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s [source path] [cluster count] [seed] [sort type] [iteration count] [dest path]\n", Args[0]);
    }
    
    return(0);
}
