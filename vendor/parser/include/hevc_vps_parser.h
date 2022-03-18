#pragma once

#include "vlc_bits.h"

typedef uint8_t  nal_u1_t;
typedef uint8_t  nal_u2_t;
typedef uint8_t  nal_u3_t;
typedef uint8_t  nal_u4_t;
typedef uint8_t  nal_u5_t;
typedef uint8_t  nal_u6_t;
typedef uint8_t  nal_u7_t;
typedef uint8_t  nal_u8_t;
typedef int32_t  nal_se_t;
typedef uint32_t nal_ue_t;

typedef struct
{
    nal_u2_t profile_space;
    nal_u1_t tier_flag;
    nal_u5_t profile_idc;
    uint32_t profile_compatibility_flag; /* nal_u1_t * 32 */
    nal_u1_t progressive_source_flag;
    nal_u1_t interlaced_source_flag;
    nal_u1_t non_packed_constraint_flag;
    nal_u1_t frame_only_constraint_flag;
    struct
    {
        nal_u1_t max_12bit_constraint_flag;
        nal_u1_t max_10bit_constraint_flag;
        nal_u1_t max_8bit_constraint_flag;
        nal_u1_t max_422chroma_constraint_flag;
        nal_u1_t max_420chroma_constraint_flag;
        nal_u1_t max_monochrome_constraint_flag;
        nal_u1_t intra_constraint_flag;
        nal_u1_t one_picture_only_constraint_flag;
        nal_u1_t lower_bit_rate_constraint_flag;
        nal_u1_t max_14bit_constraint_flag;
    } idc4to7;
    struct
    {
        nal_u1_t inbld_flag;
    } idc1to5;
} hevc_inner_profile_tier_level_t;

#define HEVC_MAX_SUBLAYERS 8
typedef struct
{
    hevc_inner_profile_tier_level_t general;
    nal_u8_t general_level_idc;
    uint8_t  sublayer_profile_present_flag;  /* nal_u1_t * 8 */
    uint8_t  sublayer_level_present_flag;    /* nal_u1_t * 8 */
    hevc_inner_profile_tier_level_t sub_layer[HEVC_MAX_SUBLAYERS];
    nal_u8_t sub_layer_level_idc[HEVC_MAX_SUBLAYERS];
} hevc_profile_tier_level_t;

struct hevc_video_parameter_set_t
{
    nal_u4_t vps_video_parameter_set_id;
    nal_u1_t vps_base_layer_internal_flag;
    nal_u1_t vps_base_layer_available_flag;
    nal_u6_t vps_max_layers_minus1;
    nal_u3_t vps_max_sub_layers_minus1;
    nal_u1_t vps_temporal_id_nesting_flag;

    hevc_profile_tier_level_t profile_tier_level;

    nal_u1_t vps_sub_layer_ordering_info_present_flag;
    struct
    {
        nal_ue_t dec_pic_buffering_minus1;
        nal_ue_t num_reorder_pics;
        nal_ue_t max_latency_increase_plus1;
    } vps_max[1 + HEVC_MAX_SUBLAYERS];

    nal_u6_t vps_max_layer_id;
    nal_ue_t vps_num_layer_set_minus1;
    // layer_id_included_flag; read but discarded

    nal_u1_t vps_timing_info_present_flag;
    uint32_t vps_num_units_in_tick;
    uint32_t vps_time_scale;

    /* incomplete */
};

static bool hevc_parse_inner_profile_tier_level_rbsp( bs_t *p_bs,
                                                      hevc_inner_profile_tier_level_t *p_in )
{
    if( bs_remain( p_bs ) < 88 )
        return false;

    p_in->profile_space = bs_read( p_bs, 2 );
    p_in->tier_flag = bs_read1( p_bs );
    p_in->profile_idc = bs_read( p_bs, 5 );
    p_in->profile_compatibility_flag = bs_read( p_bs, 32 );
    p_in->progressive_source_flag = bs_read1( p_bs );
    p_in->interlaced_source_flag = bs_read1( p_bs );
    p_in->non_packed_constraint_flag = bs_read1( p_bs );
    p_in->frame_only_constraint_flag = bs_read1( p_bs );

    if( ( p_in->profile_idc >= 4 && p_in->profile_idc <= 10 ) ||
        ( p_in->profile_compatibility_flag & 0x0F700000 ) )
    {
        p_in->idc4to7.max_12bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_10bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_8bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_422chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_420chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_monochrome_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.intra_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.one_picture_only_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.lower_bit_rate_constraint_flag = bs_read1( p_bs );
        if( p_in->profile_idc == 5 ||
            p_in->profile_idc == 9 ||
            p_in->profile_idc == 10 ||
           (p_in->profile_compatibility_flag & 0x08600000) )
        {
            p_in->idc4to7.max_14bit_constraint_flag = bs_read1( p_bs );
            bs_skip( p_bs, 33 );
        }
        else bs_skip( p_bs, 34 );
    }
    else if( p_in->profile_idc == 2 ||
            (p_in->profile_compatibility_flag & 0x20000000) )
    {
        bs_skip( p_bs, 7 );
        p_in->idc4to7.one_picture_only_constraint_flag = bs_read1( p_bs );
        bs_skip( p_bs, 35 );
    }
    else
    {
        bs_read( p_bs, 43 );
    }

    if( ( p_in->profile_idc >= 1 && p_in->profile_idc <= 5 ) ||
         p_in->profile_idc == 9 ||
        ( p_in->profile_compatibility_flag & 0x7C400000 ) )
        p_in->idc1to5.inbld_flag = bs_read1( p_bs );
    else
        bs_skip( p_bs, 1 );

    return true;
}

static bool hevc_parse_profile_tier_level_rbsp( bs_t *p_bs, bool profile_present,
                                                uint8_t max_num_sub_layers_minus1,
                                                hevc_profile_tier_level_t *p_ptl )
{
    if( profile_present && !hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->general ) )
        return false;

    if( bs_remain( p_bs ) < 8)
        return false;

    p_ptl->general_level_idc = bs_read( p_bs, 8 );

    if( max_num_sub_layers_minus1 > 0 )
    {
        if( bs_remain( p_bs ) < 16 )
            return false;

        for( uint8_t i=0; i< 8; i++ )
        {
            if( i < max_num_sub_layers_minus1 )
            {
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_profile_present_flag |= (0x80 >> i);
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_level_present_flag |= (0x80 >> i);
            }
            else
                bs_read( p_bs, 2 );
        }

        for( uint8_t i=0; i < max_num_sub_layers_minus1; i++ )
        {
            if( ( p_ptl->sublayer_profile_present_flag & (0x80 >> i) ) &&
                ! hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->sub_layer[i] ) )
                return false;

            if( p_ptl->sublayer_profile_present_flag & (0x80 >> i) )
            {
                if( bs_remain( p_bs ) < 8 )
                    return false;
                p_ptl->sub_layer_level_idc[i] = bs_read( p_bs, 8 );
            }
        }
    }

    return true;
}

static bool hevc_parse_video_parameter_set_rbsp( bs_t *p_bs,
                                                 hevc_video_parameter_set_t *p_vps )
{
    if( bs_remain( p_bs ) < 134 )
        return false;

    p_vps->vps_video_parameter_set_id = bs_read( p_bs, 4 );
    p_vps->vps_base_layer_internal_flag = bs_read1( p_bs );
    p_vps->vps_base_layer_available_flag = bs_read1( p_bs );
    p_vps->vps_max_layers_minus1 = bs_read( p_bs, 6 );
    p_vps->vps_max_sub_layers_minus1 = bs_read( p_bs, 3 );
    p_vps->vps_temporal_id_nesting_flag = bs_read1( p_bs );
    bs_skip( p_bs, 16 );

    if( !hevc_parse_profile_tier_level_rbsp( p_bs, true, p_vps->vps_max_sub_layers_minus1,
                                            &p_vps->profile_tier_level ) )
        return false;

    p_vps->vps_sub_layer_ordering_info_present_flag = bs_read1( p_bs );
    for( unsigned i= (p_vps->vps_sub_layer_ordering_info_present_flag ?
                      0 : p_vps->vps_max_sub_layers_minus1);
         i<= p_vps->vps_max_sub_layers_minus1; i++ )
    {
        p_vps->vps_max[i].dec_pic_buffering_minus1 = bs_read_ue( p_bs );
        p_vps->vps_max[i].num_reorder_pics = bs_read_ue( p_bs );
        p_vps->vps_max[i].max_latency_increase_plus1 = bs_read_ue( p_bs );
    }
    if( bs_remain( p_bs ) < 10 )
        return false;

    p_vps->vps_max_layer_id = bs_read( p_bs, 6 );
    p_vps->vps_num_layer_set_minus1 = bs_read_ue( p_bs );
    // layer_id_included_flag; read but discarded
    bs_skip( p_bs, p_vps->vps_num_layer_set_minus1 * (p_vps->vps_max_layer_id + 1) );
    if( bs_remain( p_bs ) < 2 )
        return false;

    p_vps->vps_timing_info_present_flag = bs_read1( p_bs );
    if( p_vps->vps_timing_info_present_flag )
    {
        p_vps->vps_num_units_in_tick = bs_read( p_bs, 32 );
        p_vps->vps_time_scale = bs_read( p_bs, 32 );
    }
    /* parsing incomplete */

    if( bs_remain( p_bs ) < 1 )
        return false;

    return true;
}

void hevc_rbsp_release_vps( hevc_video_parameter_set_t *p_vps )
{
    free( p_vps );
}

#define IMPL_hevc_generic_decode( name, hevctype, decode, release ) \
    hevctype * name( const uint8_t *p_buf, size_t i_buf, bool b_escaped ) \
    { \
        hevctype *p_hevctype = (hevctype *) calloc(1, sizeof(hevctype)); \
        if(likely(p_hevctype)) \
        { \
            bs_t bs; \
            struct hxxx_bsfw_ep3b_ctx_s bsctx; \
            if( b_escaped ) \
            { \
                hxxx_bsfw_ep3b_ctx_init( &bsctx ); \
                bs_init_custom( &bs, p_buf, i_buf, &hxxx_bsfw_ep3b_callbacks, &bsctx );\
            } \
            else bs_init( &bs, p_buf, i_buf ); \
            bs_skip( &bs, 7 ); /* nal_unit_header */ \
            uint8_t i_nuh_layer_id = bs_read( &bs, 6 ); \
            bs_skip( &bs, 3 ); /* !nal_unit_header */ \
            if( i_nuh_layer_id > 62 || !decode( &bs, p_hevctype ) ) \
            { \
                release( p_hevctype ); \
                p_hevctype = NULL; \
            } \
        } \
        return p_hevctype; \
    }

hevc_video_parameter_set_t *    hevc_decode_vps( const uint8_t *, size_t, bool );

IMPL_hevc_generic_decode( hevc_decode_vps, hevc_video_parameter_set_t,
hevc_parse_video_parameter_set_rbsp, hevc_rbsp_release_vps )

static int hevc_get_max_num_reorder(uint8_t *p_buffer, size_t i_buffer) {
    hevc_video_parameter_set_t * p_vps = hevc_decode_vps(p_buffer, i_buffer, true);
    if( !p_vps ) {
        return -1;
    }
    int num = p_vps->vps_max[p_vps->vps_max_sub_layers_minus1/* HighestTid */].num_reorder_pics;
    hevc_rbsp_release_vps(p_vps);
    p_vps = NULL;
    return num;
}
