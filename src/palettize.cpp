#include <stdio.h>
#include <stdlib.h>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma warning(pop)

#include "palettize.h"

static Palettize_Config parse_command_line(int argc, char **argv) {
    Palettize_Config config = {};
    config.source_path = 0;
    config.num_clusters = 5;
    config.seed = 0xFACADE;
    config.sort_type = SORT_TYPE_WEIGHT;
    config.iteration_count = 300;
    config.dest_path = "palette.bmp";

    if (argc > 1) {
        config.source_path = argv[1];
    }
    if (argc > 2) {
        config.num_clusters = clampi(1, atoi(argv[2]), 64);
    }
    if (argc > 3) {
        config.seed = (u32)atoi(argv[3]);
    }
    if (argc > 4) {
        char *sort_type_string = argv[4];
        if (strings_match(sort_type_string, "red", false)) {
            config.sort_type = SORT_TYPE_RED;
        } else if(strings_match(sort_type_string, "green", false)) {
            config.sort_type = SORT_TYPE_GREEN;
        } else if(strings_match(sort_type_string, "blue", false)) {
            config.sort_type = SORT_TYPE_BLUE;
        }
    }
    if (argc > 5) {
        config.iteration_count = (int)atoi(argv[5]);
    }
    if (argc > 6) {
        config.dest_path = argv[6];
    }

    return(config);
}

static Bitmap load_bitmap(char *path) {
    Bitmap result;
    result.memory = stbi_load(path, &result.width, &result.height, 0, 4);
    result.pitch = sizeof(u32)*result.width;
    
    return(result);
}

static Bitmap allocate_bitmap(int width, int height) {
    Bitmap result;
    result.memory = malloc(sizeof(u32)*width*height);
    result.width = width;
    result.height = height;
    result.pitch = sizeof(u32)*width;
    
    return(result);
}

static u32 sample_nearest_neighbor(Bitmap bitmap, f32 u, f32 v) {
    Assert((0.0f <= u) && (u <= 1.0f));
    Assert((0.0f <= v) && (v <= 1.0f));
    
    int sample_x = round_to_int(u*((f32)bitmap.width - 1.0f));
    int sample_y = round_to_int(v*((f32)bitmap.height - 1.0f));

    Assert((0 <= sample_x) && (sample_x < bitmap.width));
    Assert((0 <= sample_y) && (sample_y < bitmap.height));
    
    u32 sample = *(u32 *)Get_Bitmap_Ptr(bitmap, sample_x, sample_y);
    
    return(sample);
}

static void resize_bitmap(Bitmap source, Bitmap dest) {
    int min_x = 0;
    int min_y = 0;
    int max_x = dest.width;
    int max_y = dest.height;
    
    u8 *row = (u8 *)Get_Bitmap_Ptr(dest, min_x, min_y);
    for (int y = min_y; y < max_y; y++) {
        u32 *texel_ptr = (u32 *)row;
        for (int x = min_x; x < max_x; x++) {
            f32 u = (f32)x / ((f32)max_x - 1.0f);
            f32 v = (f32)y / ((f32)max_y - 1.0f);

            u32 sample = sample_nearest_neighbor(source, u, v);
            
            *texel_ptr++ = sample;
        }
        
        row += dest.pitch;
    }
}

static void clear_cluster_observations(Cluster *cluster) {
    cluster->observation_sum = V3i(0, 0, 0);
    cluster->num_observations = 0;
}

static void seed_cluster(Cluster_Group *cluster_group, Cluster *cluster) {
    u32 random_sample_x =
        random_u32_between(&cluster_group->entropy, 0, (u32)(cluster_group->bitmap.width - 1));
    u32 random_sample_y =
        random_u32_between(&cluster_group->entropy, 0, (u32)(cluster_group->bitmap.height - 1));
    u32 random_sample = *(u32 *)Get_Bitmap_Ptr(cluster_group->bitmap, random_sample_x, random_sample_y);
    
    cluster->centroid = unpack_rgba_to_cielab(random_sample);
}

static Cluster_Group *allocate_cluster_group(Bitmap bitmap, u32 seed, int num_clusters) {
    Cluster_Group *cluster_group = (Cluster_Group *)malloc(sizeof(Cluster_Group));
    
    cluster_group->entropy = seed_series(seed);
    cluster_group->bitmap = bitmap;
    cluster_group->num_clusters = num_clusters;
    cluster_group->clusters = (Cluster *)malloc(sizeof(Cluster)*num_clusters); 
    cluster_group->total_observation_count = 0;

    for (int i = 0; i < cluster_group->num_clusters; i++) {
        Cluster *cluster = &cluster_group->clusters[i];
        cluster->total_observation_count = 0;
        clear_cluster_observations(cluster);
        seed_cluster(cluster_group, cluster);
    }
    
    return(cluster_group);
}

static u32 add_observation(Cluster_Group *cluster_group, Vector3 C) {
    f32 closest_dist_sq = F32_MAX;
    Cluster* closest_cluster = 0;
    for (int i = 0; i < cluster_group->num_clusters; i++)
    {
        Cluster* cluster = &cluster_group->clusters[i];
        f32 d = length_squared(cluster->centroid - C);
        if(d < closest_dist_sq)
        {
            closest_dist_sq = d;
            closest_cluster = cluster;
        }
    }
    Assert(closest_cluster);
    
    closest_cluster->observation_sum += C;
    closest_cluster->num_observations++;
    closest_cluster->total_observation_count++;
    
    cluster_group->total_observation_count++;
    
    // Returning the index of the closest cluster for our early out in the loop
    // where this function is called
    u32 result = (u32)(closest_cluster - cluster_group->clusters);
    Assert(result < (u32)cluster_group->num_clusters);
    
    return(result);
}

static void commit_observations(Cluster_Group *cluster_group) {
    for (int i = 0; i < cluster_group->num_clusters; i++) {
        Cluster *cluster = &cluster_group->clusters[i];
        
        if (cluster->num_observations) {
            Vector3 new_centroid = cluster->centroid + cluster->observation_sum;
            f32 inv_one_past_num_observations = 1.0f / ((f32)cluster->num_observations + 1.0f);
            new_centroid *= inv_one_past_num_observations;
            cluster->centroid = new_centroid;
        } else {
            seed_cluster(cluster_group, cluster);
        }
        
        clear_cluster_observations(cluster);
    }
}

static void export_bmp(Bitmap bitmap, char *path) {
    FILE *file = fopen(path, "wb");
    if (file) {
        // Swizzling the red and blue components of each texel because
        // that's how BMPs expect channels to be ordered
        u8 *row = (u8 *)bitmap.memory;
        for (int y = 0; y < bitmap.height; y++) {
            u32 *texel_ptr = (u32 *)row;
            for (int x = 0; x < bitmap.width; x++) {
                u32 texel = *texel_ptr;
                u32 swizzled = ((((texel >> 0) & 0xFF) << 16) |
                                (((texel >> 8) & 0xFF) << 8) |
                                (((texel >> 16) & 0xFF) << 0) |
                                (((texel >> 24) & 0xFF) << 24));
                *texel_ptr++ = swizzled;
            }
            
            row += bitmap.pitch;
        }
        
        u32 bitmap_size = (u32)(bitmap.pitch*bitmap.height);
        
        Bitmap_Header bitmap_header = {};
        bitmap_header.type = 0x4D42;
        bitmap_header.file_size = sizeof(bitmap_header) + bitmap_size;
        bitmap_header.off_bits = sizeof(bitmap_header);
        bitmap_header.size = 40; // sizeof(BITMAPINFOHEADER)
        bitmap_header.width = bitmap.width;
        bitmap_header.height = bitmap.height;
        bitmap_header.planes = 1;
        bitmap_header.bit_count = 32;
        bitmap_header.compression = BI_RGB;
        
        fwrite(&bitmap_header, sizeof(bitmap_header), 1,  file);
        fwrite(bitmap.memory, bitmap_size, 1, file);
        
        fclose(file);
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        Palettize_Config config = parse_command_line(argc, argv);
        
        Bitmap source_bitmap = load_bitmap(config.source_path);
        if (source_bitmap.memory) {
            // To improve performance, the source image is resized so that its
            // largest dimension has a value of 100 pixels
            f32 dim_factor = 100.0f / (f32)Maximum(source_bitmap.width, source_bitmap.height);
            Bitmap bitmap = allocate_bitmap(round_to_int(source_bitmap.width*dim_factor),
                                            round_to_int(source_bitmap.height*dim_factor));
            resize_bitmap(source_bitmap, bitmap);
            
            Cluster_Group *cluster_group = allocate_cluster_group(bitmap,
                                                                  config.seed,
                                                                  config.num_clusters);
            
            Bitmap last_cluster_index_buffer = allocate_bitmap(bitmap.width, bitmap.height);
            for (int iteration = 0; iteration < config.iteration_count; iteration++) {
                b32 index_changed = false;

                int min_x = 0;
                int min_y = 0;
                int max_x = bitmap.width;
                int max_y = bitmap.height;
                
                u8 *row = (u8 *)Get_Bitmap_Ptr(bitmap, min_x, min_y);
                for (int Y = min_y; Y < max_y; Y++) {
                    u32 *texel_ptr = (u32 *)row;
                    for (int X = min_x; X < max_x; X++) {
                        u32 Texel = *texel_ptr;
                        
                        Vector3 TexelV3 = unpack_rgba_to_cielab(Texel);
                        u32 ClusterIndex = add_observation(cluster_group,
                                                          TexelV3);

                        u32 *PrevClusterIndexPtr =
                            (u32 *)Get_Bitmap_Ptr(last_cluster_index_buffer, X, Y);
                        if (iteration > 0) {
                            if (*PrevClusterIndexPtr != ClusterIndex) {
                                index_changed = true;
                            }
                        }
                        *PrevClusterIndexPtr = ClusterIndex;
                        
                        texel_ptr++;
                    }
                    
                    row += bitmap.pitch;
                }
                
                commit_observations(cluster_group);
                if ((iteration > 0) &&
                   !index_changed) {
                    break;
                }
            }
      
            Vector3 focal_color = V3i(0, 0, 0);
            switch (config.sort_type) {
            case SORT_TYPE_RED:
                focal_color = {53.23288178584245f,
                               80.10930952982204f,
                               67.22006831026425f};
            break;
            case SORT_TYPE_GREEN:
                focal_color = {87.73703347354422f,
                               -86.18463649762525f,
                               83.18116474777854f};
            break;
            case SORT_TYPE_BLUE: 
                focal_color = {32.302586667249486f,
                               79.19666178930935f,
                               -107.86368104495168f};
            break;
            }

            for (int outer = 0; outer < cluster_group->num_clusters; outer++) {
                b32 swapped = false;
                
                for (int inner = 0; inner < (cluster_group->num_clusters - 1); inner++) {
                    Cluster *cluster_a = &cluster_group->clusters[inner];
                    Cluster *cluster_b = &cluster_group->clusters[inner + 1];
                    
                    if(config.sort_type == SORT_TYPE_WEIGHT)
                    {
                        Assert((focal_color.x == 0.0f) && (focal_color.y == 0.0f) && (focal_color.z == 0.0f));
                        
                        if(cluster_b->total_observation_count > cluster_a->total_observation_count)
                        {
                            Cluster swap = *cluster_a;
                            *cluster_a = *cluster_b;
                            *cluster_b = swap;
                            
                            swapped = true;
                        }
                    }
                    else
                    {
                        Assert((config.sort_type == SORT_TYPE_RED) || (config.sort_type == SORT_TYPE_GREEN) || (config.sort_type == SORT_TYPE_BLUE));
                        
                        f32 dist_to_color_a_sq = length_squared(focal_color - cluster_a->centroid);
                        f32 dist_to_color_b_sq = length_squared(focal_color - cluster_b->centroid);
                        if (dist_to_color_b_sq < dist_to_color_a_sq) {
                            Cluster swap = *cluster_a;
                            *cluster_a = *cluster_b;
                            *cluster_b = swap;
                            
                            swapped = true;
                        }
                    }
                }
                
                if(!swapped)
                {
                    break;
                }
            }
    
            int palette_width = 512;
            int palette_height = 64;
            
            u8 *scan_line = (u8 *)malloc(sizeof(u32)*palette_width);
            
            u32 *row = (u32 *)scan_line;
            for (int i = 0; i < cluster_group->num_clusters; i++) {
                Cluster *cluster = &cluster_group->clusters[i];

                f32 weight = safe_ratio_0((f32)cluster->total_observation_count,
                                          (f32)cluster_group->total_observation_count);
                int cluster_pixel_width = round_to_int(weight*palette_width);

                u32 centroid_color = pack_cielab_to_rgba(cluster->centroid);
                while (cluster_pixel_width--) {
                    // @Robustness: Add a counter of how many pixels we've
                    // written out (and breaking out if we've gone too far) to
                    // prevent heap corruption bugs
                    *row++ = centroid_color;
                }
            }
   
            Bitmap palette = allocate_bitmap(palette_width, palette_height);
            u8 *dest_row = (u8 *)palette.memory;
            for (int y = 0; y < palette_height; y++) {
                u32 *src_texel_ptr = (u32 *)scan_line;
                u32 *dst_texel_ptr = (u32 *)dest_row;
                for (int X = 0; X < palette_width; X++) {
                    *dst_texel_ptr++ = *src_texel_ptr++;
                }
                
                dest_row += palette.pitch;
            }

            export_bmp(palette, config.dest_path);
        }
        else
        {
            fprintf(stderr, "stb_image failed: %s\n", stbi_failure_reason());
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s [image path] [cluster count] [seed] [sort type] [iteration count]\n", argv[0]);
    }
    
    return(0);
}
