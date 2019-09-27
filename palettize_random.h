#ifndef PALETTIZE_RANDOM_H
#define PALETTIZE_RANDOM_H

struct random_series
{
    u32 Seed;
};

inline random_series
SeedSeries(u32 Seed)
{
    random_series Result;
    Result.Seed = Seed;
    
    return(Result);
}

inline u32
RandomU32(random_series* Series)
{
    // NOTE: 32-bit xorshift implementation
    u32 Result = Series->Seed;
	Result ^= (Result << 13);
	Result ^= (Result >> 17);
    Result ^= (Result << 5);
    
    Series->Seed = Result;
    
    return(Result);
}

inline u32
RandomU32Between(random_series* Series, u32 Min, u32 Max)
{
    u32 Range = (Max - Min);
    u32 Random = RandomU32(Series);
    u32 Result = (Min + (Random % Range));
    Assert((Min <= Result) && (Result <= Max));
    return(Result);
}

#endif
