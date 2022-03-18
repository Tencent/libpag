#pragma once

#include "vlc_bits.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define H264_SPS_ID_MAX (31)
#define H264_CONSTRAINT_SET_FLAG(N) (0x80 >> N)

#define PROFILE_H264_BASELINE             66
#define PROFILE_H264_MAIN                 77
#define PROFILE_H264_EXTENDED             88
#define PROFILE_H264_HIGH                 100
#define PROFILE_H264_HIGH_10              110
#define PROFILE_H264_HIGH_422             122
#define PROFILE_H264_HIGH_444             144
#define PROFILE_H264_HIGH_444_PREDICTIVE  244

#define PROFILE_H264_CAVLC_INTRA          44
#define PROFILE_H264_SVC_BASELINE         83
#define PROFILE_H264_SVC_HIGH             86
#define PROFILE_H264_MVC_STEREO_HIGH      128
#define PROFILE_H264_MVC_MULTIVIEW_HIGH   118

#define PROFILE_H264_MFC_HIGH                          134
#define PROFILE_H264_MVC_MULTIVIEW_DEPTH_HIGH          138
#define PROFILE_H264_MVC_ENHANCED_MULTIVIEW_DEPTH_HIGH 139

/* Annex E: Colour primaries */
enum hxxx_colour_primaries
{
    HXXX_PRIMARIES_RESERVED0        = 0,
    HXXX_PRIMARIES_BT709            = 1,
    HXXX_PRIMARIES_UNSPECIFIED      = 2,
    HXXX_PRIMARIES_RESERVED3        = 3,
    HXXX_PRIMARIES_BT470M           = 4,
    HXXX_PRIMARIES_BT470BG          = 5,
    HXXX_PRIMARIES_BT601_525        = 6,
    HXXX_PRIMARIES_SMTPE_240M       = 7,
    HXXX_PRIMARIES_GENERIC_FILM     = 8,
    HXXX_PRIMARIES_BT2020           = 9,
    HXXX_PRIMARIES_SMPTE_ST_428     = 10,
};

/* Annex E: Transfer characteristics */
enum hxxx_transfer_characteristics
{
    HXXX_TRANSFER_RESERVED0         = 0,
    HXXX_TRANSFER_BT709             = 1,
    HXXX_TRANSFER_UNSPECIFIED       = 2,
    HXXX_TRANSFER_RESERVED3         = 3,
    HXXX_TRANSFER_BT470M            = 4,
    HXXX_TRANSFER_BT470BG           = 5,
    HXXX_TRANSFER_BT601_525         = 6,
    HXXX_TRANSFER_SMTPE_240M        = 7,
    HXXX_TRANSFER_LINEAR            = 8,
    HXXX_TRANSFER_LOG               = 9,
    HXXX_TRANSFER_LOG_SQRT          = 10,
    HXXX_TRANSFER_IEC61966_2_4      = 11,
    HXXX_TRANSFER_BT1361            = 12,
    HXXX_TRANSFER_IEC61966_2_1      = 13,
    HXXX_TRANSFER_BT2020_V14        = 14,
    HXXX_TRANSFER_BT2020_V15        = 15,
    HXXX_TRANSFER_SMPTE_ST_2084     = 16,
    HXXX_TRANSFER_SMPTE_ST_428      = 17,
    HXXX_TRANSFER_ARIB_STD_B67      = 18,
};

/* Annex E: Matrix coefficients */
enum hxxx_matrix_coeffs
{
    HXXX_MATRIX_IDENTITY            = 0,
    HXXX_MATRIX_BT709               = 1,
    HXXX_MATRIX_UNSPECIFIED         = 2,
    HXXX_MATRIX_RESERVED            = 3,
    HXXX_MATRIX_FCC                 = 4,
    HXXX_MATRIX_BT470BG             = 5,
    HXXX_MATRIX_BT601_525           = 6,
    HXXX_MATRIX_SMTPE_240M          = 7,
    HXXX_MATRIX_YCGCO               = 8,
    HXXX_MATRIX_BT2020_NCL          = 9,
    HXXX_MATRIX_BT2020_CL           = 10,
};

/* H264 Level limits from Table A-1 */
typedef struct
{
    unsigned i_max_dpb_mbs;
} h264_level_limits_t;

enum h264_level_numbers_e
{
    H264_LEVEL_NUMBER_1_B = 9, /* special level not following the 10x rule */
    H264_LEVEL_NUMBER_1   = 10,
    H264_LEVEL_NUMBER_1_1 = 11,
    H264_LEVEL_NUMBER_1_2 = 12,
    H264_LEVEL_NUMBER_1_3 = 13,
    H264_LEVEL_NUMBER_2   = 20,
    H264_LEVEL_NUMBER_2_1 = 21,
    H264_LEVEL_NUMBER_2_2 = 22,
    H264_LEVEL_NUMBER_3   = 30,
    H264_LEVEL_NUMBER_3_1 = 31,
    H264_LEVEL_NUMBER_3_2 = 32,
    H264_LEVEL_NUMBER_4   = 40,
    H264_LEVEL_NUMBER_4_1 = 41,
    H264_LEVEL_NUMBER_4_2 = 42,
    H264_LEVEL_NUMBER_5   = 50,
    H264_LEVEL_NUMBER_5_1 = 51,
    H264_LEVEL_NUMBER_5_2 = 52,
};

const struct
{
    const uint16_t i_level;
    const h264_level_limits_t limits;
} h264_levels_limits[] = {
    { H264_LEVEL_NUMBER_1_B, { 396 } },
    { H264_LEVEL_NUMBER_1,   { 396 } },
    { H264_LEVEL_NUMBER_1_1, { 900 } },
    { H264_LEVEL_NUMBER_1_2, { 2376 } },
    { H264_LEVEL_NUMBER_1_3, { 2376 } },
    { H264_LEVEL_NUMBER_2,   { 2376 } },
    { H264_LEVEL_NUMBER_2_1, { 4752 } },
    { H264_LEVEL_NUMBER_2_2, { 8100 } },
    { H264_LEVEL_NUMBER_3,   { 8100 } },
    { H264_LEVEL_NUMBER_3_1, { 18000 } },
    { H264_LEVEL_NUMBER_3_2, { 20480 } },
    { H264_LEVEL_NUMBER_4,   { 32768 } },
    { H264_LEVEL_NUMBER_4_1, { 32768 } },
    { H264_LEVEL_NUMBER_4_2, { 34816 } },
    { H264_LEVEL_NUMBER_5,   { 110400 } },
    { H264_LEVEL_NUMBER_5_1, { 184320 } },
    { H264_LEVEL_NUMBER_5_2, { 184320 } },
};

struct h264_sequence_parameter_set_t
{
    uint8_t i_id;
    uint8_t i_profile, i_level;
    uint8_t i_constraint_set_flags;
    /* according to avcC, 3 bits max for those */
    uint8_t i_chroma_idc;
    uint8_t i_bit_depth_luma;
    uint8_t i_bit_depth_chroma;
    uint8_t b_separate_colour_planes_flag;

    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    struct
    {
        uint32_t left_offset;
        uint32_t right_offset;
        uint32_t top_offset;
        uint32_t bottom_offset;
    } frame_crop;
    uint8_t frame_mbs_only_flag;
    uint8_t mb_adaptive_frame_field_flag;
    int i_log2_max_frame_num;
    int i_pic_order_cnt_type;
    int i_delta_pic_order_always_zero_flag;
    int32_t offset_for_non_ref_pic;
    int32_t offset_for_top_to_bottom_field;
    int i_num_ref_frames_in_pic_order_cnt_cycle;
    int32_t offset_for_ref_frame[255];
    int i_log2_max_pic_order_cnt_lsb;

    struct {
        bool b_valid;
        int i_sar_num, i_sar_den;
        struct {
            bool b_full_range;
            uint8_t i_colour_primaries;
            uint8_t i_transfer_characteristics;
            uint8_t i_matrix_coefficients;
        } colour;
        bool b_timing_info_present_flag;
        uint32_t i_num_units_in_tick;
        uint32_t i_time_scale;
        bool b_fixed_frame_rate;
        bool b_pic_struct_present_flag;
        bool b_hrd_parameters_present_flag; /* CpbDpbDelaysPresentFlag */
        uint8_t i_cpb_removal_delay_length_minus1;
        uint8_t i_dpb_output_delay_length_minus1;

        /* restrictions */
        uint8_t b_bitstream_restriction_flag;
        uint8_t i_max_num_reorder_frames;
    } vui;
};

struct h264_picture_parameter_set_t
{
    uint8_t i_id;
    uint8_t i_sps_id;
    uint8_t i_pic_order_present_flag;
    uint8_t i_redundant_pic_present_flag;
    uint8_t weighted_pred_flag;
    uint8_t weighted_bipred_idc;
};

static bool h264_parse_sequence_parameter_set_rbsp( bs_t *p_bs,
                                                    h264_sequence_parameter_set_t *p_sps )
{
    int i_tmp;

    int i_profile_idc = bs_read( p_bs, 8 );
    p_sps->i_profile = i_profile_idc;
    p_sps->i_constraint_set_flags = bs_read( p_bs, 8 );
    p_sps->i_level = bs_read( p_bs, 8 );
    /* sps id */
    uint32_t i_sps_id = bs_read_ue( p_bs );
    if( i_sps_id > H264_SPS_ID_MAX )
        return false;
    p_sps->i_id = i_sps_id;

    if( i_profile_idc == PROFILE_H264_HIGH ||
        i_profile_idc == PROFILE_H264_HIGH_10 ||
        i_profile_idc == PROFILE_H264_HIGH_422 ||
        i_profile_idc == PROFILE_H264_HIGH_444 || /* Old one, no longer on spec */
        i_profile_idc == PROFILE_H264_HIGH_444_PREDICTIVE ||
        i_profile_idc == PROFILE_H264_CAVLC_INTRA ||
        i_profile_idc == PROFILE_H264_SVC_BASELINE ||
        i_profile_idc == PROFILE_H264_SVC_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_MULTIVIEW_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_STEREO_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_MULTIVIEW_DEPTH_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_ENHANCED_MULTIVIEW_DEPTH_HIGH ||
        i_profile_idc == PROFILE_H264_MFC_HIGH )
    {
        /* chroma_format_idc */
        p_sps->i_chroma_idc = bs_read_ue( p_bs );
        if( p_sps->i_chroma_idc == 3 )
            p_sps->b_separate_colour_planes_flag = bs_read1( p_bs );
        else
            p_sps->b_separate_colour_planes_flag = 0;
        /* bit_depth_luma_minus8 */
        p_sps->i_bit_depth_luma = bs_read_ue( p_bs ) + 8;
        /* bit_depth_chroma_minus8 */
        p_sps->i_bit_depth_chroma = bs_read_ue( p_bs ) + 8;
        /* qpprime_y_zero_transform_bypass_flag */
        bs_skip( p_bs, 1 );
        /* seq_scaling_matrix_present_flag */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            for( int i = 0; i < ((3 != p_sps->i_chroma_idc) ? 8 : 12); i++ )
            {
                /* seq_scaling_list_present_flag[i] */
                i_tmp = bs_read( p_bs, 1 );
                if( !i_tmp )
                    continue;
                const int i_size_of_scaling_list = (i < 6 ) ? 16 : 64;
                /* scaling_list (...) */
                int i_lastscale = 8;
                int i_nextscale = 8;
                for( int j = 0; j < i_size_of_scaling_list; j++ )
                {
                    if( i_nextscale != 0 )
                    {
                        /* delta_scale */
                        i_tmp = bs_read_se( p_bs );
                        i_nextscale = ( i_lastscale + i_tmp + 256 ) % 256;
                        /* useDefaultScalingMatrixFlag = ... */
                    }
                    /* scalinglist[j] */
                    i_lastscale = ( i_nextscale == 0 ) ? i_lastscale : i_nextscale;
                }
            }
        }
    }
    else
    {
        p_sps->i_chroma_idc = 1; /* Not present == inferred to 4:2:0 */
        p_sps->i_bit_depth_luma = 8;
        p_sps->i_bit_depth_chroma = 8;
    }

    /* Skip i_log2_max_frame_num */
    p_sps->i_log2_max_frame_num = bs_read_ue( p_bs );
    if( p_sps->i_log2_max_frame_num > 12)
        p_sps->i_log2_max_frame_num = 12;
    /* Read poc_type */
    p_sps->i_pic_order_cnt_type = bs_read_ue( p_bs );
    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        /* skip i_log2_max_poc_lsb */
        p_sps->i_log2_max_pic_order_cnt_lsb = bs_read_ue( p_bs );
        if( p_sps->i_log2_max_pic_order_cnt_lsb > 12 )
            p_sps->i_log2_max_pic_order_cnt_lsb = 12;
    }
    else if( p_sps->i_pic_order_cnt_type == 1 )
    {
        p_sps->i_delta_pic_order_always_zero_flag = bs_read( p_bs, 1 );
        p_sps->offset_for_non_ref_pic = bs_read_se( p_bs );
        p_sps->offset_for_top_to_bottom_field = bs_read_se( p_bs );
        p_sps->i_num_ref_frames_in_pic_order_cnt_cycle = bs_read_ue( p_bs );
        if( p_sps->i_num_ref_frames_in_pic_order_cnt_cycle > 255 )
            return false;
        for( int i=0; i<p_sps->i_num_ref_frames_in_pic_order_cnt_cycle; i++ )
            p_sps->offset_for_ref_frame[i] = bs_read_se( p_bs );
    }
    /* i_num_ref_frames */
    bs_read_ue( p_bs );
    /* b_gaps_in_frame_num_value_allowed */
    bs_skip( p_bs, 1 );

    /* Read size */
    p_sps->pic_width_in_mbs_minus1 = bs_read_ue( p_bs );
    p_sps->pic_height_in_map_units_minus1 = bs_read_ue( p_bs );

    /* b_frame_mbs_only */
    p_sps->frame_mbs_only_flag = bs_read( p_bs, 1 );
    if( !p_sps->frame_mbs_only_flag )
        p_sps->mb_adaptive_frame_field_flag = bs_read( p_bs, 1 );

    /* b_direct8x8_inference */
    bs_skip( p_bs, 1 );

    /* crop */
    if( bs_read1( p_bs ) ) /* frame_cropping_flag */
    {
        p_sps->frame_crop.left_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.right_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.top_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.bottom_offset = bs_read_ue( p_bs );
    }

    /* vui */
    i_tmp = bs_read( p_bs, 1 );
    if( i_tmp )
    {
        p_sps->vui.b_valid = true;
        /* read the aspect ratio part if any */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            static const struct { int w, h; } sar[17] =
            {
                { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
                { 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
                { 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
                { 64, 33 }, { 160,99 }, {  4,  3 }, {  3,  2 },
                {  2,  1 },
            };
            int i_sar = bs_read( p_bs, 8 );
            int w, h;

            if( i_sar < 17 )
            {
                w = sar[i_sar].w;
                h = sar[i_sar].h;
            }
            else if( i_sar == 255 )
            {
                w = bs_read( p_bs, 16 );
                h = bs_read( p_bs, 16 );
            }
            else
            {
                w = 0;
                h = 0;
            }

            if( w != 0 && h != 0 )
            {
                p_sps->vui.i_sar_num = w;
                p_sps->vui.i_sar_den = h;
            }
            else
            {
                p_sps->vui.i_sar_num = 1;
                p_sps->vui.i_sar_den = 1;
            }
        }

        /* overscan */
        i_tmp = bs_read( p_bs, 1 );
        if ( i_tmp )
            bs_read( p_bs, 1 );

        /* video signal type */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            bs_read( p_bs, 3 );
            p_sps->vui.colour.b_full_range = bs_read( p_bs, 1 );
            /* colour desc */
            i_tmp = bs_read( p_bs, 1 );
            if ( i_tmp )
            {
                p_sps->vui.colour.i_colour_primaries = bs_read( p_bs, 8 );
                p_sps->vui.colour.i_transfer_characteristics = bs_read( p_bs, 8 );
                p_sps->vui.colour.i_matrix_coefficients = bs_read( p_bs, 8 );
            }
            else
            {
                p_sps->vui.colour.i_colour_primaries = HXXX_PRIMARIES_UNSPECIFIED;
                p_sps->vui.colour.i_transfer_characteristics = HXXX_TRANSFER_UNSPECIFIED;
                p_sps->vui.colour.i_matrix_coefficients = HXXX_MATRIX_UNSPECIFIED;
            }
        }

        /* chroma loc info */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            bs_read_ue( p_bs );
            bs_read_ue( p_bs );
        }

        /* timing info */
        p_sps->vui.b_timing_info_present_flag = bs_read( p_bs, 1 );
        if( p_sps->vui.b_timing_info_present_flag )
        {
            p_sps->vui.i_num_units_in_tick = bs_read( p_bs, 32 );
            p_sps->vui.i_time_scale = bs_read( p_bs, 32 );
            p_sps->vui.b_fixed_frame_rate = bs_read( p_bs, 1 );
        }

        /* Nal hrd & VC1 hrd parameters */
        p_sps->vui.b_hrd_parameters_present_flag = false;
        for ( int i=0; i<2; i++ )
        {
            i_tmp = bs_read( p_bs, 1 );
            if( i_tmp )
            {
                p_sps->vui.b_hrd_parameters_present_flag = true;
                uint32_t count = bs_read_ue( p_bs ) + 1;
                if( count > 31 )
                    return false;
                bs_read( p_bs, 4 );
                bs_read( p_bs, 4 );
                for( uint32_t j = 0; j < count; j++ )
                {
                    if( bs_remain( p_bs ) < 23 )
                        return false;
                    bs_read_ue( p_bs );
                    bs_read_ue( p_bs );
                    bs_read( p_bs, 1 );
                }
                bs_read( p_bs, 5 );
                p_sps->vui.i_cpb_removal_delay_length_minus1 = bs_read( p_bs, 5 );
                p_sps->vui.i_dpb_output_delay_length_minus1 = bs_read( p_bs, 5 );
                bs_read( p_bs, 5 );
            }
        }

        if( p_sps->vui.b_hrd_parameters_present_flag )
            bs_read( p_bs, 1 ); /* low delay hrd */

        /* pic struct info */
        p_sps->vui.b_pic_struct_present_flag = bs_read( p_bs, 1 );

        p_sps->vui.b_bitstream_restriction_flag = bs_read( p_bs, 1 );
        if( p_sps->vui.b_bitstream_restriction_flag )
        {
            bs_read( p_bs, 1 ); /* motion vector pic boundaries */
            bs_read_ue( p_bs ); /* max bytes per pic */
            bs_read_ue( p_bs ); /* max bits per mb */
            bs_read_ue( p_bs ); /* log2 max mv h */
            bs_read_ue( p_bs ); /* log2 max mv v */
            p_sps->vui.i_max_num_reorder_frames = bs_read_ue( p_bs );
            bs_read_ue( p_bs ); /* max dec frame buffering */
        }
    }

    return true;
}

void h264_release_sps( h264_sequence_parameter_set_t *p_sps )
{
    free( p_sps );
}

/* vlc_bits's bs_t forward callback for stripping emulation prevention three bytes */
static inline uint8_t *hxxx_bsfw_ep3b_to_rbsp( uint8_t *p, uint8_t *end, void *priv, size_t i_count )
{
    unsigned *pi_prev = (unsigned *) priv;
    for( size_t i=0; i<i_count; i++ )
    {
        if( ++p >= end )
            return p;

        *pi_prev = (*pi_prev << 1) | (!*p);

        if( *p == 0x03 &&
           ( p + 1 ) != end ) /* Never escape sequence if no next byte */
        {
            if( (*pi_prev & 0x06) == 0x06 )
            {
                ++p;
                *pi_prev = ((*pi_prev >> 1) << 1) | (!*p);
            }
        }
    }
    return p;
}

#define likely(x) (x)
#define IMPL_h264_generic_decode( name, h264type, decode, release ) \
h264type * name( const uint8_t *p_buf, size_t i_buf, bool b_escaped ) \
{ \
    h264type *p_h264type = (h264type *) calloc(1, sizeof(h264type)); \
    if(likely(p_h264type)) \
    { \
        bs_t bs; \
        struct hxxx_bsfw_ep3b_ctx_s bsctx; \
        if( b_escaped ) \
        { \
            hxxx_bsfw_ep3b_ctx_init( &bsctx ); \
            bs_init_custom( &bs, p_buf, i_buf, &hxxx_bsfw_ep3b_callbacks, &bsctx );\
        } \
        else bs_init( &bs, p_buf, i_buf ); \
        bs_skip( &bs, 8 ); /* Skip nal_unit_header */ \
        if( !decode( &bs, p_h264type ) ) \
        { \
            release( p_h264type ); \
            p_h264type = NULL; \
        } \
    } \
    return p_h264type; \
}

h264_sequence_parameter_set_t * h264_decode_sps( const uint8_t *, size_t, bool );

IMPL_h264_generic_decode( h264_decode_sps, h264_sequence_parameter_set_t,
h264_parse_sequence_parameter_set_rbsp, h264_release_sps )

static const h264_level_limits_t * h264_get_level_limits( const h264_sequence_parameter_set_t *p_sps )
{
    uint16_t i_level_number = p_sps->i_level;
    if( i_level_number == H264_LEVEL_NUMBER_1_1 &&
       (p_sps->i_constraint_set_flags & H264_CONSTRAINT_SET_FLAG(3)) )
    {
        i_level_number = H264_LEVEL_NUMBER_1_B;
    }

    for( size_t i=0; i< ARRAY_SIZE(h264_levels_limits); i++ )
        if( h264_levels_limits[i].i_level == i_level_number )
            return & h264_levels_limits[i].limits;

    return NULL;
}

static uint8_t h264_get_max_dpb_frames( const h264_sequence_parameter_set_t *p_sps )
{
    const h264_level_limits_t *limits = h264_get_level_limits( p_sps );
    if( limits )
    {
        unsigned i_frame_height_in_mbs = ( p_sps->pic_height_in_map_units_minus1 + 1 ) *
                                         ( 2 - p_sps->frame_mbs_only_flag );
        unsigned i_den = ( p_sps->pic_width_in_mbs_minus1 + 1 ) * i_frame_height_in_mbs;
        uint8_t i_max_dpb_frames = limits->i_max_dpb_mbs / i_den;
        if( i_max_dpb_frames < 16 )
            return i_max_dpb_frames;
    }
    return 16;
}

// 以下策略是从 chromium 和 vlc 的源码中获得
// https://cs.chromium.org/chromium/src/media/gpu/mac/vt_video_decode_accelerator_mac.cc?dr=CSs&q=vt_video_decode_accelerator_mac&g=0&l=263
// https://github.com/videolan/vlc/blob/d7ff28e96eb2cb64c5b1a502443a24229532a449/modules/packetizer/h264_nal.c#L735
static int h264_get_max_num_reorder(uint8_t *p_buffer, size_t i_buffer) {
    h264_sequence_parameter_set_t *p_sps = h264_decode_sps( p_buffer, i_buffer, true );
    if( !p_sps ) {
        return -1;
    }
    // When |pic_order_cnt_type| == 2, decode order always matches presentation
    // order.
    if (p_sps->i_pic_order_cnt_type == 2) {
        h264_release_sps(p_sps);
        p_sps = NULL;
        return 0;
    }
    // See AVC spec section E.2.1 definition of |max_num_reorder_frames|.
    uint8_t i_max_num_reorder_frames = p_sps->vui.i_max_num_reorder_frames;
    if( !p_sps->vui.b_bitstream_restriction_flag )
    {
        switch( p_sps->i_profile ) /* E-2.1 */
        {
            case PROFILE_H264_BASELINE:
                i_max_num_reorder_frames = 0; /* only I & P */
                break;
            case PROFILE_H264_CAVLC_INTRA:
            case PROFILE_H264_SVC_HIGH:
            case PROFILE_H264_HIGH:
            case PROFILE_H264_HIGH_10:
            case PROFILE_H264_HIGH_422:
            case PROFILE_H264_HIGH_444_PREDICTIVE:
                if( p_sps->i_constraint_set_flags & H264_CONSTRAINT_SET_FLAG(3) )
                {
                    i_max_num_reorder_frames = 0; /* all IDR */
                    break;
                }
                /* fallthrough */
            default:
                i_max_num_reorder_frames = h264_get_max_dpb_frames( p_sps );
                break;
        }
    }
    h264_release_sps(p_sps);
    p_sps = NULL;
    return i_max_num_reorder_frames;
}
