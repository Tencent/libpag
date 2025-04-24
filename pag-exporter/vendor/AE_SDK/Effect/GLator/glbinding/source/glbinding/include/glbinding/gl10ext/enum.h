#pragma once

#include <glbinding/nogl.h>

#include <glbinding/gl/enum.h>


namespace gl10ext
{

// import enums to namespace


// AccumOp

using gl::GL_ACCUM;
using gl::GL_LOAD;
using gl::GL_RETURN;
using gl::GL_MULT;
using gl::GL_ADD;

// AlphaFunction

using gl::GL_NEVER;
using gl::GL_LESS;
using gl::GL_EQUAL;
using gl::GL_LEQUAL;
using gl::GL_GREATER;
using gl::GL_NOTEQUAL;
using gl::GL_GEQUAL;
using gl::GL_ALWAYS;

// BlendEquationModeEXT

using gl::GL_LOGIC_OP;
using gl::GL_FUNC_ADD_EXT;
using gl::GL_MIN_EXT;
using gl::GL_MAX_EXT;
using gl::GL_FUNC_SUBTRACT_EXT;
using gl::GL_FUNC_REVERSE_SUBTRACT_EXT;
using gl::GL_ALPHA_MIN_SGIX;
using gl::GL_ALPHA_MAX_SGIX;

// BlendingFactorDest

using gl::GL_ZERO;
using gl::GL_SRC_COLOR;
using gl::GL_ONE_MINUS_SRC_COLOR;
using gl::GL_SRC_ALPHA;
using gl::GL_ONE_MINUS_SRC_ALPHA;
using gl::GL_DST_ALPHA;
using gl::GL_ONE_MINUS_DST_ALPHA;
using gl::GL_CONSTANT_COLOR_EXT;
using gl::GL_ONE_MINUS_CONSTANT_COLOR_EXT;
using gl::GL_CONSTANT_ALPHA_EXT;
using gl::GL_ONE_MINUS_CONSTANT_ALPHA_EXT;
using gl::GL_ONE;

// BlendingFactorSrc

// using gl::GL_ZERO; // reuse BlendingFactorDest
// using gl::GL_SRC_ALPHA; // reuse BlendingFactorDest
// using gl::GL_ONE_MINUS_SRC_ALPHA; // reuse BlendingFactorDest
// using gl::GL_DST_ALPHA; // reuse BlendingFactorDest
// using gl::GL_ONE_MINUS_DST_ALPHA; // reuse BlendingFactorDest
using gl::GL_DST_COLOR;
using gl::GL_ONE_MINUS_DST_COLOR;
using gl::GL_SRC_ALPHA_SATURATE;
// using gl::GL_CONSTANT_COLOR_EXT; // reuse BlendingFactorDest
// using gl::GL_ONE_MINUS_CONSTANT_COLOR_EXT; // reuse BlendingFactorDest
// using gl::GL_CONSTANT_ALPHA_EXT; // reuse BlendingFactorDest
// using gl::GL_ONE_MINUS_CONSTANT_ALPHA_EXT; // reuse BlendingFactorDest
// using gl::GL_ONE; // reuse BlendingFactorDest

// ClipPlaneName

using gl::GL_CLIP_DISTANCE0;
using gl::GL_CLIP_PLANE0;
using gl::GL_CLIP_DISTANCE1;
using gl::GL_CLIP_PLANE1;
using gl::GL_CLIP_DISTANCE2;
using gl::GL_CLIP_PLANE2;
using gl::GL_CLIP_DISTANCE3;
using gl::GL_CLIP_PLANE3;
using gl::GL_CLIP_DISTANCE4;
using gl::GL_CLIP_PLANE4;
using gl::GL_CLIP_DISTANCE5;
using gl::GL_CLIP_PLANE5;
using gl::GL_CLIP_DISTANCE6;
using gl::GL_CLIP_DISTANCE7;

// ColorMaterialFace

using gl::GL_FRONT;
using gl::GL_BACK;
using gl::GL_FRONT_AND_BACK;

// ColorMaterialParameter

using gl::GL_AMBIENT;
using gl::GL_DIFFUSE;
using gl::GL_SPECULAR;
using gl::GL_EMISSION;
using gl::GL_AMBIENT_AND_DIFFUSE;

// ColorPointerType

using gl::GL_BYTE;
using gl::GL_UNSIGNED_BYTE;
using gl::GL_SHORT;
using gl::GL_UNSIGNED_SHORT;
using gl::GL_INT;
using gl::GL_UNSIGNED_INT;
using gl::GL_FLOAT;
using gl::GL_DOUBLE;

// ColorTableParameterPNameSGI

using gl::GL_COLOR_TABLE_SCALE;
using gl::GL_COLOR_TABLE_SCALE_SGI;
using gl::GL_COLOR_TABLE_BIAS;
using gl::GL_COLOR_TABLE_BIAS_SGI;

// ColorTableTargetSGI

using gl::GL_TEXTURE_COLOR_TABLE_SGI;
using gl::GL_PROXY_TEXTURE_COLOR_TABLE_SGI;
using gl::GL_COLOR_TABLE;
using gl::GL_COLOR_TABLE_SGI;
using gl::GL_POST_CONVOLUTION_COLOR_TABLE;
using gl::GL_POST_CONVOLUTION_COLOR_TABLE_SGI;
using gl::GL_POST_COLOR_MATRIX_COLOR_TABLE;
using gl::GL_POST_COLOR_MATRIX_COLOR_TABLE_SGI;
using gl::GL_PROXY_COLOR_TABLE;
using gl::GL_PROXY_COLOR_TABLE_SGI;
using gl::GL_PROXY_POST_CONVOLUTION_COLOR_TABLE;
using gl::GL_PROXY_POST_CONVOLUTION_COLOR_TABLE_SGI;
using gl::GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE;
using gl::GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE_SGI;

// ConvolutionBorderModeEXT

using gl::GL_REDUCE;
using gl::GL_REDUCE_EXT;

// ConvolutionParameterEXT

using gl::GL_CONVOLUTION_BORDER_MODE;
using gl::GL_CONVOLUTION_BORDER_MODE_EXT;
using gl::GL_CONVOLUTION_FILTER_SCALE;
using gl::GL_CONVOLUTION_FILTER_SCALE_EXT;
using gl::GL_CONVOLUTION_FILTER_BIAS;
using gl::GL_CONVOLUTION_FILTER_BIAS_EXT;

// ConvolutionTargetEXT

using gl::GL_CONVOLUTION_1D;
using gl::GL_CONVOLUTION_1D_EXT;
using gl::GL_CONVOLUTION_2D;
using gl::GL_CONVOLUTION_2D_EXT;

// CullFaceMode

// using gl::GL_FRONT; // reuse ColorMaterialFace
// using gl::GL_BACK; // reuse ColorMaterialFace
// using gl::GL_FRONT_AND_BACK; // reuse ColorMaterialFace

// DepthFunction

// using gl::GL_NEVER; // reuse AlphaFunction
// using gl::GL_LESS; // reuse AlphaFunction
// using gl::GL_EQUAL; // reuse AlphaFunction
// using gl::GL_LEQUAL; // reuse AlphaFunction
// using gl::GL_GREATER; // reuse AlphaFunction
// using gl::GL_NOTEQUAL; // reuse AlphaFunction
// using gl::GL_GEQUAL; // reuse AlphaFunction
// using gl::GL_ALWAYS; // reuse AlphaFunction

// DrawBufferMode

using gl::GL_NONE;
using gl::GL_FRONT_LEFT;
using gl::GL_FRONT_RIGHT;
using gl::GL_BACK_LEFT;
using gl::GL_BACK_RIGHT;
// using gl::GL_FRONT; // reuse ColorMaterialFace
// using gl::GL_BACK; // reuse ColorMaterialFace
using gl::GL_LEFT;
using gl::GL_RIGHT;
// using gl::GL_FRONT_AND_BACK; // reuse ColorMaterialFace
using gl::GL_AUX0;
using gl::GL_AUX1;
using gl::GL_AUX2;
using gl::GL_AUX3;

// EnableCap

using gl::GL_POINT_SMOOTH;
using gl::GL_LINE_SMOOTH;
using gl::GL_LINE_STIPPLE;
using gl::GL_POLYGON_SMOOTH;
using gl::GL_POLYGON_STIPPLE;
using gl::GL_CULL_FACE;
using gl::GL_LIGHTING;
using gl::GL_COLOR_MATERIAL;
using gl::GL_FOG;
using gl::GL_DEPTH_TEST;
using gl::GL_STENCIL_TEST;
using gl::GL_NORMALIZE;
using gl::GL_ALPHA_TEST;
using gl::GL_DITHER;
using gl::GL_BLEND;
using gl::GL_INDEX_LOGIC_OP;
using gl::GL_COLOR_LOGIC_OP;
using gl::GL_SCISSOR_TEST;
using gl::GL_TEXTURE_GEN_S;
using gl::GL_TEXTURE_GEN_T;
using gl::GL_TEXTURE_GEN_R;
using gl::GL_TEXTURE_GEN_Q;
using gl::GL_AUTO_NORMAL;
using gl::GL_MAP1_COLOR_4;
using gl::GL_MAP1_INDEX;
using gl::GL_MAP1_NORMAL;
using gl::GL_MAP1_TEXTURE_COORD_1;
using gl::GL_MAP1_TEXTURE_COORD_2;
using gl::GL_MAP1_TEXTURE_COORD_3;
using gl::GL_MAP1_TEXTURE_COORD_4;
using gl::GL_MAP1_VERTEX_3;
using gl::GL_MAP1_VERTEX_4;
using gl::GL_MAP2_COLOR_4;
using gl::GL_MAP2_INDEX;
using gl::GL_MAP2_NORMAL;
using gl::GL_MAP2_TEXTURE_COORD_1;
using gl::GL_MAP2_TEXTURE_COORD_2;
using gl::GL_MAP2_TEXTURE_COORD_3;
using gl::GL_MAP2_TEXTURE_COORD_4;
using gl::GL_MAP2_VERTEX_3;
using gl::GL_MAP2_VERTEX_4;
using gl::GL_TEXTURE_1D;
using gl::GL_TEXTURE_2D;
using gl::GL_POLYGON_OFFSET_POINT;
using gl::GL_POLYGON_OFFSET_LINE;
// using gl::GL_CLIP_PLANE0; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE1; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE2; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE3; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE4; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE5; // reuse ClipPlaneName
using gl::GL_LIGHT0;
using gl::GL_LIGHT1;
using gl::GL_LIGHT2;
using gl::GL_LIGHT3;
using gl::GL_LIGHT4;
using gl::GL_LIGHT5;
using gl::GL_LIGHT6;
using gl::GL_LIGHT7;
// using gl::GL_CONVOLUTION_1D_EXT; // reuse ConvolutionTargetEXT
// using gl::GL_CONVOLUTION_2D_EXT; // reuse ConvolutionTargetEXT
using gl::GL_SEPARABLE_2D_EXT;
using gl::GL_HISTOGRAM_EXT;
using gl::GL_MINMAX_EXT;
using gl::GL_POLYGON_OFFSET_FILL;
using gl::GL_RESCALE_NORMAL_EXT;
using gl::GL_TEXTURE_3D_EXT;
using gl::GL_VERTEX_ARRAY;
using gl::GL_NORMAL_ARRAY;
using gl::GL_COLOR_ARRAY;
using gl::GL_INDEX_ARRAY;
using gl::GL_TEXTURE_COORD_ARRAY;
using gl::GL_EDGE_FLAG_ARRAY;
using gl::GL_INTERLACE_SGIX;
using gl::GL_MULTISAMPLE_SGIS;
using gl::GL_SAMPLE_ALPHA_TO_MASK_SGIS;
using gl::GL_SAMPLE_ALPHA_TO_ONE_SGIS;
using gl::GL_SAMPLE_MASK_SGIS;
// using gl::GL_TEXTURE_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_POST_CONVOLUTION_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_POST_COLOR_MATRIX_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
using gl::GL_TEXTURE_4D_SGIS;
using gl::GL_PIXEL_TEX_GEN_SGIX;
using gl::GL_SPRITE_SGIX;
using gl::GL_REFERENCE_PLANE_SGIX;
using gl::GL_IR_INSTRUMENT1_SGIX;
using gl::GL_CALLIGRAPHIC_FRAGMENT_SGIX;
using gl::GL_FRAMEZOOM_SGIX;
using gl::GL_FOG_OFFSET_SGIX;
using gl::GL_SHARED_TEXTURE_PALETTE_EXT;
using gl::GL_ASYNC_HISTOGRAM_SGIX;
using gl::GL_PIXEL_TEXTURE_SGIS;
using gl::GL_ASYNC_TEX_IMAGE_SGIX;
using gl::GL_ASYNC_DRAW_PIXELS_SGIX;
using gl::GL_ASYNC_READ_PIXELS_SGIX;
using gl::GL_FRAGMENT_LIGHTING_SGIX;
using gl::GL_FRAGMENT_COLOR_MATERIAL_SGIX;
using gl::GL_FRAGMENT_LIGHT0_SGIX;
using gl::GL_FRAGMENT_LIGHT1_SGIX;
using gl::GL_FRAGMENT_LIGHT2_SGIX;
using gl::GL_FRAGMENT_LIGHT3_SGIX;
using gl::GL_FRAGMENT_LIGHT4_SGIX;
using gl::GL_FRAGMENT_LIGHT5_SGIX;
using gl::GL_FRAGMENT_LIGHT6_SGIX;
using gl::GL_FRAGMENT_LIGHT7_SGIX;

// ErrorCode

using gl::GL_NO_ERROR;
using gl::GL_INVALID_ENUM;
using gl::GL_INVALID_VALUE;
using gl::GL_INVALID_OPERATION;
using gl::GL_STACK_OVERFLOW;
using gl::GL_STACK_UNDERFLOW;
using gl::GL_OUT_OF_MEMORY;
using gl::GL_INVALID_FRAMEBUFFER_OPERATION;
using gl::GL_INVALID_FRAMEBUFFER_OPERATION_EXT;
using gl::GL_TABLE_TOO_LARGE;
using gl::GL_TABLE_TOO_LARGE_EXT;
using gl::GL_TEXTURE_TOO_LARGE_EXT;

// FeedBackToken

using gl::GL_PASS_THROUGH_TOKEN;
using gl::GL_POINT_TOKEN;
using gl::GL_LINE_TOKEN;
using gl::GL_POLYGON_TOKEN;
using gl::GL_BITMAP_TOKEN;
using gl::GL_DRAW_PIXEL_TOKEN;
using gl::GL_COPY_PIXEL_TOKEN;
using gl::GL_LINE_RESET_TOKEN;

// FeedbackType

using gl::GL_2D;
using gl::GL_3D;
using gl::GL_3D_COLOR;
using gl::GL_3D_COLOR_TEXTURE;
using gl::GL_4D_COLOR_TEXTURE;

// FfdTargetSGIX

using gl::GL_GEOMETRY_DEFORMATION_SGIX;
using gl::GL_TEXTURE_DEFORMATION_SGIX;

// FogCoordinatePointerType

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FogMode

using gl::GL_EXP;
using gl::GL_EXP2;
using gl::GL_LINEAR;
using gl::GL_FOG_FUNC_SGIS;

// FogParameter

using gl::GL_FOG_INDEX;
using gl::GL_FOG_DENSITY;
using gl::GL_FOG_START;
using gl::GL_FOG_END;
using gl::GL_FOG_MODE;
using gl::GL_FOG_COLOR;
using gl::GL_FOG_OFFSET_VALUE_SGIX;

// FogPointerTypeEXT

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FogPointerTypeIBM

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FragmentLightModelParameterSGIX

using gl::GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_SGIX;
using gl::GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_SGIX;
using gl::GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX;
using gl::GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_SGIX;

// FrontFaceDirection

using gl::GL_CW;
using gl::GL_CCW;

// GetColorTableParameterPNameSGI

// using gl::GL_COLOR_TABLE_SCALE_SGI; // reuse ColorTableParameterPNameSGI
// using gl::GL_COLOR_TABLE_BIAS_SGI; // reuse ColorTableParameterPNameSGI
using gl::GL_COLOR_TABLE_FORMAT_SGI;
using gl::GL_COLOR_TABLE_WIDTH_SGI;
using gl::GL_COLOR_TABLE_RED_SIZE_SGI;
using gl::GL_COLOR_TABLE_GREEN_SIZE_SGI;
using gl::GL_COLOR_TABLE_BLUE_SIZE_SGI;
using gl::GL_COLOR_TABLE_ALPHA_SIZE_SGI;
using gl::GL_COLOR_TABLE_LUMINANCE_SIZE_SGI;
using gl::GL_COLOR_TABLE_INTENSITY_SIZE_SGI;

// GetConvolutionParameter

// using gl::GL_CONVOLUTION_BORDER_MODE_EXT; // reuse ConvolutionParameterEXT
// using gl::GL_CONVOLUTION_FILTER_SCALE_EXT; // reuse ConvolutionParameterEXT
// using gl::GL_CONVOLUTION_FILTER_BIAS_EXT; // reuse ConvolutionParameterEXT
using gl::GL_CONVOLUTION_FORMAT_EXT;
using gl::GL_CONVOLUTION_WIDTH_EXT;
using gl::GL_CONVOLUTION_HEIGHT_EXT;
using gl::GL_MAX_CONVOLUTION_WIDTH_EXT;
using gl::GL_MAX_CONVOLUTION_HEIGHT_EXT;

// GetHistogramParameterPNameEXT

using gl::GL_HISTOGRAM_WIDTH_EXT;
using gl::GL_HISTOGRAM_FORMAT_EXT;
using gl::GL_HISTOGRAM_RED_SIZE_EXT;
using gl::GL_HISTOGRAM_GREEN_SIZE_EXT;
using gl::GL_HISTOGRAM_BLUE_SIZE_EXT;
using gl::GL_HISTOGRAM_ALPHA_SIZE_EXT;
using gl::GL_HISTOGRAM_LUMINANCE_SIZE_EXT;
using gl::GL_HISTOGRAM_SINK_EXT;

// GetMapQuery

using gl::GL_COEFF;
using gl::GL_ORDER;
using gl::GL_DOMAIN;

// GetMinmaxParameterPNameEXT

using gl::GL_MINMAX_FORMAT;
using gl::GL_MINMAX_FORMAT_EXT;
using gl::GL_MINMAX_SINK;
using gl::GL_MINMAX_SINK_EXT;

// GetPName

using gl::GL_CURRENT_COLOR;
using gl::GL_CURRENT_INDEX;
using gl::GL_CURRENT_NORMAL;
using gl::GL_CURRENT_TEXTURE_COORDS;
using gl::GL_CURRENT_RASTER_COLOR;
using gl::GL_CURRENT_RASTER_INDEX;
using gl::GL_CURRENT_RASTER_TEXTURE_COORDS;
using gl::GL_CURRENT_RASTER_POSITION;
using gl::GL_CURRENT_RASTER_POSITION_VALID;
using gl::GL_CURRENT_RASTER_DISTANCE;
// using gl::GL_POINT_SMOOTH; // reuse EnableCap
using gl::GL_POINT_SIZE;
using gl::GL_POINT_SIZE_RANGE;
using gl::GL_SMOOTH_POINT_SIZE_RANGE;
using gl::GL_POINT_SIZE_GRANULARITY;
using gl::GL_SMOOTH_POINT_SIZE_GRANULARITY;
// using gl::GL_LINE_SMOOTH; // reuse EnableCap
using gl::GL_LINE_WIDTH;
using gl::GL_LINE_WIDTH_RANGE;
using gl::GL_SMOOTH_LINE_WIDTH_RANGE;
using gl::GL_LINE_WIDTH_GRANULARITY;
using gl::GL_SMOOTH_LINE_WIDTH_GRANULARITY;
// using gl::GL_LINE_STIPPLE; // reuse EnableCap
using gl::GL_LINE_STIPPLE_PATTERN;
using gl::GL_LINE_STIPPLE_REPEAT;
using gl::GL_LIST_MODE;
using gl::GL_MAX_LIST_NESTING;
using gl::GL_LIST_BASE;
using gl::GL_LIST_INDEX;
using gl::GL_POLYGON_MODE;
// using gl::GL_POLYGON_SMOOTH; // reuse EnableCap
// using gl::GL_POLYGON_STIPPLE; // reuse EnableCap
using gl::GL_EDGE_FLAG;
// using gl::GL_CULL_FACE; // reuse EnableCap
using gl::GL_CULL_FACE_MODE;
using gl::GL_FRONT_FACE;
// using gl::GL_LIGHTING; // reuse EnableCap
using gl::GL_LIGHT_MODEL_LOCAL_VIEWER;
using gl::GL_LIGHT_MODEL_TWO_SIDE;
using gl::GL_LIGHT_MODEL_AMBIENT;
using gl::GL_SHADE_MODEL;
using gl::GL_COLOR_MATERIAL_FACE;
using gl::GL_COLOR_MATERIAL_PARAMETER;
// using gl::GL_COLOR_MATERIAL; // reuse EnableCap
// using gl::GL_FOG; // reuse EnableCap
// using gl::GL_FOG_INDEX; // reuse FogParameter
// using gl::GL_FOG_DENSITY; // reuse FogParameter
// using gl::GL_FOG_START; // reuse FogParameter
// using gl::GL_FOG_END; // reuse FogParameter
// using gl::GL_FOG_MODE; // reuse FogParameter
// using gl::GL_FOG_COLOR; // reuse FogParameter
using gl::GL_DEPTH_RANGE;
// using gl::GL_DEPTH_TEST; // reuse EnableCap
using gl::GL_DEPTH_WRITEMASK;
using gl::GL_DEPTH_CLEAR_VALUE;
using gl::GL_DEPTH_FUNC;
using gl::GL_ACCUM_CLEAR_VALUE;
// using gl::GL_STENCIL_TEST; // reuse EnableCap
using gl::GL_STENCIL_CLEAR_VALUE;
using gl::GL_STENCIL_FUNC;
using gl::GL_STENCIL_VALUE_MASK;
using gl::GL_STENCIL_FAIL;
using gl::GL_STENCIL_PASS_DEPTH_FAIL;
using gl::GL_STENCIL_PASS_DEPTH_PASS;
using gl::GL_STENCIL_REF;
using gl::GL_STENCIL_WRITEMASK;
using gl::GL_MATRIX_MODE;
// using gl::GL_NORMALIZE; // reuse EnableCap
using gl::GL_VIEWPORT;
using gl::GL_MODELVIEW0_STACK_DEPTH_EXT;
using gl::GL_MODELVIEW_STACK_DEPTH;
using gl::GL_PROJECTION_STACK_DEPTH;
using gl::GL_TEXTURE_STACK_DEPTH;
using gl::GL_MODELVIEW0_MATRIX_EXT;
using gl::GL_MODELVIEW_MATRIX;
using gl::GL_PROJECTION_MATRIX;
using gl::GL_TEXTURE_MATRIX;
using gl::GL_ATTRIB_STACK_DEPTH;
using gl::GL_CLIENT_ATTRIB_STACK_DEPTH;
// using gl::GL_ALPHA_TEST; // reuse EnableCap
using gl::GL_ALPHA_TEST_FUNC;
using gl::GL_ALPHA_TEST_REF;
// using gl::GL_DITHER; // reuse EnableCap
using gl::GL_BLEND_DST;
using gl::GL_BLEND_SRC;
// using gl::GL_BLEND; // reuse EnableCap
using gl::GL_LOGIC_OP_MODE;
// using gl::GL_INDEX_LOGIC_OP; // reuse EnableCap
// using gl::GL_LOGIC_OP; // reuse BlendEquationModeEXT
// using gl::GL_COLOR_LOGIC_OP; // reuse EnableCap
using gl::GL_AUX_BUFFERS;
using gl::GL_DRAW_BUFFER;
using gl::GL_READ_BUFFER;
using gl::GL_SCISSOR_BOX;
// using gl::GL_SCISSOR_TEST; // reuse EnableCap
using gl::GL_INDEX_CLEAR_VALUE;
using gl::GL_INDEX_WRITEMASK;
using gl::GL_COLOR_CLEAR_VALUE;
using gl::GL_COLOR_WRITEMASK;
using gl::GL_INDEX_MODE;
using gl::GL_RGBA_MODE;
using gl::GL_DOUBLEBUFFER;
using gl::GL_STEREO;
using gl::GL_RENDER_MODE;
using gl::GL_PERSPECTIVE_CORRECTION_HINT;
using gl::GL_POINT_SMOOTH_HINT;
using gl::GL_LINE_SMOOTH_HINT;
using gl::GL_POLYGON_SMOOTH_HINT;
using gl::GL_FOG_HINT;
// using gl::GL_TEXTURE_GEN_S; // reuse EnableCap
// using gl::GL_TEXTURE_GEN_T; // reuse EnableCap
// using gl::GL_TEXTURE_GEN_R; // reuse EnableCap
// using gl::GL_TEXTURE_GEN_Q; // reuse EnableCap
using gl::GL_PIXEL_MAP_I_TO_I_SIZE;
using gl::GL_PIXEL_MAP_S_TO_S_SIZE;
using gl::GL_PIXEL_MAP_I_TO_R_SIZE;
using gl::GL_PIXEL_MAP_I_TO_G_SIZE;
using gl::GL_PIXEL_MAP_I_TO_B_SIZE;
using gl::GL_PIXEL_MAP_I_TO_A_SIZE;
using gl::GL_PIXEL_MAP_R_TO_R_SIZE;
using gl::GL_PIXEL_MAP_G_TO_G_SIZE;
using gl::GL_PIXEL_MAP_B_TO_B_SIZE;
using gl::GL_PIXEL_MAP_A_TO_A_SIZE;
using gl::GL_UNPACK_SWAP_BYTES;
using gl::GL_UNPACK_LSB_FIRST;
using gl::GL_UNPACK_ROW_LENGTH;
using gl::GL_UNPACK_SKIP_ROWS;
using gl::GL_UNPACK_SKIP_PIXELS;
using gl::GL_UNPACK_ALIGNMENT;
using gl::GL_PACK_SWAP_BYTES;
using gl::GL_PACK_LSB_FIRST;
using gl::GL_PACK_ROW_LENGTH;
using gl::GL_PACK_SKIP_ROWS;
using gl::GL_PACK_SKIP_PIXELS;
using gl::GL_PACK_ALIGNMENT;
using gl::GL_MAP_COLOR;
using gl::GL_MAP_STENCIL;
using gl::GL_INDEX_SHIFT;
using gl::GL_INDEX_OFFSET;
using gl::GL_RED_SCALE;
using gl::GL_RED_BIAS;
using gl::GL_ZOOM_X;
using gl::GL_ZOOM_Y;
using gl::GL_GREEN_SCALE;
using gl::GL_GREEN_BIAS;
using gl::GL_BLUE_SCALE;
using gl::GL_BLUE_BIAS;
using gl::GL_ALPHA_SCALE;
using gl::GL_ALPHA_BIAS;
using gl::GL_DEPTH_SCALE;
using gl::GL_DEPTH_BIAS;
using gl::GL_MAX_EVAL_ORDER;
using gl::GL_MAX_LIGHTS;
using gl::GL_MAX_CLIP_DISTANCES;
using gl::GL_MAX_CLIP_PLANES;
using gl::GL_MAX_TEXTURE_SIZE;
using gl::GL_MAX_PIXEL_MAP_TABLE;
using gl::GL_MAX_ATTRIB_STACK_DEPTH;
using gl::GL_MAX_MODELVIEW_STACK_DEPTH;
using gl::GL_MAX_NAME_STACK_DEPTH;
using gl::GL_MAX_PROJECTION_STACK_DEPTH;
using gl::GL_MAX_TEXTURE_STACK_DEPTH;
using gl::GL_MAX_VIEWPORT_DIMS;
using gl::GL_MAX_CLIENT_ATTRIB_STACK_DEPTH;
using gl::GL_SUBPIXEL_BITS;
using gl::GL_INDEX_BITS;
using gl::GL_RED_BITS;
using gl::GL_GREEN_BITS;
using gl::GL_BLUE_BITS;
using gl::GL_ALPHA_BITS;
using gl::GL_DEPTH_BITS;
using gl::GL_STENCIL_BITS;
using gl::GL_ACCUM_RED_BITS;
using gl::GL_ACCUM_GREEN_BITS;
using gl::GL_ACCUM_BLUE_BITS;
using gl::GL_ACCUM_ALPHA_BITS;
using gl::GL_NAME_STACK_DEPTH;
// using gl::GL_AUTO_NORMAL; // reuse EnableCap
// using gl::GL_MAP1_COLOR_4; // reuse EnableCap
// using gl::GL_MAP1_INDEX; // reuse EnableCap
// using gl::GL_MAP1_NORMAL; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_1; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_2; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_3; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_4; // reuse EnableCap
// using gl::GL_MAP1_VERTEX_3; // reuse EnableCap
// using gl::GL_MAP1_VERTEX_4; // reuse EnableCap
// using gl::GL_MAP2_COLOR_4; // reuse EnableCap
// using gl::GL_MAP2_INDEX; // reuse EnableCap
// using gl::GL_MAP2_NORMAL; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_1; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_2; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_3; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_4; // reuse EnableCap
// using gl::GL_MAP2_VERTEX_3; // reuse EnableCap
// using gl::GL_MAP2_VERTEX_4; // reuse EnableCap
using gl::GL_MAP1_GRID_DOMAIN;
using gl::GL_MAP1_GRID_SEGMENTS;
using gl::GL_MAP2_GRID_DOMAIN;
using gl::GL_MAP2_GRID_SEGMENTS;
// using gl::GL_TEXTURE_1D; // reuse EnableCap
// using gl::GL_TEXTURE_2D; // reuse EnableCap
using gl::GL_FEEDBACK_BUFFER_SIZE;
using gl::GL_FEEDBACK_BUFFER_TYPE;
using gl::GL_SELECTION_BUFFER_SIZE;
using gl::GL_POLYGON_OFFSET_UNITS;
// using gl::GL_POLYGON_OFFSET_POINT; // reuse EnableCap
// using gl::GL_POLYGON_OFFSET_LINE; // reuse EnableCap
// using gl::GL_CLIP_PLANE0; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE1; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE2; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE3; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE4; // reuse ClipPlaneName
// using gl::GL_CLIP_PLANE5; // reuse ClipPlaneName
// using gl::GL_LIGHT0; // reuse EnableCap
// using gl::GL_LIGHT1; // reuse EnableCap
// using gl::GL_LIGHT2; // reuse EnableCap
// using gl::GL_LIGHT3; // reuse EnableCap
// using gl::GL_LIGHT4; // reuse EnableCap
// using gl::GL_LIGHT5; // reuse EnableCap
// using gl::GL_LIGHT6; // reuse EnableCap
// using gl::GL_LIGHT7; // reuse EnableCap
using gl::GL_BLEND_COLOR_EXT;
using gl::GL_BLEND_EQUATION_EXT;
using gl::GL_PACK_CMYK_HINT_EXT;
using gl::GL_UNPACK_CMYK_HINT_EXT;
// using gl::GL_CONVOLUTION_1D_EXT; // reuse ConvolutionTargetEXT
// using gl::GL_CONVOLUTION_2D_EXT; // reuse ConvolutionTargetEXT
// using gl::GL_SEPARABLE_2D_EXT; // reuse EnableCap
using gl::GL_POST_CONVOLUTION_RED_SCALE_EXT;
using gl::GL_POST_CONVOLUTION_GREEN_SCALE_EXT;
using gl::GL_POST_CONVOLUTION_BLUE_SCALE_EXT;
using gl::GL_POST_CONVOLUTION_ALPHA_SCALE_EXT;
using gl::GL_POST_CONVOLUTION_RED_BIAS_EXT;
using gl::GL_POST_CONVOLUTION_GREEN_BIAS_EXT;
using gl::GL_POST_CONVOLUTION_BLUE_BIAS_EXT;
using gl::GL_POST_CONVOLUTION_ALPHA_BIAS_EXT;
// using gl::GL_HISTOGRAM_EXT; // reuse EnableCap
// using gl::GL_MINMAX_EXT; // reuse EnableCap
// using gl::GL_POLYGON_OFFSET_FILL; // reuse EnableCap
using gl::GL_POLYGON_OFFSET_FACTOR;
using gl::GL_POLYGON_OFFSET_BIAS_EXT;
// using gl::GL_RESCALE_NORMAL_EXT; // reuse EnableCap
using gl::GL_TEXTURE_BINDING_1D;
using gl::GL_TEXTURE_BINDING_2D;
using gl::GL_TEXTURE_3D_BINDING_EXT;
using gl::GL_TEXTURE_BINDING_3D;
using gl::GL_PACK_SKIP_IMAGES_EXT;
using gl::GL_PACK_IMAGE_HEIGHT_EXT;
using gl::GL_UNPACK_SKIP_IMAGES_EXT;
using gl::GL_UNPACK_IMAGE_HEIGHT_EXT;
// using gl::GL_TEXTURE_3D_EXT; // reuse EnableCap
using gl::GL_MAX_3D_TEXTURE_SIZE_EXT;
// using gl::GL_VERTEX_ARRAY; // reuse EnableCap
// using gl::GL_NORMAL_ARRAY; // reuse EnableCap
// using gl::GL_COLOR_ARRAY; // reuse EnableCap
// using gl::GL_INDEX_ARRAY; // reuse EnableCap
// using gl::GL_TEXTURE_COORD_ARRAY; // reuse EnableCap
// using gl::GL_EDGE_FLAG_ARRAY; // reuse EnableCap
using gl::GL_VERTEX_ARRAY_SIZE;
using gl::GL_VERTEX_ARRAY_TYPE;
using gl::GL_VERTEX_ARRAY_STRIDE;
using gl::GL_VERTEX_ARRAY_COUNT_EXT;
using gl::GL_NORMAL_ARRAY_TYPE;
using gl::GL_NORMAL_ARRAY_STRIDE;
using gl::GL_NORMAL_ARRAY_COUNT_EXT;
using gl::GL_COLOR_ARRAY_SIZE;
using gl::GL_COLOR_ARRAY_TYPE;
using gl::GL_COLOR_ARRAY_STRIDE;
using gl::GL_COLOR_ARRAY_COUNT_EXT;
using gl::GL_INDEX_ARRAY_TYPE;
using gl::GL_INDEX_ARRAY_STRIDE;
using gl::GL_INDEX_ARRAY_COUNT_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_SIZE;
using gl::GL_TEXTURE_COORD_ARRAY_TYPE;
using gl::GL_TEXTURE_COORD_ARRAY_STRIDE;
using gl::GL_TEXTURE_COORD_ARRAY_COUNT_EXT;
using gl::GL_EDGE_FLAG_ARRAY_STRIDE;
using gl::GL_EDGE_FLAG_ARRAY_COUNT_EXT;
// using gl::GL_INTERLACE_SGIX; // reuse EnableCap
using gl::GL_DETAIL_TEXTURE_2D_BINDING_SGIS;
// using gl::GL_MULTISAMPLE_SGIS; // reuse EnableCap
// using gl::GL_SAMPLE_ALPHA_TO_MASK_SGIS; // reuse EnableCap
// using gl::GL_SAMPLE_ALPHA_TO_ONE_SGIS; // reuse EnableCap
// using gl::GL_SAMPLE_MASK_SGIS; // reuse EnableCap
using gl::GL_SAMPLE_BUFFERS_SGIS;
using gl::GL_SAMPLES_SGIS;
using gl::GL_SAMPLE_MASK_VALUE_SGIS;
using gl::GL_SAMPLE_MASK_INVERT_SGIS;
using gl::GL_SAMPLE_PATTERN_SGIS;
using gl::GL_COLOR_MATRIX_SGI;
using gl::GL_COLOR_MATRIX_STACK_DEPTH_SGI;
using gl::GL_MAX_COLOR_MATRIX_STACK_DEPTH_SGI;
using gl::GL_POST_COLOR_MATRIX_RED_SCALE_SGI;
using gl::GL_POST_COLOR_MATRIX_GREEN_SCALE_SGI;
using gl::GL_POST_COLOR_MATRIX_BLUE_SCALE_SGI;
using gl::GL_POST_COLOR_MATRIX_ALPHA_SCALE_SGI;
using gl::GL_POST_COLOR_MATRIX_RED_BIAS_SGI;
using gl::GL_POST_COLOR_MATRIX_GREEN_BIAS_SGI;
using gl::GL_POST_COLOR_MATRIX_BLUE_BIAS_SGI;
using gl::GL_POST_COLOR_MATRIX_ALPHA_BIAS_SGI;
// using gl::GL_TEXTURE_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_POST_CONVOLUTION_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
// using gl::GL_POST_COLOR_MATRIX_COLOR_TABLE_SGI; // reuse ColorTableTargetSGI
using gl::GL_POINT_SIZE_MIN_SGIS;
using gl::GL_POINT_SIZE_MAX_SGIS;
using gl::GL_POINT_FADE_THRESHOLD_SIZE_SGIS;
using gl::GL_DISTANCE_ATTENUATION_SGIS;
using gl::GL_FOG_FUNC_POINTS_SGIS;
using gl::GL_MAX_FOG_FUNC_POINTS_SGIS;
using gl::GL_PACK_SKIP_VOLUMES_SGIS;
using gl::GL_PACK_IMAGE_DEPTH_SGIS;
using gl::GL_UNPACK_SKIP_VOLUMES_SGIS;
using gl::GL_UNPACK_IMAGE_DEPTH_SGIS;
// using gl::GL_TEXTURE_4D_SGIS; // reuse EnableCap
using gl::GL_MAX_4D_TEXTURE_SIZE_SGIS;
// using gl::GL_PIXEL_TEX_GEN_SGIX; // reuse EnableCap
using gl::GL_PIXEL_TILE_BEST_ALIGNMENT_SGIX;
using gl::GL_PIXEL_TILE_CACHE_INCREMENT_SGIX;
using gl::GL_PIXEL_TILE_WIDTH_SGIX;
using gl::GL_PIXEL_TILE_HEIGHT_SGIX;
using gl::GL_PIXEL_TILE_GRID_WIDTH_SGIX;
using gl::GL_PIXEL_TILE_GRID_HEIGHT_SGIX;
using gl::GL_PIXEL_TILE_GRID_DEPTH_SGIX;
using gl::GL_PIXEL_TILE_CACHE_SIZE_SGIX;
// using gl::GL_SPRITE_SGIX; // reuse EnableCap
using gl::GL_SPRITE_MODE_SGIX;
using gl::GL_SPRITE_AXIS_SGIX;
using gl::GL_SPRITE_TRANSLATION_SGIX;
using gl::GL_TEXTURE_4D_BINDING_SGIS;
using gl::GL_MAX_CLIPMAP_DEPTH_SGIX;
using gl::GL_MAX_CLIPMAP_VIRTUAL_DEPTH_SGIX;
using gl::GL_POST_TEXTURE_FILTER_BIAS_RANGE_SGIX;
using gl::GL_POST_TEXTURE_FILTER_SCALE_RANGE_SGIX;
// using gl::GL_REFERENCE_PLANE_SGIX; // reuse EnableCap
using gl::GL_REFERENCE_PLANE_EQUATION_SGIX;
// using gl::GL_IR_INSTRUMENT1_SGIX; // reuse EnableCap
using gl::GL_INSTRUMENT_MEASUREMENTS_SGIX;
// using gl::GL_CALLIGRAPHIC_FRAGMENT_SGIX; // reuse EnableCap
// using gl::GL_FRAMEZOOM_SGIX; // reuse EnableCap
using gl::GL_FRAMEZOOM_FACTOR_SGIX;
using gl::GL_MAX_FRAMEZOOM_FACTOR_SGIX;
using gl::GL_GENERATE_MIPMAP_HINT_SGIS;
using gl::GL_DEFORMATIONS_MASK_SGIX;
// using gl::GL_FOG_OFFSET_SGIX; // reuse EnableCap
// using gl::GL_FOG_OFFSET_VALUE_SGIX; // reuse FogParameter
using gl::GL_LIGHT_MODEL_COLOR_CONTROL;
// using gl::GL_SHARED_TEXTURE_PALETTE_EXT; // reuse EnableCap
using gl::GL_CONVOLUTION_HINT_SGIX;
using gl::GL_ASYNC_MARKER_SGIX;
using gl::GL_PIXEL_TEX_GEN_MODE_SGIX;
// using gl::GL_ASYNC_HISTOGRAM_SGIX; // reuse EnableCap
using gl::GL_MAX_ASYNC_HISTOGRAM_SGIX;
// using gl::GL_PIXEL_TEXTURE_SGIS; // reuse EnableCap
// using gl::GL_ASYNC_TEX_IMAGE_SGIX; // reuse EnableCap
// using gl::GL_ASYNC_DRAW_PIXELS_SGIX; // reuse EnableCap
// using gl::GL_ASYNC_READ_PIXELS_SGIX; // reuse EnableCap
using gl::GL_MAX_ASYNC_TEX_IMAGE_SGIX;
using gl::GL_MAX_ASYNC_DRAW_PIXELS_SGIX;
using gl::GL_MAX_ASYNC_READ_PIXELS_SGIX;
using gl::GL_VERTEX_PRECLIP_SGIX;
using gl::GL_VERTEX_PRECLIP_HINT_SGIX;
// using gl::GL_FRAGMENT_LIGHTING_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_COLOR_MATERIAL_SGIX; // reuse EnableCap
using gl::GL_FRAGMENT_COLOR_MATERIAL_FACE_SGIX;
using gl::GL_FRAGMENT_COLOR_MATERIAL_PARAMETER_SGIX;
using gl::GL_MAX_FRAGMENT_LIGHTS_SGIX;
using gl::GL_MAX_ACTIVE_LIGHTS_SGIX;
using gl::GL_LIGHT_ENV_MODE_SGIX;
// using gl::GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_SGIX; // reuse FragmentLightModelParameterSGIX
// using gl::GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_SGIX; // reuse FragmentLightModelParameterSGIX
// using gl::GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX; // reuse FragmentLightModelParameterSGIX
// using gl::GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_SGIX; // reuse FragmentLightModelParameterSGIX
// using gl::GL_FRAGMENT_LIGHT0_SGIX; // reuse EnableCap
using gl::GL_PACK_RESAMPLE_SGIX;
using gl::GL_UNPACK_RESAMPLE_SGIX;
using gl::GL_ALIASED_POINT_SIZE_RANGE;
using gl::GL_ALIASED_LINE_WIDTH_RANGE;
using gl::GL_PACK_SUBSAMPLE_RATE_SGIX;
using gl::GL_UNPACK_SUBSAMPLE_RATE_SGIX;

// GetPixelMap

using gl::GL_PIXEL_MAP_I_TO_I;
using gl::GL_PIXEL_MAP_S_TO_S;
using gl::GL_PIXEL_MAP_I_TO_R;
using gl::GL_PIXEL_MAP_I_TO_G;
using gl::GL_PIXEL_MAP_I_TO_B;
using gl::GL_PIXEL_MAP_I_TO_A;
using gl::GL_PIXEL_MAP_R_TO_R;
using gl::GL_PIXEL_MAP_G_TO_G;
using gl::GL_PIXEL_MAP_B_TO_B;
using gl::GL_PIXEL_MAP_A_TO_A;

// GetPointervPName

using gl::GL_FEEDBACK_BUFFER_POINTER;
using gl::GL_SELECTION_BUFFER_POINTER;
using gl::GL_VERTEX_ARRAY_POINTER;
using gl::GL_VERTEX_ARRAY_POINTER_EXT;
using gl::GL_NORMAL_ARRAY_POINTER;
using gl::GL_NORMAL_ARRAY_POINTER_EXT;
using gl::GL_COLOR_ARRAY_POINTER;
using gl::GL_COLOR_ARRAY_POINTER_EXT;
using gl::GL_INDEX_ARRAY_POINTER;
using gl::GL_INDEX_ARRAY_POINTER_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_POINTER;
using gl::GL_TEXTURE_COORD_ARRAY_POINTER_EXT;
using gl::GL_EDGE_FLAG_ARRAY_POINTER;
using gl::GL_EDGE_FLAG_ARRAY_POINTER_EXT;
using gl::GL_INSTRUMENT_BUFFER_POINTER_SGIX;

// GetTextureParameter

using gl::GL_TEXTURE_WIDTH;
using gl::GL_TEXTURE_HEIGHT;
using gl::GL_TEXTURE_COMPONENTS;
using gl::GL_TEXTURE_INTERNAL_FORMAT;
using gl::GL_TEXTURE_BORDER_COLOR;
using gl::GL_TEXTURE_BORDER;
using gl::GL_TEXTURE_MAG_FILTER;
using gl::GL_TEXTURE_MIN_FILTER;
using gl::GL_TEXTURE_WRAP_S;
using gl::GL_TEXTURE_WRAP_T;
using gl::GL_TEXTURE_RED_SIZE;
using gl::GL_TEXTURE_GREEN_SIZE;
using gl::GL_TEXTURE_BLUE_SIZE;
using gl::GL_TEXTURE_ALPHA_SIZE;
using gl::GL_TEXTURE_LUMINANCE_SIZE;
using gl::GL_TEXTURE_INTENSITY_SIZE;
using gl::GL_TEXTURE_PRIORITY;
using gl::GL_TEXTURE_RESIDENT;
using gl::GL_TEXTURE_DEPTH_EXT;
using gl::GL_TEXTURE_WRAP_R_EXT;
using gl::GL_DETAIL_TEXTURE_LEVEL_SGIS;
using gl::GL_DETAIL_TEXTURE_MODE_SGIS;
using gl::GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS;
using gl::GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS;
using gl::GL_SHADOW_AMBIENT_SGIX;
using gl::GL_DUAL_TEXTURE_SELECT_SGIS;
using gl::GL_QUAD_TEXTURE_SELECT_SGIS;
using gl::GL_TEXTURE_4DSIZE_SGIS;
using gl::GL_TEXTURE_WRAP_Q_SGIS;
using gl::GL_TEXTURE_MIN_LOD_SGIS;
using gl::GL_TEXTURE_MAX_LOD_SGIS;
using gl::GL_TEXTURE_BASE_LEVEL_SGIS;
using gl::GL_TEXTURE_MAX_LEVEL_SGIS;
using gl::GL_TEXTURE_FILTER4_SIZE_SGIS;
using gl::GL_TEXTURE_CLIPMAP_CENTER_SGIX;
using gl::GL_TEXTURE_CLIPMAP_FRAME_SGIX;
using gl::GL_TEXTURE_CLIPMAP_OFFSET_SGIX;
using gl::GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX;
using gl::GL_TEXTURE_CLIPMAP_LOD_OFFSET_SGIX;
using gl::GL_TEXTURE_CLIPMAP_DEPTH_SGIX;
using gl::GL_POST_TEXTURE_FILTER_BIAS_SGIX;
using gl::GL_POST_TEXTURE_FILTER_SCALE_SGIX;
using gl::GL_TEXTURE_LOD_BIAS_S_SGIX;
using gl::GL_TEXTURE_LOD_BIAS_T_SGIX;
using gl::GL_TEXTURE_LOD_BIAS_R_SGIX;
using gl::GL_GENERATE_MIPMAP_SGIS;
using gl::GL_TEXTURE_COMPARE_SGIX;
using gl::GL_TEXTURE_COMPARE_OPERATOR_SGIX;
using gl::GL_TEXTURE_LEQUAL_R_SGIX;
using gl::GL_TEXTURE_GEQUAL_R_SGIX;
using gl::GL_TEXTURE_MAX_CLAMP_S_SGIX;
using gl::GL_TEXTURE_MAX_CLAMP_T_SGIX;
using gl::GL_TEXTURE_MAX_CLAMP_R_SGIX;

// HintMode

using gl::GL_DONT_CARE;
using gl::GL_FASTEST;
using gl::GL_NICEST;

// HintTarget

// using gl::GL_PERSPECTIVE_CORRECTION_HINT; // reuse GetPName
// using gl::GL_POINT_SMOOTH_HINT; // reuse GetPName
// using gl::GL_LINE_SMOOTH_HINT; // reuse GetPName
// using gl::GL_POLYGON_SMOOTH_HINT; // reuse GetPName
// using gl::GL_FOG_HINT; // reuse GetPName
using gl::GL_PREFER_DOUBLEBUFFER_HINT_PGI;
using gl::GL_CONSERVE_MEMORY_HINT_PGI;
using gl::GL_RECLAIM_MEMORY_HINT_PGI;
using gl::GL_NATIVE_GRAPHICS_BEGIN_HINT_PGI;
using gl::GL_NATIVE_GRAPHICS_END_HINT_PGI;
using gl::GL_ALWAYS_FAST_HINT_PGI;
using gl::GL_ALWAYS_SOFT_HINT_PGI;
using gl::GL_ALLOW_DRAW_OBJ_HINT_PGI;
using gl::GL_ALLOW_DRAW_WIN_HINT_PGI;
using gl::GL_ALLOW_DRAW_FRG_HINT_PGI;
using gl::GL_ALLOW_DRAW_MEM_HINT_PGI;
using gl::GL_STRICT_DEPTHFUNC_HINT_PGI;
using gl::GL_STRICT_LIGHTING_HINT_PGI;
using gl::GL_STRICT_SCISSOR_HINT_PGI;
using gl::GL_FULL_STIPPLE_HINT_PGI;
using gl::GL_CLIP_NEAR_HINT_PGI;
using gl::GL_CLIP_FAR_HINT_PGI;
using gl::GL_WIDE_LINE_HINT_PGI;
using gl::GL_BACK_NORMALS_HINT_PGI;
using gl::GL_VERTEX_DATA_HINT_PGI;
using gl::GL_VERTEX_CONSISTENT_HINT_PGI;
using gl::GL_MATERIAL_SIDE_HINT_PGI;
using gl::GL_MAX_VERTEX_HINT_PGI;
// using gl::GL_PACK_CMYK_HINT_EXT; // reuse GetPName
// using gl::GL_UNPACK_CMYK_HINT_EXT; // reuse GetPName
using gl::GL_PHONG_HINT_WIN;
using gl::GL_CLIP_VOLUME_CLIPPING_HINT_EXT;
using gl::GL_TEXTURE_MULTI_BUFFER_HINT_SGIX;
using gl::GL_GENERATE_MIPMAP_HINT;
// using gl::GL_GENERATE_MIPMAP_HINT_SGIS; // reuse GetPName
using gl::GL_PROGRAM_BINARY_RETRIEVABLE_HINT;
// using gl::GL_CONVOLUTION_HINT_SGIX; // reuse GetPName
using gl::GL_SCALEBIAS_HINT_SGIX;
// using gl::GL_VERTEX_PRECLIP_SGIX; // reuse GetPName
// using gl::GL_VERTEX_PRECLIP_HINT_SGIX; // reuse GetPName
using gl::GL_TEXTURE_COMPRESSION_HINT;
using gl::GL_TEXTURE_COMPRESSION_HINT_ARB;
using gl::GL_VERTEX_ARRAY_STORAGE_HINT_APPLE;
using gl::GL_MULTISAMPLE_FILTER_HINT_NV;
using gl::GL_TRANSFORM_HINT_APPLE;
using gl::GL_TEXTURE_STORAGE_HINT_APPLE;
using gl::GL_FRAGMENT_SHADER_DERIVATIVE_HINT;
using gl::GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB;

// HistogramTargetEXT

using gl::GL_HISTOGRAM;
// using gl::GL_HISTOGRAM_EXT; // reuse EnableCap
using gl::GL_PROXY_HISTOGRAM;
using gl::GL_PROXY_HISTOGRAM_EXT;

// IndexPointerType

// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// InterleavedArrayFormat

using gl::GL_V2F;
using gl::GL_V3F;
using gl::GL_C4UB_V2F;
using gl::GL_C4UB_V3F;
using gl::GL_C3F_V3F;
using gl::GL_N3F_V3F;
using gl::GL_C4F_N3F_V3F;
using gl::GL_T2F_V3F;
using gl::GL_T4F_V4F;
using gl::GL_T2F_C4UB_V3F;
using gl::GL_T2F_C3F_V3F;
using gl::GL_T2F_N3F_V3F;
using gl::GL_T2F_C4F_N3F_V3F;
using gl::GL_T4F_C4F_N3F_V4F;

// InternalFormat

using gl::GL_R3_G3_B2;
using gl::GL_ALPHA4;
using gl::GL_ALPHA8;
using gl::GL_ALPHA12;
using gl::GL_ALPHA16;
using gl::GL_LUMINANCE4;
using gl::GL_LUMINANCE8;
using gl::GL_LUMINANCE12;
using gl::GL_LUMINANCE16;
using gl::GL_LUMINANCE4_ALPHA4;
using gl::GL_LUMINANCE6_ALPHA2;
using gl::GL_LUMINANCE8_ALPHA8;
using gl::GL_LUMINANCE12_ALPHA4;
using gl::GL_LUMINANCE12_ALPHA12;
using gl::GL_LUMINANCE16_ALPHA16;
using gl::GL_INTENSITY;
using gl::GL_INTENSITY4;
using gl::GL_INTENSITY8;
using gl::GL_INTENSITY12;
using gl::GL_INTENSITY16;
using gl::GL_RGB2_EXT;
using gl::GL_RGB4;
using gl::GL_RGB5;
using gl::GL_RGB8;
using gl::GL_RGB10;
using gl::GL_RGB12;
using gl::GL_RGB16;
using gl::GL_RGBA2;
using gl::GL_RGBA4;
using gl::GL_RGB5_A1;
using gl::GL_RGBA8;
using gl::GL_RGB10_A2;
using gl::GL_RGBA12;
using gl::GL_RGBA16;
using gl::GL_DUAL_ALPHA4_SGIS;
using gl::GL_DUAL_ALPHA8_SGIS;
using gl::GL_DUAL_ALPHA12_SGIS;
using gl::GL_DUAL_ALPHA16_SGIS;
using gl::GL_DUAL_LUMINANCE4_SGIS;
using gl::GL_DUAL_LUMINANCE8_SGIS;
using gl::GL_DUAL_LUMINANCE12_SGIS;
using gl::GL_DUAL_LUMINANCE16_SGIS;
using gl::GL_DUAL_INTENSITY4_SGIS;
using gl::GL_DUAL_INTENSITY8_SGIS;
using gl::GL_DUAL_INTENSITY12_SGIS;
using gl::GL_DUAL_INTENSITY16_SGIS;
using gl::GL_DUAL_LUMINANCE_ALPHA4_SGIS;
using gl::GL_DUAL_LUMINANCE_ALPHA8_SGIS;
using gl::GL_QUAD_ALPHA4_SGIS;
using gl::GL_QUAD_ALPHA8_SGIS;
using gl::GL_QUAD_LUMINANCE4_SGIS;
using gl::GL_QUAD_LUMINANCE8_SGIS;
using gl::GL_QUAD_INTENSITY4_SGIS;
using gl::GL_QUAD_INTENSITY8_SGIS;
using gl::GL_DEPTH_COMPONENT16_SGIX;
using gl::GL_DEPTH_COMPONENT24_SGIX;
using gl::GL_DEPTH_COMPONENT32_SGIX;

// LightEnvModeSGIX

// using gl::GL_ADD; // reuse AccumOp
using gl::GL_REPLACE;
using gl::GL_MODULATE;

// LightEnvParameterSGIX

// using gl::GL_LIGHT_ENV_MODE_SGIX; // reuse GetPName

// LightModelColorControl

using gl::GL_SINGLE_COLOR;
using gl::GL_SINGLE_COLOR_EXT;
using gl::GL_SEPARATE_SPECULAR_COLOR;
using gl::GL_SEPARATE_SPECULAR_COLOR_EXT;

// LightModelParameter

// using gl::GL_LIGHT_MODEL_LOCAL_VIEWER; // reuse GetPName
// using gl::GL_LIGHT_MODEL_TWO_SIDE; // reuse GetPName
// using gl::GL_LIGHT_MODEL_AMBIENT; // reuse GetPName
// using gl::GL_LIGHT_MODEL_COLOR_CONTROL; // reuse GetPName
using gl::GL_LIGHT_MODEL_COLOR_CONTROL_EXT;

// LightName

// using gl::GL_LIGHT0; // reuse EnableCap
// using gl::GL_LIGHT1; // reuse EnableCap
// using gl::GL_LIGHT2; // reuse EnableCap
// using gl::GL_LIGHT3; // reuse EnableCap
// using gl::GL_LIGHT4; // reuse EnableCap
// using gl::GL_LIGHT5; // reuse EnableCap
// using gl::GL_LIGHT6; // reuse EnableCap
// using gl::GL_LIGHT7; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT0_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT1_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT2_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT3_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT4_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT5_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT6_SGIX; // reuse EnableCap
// using gl::GL_FRAGMENT_LIGHT7_SGIX; // reuse EnableCap

// LightParameter

// using gl::GL_AMBIENT; // reuse ColorMaterialParameter
// using gl::GL_DIFFUSE; // reuse ColorMaterialParameter
// using gl::GL_SPECULAR; // reuse ColorMaterialParameter
using gl::GL_POSITION;
using gl::GL_SPOT_DIRECTION;
using gl::GL_SPOT_EXPONENT;
using gl::GL_SPOT_CUTOFF;
using gl::GL_CONSTANT_ATTENUATION;
using gl::GL_LINEAR_ATTENUATION;
using gl::GL_QUADRATIC_ATTENUATION;

// ListMode

using gl::GL_COMPILE;
using gl::GL_COMPILE_AND_EXECUTE;

// ListNameType

// using gl::GL_BYTE; // reuse ColorPointerType
// using gl::GL_UNSIGNED_BYTE; // reuse ColorPointerType
// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_UNSIGNED_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_UNSIGNED_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
using gl::GL_2_BYTES;
using gl::GL_3_BYTES;
using gl::GL_4_BYTES;

// ListParameterName

using gl::GL_LIST_PRIORITY_SGIX;

// LogicOp

using gl::GL_CLEAR;
using gl::GL_AND;
using gl::GL_AND_REVERSE;
using gl::GL_COPY;
using gl::GL_AND_INVERTED;
using gl::GL_NOOP;
using gl::GL_XOR;
using gl::GL_OR;
using gl::GL_NOR;
using gl::GL_EQUIV;
using gl::GL_INVERT;
using gl::GL_OR_REVERSE;
using gl::GL_COPY_INVERTED;
using gl::GL_OR_INVERTED;
using gl::GL_NAND;
using gl::GL_SET;

// MapTarget

// using gl::GL_MAP1_COLOR_4; // reuse EnableCap
// using gl::GL_MAP1_INDEX; // reuse EnableCap
// using gl::GL_MAP1_NORMAL; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_1; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_2; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_3; // reuse EnableCap
// using gl::GL_MAP1_TEXTURE_COORD_4; // reuse EnableCap
// using gl::GL_MAP1_VERTEX_3; // reuse EnableCap
// using gl::GL_MAP1_VERTEX_4; // reuse EnableCap
// using gl::GL_MAP2_COLOR_4; // reuse EnableCap
// using gl::GL_MAP2_INDEX; // reuse EnableCap
// using gl::GL_MAP2_NORMAL; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_1; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_2; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_3; // reuse EnableCap
// using gl::GL_MAP2_TEXTURE_COORD_4; // reuse EnableCap
// using gl::GL_MAP2_VERTEX_3; // reuse EnableCap
// using gl::GL_MAP2_VERTEX_4; // reuse EnableCap
// using gl::GL_GEOMETRY_DEFORMATION_SGIX; // reuse FfdTargetSGIX
// using gl::GL_TEXTURE_DEFORMATION_SGIX; // reuse FfdTargetSGIX

// MapTextureFormatINTEL

using gl::GL_LAYOUT_DEFAULT_INTEL;
using gl::GL_LAYOUT_LINEAR_INTEL;
using gl::GL_LAYOUT_LINEAR_CPU_CACHED_INTEL;

// MaterialFace

// using gl::GL_FRONT; // reuse ColorMaterialFace
// using gl::GL_BACK; // reuse ColorMaterialFace
// using gl::GL_FRONT_AND_BACK; // reuse ColorMaterialFace

// MaterialParameter

// using gl::GL_AMBIENT; // reuse ColorMaterialParameter
// using gl::GL_DIFFUSE; // reuse ColorMaterialParameter
// using gl::GL_SPECULAR; // reuse ColorMaterialParameter
// using gl::GL_EMISSION; // reuse ColorMaterialParameter
using gl::GL_SHININESS;
// using gl::GL_AMBIENT_AND_DIFFUSE; // reuse ColorMaterialParameter
using gl::GL_COLOR_INDEXES;

// MatrixMode

using gl::GL_MODELVIEW;
using gl::GL_MODELVIEW0_EXT;
using gl::GL_PROJECTION;
using gl::GL_TEXTURE;

// MeshMode1

using gl::GL_POINT;
using gl::GL_LINE;

// MeshMode2

// using gl::GL_POINT; // reuse MeshMode1
// using gl::GL_LINE; // reuse MeshMode1
using gl::GL_FILL;

// MinmaxTargetEXT

using gl::GL_MINMAX;
// using gl::GL_MINMAX_EXT; // reuse EnableCap

// NormalPointerType

// using gl::GL_BYTE; // reuse ColorPointerType
// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// OcclusionQueryEventMaskAMD

using gl::GL_QUERY_DEPTH_PASS_EVENT_BIT_AMD;
using gl::GL_QUERY_DEPTH_FAIL_EVENT_BIT_AMD;
using gl::GL_QUERY_STENCIL_FAIL_EVENT_BIT_AMD;
using gl::GL_QUERY_DEPTH_BOUNDS_FAIL_EVENT_BIT_AMD;
using gl::GL_QUERY_ALL_EVENT_BITS_AMD;

// PixelCopyType

using gl::GL_COLOR;
using gl::GL_DEPTH;
using gl::GL_STENCIL;

// PixelFormat

// using gl::GL_UNSIGNED_SHORT; // reuse ColorPointerType
// using gl::GL_UNSIGNED_INT; // reuse ColorPointerType
using gl::GL_COLOR_INDEX;
using gl::GL_STENCIL_INDEX;
using gl::GL_DEPTH_COMPONENT;
using gl::GL_RED;
using gl::GL_GREEN;
using gl::GL_BLUE;
using gl::GL_ALPHA;
using gl::GL_RGB;
using gl::GL_RGBA;
using gl::GL_LUMINANCE;
using gl::GL_LUMINANCE_ALPHA;
using gl::GL_ABGR_EXT;
using gl::GL_CMYK_EXT;
using gl::GL_CMYKA_EXT;
using gl::GL_YCRCB_422_SGIX;
using gl::GL_YCRCB_444_SGIX;

// PixelMap

// using gl::GL_PIXEL_MAP_I_TO_I; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_S_TO_S; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_I_TO_R; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_I_TO_G; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_I_TO_B; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_I_TO_A; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_R_TO_R; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_G_TO_G; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_B_TO_B; // reuse GetPixelMap
// using gl::GL_PIXEL_MAP_A_TO_A; // reuse GetPixelMap

// PixelStoreParameter

// using gl::GL_UNPACK_SWAP_BYTES; // reuse GetPName
// using gl::GL_UNPACK_LSB_FIRST; // reuse GetPName
// using gl::GL_UNPACK_ROW_LENGTH; // reuse GetPName
// using gl::GL_UNPACK_SKIP_ROWS; // reuse GetPName
// using gl::GL_UNPACK_SKIP_PIXELS; // reuse GetPName
// using gl::GL_UNPACK_ALIGNMENT; // reuse GetPName
// using gl::GL_PACK_SWAP_BYTES; // reuse GetPName
// using gl::GL_PACK_LSB_FIRST; // reuse GetPName
// using gl::GL_PACK_ROW_LENGTH; // reuse GetPName
// using gl::GL_PACK_SKIP_ROWS; // reuse GetPName
// using gl::GL_PACK_SKIP_PIXELS; // reuse GetPName
// using gl::GL_PACK_ALIGNMENT; // reuse GetPName
using gl::GL_PACK_SKIP_IMAGES;
// using gl::GL_PACK_SKIP_IMAGES_EXT; // reuse GetPName
using gl::GL_PACK_IMAGE_HEIGHT;
// using gl::GL_PACK_IMAGE_HEIGHT_EXT; // reuse GetPName
using gl::GL_UNPACK_SKIP_IMAGES;
// using gl::GL_UNPACK_SKIP_IMAGES_EXT; // reuse GetPName
using gl::GL_UNPACK_IMAGE_HEIGHT;
// using gl::GL_UNPACK_IMAGE_HEIGHT_EXT; // reuse GetPName
// using gl::GL_PACK_SKIP_VOLUMES_SGIS; // reuse GetPName
// using gl::GL_PACK_IMAGE_DEPTH_SGIS; // reuse GetPName
// using gl::GL_UNPACK_SKIP_VOLUMES_SGIS; // reuse GetPName
// using gl::GL_UNPACK_IMAGE_DEPTH_SGIS; // reuse GetPName
// using gl::GL_PIXEL_TILE_WIDTH_SGIX; // reuse GetPName
// using gl::GL_PIXEL_TILE_HEIGHT_SGIX; // reuse GetPName
// using gl::GL_PIXEL_TILE_GRID_WIDTH_SGIX; // reuse GetPName
// using gl::GL_PIXEL_TILE_GRID_HEIGHT_SGIX; // reuse GetPName
// using gl::GL_PIXEL_TILE_GRID_DEPTH_SGIX; // reuse GetPName
// using gl::GL_PIXEL_TILE_CACHE_SIZE_SGIX; // reuse GetPName
// using gl::GL_PACK_RESAMPLE_SGIX; // reuse GetPName
// using gl::GL_UNPACK_RESAMPLE_SGIX; // reuse GetPName
// using gl::GL_PACK_SUBSAMPLE_RATE_SGIX; // reuse GetPName
// using gl::GL_UNPACK_SUBSAMPLE_RATE_SGIX; // reuse GetPName
using gl::GL_PACK_RESAMPLE_OML;
using gl::GL_UNPACK_RESAMPLE_OML;

// PixelStoreResampleMode

using gl::GL_RESAMPLE_DECIMATE_SGIX;
using gl::GL_RESAMPLE_REPLICATE_SGIX;
using gl::GL_RESAMPLE_ZERO_FILL_SGIX;

// PixelStoreSubsampleRate

using gl::GL_PIXEL_SUBSAMPLE_4444_SGIX;
using gl::GL_PIXEL_SUBSAMPLE_2424_SGIX;
using gl::GL_PIXEL_SUBSAMPLE_4242_SGIX;

// PixelTexGenMode

// using gl::GL_NONE; // reuse DrawBufferMode
// using gl::GL_RGB; // reuse PixelFormat
// using gl::GL_RGBA; // reuse PixelFormat
// using gl::GL_LUMINANCE; // reuse PixelFormat
// using gl::GL_LUMINANCE_ALPHA; // reuse PixelFormat

// PixelTexGenParameterNameSGIS

using gl::GL_PIXEL_FRAGMENT_RGB_SOURCE_SGIS;
using gl::GL_PIXEL_FRAGMENT_ALPHA_SOURCE_SGIS;

// PixelTransferParameter

// using gl::GL_MAP_COLOR; // reuse GetPName
// using gl::GL_MAP_STENCIL; // reuse GetPName
// using gl::GL_INDEX_SHIFT; // reuse GetPName
// using gl::GL_INDEX_OFFSET; // reuse GetPName
// using gl::GL_RED_SCALE; // reuse GetPName
// using gl::GL_RED_BIAS; // reuse GetPName
// using gl::GL_GREEN_SCALE; // reuse GetPName
// using gl::GL_GREEN_BIAS; // reuse GetPName
// using gl::GL_BLUE_SCALE; // reuse GetPName
// using gl::GL_BLUE_BIAS; // reuse GetPName
// using gl::GL_ALPHA_SCALE; // reuse GetPName
// using gl::GL_ALPHA_BIAS; // reuse GetPName
// using gl::GL_DEPTH_SCALE; // reuse GetPName
// using gl::GL_DEPTH_BIAS; // reuse GetPName
using gl::GL_POST_CONVOLUTION_RED_SCALE;
// using gl::GL_POST_CONVOLUTION_RED_SCALE_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_GREEN_SCALE;
// using gl::GL_POST_CONVOLUTION_GREEN_SCALE_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_BLUE_SCALE;
// using gl::GL_POST_CONVOLUTION_BLUE_SCALE_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_ALPHA_SCALE;
// using gl::GL_POST_CONVOLUTION_ALPHA_SCALE_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_RED_BIAS;
// using gl::GL_POST_CONVOLUTION_RED_BIAS_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_GREEN_BIAS;
// using gl::GL_POST_CONVOLUTION_GREEN_BIAS_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_BLUE_BIAS;
// using gl::GL_POST_CONVOLUTION_BLUE_BIAS_EXT; // reuse GetPName
using gl::GL_POST_CONVOLUTION_ALPHA_BIAS;
// using gl::GL_POST_CONVOLUTION_ALPHA_BIAS_EXT; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_RED_SCALE;
// using gl::GL_POST_COLOR_MATRIX_RED_SCALE_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_GREEN_SCALE;
// using gl::GL_POST_COLOR_MATRIX_GREEN_SCALE_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_BLUE_SCALE;
// using gl::GL_POST_COLOR_MATRIX_BLUE_SCALE_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_ALPHA_SCALE;
// using gl::GL_POST_COLOR_MATRIX_ALPHA_SCALE_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_RED_BIAS;
// using gl::GL_POST_COLOR_MATRIX_RED_BIAS_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_GREEN_BIAS;
// using gl::GL_POST_COLOR_MATRIX_GREEN_BIAS_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_BLUE_BIAS;
// using gl::GL_POST_COLOR_MATRIX_BLUE_BIAS_SGI; // reuse GetPName
using gl::GL_POST_COLOR_MATRIX_ALPHA_BIAS;
// using gl::GL_POST_COLOR_MATRIX_ALPHA_BIAS_SGI; // reuse GetPName

// PixelType

// using gl::GL_BYTE; // reuse ColorPointerType
// using gl::GL_UNSIGNED_BYTE; // reuse ColorPointerType
// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_UNSIGNED_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_UNSIGNED_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
using gl::GL_BITMAP;
using gl::GL_UNSIGNED_BYTE_3_3_2;
using gl::GL_UNSIGNED_BYTE_3_3_2_EXT;
using gl::GL_UNSIGNED_SHORT_4_4_4_4;
using gl::GL_UNSIGNED_SHORT_4_4_4_4_EXT;
using gl::GL_UNSIGNED_SHORT_5_5_5_1;
using gl::GL_UNSIGNED_SHORT_5_5_5_1_EXT;
using gl::GL_UNSIGNED_INT_8_8_8_8;
using gl::GL_UNSIGNED_INT_8_8_8_8_EXT;
using gl::GL_UNSIGNED_INT_10_10_10_2;
using gl::GL_UNSIGNED_INT_10_10_10_2_EXT;

// PointParameterNameSGIS

using gl::GL_POINT_SIZE_MIN;
using gl::GL_POINT_SIZE_MIN_ARB;
using gl::GL_POINT_SIZE_MIN_EXT;
// using gl::GL_POINT_SIZE_MIN_SGIS; // reuse GetPName
using gl::GL_POINT_SIZE_MAX;
using gl::GL_POINT_SIZE_MAX_ARB;
using gl::GL_POINT_SIZE_MAX_EXT;
// using gl::GL_POINT_SIZE_MAX_SGIS; // reuse GetPName
using gl::GL_POINT_FADE_THRESHOLD_SIZE;
using gl::GL_POINT_FADE_THRESHOLD_SIZE_ARB;
using gl::GL_POINT_FADE_THRESHOLD_SIZE_EXT;
// using gl::GL_POINT_FADE_THRESHOLD_SIZE_SGIS; // reuse GetPName
using gl::GL_DISTANCE_ATTENUATION_EXT;
// using gl::GL_DISTANCE_ATTENUATION_SGIS; // reuse GetPName
using gl::GL_POINT_DISTANCE_ATTENUATION;
using gl::GL_POINT_DISTANCE_ATTENUATION_ARB;

// PolygonMode

// using gl::GL_POINT; // reuse MeshMode1
// using gl::GL_LINE; // reuse MeshMode1
// using gl::GL_FILL; // reuse MeshMode2

// PrimitiveType

using gl::GL_POINTS;
using gl::GL_LINES;
using gl::GL_LINE_LOOP;
using gl::GL_LINE_STRIP;
using gl::GL_TRIANGLES;
using gl::GL_TRIANGLE_STRIP;
using gl::GL_TRIANGLE_FAN;
using gl::GL_QUADS;
using gl::GL_QUAD_STRIP;
using gl::GL_POLYGON;
using gl::GL_LINES_ADJACENCY;
using gl::GL_LINES_ADJACENCY_ARB;
using gl::GL_LINES_ADJACENCY_EXT;
using gl::GL_LINE_STRIP_ADJACENCY;
using gl::GL_LINE_STRIP_ADJACENCY_ARB;
using gl::GL_LINE_STRIP_ADJACENCY_EXT;
using gl::GL_TRIANGLES_ADJACENCY;
using gl::GL_TRIANGLES_ADJACENCY_ARB;
using gl::GL_TRIANGLES_ADJACENCY_EXT;
using gl::GL_TRIANGLE_STRIP_ADJACENCY;
using gl::GL_TRIANGLE_STRIP_ADJACENCY_ARB;
using gl::GL_TRIANGLE_STRIP_ADJACENCY_EXT;
using gl::GL_PATCHES;

// ReadBufferMode

// using gl::GL_FRONT_LEFT; // reuse DrawBufferMode
// using gl::GL_FRONT_RIGHT; // reuse DrawBufferMode
// using gl::GL_BACK_LEFT; // reuse DrawBufferMode
// using gl::GL_BACK_RIGHT; // reuse DrawBufferMode
// using gl::GL_FRONT; // reuse ColorMaterialFace
// using gl::GL_BACK; // reuse ColorMaterialFace
// using gl::GL_LEFT; // reuse DrawBufferMode
// using gl::GL_RIGHT; // reuse DrawBufferMode
// using gl::GL_AUX0; // reuse DrawBufferMode
// using gl::GL_AUX1; // reuse DrawBufferMode
// using gl::GL_AUX2; // reuse DrawBufferMode
// using gl::GL_AUX3; // reuse DrawBufferMode

// RenderingMode

using gl::GL_RENDER;
using gl::GL_FEEDBACK;
using gl::GL_SELECT;

// SamplePatternSGIS

using gl::GL_1PASS_EXT;
using gl::GL_1PASS_SGIS;
using gl::GL_2PASS_0_EXT;
using gl::GL_2PASS_0_SGIS;
using gl::GL_2PASS_1_EXT;
using gl::GL_2PASS_1_SGIS;
using gl::GL_4PASS_0_EXT;
using gl::GL_4PASS_0_SGIS;
using gl::GL_4PASS_1_EXT;
using gl::GL_4PASS_1_SGIS;
using gl::GL_4PASS_2_EXT;
using gl::GL_4PASS_2_SGIS;
using gl::GL_4PASS_3_EXT;
using gl::GL_4PASS_3_SGIS;

// SeparableTargetEXT

using gl::GL_SEPARABLE_2D;
// using gl::GL_SEPARABLE_2D_EXT; // reuse EnableCap

// ShadingModel

using gl::GL_FLAT;
using gl::GL_SMOOTH;

// StencilFunction

// using gl::GL_NEVER; // reuse AlphaFunction
// using gl::GL_LESS; // reuse AlphaFunction
// using gl::GL_EQUAL; // reuse AlphaFunction
// using gl::GL_LEQUAL; // reuse AlphaFunction
// using gl::GL_GREATER; // reuse AlphaFunction
// using gl::GL_NOTEQUAL; // reuse AlphaFunction
// using gl::GL_GEQUAL; // reuse AlphaFunction
// using gl::GL_ALWAYS; // reuse AlphaFunction

// StencilOp

// using gl::GL_ZERO; // reuse BlendingFactorDest
// using gl::GL_INVERT; // reuse LogicOp
using gl::GL_KEEP;
// using gl::GL_REPLACE; // reuse LightEnvModeSGIX
using gl::GL_INCR;
using gl::GL_DECR;

// StringName

using gl::GL_VENDOR;
using gl::GL_RENDERER;
using gl::GL_VERSION;
using gl::GL_EXTENSIONS;

// TexCoordPointerType

// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// TextureCoordName

using gl::GL_S;
using gl::GL_T;
using gl::GL_R;
using gl::GL_Q;

// TextureEnvMode

// using gl::GL_ADD; // reuse AccumOp
// using gl::GL_BLEND; // reuse EnableCap
// using gl::GL_MODULATE; // reuse LightEnvModeSGIX
using gl::GL_DECAL;
using gl::GL_REPLACE_EXT;
using gl::GL_TEXTURE_ENV_BIAS_SGIX;

// TextureEnvParameter

using gl::GL_TEXTURE_ENV_MODE;
using gl::GL_TEXTURE_ENV_COLOR;

// TextureEnvTarget

using gl::GL_TEXTURE_ENV;

// TextureFilterFuncSGIS

using gl::GL_FILTER4_SGIS;

// TextureGenMode

using gl::GL_EYE_LINEAR;
using gl::GL_OBJECT_LINEAR;
using gl::GL_SPHERE_MAP;
using gl::GL_EYE_DISTANCE_TO_POINT_SGIS;
using gl::GL_OBJECT_DISTANCE_TO_POINT_SGIS;
using gl::GL_EYE_DISTANCE_TO_LINE_SGIS;
using gl::GL_OBJECT_DISTANCE_TO_LINE_SGIS;

// TextureGenParameter

using gl::GL_TEXTURE_GEN_MODE;
using gl::GL_OBJECT_PLANE;
using gl::GL_EYE_PLANE;
using gl::GL_EYE_POINT_SGIS;
using gl::GL_OBJECT_POINT_SGIS;
using gl::GL_EYE_LINE_SGIS;
using gl::GL_OBJECT_LINE_SGIS;

// TextureMagFilter

using gl::GL_NEAREST;
// using gl::GL_LINEAR; // reuse FogMode
using gl::GL_LINEAR_DETAIL_SGIS;
using gl::GL_LINEAR_DETAIL_ALPHA_SGIS;
using gl::GL_LINEAR_DETAIL_COLOR_SGIS;
using gl::GL_LINEAR_SHARPEN_SGIS;
using gl::GL_LINEAR_SHARPEN_ALPHA_SGIS;
using gl::GL_LINEAR_SHARPEN_COLOR_SGIS;
// using gl::GL_FILTER4_SGIS; // reuse TextureFilterFuncSGIS

// TextureMinFilter

// using gl::GL_NEAREST; // reuse TextureMagFilter
// using gl::GL_LINEAR; // reuse FogMode
using gl::GL_NEAREST_MIPMAP_NEAREST;
using gl::GL_LINEAR_MIPMAP_NEAREST;
using gl::GL_NEAREST_MIPMAP_LINEAR;
using gl::GL_LINEAR_MIPMAP_LINEAR;
// using gl::GL_FILTER4_SGIS; // reuse TextureFilterFuncSGIS
using gl::GL_LINEAR_CLIPMAP_LINEAR_SGIX;
using gl::GL_NEAREST_CLIPMAP_NEAREST_SGIX;
using gl::GL_NEAREST_CLIPMAP_LINEAR_SGIX;
using gl::GL_LINEAR_CLIPMAP_NEAREST_SGIX;

// TextureParameterName

// using gl::GL_TEXTURE_BORDER_COLOR; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MAG_FILTER; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MIN_FILTER; // reuse GetTextureParameter
// using gl::GL_TEXTURE_WRAP_S; // reuse GetTextureParameter
// using gl::GL_TEXTURE_WRAP_T; // reuse GetTextureParameter
// using gl::GL_TEXTURE_PRIORITY; // reuse GetTextureParameter
using gl::GL_TEXTURE_PRIORITY_EXT;
using gl::GL_TEXTURE_WRAP_R;
// using gl::GL_TEXTURE_WRAP_R_EXT; // reuse GetTextureParameter
// using gl::GL_DETAIL_TEXTURE_LEVEL_SGIS; // reuse GetTextureParameter
// using gl::GL_DETAIL_TEXTURE_MODE_SGIS; // reuse GetTextureParameter
// using gl::GL_SHADOW_AMBIENT_SGIX; // reuse GetTextureParameter
// using gl::GL_DUAL_TEXTURE_SELECT_SGIS; // reuse GetTextureParameter
// using gl::GL_QUAD_TEXTURE_SELECT_SGIS; // reuse GetTextureParameter
// using gl::GL_TEXTURE_WRAP_Q_SGIS; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_CENTER_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_FRAME_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_OFFSET_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_LOD_OFFSET_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_CLIPMAP_DEPTH_SGIX; // reuse GetTextureParameter
// using gl::GL_POST_TEXTURE_FILTER_BIAS_SGIX; // reuse GetTextureParameter
// using gl::GL_POST_TEXTURE_FILTER_SCALE_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_LOD_BIAS_S_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_LOD_BIAS_T_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_LOD_BIAS_R_SGIX; // reuse GetTextureParameter
using gl::GL_GENERATE_MIPMAP;
// using gl::GL_GENERATE_MIPMAP_SGIS; // reuse GetTextureParameter
// using gl::GL_TEXTURE_COMPARE_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MAX_CLAMP_S_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MAX_CLAMP_T_SGIX; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MAX_CLAMP_R_SGIX; // reuse GetTextureParameter

// TextureTarget

// using gl::GL_TEXTURE_1D; // reuse EnableCap
// using gl::GL_TEXTURE_2D; // reuse EnableCap
using gl::GL_PROXY_TEXTURE_1D;
using gl::GL_PROXY_TEXTURE_1D_EXT;
using gl::GL_PROXY_TEXTURE_2D;
using gl::GL_PROXY_TEXTURE_2D_EXT;
using gl::GL_TEXTURE_3D;
// using gl::GL_TEXTURE_3D_EXT; // reuse EnableCap
using gl::GL_PROXY_TEXTURE_3D;
using gl::GL_PROXY_TEXTURE_3D_EXT;
using gl::GL_DETAIL_TEXTURE_2D_SGIS;
// using gl::GL_TEXTURE_4D_SGIS; // reuse EnableCap
using gl::GL_PROXY_TEXTURE_4D_SGIS;
using gl::GL_TEXTURE_MIN_LOD;
// using gl::GL_TEXTURE_MIN_LOD_SGIS; // reuse GetTextureParameter
using gl::GL_TEXTURE_MAX_LOD;
// using gl::GL_TEXTURE_MAX_LOD_SGIS; // reuse GetTextureParameter
using gl::GL_TEXTURE_BASE_LEVEL;
// using gl::GL_TEXTURE_BASE_LEVEL_SGIS; // reuse GetTextureParameter
using gl::GL_TEXTURE_MAX_LEVEL;
// using gl::GL_TEXTURE_MAX_LEVEL_SGIS; // reuse GetTextureParameter

// TextureWrapMode

using gl::GL_CLAMP;
using gl::GL_REPEAT;
using gl::GL_CLAMP_TO_BORDER;
using gl::GL_CLAMP_TO_BORDER_ARB;
using gl::GL_CLAMP_TO_BORDER_SGIS;
using gl::GL_CLAMP_TO_EDGE;
using gl::GL_CLAMP_TO_EDGE_SGIS;

// VertexPointerType

// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// __UNGROUPED__

using gl::GL_NEXT_BUFFER_NV;
using gl::GL_SKIP_COMPONENTS4_NV;
using gl::GL_SKIP_COMPONENTS3_NV;
using gl::GL_SKIP_COMPONENTS2_NV;
using gl::GL_SKIP_COMPONENTS1_NV;
using gl::GL_CLOSE_PATH_NV;
using gl::GL_TERMINATE_SEQUENCE_COMMAND_NV;
using gl::GL_NOP_COMMAND_NV;
using gl::GL_RESTART_SUN;
using gl::GL_DRAW_ELEMENTS_COMMAND_NV;
using gl::GL_REPLACE_MIDDLE_SUN;
using gl::GL_DRAW_ARRAYS_COMMAND_NV;
using gl::GL_REPLACE_OLDEST_SUN;
using gl::GL_DRAW_ELEMENTS_STRIP_COMMAND_NV;
using gl::GL_DRAW_ARRAYS_STRIP_COMMAND_NV;
using gl::GL_DRAW_ELEMENTS_INSTANCED_COMMAND_NV;
using gl::GL_DRAW_ARRAYS_INSTANCED_COMMAND_NV;
using gl::GL_ELEMENT_ADDRESS_COMMAND_NV;
using gl::GL_ATTRIBUTE_ADDRESS_COMMAND_NV;
using gl::GL_UNIFORM_ADDRESS_COMMAND_NV;
using gl::GL_BLEND_COLOR_COMMAND_NV;
using gl::GL_STENCIL_REF_COMMAND_NV;
using gl::GL_LINE_WIDTH_COMMAND_NV;
using gl::GL_POLYGON_OFFSET_COMMAND_NV;
using gl::GL_ALPHA_REF_COMMAND_NV;
using gl::GL_VIEWPORT_COMMAND_NV;
using gl::GL_SCISSOR_COMMAND_NV;
using gl::GL_FRONT_FACE_COMMAND_NV;
using gl::GL_MOVE_TO_NV;
using gl::GL_RELATIVE_MOVE_TO_NV;
using gl::GL_LINE_TO_NV;
using gl::GL_RELATIVE_LINE_TO_NV;
using gl::GL_CONTEXT_LOST;
using gl::GL_HORIZONTAL_LINE_TO_NV;
using gl::GL_RELATIVE_HORIZONTAL_LINE_TO_NV;
using gl::GL_VERTICAL_LINE_TO_NV;
using gl::GL_RELATIVE_VERTICAL_LINE_TO_NV;
using gl::GL_QUADRATIC_CURVE_TO_NV;
using gl::GL_RELATIVE_QUADRATIC_CURVE_TO_NV;
using gl::GL_PATH_MODELVIEW_STACK_DEPTH_NV;
using gl::GL_PATH_PROJECTION_STACK_DEPTH_NV;
using gl::GL_PATH_MODELVIEW_MATRIX_NV;
using gl::GL_PATH_PROJECTION_MATRIX_NV;
using gl::GL_CUBIC_CURVE_TO_NV;
using gl::GL_RELATIVE_CUBIC_CURVE_TO_NV;
using gl::GL_PATH_MAX_MODELVIEW_STACK_DEPTH_NV;
using gl::GL_PATH_MAX_PROJECTION_STACK_DEPTH_NV;
using gl::GL_SMOOTH_QUADRATIC_CURVE_TO_NV;
using gl::GL_RELATIVE_SMOOTH_QUADRATIC_CURVE_TO_NV;
using gl::GL_SMOOTH_CUBIC_CURVE_TO_NV;
using gl::GL_TEXTURE_TARGET;
using gl::GL_RELATIVE_SMOOTH_CUBIC_CURVE_TO_NV;
using gl::GL_SMALL_CCW_ARC_TO_NV;
using gl::GL_RELATIVE_SMALL_CCW_ARC_TO_NV;
using gl::GL_SMALL_CW_ARC_TO_NV;
using gl::GL_2_BYTES_NV;
using gl::GL_3_BYTES_NV;
using gl::GL_4_BYTES_NV;
using gl::GL_HALF_APPLE;
using gl::GL_HALF_FLOAT;
using gl::GL_HALF_FLOAT_ARB;
using gl::GL_HALF_FLOAT_NV;
using gl::GL_FIXED;
using gl::GL_FIXED_OES;
using gl::GL_INT64_ARB;
using gl::GL_INT64_NV;
using gl::GL_UNSIGNED_INT64_ARB;
using gl::GL_UNSIGNED_INT64_NV;
using gl::GL_RELATIVE_SMALL_CW_ARC_TO_NV;
using gl::GL_XOR_NV;
using gl::GL_LARGE_CCW_ARC_TO_NV;
using gl::GL_RELATIVE_LARGE_CCW_ARC_TO_NV;
using gl::GL_MODELVIEW0_ARB;
using gl::GL_PATH_MODELVIEW_NV;
using gl::GL_PATH_PROJECTION_NV;
using gl::GL_LARGE_CW_ARC_TO_NV;
using gl::GL_RELATIVE_LARGE_CW_ARC_TO_NV;
using gl::GL_RED_NV;
using gl::GL_GREEN_NV;
using gl::GL_BLUE_NV;
using gl::GL_RASTER_POSITION_UNCLIPPED_IBM;
using gl::GL_CONIC_CURVE_TO_NV;
using gl::GL_NATIVE_GRAPHICS_HANDLE_PGI;
using gl::GL_RELATIVE_CONIC_CURVE_TO_NV;
using gl::GL_EYE_LINEAR_NV;
using gl::GL_OBJECT_LINEAR_NV;
using gl::GL_CONSTANT_COLOR;
using gl::GL_ONE_MINUS_CONSTANT_COLOR;
using gl::GL_CONSTANT_ALPHA;
using gl::GL_ONE_MINUS_CONSTANT_ALPHA;
using gl::GL_BLEND_COLOR;
using gl::GL_FUNC_ADD;
using gl::GL_MIN;
using gl::GL_MAX;
using gl::GL_BLEND_EQUATION;
using gl::GL_BLEND_EQUATION_RGB;
using gl::GL_BLEND_EQUATION_RGB_EXT;
using gl::GL_FUNC_SUBTRACT;
using gl::GL_FUNC_REVERSE_SUBTRACT;
using gl::GL_CONVOLUTION_FORMAT;
using gl::GL_CONVOLUTION_WIDTH;
using gl::GL_CONVOLUTION_HEIGHT;
using gl::GL_MAX_CONVOLUTION_WIDTH;
using gl::GL_MAX_CONVOLUTION_HEIGHT;
using gl::GL_HISTOGRAM_WIDTH;
using gl::GL_HISTOGRAM_FORMAT;
using gl::GL_HISTOGRAM_RED_SIZE;
using gl::GL_HISTOGRAM_GREEN_SIZE;
using gl::GL_HISTOGRAM_BLUE_SIZE;
using gl::GL_HISTOGRAM_ALPHA_SIZE;
using gl::GL_HISTOGRAM_LUMINANCE_SIZE;
using gl::GL_HISTOGRAM_SINK;
using gl::GL_POLYGON_OFFSET_EXT;
using gl::GL_POLYGON_OFFSET_FACTOR_EXT;
using gl::GL_RESCALE_NORMAL;
using gl::GL_ALPHA4_EXT;
using gl::GL_ALPHA8_EXT;
using gl::GL_ALPHA12_EXT;
using gl::GL_ALPHA16_EXT;
using gl::GL_LUMINANCE4_EXT;
using gl::GL_LUMINANCE8_EXT;
using gl::GL_LUMINANCE12_EXT;
using gl::GL_LUMINANCE16_EXT;
using gl::GL_LUMINANCE4_ALPHA4_EXT;
using gl::GL_LUMINANCE6_ALPHA2_EXT;
using gl::GL_LUMINANCE8_ALPHA8_EXT;
using gl::GL_LUMINANCE12_ALPHA4_EXT;
using gl::GL_LUMINANCE12_ALPHA12_EXT;
using gl::GL_LUMINANCE16_ALPHA16_EXT;
using gl::GL_INTENSITY_EXT;
using gl::GL_INTENSITY4_EXT;
using gl::GL_INTENSITY8_EXT;
using gl::GL_INTENSITY12_EXT;
using gl::GL_INTENSITY16_EXT;
using gl::GL_RGB4_EXT;
using gl::GL_RGB5_EXT;
using gl::GL_RGB8_EXT;
using gl::GL_RGB10_EXT;
using gl::GL_RGB12_EXT;
using gl::GL_RGB16_EXT;
using gl::GL_RGBA2_EXT;
using gl::GL_RGBA4_EXT;
using gl::GL_RGB5_A1_EXT;
using gl::GL_RGBA8_EXT;
using gl::GL_RGB10_A2_EXT;
using gl::GL_RGBA12_EXT;
using gl::GL_RGBA16_EXT;
using gl::GL_TEXTURE_RED_SIZE_EXT;
using gl::GL_TEXTURE_GREEN_SIZE_EXT;
using gl::GL_TEXTURE_BLUE_SIZE_EXT;
using gl::GL_TEXTURE_ALPHA_SIZE_EXT;
using gl::GL_TEXTURE_LUMINANCE_SIZE_EXT;
using gl::GL_TEXTURE_INTENSITY_SIZE_EXT;
using gl::GL_TEXTURE_RESIDENT_EXT;
using gl::GL_TEXTURE_1D_BINDING_EXT;
using gl::GL_TEXTURE_2D_BINDING_EXT;
using gl::GL_TEXTURE_DEPTH;
using gl::GL_MAX_3D_TEXTURE_SIZE;
using gl::GL_VERTEX_ARRAY_EXT;
using gl::GL_NORMAL_ARRAY_EXT;
using gl::GL_COLOR_ARRAY_EXT;
using gl::GL_INDEX_ARRAY_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_EXT;
using gl::GL_EDGE_FLAG_ARRAY_EXT;
using gl::GL_VERTEX_ARRAY_SIZE_EXT;
using gl::GL_VERTEX_ARRAY_TYPE_EXT;
using gl::GL_VERTEX_ARRAY_STRIDE_EXT;
using gl::GL_NORMAL_ARRAY_TYPE_EXT;
using gl::GL_NORMAL_ARRAY_STRIDE_EXT;
using gl::GL_COLOR_ARRAY_SIZE_EXT;
using gl::GL_COLOR_ARRAY_TYPE_EXT;
using gl::GL_COLOR_ARRAY_STRIDE_EXT;
using gl::GL_INDEX_ARRAY_TYPE_EXT;
using gl::GL_INDEX_ARRAY_STRIDE_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_SIZE_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_TYPE_EXT;
using gl::GL_TEXTURE_COORD_ARRAY_STRIDE_EXT;
using gl::GL_EDGE_FLAG_ARRAY_STRIDE_EXT;
using gl::GL_MULTISAMPLE;
using gl::GL_MULTISAMPLE_ARB;
using gl::GL_MULTISAMPLE_EXT;
using gl::GL_SAMPLE_ALPHA_TO_COVERAGE;
using gl::GL_SAMPLE_ALPHA_TO_COVERAGE_ARB;
using gl::GL_SAMPLE_ALPHA_TO_MASK_EXT;
using gl::GL_SAMPLE_ALPHA_TO_ONE;
using gl::GL_SAMPLE_ALPHA_TO_ONE_ARB;
using gl::GL_SAMPLE_ALPHA_TO_ONE_EXT;
using gl::GL_SAMPLE_COVERAGE;
using gl::GL_SAMPLE_COVERAGE_ARB;
using gl::GL_SAMPLE_MASK_EXT;
using gl::GL_SAMPLE_BUFFERS;
using gl::GL_SAMPLE_BUFFERS_ARB;
using gl::GL_SAMPLE_BUFFERS_EXT;
using gl::GL_SAMPLES;
using gl::GL_SAMPLES_ARB;
using gl::GL_SAMPLES_EXT;
using gl::GL_SAMPLE_COVERAGE_VALUE;
using gl::GL_SAMPLE_COVERAGE_VALUE_ARB;
using gl::GL_SAMPLE_MASK_VALUE_EXT;
using gl::GL_SAMPLE_COVERAGE_INVERT;
using gl::GL_SAMPLE_COVERAGE_INVERT_ARB;
using gl::GL_SAMPLE_MASK_INVERT_EXT;
using gl::GL_SAMPLE_PATTERN_EXT;
using gl::GL_COLOR_MATRIX;
using gl::GL_COLOR_MATRIX_STACK_DEPTH;
using gl::GL_MAX_COLOR_MATRIX_STACK_DEPTH;
using gl::GL_TEXTURE_COMPARE_FAIL_VALUE_ARB;
using gl::GL_BLEND_DST_RGB;
using gl::GL_BLEND_DST_RGB_EXT;
using gl::GL_BLEND_SRC_RGB;
using gl::GL_BLEND_SRC_RGB_EXT;
using gl::GL_BLEND_DST_ALPHA;
using gl::GL_BLEND_DST_ALPHA_EXT;
using gl::GL_BLEND_SRC_ALPHA;
using gl::GL_BLEND_SRC_ALPHA_EXT;
using gl::GL_422_EXT;
using gl::GL_422_REV_EXT;
using gl::GL_422_AVERAGE_EXT;
using gl::GL_422_REV_AVERAGE_EXT;
using gl::GL_COLOR_TABLE_FORMAT;
using gl::GL_COLOR_TABLE_WIDTH;
using gl::GL_COLOR_TABLE_RED_SIZE;
using gl::GL_COLOR_TABLE_GREEN_SIZE;
using gl::GL_COLOR_TABLE_BLUE_SIZE;
using gl::GL_COLOR_TABLE_ALPHA_SIZE;
using gl::GL_COLOR_TABLE_LUMINANCE_SIZE;
using gl::GL_COLOR_TABLE_INTENSITY_SIZE;
using gl::GL_BGR;
using gl::GL_BGR_EXT;
using gl::GL_BGRA;
using gl::GL_BGRA_EXT;
using gl::GL_COLOR_INDEX1_EXT;
using gl::GL_COLOR_INDEX2_EXT;
using gl::GL_COLOR_INDEX4_EXT;
using gl::GL_COLOR_INDEX8_EXT;
using gl::GL_COLOR_INDEX12_EXT;
using gl::GL_COLOR_INDEX16_EXT;
using gl::GL_MAX_ELEMENTS_VERTICES;
using gl::GL_MAX_ELEMENTS_VERTICES_EXT;
using gl::GL_MAX_ELEMENTS_INDICES;
using gl::GL_MAX_ELEMENTS_INDICES_EXT;
using gl::GL_PHONG_WIN;
using gl::GL_FOG_SPECULAR_TEXTURE_WIN;
using gl::GL_TEXTURE_INDEX_SIZE_EXT;
using gl::GL_PARAMETER_BUFFER_ARB;
using gl::GL_PARAMETER_BUFFER_BINDING_ARB;
using gl::GL_SPRITE_AXIAL_SGIX;
using gl::GL_SPRITE_OBJECT_ALIGNED_SGIX;
using gl::GL_SPRITE_EYE_ALIGNED_SGIX;
using gl::GL_IGNORE_BORDER_HP;
using gl::GL_CONSTANT_BORDER;
using gl::GL_CONSTANT_BORDER_HP;
using gl::GL_REPLICATE_BORDER;
using gl::GL_REPLICATE_BORDER_HP;
using gl::GL_CONVOLUTION_BORDER_COLOR;
using gl::GL_CONVOLUTION_BORDER_COLOR_HP;
using gl::GL_IMAGE_SCALE_X_HP;
using gl::GL_IMAGE_SCALE_Y_HP;
using gl::GL_IMAGE_TRANSLATE_X_HP;
using gl::GL_IMAGE_TRANSLATE_Y_HP;
using gl::GL_IMAGE_ROTATE_ANGLE_HP;
using gl::GL_IMAGE_ROTATE_ORIGIN_X_HP;
using gl::GL_IMAGE_ROTATE_ORIGIN_Y_HP;
using gl::GL_IMAGE_MAG_FILTER_HP;
using gl::GL_IMAGE_MIN_FILTER_HP;
using gl::GL_IMAGE_CUBIC_WEIGHT_HP;
using gl::GL_CUBIC_HP;
using gl::GL_AVERAGE_HP;
using gl::GL_IMAGE_TRANSFORM_2D_HP;
using gl::GL_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP;
using gl::GL_PROXY_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP;
using gl::GL_OCCLUSION_TEST_HP;
using gl::GL_OCCLUSION_TEST_RESULT_HP;
using gl::GL_TEXTURE_LIGHTING_MODE_HP;
using gl::GL_TEXTURE_POST_SPECULAR_HP;
using gl::GL_TEXTURE_PRE_SPECULAR_HP;
using gl::GL_MAX_DEFORMATION_ORDER_SGIX;
using gl::GL_DEPTH_COMPONENT16;
using gl::GL_DEPTH_COMPONENT16_ARB;
using gl::GL_DEPTH_COMPONENT24;
using gl::GL_DEPTH_COMPONENT24_ARB;
using gl::GL_DEPTH_COMPONENT32;
using gl::GL_DEPTH_COMPONENT32_ARB;
using gl::GL_ARRAY_ELEMENT_LOCK_FIRST_EXT;
using gl::GL_ARRAY_ELEMENT_LOCK_COUNT_EXT;
using gl::GL_CULL_VERTEX_EXT;
using gl::GL_CULL_VERTEX_EYE_POSITION_EXT;
using gl::GL_CULL_VERTEX_OBJECT_POSITION_EXT;
using gl::GL_IUI_V2F_EXT;
using gl::GL_IUI_V3F_EXT;
using gl::GL_IUI_N3F_V2F_EXT;
using gl::GL_IUI_N3F_V3F_EXT;
using gl::GL_T2F_IUI_V2F_EXT;
using gl::GL_T2F_IUI_V3F_EXT;
using gl::GL_T2F_IUI_N3F_V2F_EXT;
using gl::GL_T2F_IUI_N3F_V3F_EXT;
using gl::GL_INDEX_TEST_EXT;
using gl::GL_INDEX_TEST_FUNC_EXT;
using gl::GL_INDEX_TEST_REF_EXT;
using gl::GL_INDEX_MATERIAL_EXT;
using gl::GL_INDEX_MATERIAL_PARAMETER_EXT;
using gl::GL_INDEX_MATERIAL_FACE_EXT;
using gl::GL_WRAP_BORDER_SUN;
using gl::GL_UNPACK_CONSTANT_DATA_SUNX;
using gl::GL_TEXTURE_CONSTANT_DATA_SUNX;
using gl::GL_TRIANGLE_LIST_SUN;
using gl::GL_REPLACEMENT_CODE_SUN;
using gl::GL_GLOBAL_ALPHA_SUN;
using gl::GL_GLOBAL_ALPHA_FACTOR_SUN;
using gl::GL_TEXTURE_COLOR_WRITEMASK_SGIS;
using gl::GL_TEXT_FRAGMENT_SHADER_ATI;
using gl::GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING;
using gl::GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE;
using gl::GL_FRAMEBUFFER_DEFAULT;
using gl::GL_FRAMEBUFFER_UNDEFINED;
using gl::GL_DEPTH_STENCIL_ATTACHMENT;
using gl::GL_MAJOR_VERSION;
using gl::GL_MINOR_VERSION;
using gl::GL_NUM_EXTENSIONS;
using gl::GL_CONTEXT_FLAGS;
using gl::GL_BUFFER_IMMUTABLE_STORAGE;
using gl::GL_BUFFER_STORAGE_FLAGS;
using gl::GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED;
using gl::GL_INDEX;
using gl::GL_COMPRESSED_RED;
using gl::GL_COMPRESSED_RG;
using gl::GL_RG;
using gl::GL_RG_INTEGER;
using gl::GL_R8;
using gl::GL_R16;
using gl::GL_RG8;
using gl::GL_RG16;
using gl::GL_R16F;
using gl::GL_R32F;
using gl::GL_RG16F;
using gl::GL_RG32F;
using gl::GL_R8I;
using gl::GL_R8UI;
using gl::GL_R16I;
using gl::GL_R16UI;
using gl::GL_R32I;
using gl::GL_R32UI;
using gl::GL_RG8I;
using gl::GL_RG8UI;
using gl::GL_RG16I;
using gl::GL_RG16UI;
using gl::GL_RG32I;
using gl::GL_RG32UI;
using gl::GL_SYNC_CL_EVENT_ARB;
using gl::GL_SYNC_CL_EVENT_COMPLETE_ARB;
using gl::GL_DEBUG_OUTPUT_SYNCHRONOUS;
using gl::GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB;
using gl::GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH;
using gl::GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_ARB;
using gl::GL_DEBUG_CALLBACK_FUNCTION;
using gl::GL_DEBUG_CALLBACK_FUNCTION_ARB;
using gl::GL_DEBUG_CALLBACK_USER_PARAM;
using gl::GL_DEBUG_CALLBACK_USER_PARAM_ARB;
using gl::GL_DEBUG_SOURCE_API;
using gl::GL_DEBUG_SOURCE_API_ARB;
using gl::GL_DEBUG_SOURCE_WINDOW_SYSTEM;
using gl::GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB;
using gl::GL_DEBUG_SOURCE_SHADER_COMPILER;
using gl::GL_DEBUG_SOURCE_SHADER_COMPILER_ARB;
using gl::GL_DEBUG_SOURCE_THIRD_PARTY;
using gl::GL_DEBUG_SOURCE_THIRD_PARTY_ARB;
using gl::GL_DEBUG_SOURCE_APPLICATION;
using gl::GL_DEBUG_SOURCE_APPLICATION_ARB;
using gl::GL_DEBUG_SOURCE_OTHER;
using gl::GL_DEBUG_SOURCE_OTHER_ARB;
using gl::GL_DEBUG_TYPE_ERROR;
using gl::GL_DEBUG_TYPE_ERROR_ARB;
using gl::GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR;
using gl::GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB;
using gl::GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR;
using gl::GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB;
using gl::GL_DEBUG_TYPE_PORTABILITY;
using gl::GL_DEBUG_TYPE_PORTABILITY_ARB;
using gl::GL_DEBUG_TYPE_PERFORMANCE;
using gl::GL_DEBUG_TYPE_PERFORMANCE_ARB;
using gl::GL_DEBUG_TYPE_OTHER;
using gl::GL_DEBUG_TYPE_OTHER_ARB;
using gl::GL_LOSE_CONTEXT_ON_RESET;
using gl::GL_LOSE_CONTEXT_ON_RESET_ARB;
using gl::GL_GUILTY_CONTEXT_RESET;
using gl::GL_GUILTY_CONTEXT_RESET_ARB;
using gl::GL_INNOCENT_CONTEXT_RESET;
using gl::GL_INNOCENT_CONTEXT_RESET_ARB;
using gl::GL_UNKNOWN_CONTEXT_RESET;
using gl::GL_UNKNOWN_CONTEXT_RESET_ARB;
using gl::GL_RESET_NOTIFICATION_STRATEGY;
using gl::GL_RESET_NOTIFICATION_STRATEGY_ARB;
using gl::GL_PROGRAM_SEPARABLE;
using gl::GL_ACTIVE_PROGRAM;
using gl::GL_PROGRAM_PIPELINE_BINDING;
using gl::GL_MAX_VIEWPORTS;
using gl::GL_VIEWPORT_SUBPIXEL_BITS;
using gl::GL_VIEWPORT_BOUNDS_RANGE;
using gl::GL_LAYER_PROVOKING_VERTEX;
using gl::GL_VIEWPORT_INDEX_PROVOKING_VERTEX;
using gl::GL_UNDEFINED_VERTEX;
using gl::GL_NO_RESET_NOTIFICATION;
using gl::GL_NO_RESET_NOTIFICATION_ARB;
using gl::GL_MAX_COMPUTE_SHARED_MEMORY_SIZE;
using gl::GL_MAX_COMPUTE_UNIFORM_COMPONENTS;
using gl::GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_COMPUTE_ATOMIC_COUNTERS;
using gl::GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS;
using gl::GL_COMPUTE_WORK_GROUP_SIZE;
using gl::GL_DEBUG_TYPE_MARKER;
using gl::GL_DEBUG_TYPE_PUSH_GROUP;
using gl::GL_DEBUG_TYPE_POP_GROUP;
using gl::GL_DEBUG_SEVERITY_NOTIFICATION;
using gl::GL_MAX_DEBUG_GROUP_STACK_DEPTH;
using gl::GL_DEBUG_GROUP_STACK_DEPTH;
using gl::GL_MAX_UNIFORM_LOCATIONS;
using gl::GL_INTERNALFORMAT_SUPPORTED;
using gl::GL_INTERNALFORMAT_PREFERRED;
using gl::GL_INTERNALFORMAT_RED_SIZE;
using gl::GL_INTERNALFORMAT_GREEN_SIZE;
using gl::GL_INTERNALFORMAT_BLUE_SIZE;
using gl::GL_INTERNALFORMAT_ALPHA_SIZE;
using gl::GL_INTERNALFORMAT_DEPTH_SIZE;
using gl::GL_INTERNALFORMAT_STENCIL_SIZE;
using gl::GL_INTERNALFORMAT_SHARED_SIZE;
using gl::GL_INTERNALFORMAT_RED_TYPE;
using gl::GL_INTERNALFORMAT_GREEN_TYPE;
using gl::GL_INTERNALFORMAT_BLUE_TYPE;
using gl::GL_INTERNALFORMAT_ALPHA_TYPE;
using gl::GL_INTERNALFORMAT_DEPTH_TYPE;
using gl::GL_INTERNALFORMAT_STENCIL_TYPE;
using gl::GL_MAX_WIDTH;
using gl::GL_MAX_HEIGHT;
using gl::GL_MAX_DEPTH;
using gl::GL_MAX_LAYERS;
using gl::GL_MAX_COMBINED_DIMENSIONS;
using gl::GL_COLOR_COMPONENTS;
using gl::GL_DEPTH_COMPONENTS;
using gl::GL_STENCIL_COMPONENTS;
using gl::GL_COLOR_RENDERABLE;
using gl::GL_DEPTH_RENDERABLE;
using gl::GL_STENCIL_RENDERABLE;
using gl::GL_FRAMEBUFFER_RENDERABLE;
using gl::GL_FRAMEBUFFER_RENDERABLE_LAYERED;
using gl::GL_FRAMEBUFFER_BLEND;
using gl::GL_READ_PIXELS;
using gl::GL_READ_PIXELS_FORMAT;
using gl::GL_READ_PIXELS_TYPE;
using gl::GL_TEXTURE_IMAGE_FORMAT;
using gl::GL_TEXTURE_IMAGE_TYPE;
using gl::GL_GET_TEXTURE_IMAGE_FORMAT;
using gl::GL_GET_TEXTURE_IMAGE_TYPE;
using gl::GL_MIPMAP;
using gl::GL_MANUAL_GENERATE_MIPMAP;
using gl::GL_AUTO_GENERATE_MIPMAP;
using gl::GL_COLOR_ENCODING;
using gl::GL_SRGB_READ;
using gl::GL_SRGB_WRITE;
using gl::GL_SRGB_DECODE_ARB;
using gl::GL_FILTER;
using gl::GL_VERTEX_TEXTURE;
using gl::GL_TESS_CONTROL_TEXTURE;
using gl::GL_TESS_EVALUATION_TEXTURE;
using gl::GL_GEOMETRY_TEXTURE;
using gl::GL_FRAGMENT_TEXTURE;
using gl::GL_COMPUTE_TEXTURE;
using gl::GL_TEXTURE_SHADOW;
using gl::GL_TEXTURE_GATHER;
using gl::GL_TEXTURE_GATHER_SHADOW;
using gl::GL_SHADER_IMAGE_LOAD;
using gl::GL_SHADER_IMAGE_STORE;
using gl::GL_SHADER_IMAGE_ATOMIC;
using gl::GL_IMAGE_TEXEL_SIZE;
using gl::GL_IMAGE_COMPATIBILITY_CLASS;
using gl::GL_IMAGE_PIXEL_FORMAT;
using gl::GL_IMAGE_PIXEL_TYPE;
using gl::GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST;
using gl::GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST;
using gl::GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE;
using gl::GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE;
using gl::GL_TEXTURE_COMPRESSED_BLOCK_WIDTH;
using gl::GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT;
using gl::GL_TEXTURE_COMPRESSED_BLOCK_SIZE;
using gl::GL_CLEAR_BUFFER;
using gl::GL_TEXTURE_VIEW;
using gl::GL_VIEW_COMPATIBILITY_CLASS;
using gl::GL_FULL_SUPPORT;
using gl::GL_CAVEAT_SUPPORT;
using gl::GL_IMAGE_CLASS_4_X_32;
using gl::GL_IMAGE_CLASS_2_X_32;
using gl::GL_IMAGE_CLASS_1_X_32;
using gl::GL_IMAGE_CLASS_4_X_16;
using gl::GL_IMAGE_CLASS_2_X_16;
using gl::GL_IMAGE_CLASS_1_X_16;
using gl::GL_IMAGE_CLASS_4_X_8;
using gl::GL_IMAGE_CLASS_2_X_8;
using gl::GL_IMAGE_CLASS_1_X_8;
using gl::GL_IMAGE_CLASS_11_11_10;
using gl::GL_IMAGE_CLASS_10_10_10_2;
using gl::GL_VIEW_CLASS_128_BITS;
using gl::GL_VIEW_CLASS_96_BITS;
using gl::GL_VIEW_CLASS_64_BITS;
using gl::GL_VIEW_CLASS_48_BITS;
using gl::GL_VIEW_CLASS_32_BITS;
using gl::GL_VIEW_CLASS_24_BITS;
using gl::GL_VIEW_CLASS_16_BITS;
using gl::GL_VIEW_CLASS_8_BITS;
using gl::GL_VIEW_CLASS_S3TC_DXT1_RGB;
using gl::GL_VIEW_CLASS_S3TC_DXT1_RGBA;
using gl::GL_VIEW_CLASS_S3TC_DXT3_RGBA;
using gl::GL_VIEW_CLASS_S3TC_DXT5_RGBA;
using gl::GL_VIEW_CLASS_RGTC1_RED;
using gl::GL_VIEW_CLASS_RGTC2_RG;
using gl::GL_VIEW_CLASS_BPTC_UNORM;
using gl::GL_VIEW_CLASS_BPTC_FLOAT;
using gl::GL_VERTEX_ATTRIB_BINDING;
using gl::GL_VERTEX_ATTRIB_RELATIVE_OFFSET;
using gl::GL_VERTEX_BINDING_DIVISOR;
using gl::GL_VERTEX_BINDING_OFFSET;
using gl::GL_VERTEX_BINDING_STRIDE;
using gl::GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET;
using gl::GL_MAX_VERTEX_ATTRIB_BINDINGS;
using gl::GL_TEXTURE_VIEW_MIN_LEVEL;
using gl::GL_TEXTURE_VIEW_NUM_LEVELS;
using gl::GL_TEXTURE_VIEW_MIN_LAYER;
using gl::GL_TEXTURE_VIEW_NUM_LAYERS;
using gl::GL_TEXTURE_IMMUTABLE_LEVELS;
using gl::GL_BUFFER;
using gl::GL_SHADER;
using gl::GL_PROGRAM;
using gl::GL_QUERY;
using gl::GL_PROGRAM_PIPELINE;
using gl::GL_MAX_VERTEX_ATTRIB_STRIDE;
using gl::GL_SAMPLER;
using gl::GL_DISPLAY_LIST;
using gl::GL_MAX_LABEL_LENGTH;
using gl::GL_NUM_SHADING_LANGUAGE_VERSIONS;
using gl::GL_QUERY_TARGET;
using gl::GL_TRANSFORM_FEEDBACK_OVERFLOW_ARB;
using gl::GL_TRANSFORM_FEEDBACK_STREAM_OVERFLOW_ARB;
using gl::GL_VERTICES_SUBMITTED_ARB;
using gl::GL_PRIMITIVES_SUBMITTED_ARB;
using gl::GL_VERTEX_SHADER_INVOCATIONS_ARB;
using gl::GL_TESS_CONTROL_SHADER_PATCHES_ARB;
using gl::GL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB;
using gl::GL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB;
using gl::GL_FRAGMENT_SHADER_INVOCATIONS_ARB;
using gl::GL_COMPUTE_SHADER_INVOCATIONS_ARB;
using gl::GL_CLIPPING_INPUT_PRIMITIVES_ARB;
using gl::GL_CLIPPING_OUTPUT_PRIMITIVES_ARB;
using gl::GL_SPARSE_BUFFER_PAGE_SIZE_ARB;
using gl::GL_MAX_CULL_DISTANCES;
using gl::GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES;
using gl::GL_CONTEXT_RELEASE_BEHAVIOR;
using gl::GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH;
using gl::GL_YCRCB_SGIX;
using gl::GL_YCRCBA_SGIX;
using gl::GL_PIXEL_TRANSFORM_2D_EXT;
using gl::GL_PIXEL_MAG_FILTER_EXT;
using gl::GL_PIXEL_MIN_FILTER_EXT;
using gl::GL_PIXEL_CUBIC_WEIGHT_EXT;
using gl::GL_CUBIC_EXT;
using gl::GL_AVERAGE_EXT;
using gl::GL_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT;
using gl::GL_MAX_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT;
using gl::GL_PIXEL_TRANSFORM_2D_MATRIX_EXT;
using gl::GL_FRAGMENT_MATERIAL_EXT;
using gl::GL_FRAGMENT_NORMAL_EXT;
using gl::GL_FRAGMENT_COLOR_EXT;
using gl::GL_ATTENUATION_EXT;
using gl::GL_SHADOW_ATTENUATION_EXT;
using gl::GL_TEXTURE_APPLICATION_MODE_EXT;
using gl::GL_TEXTURE_LIGHT_EXT;
using gl::GL_TEXTURE_MATERIAL_FACE_EXT;
using gl::GL_TEXTURE_MATERIAL_PARAMETER_EXT;
using gl::GL_PIXEL_GROUP_COLOR_SGIS;
using gl::GL_UNSIGNED_BYTE_2_3_3_REV;
using gl::GL_UNSIGNED_SHORT_5_6_5;
using gl::GL_UNSIGNED_SHORT_5_6_5_REV;
using gl::GL_UNSIGNED_SHORT_4_4_4_4_REV;
using gl::GL_UNSIGNED_SHORT_1_5_5_5_REV;
using gl::GL_UNSIGNED_INT_8_8_8_8_REV;
using gl::GL_UNSIGNED_INT_2_10_10_10_REV;
using gl::GL_MIRRORED_REPEAT;
using gl::GL_MIRRORED_REPEAT_ARB;
using gl::GL_MIRRORED_REPEAT_IBM;
using gl::GL_RGB_S3TC;
using gl::GL_RGB4_S3TC;
using gl::GL_RGBA_S3TC;
using gl::GL_RGBA4_S3TC;
using gl::GL_RGBA_DXT5_S3TC;
using gl::GL_RGBA4_DXT5_S3TC;
using gl::GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
using gl::GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
using gl::GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
using gl::GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
using gl::GL_PARALLEL_ARRAYS_INTEL;
using gl::GL_VERTEX_ARRAY_PARALLEL_POINTERS_INTEL;
using gl::GL_NORMAL_ARRAY_PARALLEL_POINTERS_INTEL;
using gl::GL_COLOR_ARRAY_PARALLEL_POINTERS_INTEL;
using gl::GL_TEXTURE_COORD_ARRAY_PARALLEL_POINTERS_INTEL;
using gl::GL_PERFQUERY_DONOT_FLUSH_INTEL;
using gl::GL_PERFQUERY_FLUSH_INTEL;
using gl::GL_PERFQUERY_WAIT_INTEL;
using gl::GL_TEXTURE_MEMORY_LAYOUT_INTEL;
using gl::GL_CURRENT_RASTER_NORMAL_SGIX;
using gl::GL_TANGENT_ARRAY_EXT;
using gl::GL_BINORMAL_ARRAY_EXT;
using gl::GL_CURRENT_TANGENT_EXT;
using gl::GL_CURRENT_BINORMAL_EXT;
using gl::GL_TANGENT_ARRAY_TYPE_EXT;
using gl::GL_TANGENT_ARRAY_STRIDE_EXT;
using gl::GL_BINORMAL_ARRAY_TYPE_EXT;
using gl::GL_BINORMAL_ARRAY_STRIDE_EXT;
using gl::GL_TANGENT_ARRAY_POINTER_EXT;
using gl::GL_BINORMAL_ARRAY_POINTER_EXT;
using gl::GL_MAP1_TANGENT_EXT;
using gl::GL_MAP2_TANGENT_EXT;
using gl::GL_MAP1_BINORMAL_EXT;
using gl::GL_MAP2_BINORMAL_EXT;
using gl::GL_FOG_COORDINATE_SOURCE;
using gl::GL_FOG_COORDINATE_SOURCE_EXT;
using gl::GL_FOG_COORD_SRC;
using gl::GL_FOG_COORD;
using gl::GL_FOG_COORDINATE;
using gl::GL_FOG_COORDINATE_EXT;
using gl::GL_FRAGMENT_DEPTH;
using gl::GL_FRAGMENT_DEPTH_EXT;
using gl::GL_CURRENT_FOG_COORD;
using gl::GL_CURRENT_FOG_COORDINATE;
using gl::GL_CURRENT_FOG_COORDINATE_EXT;
using gl::GL_FOG_COORDINATE_ARRAY_TYPE;
using gl::GL_FOG_COORDINATE_ARRAY_TYPE_EXT;
using gl::GL_FOG_COORD_ARRAY_TYPE;
using gl::GL_FOG_COORDINATE_ARRAY_STRIDE;
using gl::GL_FOG_COORDINATE_ARRAY_STRIDE_EXT;
using gl::GL_FOG_COORD_ARRAY_STRIDE;
using gl::GL_FOG_COORDINATE_ARRAY_POINTER;
using gl::GL_FOG_COORDINATE_ARRAY_POINTER_EXT;
using gl::GL_FOG_COORD_ARRAY_POINTER;
using gl::GL_FOG_COORDINATE_ARRAY;
using gl::GL_FOG_COORDINATE_ARRAY_EXT;
using gl::GL_FOG_COORD_ARRAY;
using gl::GL_COLOR_SUM;
using gl::GL_COLOR_SUM_ARB;
using gl::GL_COLOR_SUM_EXT;
using gl::GL_CURRENT_SECONDARY_COLOR;
using gl::GL_CURRENT_SECONDARY_COLOR_EXT;
using gl::GL_SECONDARY_COLOR_ARRAY_SIZE;
using gl::GL_SECONDARY_COLOR_ARRAY_SIZE_EXT;
using gl::GL_SECONDARY_COLOR_ARRAY_TYPE;
using gl::GL_SECONDARY_COLOR_ARRAY_TYPE_EXT;
using gl::GL_SECONDARY_COLOR_ARRAY_STRIDE;
using gl::GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT;
using gl::GL_SECONDARY_COLOR_ARRAY_POINTER;
using gl::GL_SECONDARY_COLOR_ARRAY_POINTER_EXT;
using gl::GL_SECONDARY_COLOR_ARRAY;
using gl::GL_SECONDARY_COLOR_ARRAY_EXT;
using gl::GL_CURRENT_RASTER_SECONDARY_COLOR;
using gl::GL_SCREEN_COORDINATES_REND;
using gl::GL_INVERTED_SCREEN_W_REND;
using gl::GL_TEXTURE0;
using gl::GL_TEXTURE0_ARB;
using gl::GL_TEXTURE1;
using gl::GL_TEXTURE1_ARB;
using gl::GL_TEXTURE2;
using gl::GL_TEXTURE2_ARB;
using gl::GL_TEXTURE3;
using gl::GL_TEXTURE3_ARB;
using gl::GL_TEXTURE4;
using gl::GL_TEXTURE4_ARB;
using gl::GL_TEXTURE5;
using gl::GL_TEXTURE5_ARB;
using gl::GL_TEXTURE6;
using gl::GL_TEXTURE6_ARB;
using gl::GL_TEXTURE7;
using gl::GL_TEXTURE7_ARB;
using gl::GL_TEXTURE8;
using gl::GL_TEXTURE8_ARB;
using gl::GL_TEXTURE9;
using gl::GL_TEXTURE9_ARB;
using gl::GL_TEXTURE10;
using gl::GL_TEXTURE10_ARB;
using gl::GL_TEXTURE11;
using gl::GL_TEXTURE11_ARB;
using gl::GL_TEXTURE12;
using gl::GL_TEXTURE12_ARB;
using gl::GL_TEXTURE13;
using gl::GL_TEXTURE13_ARB;
using gl::GL_TEXTURE14;
using gl::GL_TEXTURE14_ARB;
using gl::GL_TEXTURE15;
using gl::GL_TEXTURE15_ARB;
using gl::GL_TEXTURE16;
using gl::GL_TEXTURE16_ARB;
using gl::GL_TEXTURE17;
using gl::GL_TEXTURE17_ARB;
using gl::GL_TEXTURE18;
using gl::GL_TEXTURE18_ARB;
using gl::GL_TEXTURE19;
using gl::GL_TEXTURE19_ARB;
using gl::GL_TEXTURE20;
using gl::GL_TEXTURE20_ARB;
using gl::GL_TEXTURE21;
using gl::GL_TEXTURE21_ARB;
using gl::GL_TEXTURE22;
using gl::GL_TEXTURE22_ARB;
using gl::GL_TEXTURE23;
using gl::GL_TEXTURE23_ARB;
using gl::GL_TEXTURE24;
using gl::GL_TEXTURE24_ARB;
using gl::GL_TEXTURE25;
using gl::GL_TEXTURE25_ARB;
using gl::GL_TEXTURE26;
using gl::GL_TEXTURE26_ARB;
using gl::GL_TEXTURE27;
using gl::GL_TEXTURE27_ARB;
using gl::GL_TEXTURE28;
using gl::GL_TEXTURE28_ARB;
using gl::GL_TEXTURE29;
using gl::GL_TEXTURE29_ARB;
using gl::GL_TEXTURE30;
using gl::GL_TEXTURE30_ARB;
using gl::GL_TEXTURE31;
using gl::GL_TEXTURE31_ARB;
using gl::GL_ACTIVE_TEXTURE;
using gl::GL_ACTIVE_TEXTURE_ARB;
using gl::GL_CLIENT_ACTIVE_TEXTURE;
using gl::GL_CLIENT_ACTIVE_TEXTURE_ARB;
using gl::GL_MAX_TEXTURE_UNITS;
using gl::GL_MAX_TEXTURE_UNITS_ARB;
using gl::GL_PATH_TRANSPOSE_MODELVIEW_MATRIX_NV;
using gl::GL_TRANSPOSE_MODELVIEW_MATRIX;
using gl::GL_TRANSPOSE_MODELVIEW_MATRIX_ARB;
using gl::GL_PATH_TRANSPOSE_PROJECTION_MATRIX_NV;
using gl::GL_TRANSPOSE_PROJECTION_MATRIX;
using gl::GL_TRANSPOSE_PROJECTION_MATRIX_ARB;
using gl::GL_TRANSPOSE_TEXTURE_MATRIX;
using gl::GL_TRANSPOSE_TEXTURE_MATRIX_ARB;
using gl::GL_TRANSPOSE_COLOR_MATRIX;
using gl::GL_TRANSPOSE_COLOR_MATRIX_ARB;
using gl::GL_SUBTRACT;
using gl::GL_SUBTRACT_ARB;
using gl::GL_MAX_RENDERBUFFER_SIZE;
using gl::GL_MAX_RENDERBUFFER_SIZE_EXT;
using gl::GL_COMPRESSED_ALPHA;
using gl::GL_COMPRESSED_ALPHA_ARB;
using gl::GL_COMPRESSED_LUMINANCE;
using gl::GL_COMPRESSED_LUMINANCE_ARB;
using gl::GL_COMPRESSED_LUMINANCE_ALPHA;
using gl::GL_COMPRESSED_LUMINANCE_ALPHA_ARB;
using gl::GL_COMPRESSED_INTENSITY;
using gl::GL_COMPRESSED_INTENSITY_ARB;
using gl::GL_COMPRESSED_RGB;
using gl::GL_COMPRESSED_RGB_ARB;
using gl::GL_COMPRESSED_RGBA;
using gl::GL_COMPRESSED_RGBA_ARB;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER;
using gl::GL_ALL_COMPLETED_NV;
using gl::GL_FENCE_STATUS_NV;
using gl::GL_FENCE_CONDITION_NV;
using gl::GL_TEXTURE_RECTANGLE;
using gl::GL_TEXTURE_RECTANGLE_ARB;
using gl::GL_TEXTURE_RECTANGLE_NV;
using gl::GL_TEXTURE_BINDING_RECTANGLE;
using gl::GL_TEXTURE_BINDING_RECTANGLE_ARB;
using gl::GL_TEXTURE_BINDING_RECTANGLE_NV;
using gl::GL_PROXY_TEXTURE_RECTANGLE;
using gl::GL_PROXY_TEXTURE_RECTANGLE_ARB;
using gl::GL_PROXY_TEXTURE_RECTANGLE_NV;
using gl::GL_MAX_RECTANGLE_TEXTURE_SIZE;
using gl::GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB;
using gl::GL_MAX_RECTANGLE_TEXTURE_SIZE_NV;
using gl::GL_DEPTH_STENCIL;
using gl::GL_DEPTH_STENCIL_EXT;
using gl::GL_DEPTH_STENCIL_NV;
using gl::GL_UNSIGNED_INT_24_8;
using gl::GL_UNSIGNED_INT_24_8_EXT;
using gl::GL_UNSIGNED_INT_24_8_NV;
using gl::GL_MAX_TEXTURE_LOD_BIAS;
using gl::GL_MAX_TEXTURE_LOD_BIAS_EXT;
using gl::GL_TEXTURE_MAX_ANISOTROPY_EXT;
using gl::GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT;
using gl::GL_TEXTURE_FILTER_CONTROL;
using gl::GL_TEXTURE_FILTER_CONTROL_EXT;
using gl::GL_TEXTURE_LOD_BIAS;
using gl::GL_TEXTURE_LOD_BIAS_EXT;
using gl::GL_MODELVIEW1_STACK_DEPTH_EXT;
using gl::GL_COMBINE4_NV;
using gl::GL_MAX_SHININESS_NV;
using gl::GL_MAX_SPOT_EXPONENT_NV;
using gl::GL_MODELVIEW1_MATRIX_EXT;
using gl::GL_INCR_WRAP;
using gl::GL_INCR_WRAP_EXT;
using gl::GL_DECR_WRAP;
using gl::GL_DECR_WRAP_EXT;
using gl::GL_VERTEX_WEIGHTING_EXT;
using gl::GL_MODELVIEW1_ARB;
using gl::GL_MODELVIEW1_EXT;
using gl::GL_CURRENT_VERTEX_WEIGHT_EXT;
using gl::GL_VERTEX_WEIGHT_ARRAY_EXT;
using gl::GL_VERTEX_WEIGHT_ARRAY_SIZE_EXT;
using gl::GL_VERTEX_WEIGHT_ARRAY_TYPE_EXT;
using gl::GL_VERTEX_WEIGHT_ARRAY_STRIDE_EXT;
using gl::GL_VERTEX_WEIGHT_ARRAY_POINTER_EXT;
using gl::GL_NORMAL_MAP;
using gl::GL_NORMAL_MAP_ARB;
using gl::GL_NORMAL_MAP_EXT;
using gl::GL_NORMAL_MAP_NV;
using gl::GL_REFLECTION_MAP;
using gl::GL_REFLECTION_MAP_ARB;
using gl::GL_REFLECTION_MAP_EXT;
using gl::GL_REFLECTION_MAP_NV;
using gl::GL_TEXTURE_CUBE_MAP;
using gl::GL_TEXTURE_CUBE_MAP_ARB;
using gl::GL_TEXTURE_CUBE_MAP_EXT;
using gl::GL_TEXTURE_BINDING_CUBE_MAP;
using gl::GL_TEXTURE_BINDING_CUBE_MAP_ARB;
using gl::GL_TEXTURE_BINDING_CUBE_MAP_EXT;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_X_EXT;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Y_EXT;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_EXT;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB;
using gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Z_EXT;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB;
using gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EXT;
using gl::GL_PROXY_TEXTURE_CUBE_MAP;
using gl::GL_PROXY_TEXTURE_CUBE_MAP_ARB;
using gl::GL_PROXY_TEXTURE_CUBE_MAP_EXT;
using gl::GL_MAX_CUBE_MAP_TEXTURE_SIZE;
using gl::GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB;
using gl::GL_MAX_CUBE_MAP_TEXTURE_SIZE_EXT;
using gl::GL_VERTEX_ARRAY_RANGE_APPLE;
using gl::GL_VERTEX_ARRAY_RANGE_NV;
using gl::GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE;
using gl::GL_VERTEX_ARRAY_RANGE_LENGTH_NV;
using gl::GL_VERTEX_ARRAY_RANGE_VALID_NV;
using gl::GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV;
using gl::GL_VERTEX_ARRAY_RANGE_POINTER_APPLE;
using gl::GL_VERTEX_ARRAY_RANGE_POINTER_NV;
using gl::GL_REGISTER_COMBINERS_NV;
using gl::GL_VARIABLE_A_NV;
using gl::GL_VARIABLE_B_NV;
using gl::GL_VARIABLE_C_NV;
using gl::GL_VARIABLE_D_NV;
using gl::GL_VARIABLE_E_NV;
using gl::GL_VARIABLE_F_NV;
using gl::GL_VARIABLE_G_NV;
using gl::GL_CONSTANT_COLOR0_NV;
using gl::GL_CONSTANT_COLOR1_NV;
using gl::GL_PRIMARY_COLOR_NV;
using gl::GL_SECONDARY_COLOR_NV;
using gl::GL_SPARE0_NV;
using gl::GL_SPARE1_NV;
using gl::GL_DISCARD_NV;
using gl::GL_E_TIMES_F_NV;
using gl::GL_SPARE0_PLUS_SECONDARY_COLOR_NV;
using gl::GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV;
using gl::GL_PER_STAGE_CONSTANTS_NV;
using gl::GL_UNSIGNED_IDENTITY_NV;
using gl::GL_UNSIGNED_INVERT_NV;
using gl::GL_EXPAND_NORMAL_NV;
using gl::GL_EXPAND_NEGATE_NV;
using gl::GL_HALF_BIAS_NORMAL_NV;
using gl::GL_HALF_BIAS_NEGATE_NV;
using gl::GL_SIGNED_IDENTITY_NV;
using gl::GL_SIGNED_NEGATE_NV;
using gl::GL_SCALE_BY_TWO_NV;
using gl::GL_SCALE_BY_FOUR_NV;
using gl::GL_SCALE_BY_ONE_HALF_NV;
using gl::GL_BIAS_BY_NEGATIVE_ONE_HALF_NV;
using gl::GL_COMBINER_INPUT_NV;
using gl::GL_COMBINER_MAPPING_NV;
using gl::GL_COMBINER_COMPONENT_USAGE_NV;
using gl::GL_COMBINER_AB_DOT_PRODUCT_NV;
using gl::GL_COMBINER_CD_DOT_PRODUCT_NV;
using gl::GL_COMBINER_MUX_SUM_NV;
using gl::GL_COMBINER_SCALE_NV;
using gl::GL_COMBINER_BIAS_NV;
using gl::GL_COMBINER_AB_OUTPUT_NV;
using gl::GL_COMBINER_CD_OUTPUT_NV;
using gl::GL_COMBINER_SUM_OUTPUT_NV;
using gl::GL_MAX_GENERAL_COMBINERS_NV;
using gl::GL_NUM_GENERAL_COMBINERS_NV;
using gl::GL_COLOR_SUM_CLAMP_NV;
using gl::GL_COMBINER0_NV;
using gl::GL_COMBINER1_NV;
using gl::GL_COMBINER2_NV;
using gl::GL_COMBINER3_NV;
using gl::GL_COMBINER4_NV;
using gl::GL_COMBINER5_NV;
using gl::GL_COMBINER6_NV;
using gl::GL_COMBINER7_NV;
using gl::GL_PRIMITIVE_RESTART_NV;
using gl::GL_PRIMITIVE_RESTART_INDEX_NV;
using gl::GL_FOG_DISTANCE_MODE_NV;
using gl::GL_EYE_RADIAL_NV;
using gl::GL_EYE_PLANE_ABSOLUTE_NV;
using gl::GL_EMBOSS_LIGHT_NV;
using gl::GL_EMBOSS_CONSTANT_NV;
using gl::GL_EMBOSS_MAP_NV;
using gl::GL_RED_MIN_CLAMP_INGR;
using gl::GL_GREEN_MIN_CLAMP_INGR;
using gl::GL_BLUE_MIN_CLAMP_INGR;
using gl::GL_ALPHA_MIN_CLAMP_INGR;
using gl::GL_RED_MAX_CLAMP_INGR;
using gl::GL_GREEN_MAX_CLAMP_INGR;
using gl::GL_BLUE_MAX_CLAMP_INGR;
using gl::GL_ALPHA_MAX_CLAMP_INGR;
using gl::GL_INTERLACE_READ_INGR;
using gl::GL_COMBINE;
using gl::GL_COMBINE_ARB;
using gl::GL_COMBINE_EXT;
using gl::GL_COMBINE_RGB;
using gl::GL_COMBINE_RGB_ARB;
using gl::GL_COMBINE_RGB_EXT;
using gl::GL_COMBINE_ALPHA;
using gl::GL_COMBINE_ALPHA_ARB;
using gl::GL_COMBINE_ALPHA_EXT;
using gl::GL_RGB_SCALE;
using gl::GL_RGB_SCALE_ARB;
using gl::GL_RGB_SCALE_EXT;
using gl::GL_ADD_SIGNED;
using gl::GL_ADD_SIGNED_ARB;
using gl::GL_ADD_SIGNED_EXT;
using gl::GL_INTERPOLATE;
using gl::GL_INTERPOLATE_ARB;
using gl::GL_INTERPOLATE_EXT;
using gl::GL_CONSTANT;
using gl::GL_CONSTANT_ARB;
using gl::GL_CONSTANT_EXT;
using gl::GL_CONSTANT_NV;
using gl::GL_PRIMARY_COLOR;
using gl::GL_PRIMARY_COLOR_ARB;
using gl::GL_PRIMARY_COLOR_EXT;
using gl::GL_PREVIOUS;
using gl::GL_PREVIOUS_ARB;
using gl::GL_PREVIOUS_EXT;
using gl::GL_SOURCE0_RGB;
using gl::GL_SOURCE0_RGB_ARB;
using gl::GL_SOURCE0_RGB_EXT;
using gl::GL_SRC0_RGB;
using gl::GL_SOURCE1_RGB;
using gl::GL_SOURCE1_RGB_ARB;
using gl::GL_SOURCE1_RGB_EXT;
using gl::GL_SRC1_RGB;
using gl::GL_SOURCE2_RGB;
using gl::GL_SOURCE2_RGB_ARB;
using gl::GL_SOURCE2_RGB_EXT;
using gl::GL_SRC2_RGB;
using gl::GL_SOURCE3_RGB_NV;
using gl::GL_SOURCE0_ALPHA;
using gl::GL_SOURCE0_ALPHA_ARB;
using gl::GL_SOURCE0_ALPHA_EXT;
using gl::GL_SRC0_ALPHA;
using gl::GL_SOURCE1_ALPHA;
using gl::GL_SOURCE1_ALPHA_ARB;
using gl::GL_SOURCE1_ALPHA_EXT;
using gl::GL_SRC1_ALPHA;
using gl::GL_SOURCE2_ALPHA;
using gl::GL_SOURCE2_ALPHA_ARB;
using gl::GL_SOURCE2_ALPHA_EXT;
using gl::GL_SRC2_ALPHA;
using gl::GL_SOURCE3_ALPHA_NV;
using gl::GL_OPERAND0_RGB;
using gl::GL_OPERAND0_RGB_ARB;
using gl::GL_OPERAND0_RGB_EXT;
using gl::GL_OPERAND1_RGB;
using gl::GL_OPERAND1_RGB_ARB;
using gl::GL_OPERAND1_RGB_EXT;
using gl::GL_OPERAND2_RGB;
using gl::GL_OPERAND2_RGB_ARB;
using gl::GL_OPERAND2_RGB_EXT;
using gl::GL_OPERAND3_RGB_NV;
using gl::GL_OPERAND0_ALPHA;
using gl::GL_OPERAND0_ALPHA_ARB;
using gl::GL_OPERAND0_ALPHA_EXT;
using gl::GL_OPERAND1_ALPHA;
using gl::GL_OPERAND1_ALPHA_ARB;
using gl::GL_OPERAND1_ALPHA_EXT;
using gl::GL_OPERAND2_ALPHA;
using gl::GL_OPERAND2_ALPHA_ARB;
using gl::GL_OPERAND2_ALPHA_EXT;
using gl::GL_OPERAND3_ALPHA_NV;
using gl::GL_PERTURB_EXT;
using gl::GL_TEXTURE_NORMAL_EXT;
using gl::GL_LIGHT_MODEL_SPECULAR_VECTOR_APPLE;
using gl::GL_UNPACK_CLIENT_STORAGE_APPLE;
using gl::GL_BUFFER_OBJECT_APPLE;
using gl::GL_STORAGE_CLIENT_APPLE;
using gl::GL_VERTEX_ARRAY_BINDING;
using gl::GL_VERTEX_ARRAY_BINDING_APPLE;
using gl::GL_TEXTURE_RANGE_LENGTH_APPLE;
using gl::GL_TEXTURE_RANGE_POINTER_APPLE;
using gl::GL_YCBCR_422_APPLE;
using gl::GL_UNSIGNED_SHORT_8_8_APPLE;
using gl::GL_UNSIGNED_SHORT_8_8_MESA;
using gl::GL_UNSIGNED_SHORT_8_8_REV_APPLE;
using gl::GL_UNSIGNED_SHORT_8_8_REV_MESA;
using gl::GL_STORAGE_PRIVATE_APPLE;
using gl::GL_STORAGE_CACHED_APPLE;
using gl::GL_STORAGE_SHARED_APPLE;
using gl::GL_REPLACEMENT_CODE_ARRAY_SUN;
using gl::GL_REPLACEMENT_CODE_ARRAY_TYPE_SUN;
using gl::GL_REPLACEMENT_CODE_ARRAY_STRIDE_SUN;
using gl::GL_REPLACEMENT_CODE_ARRAY_POINTER_SUN;
using gl::GL_R1UI_V3F_SUN;
using gl::GL_R1UI_C4UB_V3F_SUN;
using gl::GL_R1UI_C3F_V3F_SUN;
using gl::GL_R1UI_N3F_V3F_SUN;
using gl::GL_R1UI_C4F_N3F_V3F_SUN;
using gl::GL_R1UI_T2F_V3F_SUN;
using gl::GL_R1UI_T2F_N3F_V3F_SUN;
using gl::GL_R1UI_T2F_C4F_N3F_V3F_SUN;
using gl::GL_SLICE_ACCUM_SUN;
using gl::GL_QUAD_MESH_SUN;
using gl::GL_TRIANGLE_MESH_SUN;
using gl::GL_VERTEX_PROGRAM_ARB;
using gl::GL_VERTEX_PROGRAM_NV;
using gl::GL_VERTEX_STATE_PROGRAM_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_ENABLED;
using gl::GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB;
using gl::GL_ATTRIB_ARRAY_SIZE_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_SIZE;
using gl::GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB;
using gl::GL_ATTRIB_ARRAY_STRIDE_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_STRIDE;
using gl::GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB;
using gl::GL_ATTRIB_ARRAY_TYPE_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_TYPE;
using gl::GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB;
using gl::GL_CURRENT_ATTRIB_NV;
using gl::GL_CURRENT_VERTEX_ATTRIB;
using gl::GL_CURRENT_VERTEX_ATTRIB_ARB;
using gl::GL_PROGRAM_LENGTH_ARB;
using gl::GL_PROGRAM_LENGTH_NV;
using gl::GL_PROGRAM_STRING_ARB;
using gl::GL_PROGRAM_STRING_NV;
using gl::GL_MODELVIEW_PROJECTION_NV;
using gl::GL_IDENTITY_NV;
using gl::GL_INVERSE_NV;
using gl::GL_TRANSPOSE_NV;
using gl::GL_INVERSE_TRANSPOSE_NV;
using gl::GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB;
using gl::GL_MAX_TRACK_MATRIX_STACK_DEPTH_NV;
using gl::GL_MAX_PROGRAM_MATRICES_ARB;
using gl::GL_MAX_TRACK_MATRICES_NV;
using gl::GL_MATRIX0_NV;
using gl::GL_MATRIX1_NV;
using gl::GL_MATRIX2_NV;
using gl::GL_MATRIX3_NV;
using gl::GL_MATRIX4_NV;
using gl::GL_MATRIX5_NV;
using gl::GL_MATRIX6_NV;
using gl::GL_MATRIX7_NV;
using gl::GL_CURRENT_MATRIX_STACK_DEPTH_ARB;
using gl::GL_CURRENT_MATRIX_STACK_DEPTH_NV;
using gl::GL_CURRENT_MATRIX_ARB;
using gl::GL_CURRENT_MATRIX_NV;
using gl::GL_PROGRAM_POINT_SIZE;
using gl::GL_PROGRAM_POINT_SIZE_ARB;
using gl::GL_PROGRAM_POINT_SIZE_EXT;
using gl::GL_VERTEX_PROGRAM_POINT_SIZE;
using gl::GL_VERTEX_PROGRAM_POINT_SIZE_ARB;
using gl::GL_VERTEX_PROGRAM_POINT_SIZE_NV;
using gl::GL_VERTEX_PROGRAM_TWO_SIDE;
using gl::GL_VERTEX_PROGRAM_TWO_SIDE_ARB;
using gl::GL_VERTEX_PROGRAM_TWO_SIDE_NV;
using gl::GL_PROGRAM_PARAMETER_NV;
using gl::GL_ATTRIB_ARRAY_POINTER_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_POINTER;
using gl::GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB;
using gl::GL_PROGRAM_TARGET_NV;
using gl::GL_PROGRAM_RESIDENT_NV;
using gl::GL_TRACK_MATRIX_NV;
using gl::GL_TRACK_MATRIX_TRANSFORM_NV;
using gl::GL_VERTEX_PROGRAM_BINDING_NV;
using gl::GL_PROGRAM_ERROR_POSITION_ARB;
using gl::GL_PROGRAM_ERROR_POSITION_NV;
using gl::GL_OFFSET_TEXTURE_RECTANGLE_NV;
using gl::GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV;
using gl::GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV;
using gl::GL_DEPTH_CLAMP;
using gl::GL_DEPTH_CLAMP_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY0_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY1_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY2_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY3_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY4_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY5_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY6_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY7_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY8_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY9_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY10_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY11_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY12_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY13_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY14_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY15_NV;
using gl::GL_MAP1_VERTEX_ATTRIB0_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB1_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB2_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB3_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB4_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB5_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB6_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB7_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB8_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB9_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB10_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB11_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB12_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB13_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB14_4_NV;
using gl::GL_MAP1_VERTEX_ATTRIB15_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB0_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB1_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB2_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB3_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB4_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB5_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB6_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB7_4_NV;
using gl::GL_PROGRAM_BINDING_ARB;
using gl::GL_MAP2_VERTEX_ATTRIB8_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB9_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB10_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB11_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB12_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB13_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB14_4_NV;
using gl::GL_MAP2_VERTEX_ATTRIB15_4_NV;
using gl::GL_TEXTURE_COMPRESSED_IMAGE_SIZE;
using gl::GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB;
using gl::GL_TEXTURE_COMPRESSED;
using gl::GL_TEXTURE_COMPRESSED_ARB;
using gl::GL_NUM_COMPRESSED_TEXTURE_FORMATS;
using gl::GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB;
using gl::GL_COMPRESSED_TEXTURE_FORMATS;
using gl::GL_COMPRESSED_TEXTURE_FORMATS_ARB;
using gl::GL_MAX_VERTEX_UNITS_ARB;
using gl::GL_ACTIVE_VERTEX_UNITS_ARB;
using gl::GL_WEIGHT_SUM_UNITY_ARB;
using gl::GL_VERTEX_BLEND_ARB;
using gl::GL_CURRENT_WEIGHT_ARB;
using gl::GL_WEIGHT_ARRAY_TYPE_ARB;
using gl::GL_WEIGHT_ARRAY_STRIDE_ARB;
using gl::GL_WEIGHT_ARRAY_SIZE_ARB;
using gl::GL_WEIGHT_ARRAY_POINTER_ARB;
using gl::GL_WEIGHT_ARRAY_ARB;
using gl::GL_DOT3_RGB;
using gl::GL_DOT3_RGB_ARB;
using gl::GL_DOT3_RGBA;
using gl::GL_DOT3_RGBA_ARB;
using gl::GL_COMPRESSED_RGB_FXT1_3DFX;
using gl::GL_COMPRESSED_RGBA_FXT1_3DFX;
using gl::GL_MULTISAMPLE_3DFX;
using gl::GL_SAMPLE_BUFFERS_3DFX;
using gl::GL_SAMPLES_3DFX;
using gl::GL_EVAL_2D_NV;
using gl::GL_EVAL_TRIANGULAR_2D_NV;
using gl::GL_MAP_TESSELLATION_NV;
using gl::GL_MAP_ATTRIB_U_ORDER_NV;
using gl::GL_MAP_ATTRIB_V_ORDER_NV;
using gl::GL_EVAL_FRACTIONAL_TESSELLATION_NV;
using gl::GL_EVAL_VERTEX_ATTRIB0_NV;
using gl::GL_EVAL_VERTEX_ATTRIB1_NV;
using gl::GL_EVAL_VERTEX_ATTRIB2_NV;
using gl::GL_EVAL_VERTEX_ATTRIB3_NV;
using gl::GL_EVAL_VERTEX_ATTRIB4_NV;
using gl::GL_EVAL_VERTEX_ATTRIB5_NV;
using gl::GL_EVAL_VERTEX_ATTRIB6_NV;
using gl::GL_EVAL_VERTEX_ATTRIB7_NV;
using gl::GL_EVAL_VERTEX_ATTRIB8_NV;
using gl::GL_EVAL_VERTEX_ATTRIB9_NV;
using gl::GL_EVAL_VERTEX_ATTRIB10_NV;
using gl::GL_EVAL_VERTEX_ATTRIB11_NV;
using gl::GL_EVAL_VERTEX_ATTRIB12_NV;
using gl::GL_EVAL_VERTEX_ATTRIB13_NV;
using gl::GL_EVAL_VERTEX_ATTRIB14_NV;
using gl::GL_EVAL_VERTEX_ATTRIB15_NV;
using gl::GL_MAX_MAP_TESSELLATION_NV;
using gl::GL_MAX_RATIONAL_EVAL_ORDER_NV;
using gl::GL_MAX_PROGRAM_PATCH_ATTRIBS_NV;
using gl::GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV;
using gl::GL_UNSIGNED_INT_S8_S8_8_8_NV;
using gl::GL_UNSIGNED_INT_8_8_S8_S8_REV_NV;
using gl::GL_DSDT_MAG_INTENSITY_NV;
using gl::GL_SHADER_CONSISTENT_NV;
using gl::GL_TEXTURE_SHADER_NV;
using gl::GL_SHADER_OPERATION_NV;
using gl::GL_CULL_MODES_NV;
using gl::GL_OFFSET_TEXTURE_2D_MATRIX_NV;
using gl::GL_OFFSET_TEXTURE_MATRIX_NV;
using gl::GL_OFFSET_TEXTURE_2D_SCALE_NV;
using gl::GL_OFFSET_TEXTURE_SCALE_NV;
using gl::GL_OFFSET_TEXTURE_2D_BIAS_NV;
using gl::GL_OFFSET_TEXTURE_BIAS_NV;
using gl::GL_PREVIOUS_TEXTURE_INPUT_NV;
using gl::GL_CONST_EYE_NV;
using gl::GL_PASS_THROUGH_NV;
using gl::GL_CULL_FRAGMENT_NV;
using gl::GL_OFFSET_TEXTURE_2D_NV;
using gl::GL_DEPENDENT_AR_TEXTURE_2D_NV;
using gl::GL_DEPENDENT_GB_TEXTURE_2D_NV;
using gl::GL_SURFACE_STATE_NV;
using gl::GL_DOT_PRODUCT_NV;
using gl::GL_DOT_PRODUCT_DEPTH_REPLACE_NV;
using gl::GL_DOT_PRODUCT_TEXTURE_2D_NV;
using gl::GL_DOT_PRODUCT_TEXTURE_3D_NV;
using gl::GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV;
using gl::GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV;
using gl::GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV;
using gl::GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV;
using gl::GL_HILO_NV;
using gl::GL_DSDT_NV;
using gl::GL_DSDT_MAG_NV;
using gl::GL_DSDT_MAG_VIB_NV;
using gl::GL_HILO16_NV;
using gl::GL_SIGNED_HILO_NV;
using gl::GL_SIGNED_HILO16_NV;
using gl::GL_SIGNED_RGBA_NV;
using gl::GL_SIGNED_RGBA8_NV;
using gl::GL_SURFACE_REGISTERED_NV;
using gl::GL_SIGNED_RGB_NV;
using gl::GL_SIGNED_RGB8_NV;
using gl::GL_SURFACE_MAPPED_NV;
using gl::GL_SIGNED_LUMINANCE_NV;
using gl::GL_SIGNED_LUMINANCE8_NV;
using gl::GL_SIGNED_LUMINANCE_ALPHA_NV;
using gl::GL_SIGNED_LUMINANCE8_ALPHA8_NV;
using gl::GL_SIGNED_ALPHA_NV;
using gl::GL_SIGNED_ALPHA8_NV;
using gl::GL_SIGNED_INTENSITY_NV;
using gl::GL_SIGNED_INTENSITY8_NV;
using gl::GL_DSDT8_NV;
using gl::GL_DSDT8_MAG8_NV;
using gl::GL_DSDT8_MAG8_INTENSITY8_NV;
using gl::GL_SIGNED_RGB_UNSIGNED_ALPHA_NV;
using gl::GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV;
using gl::GL_HI_SCALE_NV;
using gl::GL_LO_SCALE_NV;
using gl::GL_DS_SCALE_NV;
using gl::GL_DT_SCALE_NV;
using gl::GL_MAGNITUDE_SCALE_NV;
using gl::GL_VIBRANCE_SCALE_NV;
using gl::GL_HI_BIAS_NV;
using gl::GL_LO_BIAS_NV;
using gl::GL_DS_BIAS_NV;
using gl::GL_DT_BIAS_NV;
using gl::GL_MAGNITUDE_BIAS_NV;
using gl::GL_VIBRANCE_BIAS_NV;
using gl::GL_TEXTURE_BORDER_VALUES_NV;
using gl::GL_TEXTURE_HI_SIZE_NV;
using gl::GL_TEXTURE_LO_SIZE_NV;
using gl::GL_TEXTURE_DS_SIZE_NV;
using gl::GL_TEXTURE_DT_SIZE_NV;
using gl::GL_TEXTURE_MAG_SIZE_NV;
using gl::GL_MODELVIEW2_ARB;
using gl::GL_MODELVIEW3_ARB;
using gl::GL_MODELVIEW4_ARB;
using gl::GL_MODELVIEW5_ARB;
using gl::GL_MODELVIEW6_ARB;
using gl::GL_MODELVIEW7_ARB;
using gl::GL_MODELVIEW8_ARB;
using gl::GL_MODELVIEW9_ARB;
using gl::GL_MODELVIEW10_ARB;
using gl::GL_MODELVIEW11_ARB;
using gl::GL_MODELVIEW12_ARB;
using gl::GL_MODELVIEW13_ARB;
using gl::GL_MODELVIEW14_ARB;
using gl::GL_MODELVIEW15_ARB;
using gl::GL_MODELVIEW16_ARB;
using gl::GL_MODELVIEW17_ARB;
using gl::GL_MODELVIEW18_ARB;
using gl::GL_MODELVIEW19_ARB;
using gl::GL_MODELVIEW20_ARB;
using gl::GL_MODELVIEW21_ARB;
using gl::GL_MODELVIEW22_ARB;
using gl::GL_MODELVIEW23_ARB;
using gl::GL_MODELVIEW24_ARB;
using gl::GL_MODELVIEW25_ARB;
using gl::GL_MODELVIEW26_ARB;
using gl::GL_MODELVIEW27_ARB;
using gl::GL_MODELVIEW28_ARB;
using gl::GL_MODELVIEW29_ARB;
using gl::GL_MODELVIEW30_ARB;
using gl::GL_MODELVIEW31_ARB;
using gl::GL_DOT3_RGB_EXT;
using gl::GL_DOT3_RGBA_EXT;
using gl::GL_PROGRAM_BINARY_LENGTH;
using gl::GL_MIRROR_CLAMP_ATI;
using gl::GL_MIRROR_CLAMP_EXT;
using gl::GL_MIRROR_CLAMP_TO_EDGE;
using gl::GL_MIRROR_CLAMP_TO_EDGE_ATI;
using gl::GL_MIRROR_CLAMP_TO_EDGE_EXT;
using gl::GL_MODULATE_ADD_ATI;
using gl::GL_MODULATE_SIGNED_ADD_ATI;
using gl::GL_MODULATE_SUBTRACT_ATI;
using gl::GL_SET_AMD;
using gl::GL_REPLACE_VALUE_AMD;
using gl::GL_STENCIL_OP_VALUE_AMD;
using gl::GL_STENCIL_BACK_OP_VALUE_AMD;
using gl::GL_VERTEX_ATTRIB_ARRAY_LONG;
using gl::GL_OCCLUSION_QUERY_EVENT_MASK_AMD;
using gl::GL_YCBCR_MESA;
using gl::GL_PACK_INVERT_MESA;
using gl::GL_TEXTURE_1D_STACK_MESAX;
using gl::GL_TEXTURE_2D_STACK_MESAX;
using gl::GL_PROXY_TEXTURE_1D_STACK_MESAX;
using gl::GL_PROXY_TEXTURE_2D_STACK_MESAX;
using gl::GL_TEXTURE_1D_STACK_BINDING_MESAX;
using gl::GL_TEXTURE_2D_STACK_BINDING_MESAX;
using gl::GL_STATIC_ATI;
using gl::GL_DYNAMIC_ATI;
using gl::GL_PRESERVE_ATI;
using gl::GL_DISCARD_ATI;
using gl::GL_BUFFER_SIZE;
using gl::GL_BUFFER_SIZE_ARB;
using gl::GL_OBJECT_BUFFER_SIZE_ATI;
using gl::GL_BUFFER_USAGE;
using gl::GL_BUFFER_USAGE_ARB;
using gl::GL_OBJECT_BUFFER_USAGE_ATI;
using gl::GL_ARRAY_OBJECT_BUFFER_ATI;
using gl::GL_ARRAY_OBJECT_OFFSET_ATI;
using gl::GL_ELEMENT_ARRAY_ATI;
using gl::GL_ELEMENT_ARRAY_TYPE_ATI;
using gl::GL_ELEMENT_ARRAY_POINTER_ATI;
using gl::GL_MAX_VERTEX_STREAMS_ATI;
using gl::GL_VERTEX_STREAM0_ATI;
using gl::GL_VERTEX_STREAM1_ATI;
using gl::GL_VERTEX_STREAM2_ATI;
using gl::GL_VERTEX_STREAM3_ATI;
using gl::GL_VERTEX_STREAM4_ATI;
using gl::GL_VERTEX_STREAM5_ATI;
using gl::GL_VERTEX_STREAM6_ATI;
using gl::GL_VERTEX_STREAM7_ATI;
using gl::GL_VERTEX_SOURCE_ATI;
using gl::GL_BUMP_ROT_MATRIX_ATI;
using gl::GL_BUMP_ROT_MATRIX_SIZE_ATI;
using gl::GL_BUMP_NUM_TEX_UNITS_ATI;
using gl::GL_BUMP_TEX_UNITS_ATI;
using gl::GL_DUDV_ATI;
using gl::GL_DU8DV8_ATI;
using gl::GL_BUMP_ENVMAP_ATI;
using gl::GL_BUMP_TARGET_ATI;
using gl::GL_VERTEX_SHADER_EXT;
using gl::GL_VERTEX_SHADER_BINDING_EXT;
using gl::GL_OP_INDEX_EXT;
using gl::GL_OP_NEGATE_EXT;
using gl::GL_OP_DOT3_EXT;
using gl::GL_OP_DOT4_EXT;
using gl::GL_OP_MUL_EXT;
using gl::GL_OP_ADD_EXT;
using gl::GL_OP_MADD_EXT;
using gl::GL_OP_FRAC_EXT;
using gl::GL_OP_MAX_EXT;
using gl::GL_OP_MIN_EXT;
using gl::GL_OP_SET_GE_EXT;
using gl::GL_OP_SET_LT_EXT;
using gl::GL_OP_CLAMP_EXT;
using gl::GL_OP_FLOOR_EXT;
using gl::GL_OP_ROUND_EXT;
using gl::GL_OP_EXP_BASE_2_EXT;
using gl::GL_OP_LOG_BASE_2_EXT;
using gl::GL_OP_POWER_EXT;
using gl::GL_OP_RECIP_EXT;
using gl::GL_OP_RECIP_SQRT_EXT;
using gl::GL_OP_SUB_EXT;
using gl::GL_OP_CROSS_PRODUCT_EXT;
using gl::GL_OP_MULTIPLY_MATRIX_EXT;
using gl::GL_OP_MOV_EXT;
using gl::GL_OUTPUT_VERTEX_EXT;
using gl::GL_OUTPUT_COLOR0_EXT;
using gl::GL_OUTPUT_COLOR1_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD0_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD1_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD2_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD3_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD4_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD5_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD6_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD7_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD8_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD9_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD10_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD11_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD12_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD13_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD14_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD15_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD16_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD17_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD18_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD19_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD20_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD21_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD22_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD23_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD24_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD25_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD26_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD27_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD28_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD29_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD30_EXT;
using gl::GL_OUTPUT_TEXTURE_COORD31_EXT;
using gl::GL_OUTPUT_FOG_EXT;
using gl::GL_SCALAR_EXT;
using gl::GL_VECTOR_EXT;
using gl::GL_MATRIX_EXT;
using gl::GL_VARIANT_EXT;
using gl::GL_INVARIANT_EXT;
using gl::GL_LOCAL_CONSTANT_EXT;
using gl::GL_LOCAL_EXT;
using gl::GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT;
using gl::GL_MAX_VERTEX_SHADER_VARIANTS_EXT;
using gl::GL_MAX_VERTEX_SHADER_INVARIANTS_EXT;
using gl::GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT;
using gl::GL_MAX_VERTEX_SHADER_LOCALS_EXT;
using gl::GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT;
using gl::GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT;
using gl::GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT;
using gl::GL_MAX_OPTIMIZED_VERTEX_SHADER_INVARIANTS_EXT;
using gl::GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCALS_EXT;
using gl::GL_VERTEX_SHADER_INSTRUCTIONS_EXT;
using gl::GL_VERTEX_SHADER_VARIANTS_EXT;
using gl::GL_VERTEX_SHADER_INVARIANTS_EXT;
using gl::GL_VERTEX_SHADER_LOCAL_CONSTANTS_EXT;
using gl::GL_VERTEX_SHADER_LOCALS_EXT;
using gl::GL_VERTEX_SHADER_OPTIMIZED_EXT;
using gl::GL_X_EXT;
using gl::GL_Y_EXT;
using gl::GL_Z_EXT;
using gl::GL_W_EXT;
using gl::GL_NEGATIVE_X_EXT;
using gl::GL_NEGATIVE_Y_EXT;
using gl::GL_NEGATIVE_Z_EXT;
using gl::GL_NEGATIVE_W_EXT;
using gl::GL_ZERO_EXT;
using gl::GL_ONE_EXT;
using gl::GL_NEGATIVE_ONE_EXT;
using gl::GL_NORMALIZED_RANGE_EXT;
using gl::GL_FULL_RANGE_EXT;
using gl::GL_CURRENT_VERTEX_EXT;
using gl::GL_MVP_MATRIX_EXT;
using gl::GL_VARIANT_VALUE_EXT;
using gl::GL_VARIANT_DATATYPE_EXT;
using gl::GL_VARIANT_ARRAY_STRIDE_EXT;
using gl::GL_VARIANT_ARRAY_TYPE_EXT;
using gl::GL_VARIANT_ARRAY_EXT;
using gl::GL_VARIANT_ARRAY_POINTER_EXT;
using gl::GL_INVARIANT_VALUE_EXT;
using gl::GL_INVARIANT_DATATYPE_EXT;
using gl::GL_LOCAL_CONSTANT_VALUE_EXT;
using gl::GL_LOCAL_CONSTANT_DATATYPE_EXT;
using gl::GL_PN_TRIANGLES_ATI;
using gl::GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI;
using gl::GL_PN_TRIANGLES_POINT_MODE_ATI;
using gl::GL_PN_TRIANGLES_NORMAL_MODE_ATI;
using gl::GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI;
using gl::GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI;
using gl::GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI;
using gl::GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI;
using gl::GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI;
using gl::GL_VBO_FREE_MEMORY_ATI;
using gl::GL_TEXTURE_FREE_MEMORY_ATI;
using gl::GL_RENDERBUFFER_FREE_MEMORY_ATI;
using gl::GL_NUM_PROGRAM_BINARY_FORMATS;
using gl::GL_PROGRAM_BINARY_FORMATS;
using gl::GL_STENCIL_BACK_FUNC;
using gl::GL_STENCIL_BACK_FUNC_ATI;
using gl::GL_STENCIL_BACK_FAIL;
using gl::GL_STENCIL_BACK_FAIL_ATI;
using gl::GL_STENCIL_BACK_PASS_DEPTH_FAIL;
using gl::GL_STENCIL_BACK_PASS_DEPTH_FAIL_ATI;
using gl::GL_STENCIL_BACK_PASS_DEPTH_PASS;
using gl::GL_STENCIL_BACK_PASS_DEPTH_PASS_ATI;
using gl::GL_FRAGMENT_PROGRAM_ARB;
using gl::GL_PROGRAM_ALU_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_TEX_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_TEX_INDIRECTIONS_ARB;
using gl::GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB;
using gl::GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB;
using gl::GL_RGBA32F;
using gl::GL_RGBA32F_ARB;
using gl::GL_RGBA_FLOAT32_APPLE;
using gl::GL_RGBA_FLOAT32_ATI;
using gl::GL_RGB32F;
using gl::GL_RGB32F_ARB;
using gl::GL_RGB_FLOAT32_APPLE;
using gl::GL_RGB_FLOAT32_ATI;
using gl::GL_ALPHA32F_ARB;
using gl::GL_ALPHA_FLOAT32_APPLE;
using gl::GL_ALPHA_FLOAT32_ATI;
using gl::GL_INTENSITY32F_ARB;
using gl::GL_INTENSITY_FLOAT32_APPLE;
using gl::GL_INTENSITY_FLOAT32_ATI;
using gl::GL_LUMINANCE32F_ARB;
using gl::GL_LUMINANCE_FLOAT32_APPLE;
using gl::GL_LUMINANCE_FLOAT32_ATI;
using gl::GL_LUMINANCE_ALPHA32F_ARB;
using gl::GL_LUMINANCE_ALPHA_FLOAT32_APPLE;
using gl::GL_LUMINANCE_ALPHA_FLOAT32_ATI;
using gl::GL_RGBA16F;
using gl::GL_RGBA16F_ARB;
using gl::GL_RGBA_FLOAT16_APPLE;
using gl::GL_RGBA_FLOAT16_ATI;
using gl::GL_RGB16F;
using gl::GL_RGB16F_ARB;
using gl::GL_RGB_FLOAT16_APPLE;
using gl::GL_RGB_FLOAT16_ATI;
using gl::GL_ALPHA16F_ARB;
using gl::GL_ALPHA_FLOAT16_APPLE;
using gl::GL_ALPHA_FLOAT16_ATI;
using gl::GL_INTENSITY16F_ARB;
using gl::GL_INTENSITY_FLOAT16_APPLE;
using gl::GL_INTENSITY_FLOAT16_ATI;
using gl::GL_LUMINANCE16F_ARB;
using gl::GL_LUMINANCE_FLOAT16_APPLE;
using gl::GL_LUMINANCE_FLOAT16_ATI;
using gl::GL_LUMINANCE_ALPHA16F_ARB;
using gl::GL_LUMINANCE_ALPHA_FLOAT16_APPLE;
using gl::GL_LUMINANCE_ALPHA_FLOAT16_ATI;
using gl::GL_RGBA_FLOAT_MODE_ARB;
using gl::GL_RGBA_FLOAT_MODE_ATI;
using gl::GL_MAX_DRAW_BUFFERS;
using gl::GL_MAX_DRAW_BUFFERS_ARB;
using gl::GL_MAX_DRAW_BUFFERS_ATI;
using gl::GL_DRAW_BUFFER0;
using gl::GL_DRAW_BUFFER0_ARB;
using gl::GL_DRAW_BUFFER0_ATI;
using gl::GL_DRAW_BUFFER1;
using gl::GL_DRAW_BUFFER1_ARB;
using gl::GL_DRAW_BUFFER1_ATI;
using gl::GL_DRAW_BUFFER2;
using gl::GL_DRAW_BUFFER2_ARB;
using gl::GL_DRAW_BUFFER2_ATI;
using gl::GL_DRAW_BUFFER3;
using gl::GL_DRAW_BUFFER3_ARB;
using gl::GL_DRAW_BUFFER3_ATI;
using gl::GL_DRAW_BUFFER4;
using gl::GL_DRAW_BUFFER4_ARB;
using gl::GL_DRAW_BUFFER4_ATI;
using gl::GL_DRAW_BUFFER5;
using gl::GL_DRAW_BUFFER5_ARB;
using gl::GL_DRAW_BUFFER5_ATI;
using gl::GL_DRAW_BUFFER6;
using gl::GL_DRAW_BUFFER6_ARB;
using gl::GL_DRAW_BUFFER6_ATI;
using gl::GL_DRAW_BUFFER7;
using gl::GL_DRAW_BUFFER7_ARB;
using gl::GL_DRAW_BUFFER7_ATI;
using gl::GL_DRAW_BUFFER8;
using gl::GL_DRAW_BUFFER8_ARB;
using gl::GL_DRAW_BUFFER8_ATI;
using gl::GL_DRAW_BUFFER9;
using gl::GL_DRAW_BUFFER9_ARB;
using gl::GL_DRAW_BUFFER9_ATI;
using gl::GL_DRAW_BUFFER10;
using gl::GL_DRAW_BUFFER10_ARB;
using gl::GL_DRAW_BUFFER10_ATI;
using gl::GL_DRAW_BUFFER11;
using gl::GL_DRAW_BUFFER11_ARB;
using gl::GL_DRAW_BUFFER11_ATI;
using gl::GL_DRAW_BUFFER12;
using gl::GL_DRAW_BUFFER12_ARB;
using gl::GL_DRAW_BUFFER12_ATI;
using gl::GL_DRAW_BUFFER13;
using gl::GL_DRAW_BUFFER13_ARB;
using gl::GL_DRAW_BUFFER13_ATI;
using gl::GL_DRAW_BUFFER14;
using gl::GL_DRAW_BUFFER14_ARB;
using gl::GL_DRAW_BUFFER14_ATI;
using gl::GL_DRAW_BUFFER15;
using gl::GL_DRAW_BUFFER15_ARB;
using gl::GL_DRAW_BUFFER15_ATI;
using gl::GL_COLOR_CLEAR_UNCLAMPED_VALUE_ATI;
using gl::GL_BLEND_EQUATION_ALPHA;
using gl::GL_BLEND_EQUATION_ALPHA_EXT;
using gl::GL_SUBSAMPLE_DISTANCE_AMD;
using gl::GL_MATRIX_PALETTE_ARB;
using gl::GL_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB;
using gl::GL_MAX_PALETTE_MATRICES_ARB;
using gl::GL_CURRENT_PALETTE_MATRIX_ARB;
using gl::GL_MATRIX_INDEX_ARRAY_ARB;
using gl::GL_CURRENT_MATRIX_INDEX_ARB;
using gl::GL_MATRIX_INDEX_ARRAY_SIZE_ARB;
using gl::GL_MATRIX_INDEX_ARRAY_TYPE_ARB;
using gl::GL_MATRIX_INDEX_ARRAY_STRIDE_ARB;
using gl::GL_MATRIX_INDEX_ARRAY_POINTER_ARB;
using gl::GL_TEXTURE_DEPTH_SIZE;
using gl::GL_TEXTURE_DEPTH_SIZE_ARB;
using gl::GL_DEPTH_TEXTURE_MODE;
using gl::GL_DEPTH_TEXTURE_MODE_ARB;
using gl::GL_TEXTURE_COMPARE_MODE;
using gl::GL_TEXTURE_COMPARE_MODE_ARB;
using gl::GL_TEXTURE_COMPARE_FUNC;
using gl::GL_TEXTURE_COMPARE_FUNC_ARB;
using gl::GL_COMPARE_REF_DEPTH_TO_TEXTURE_EXT;
using gl::GL_COMPARE_REF_TO_TEXTURE;
using gl::GL_COMPARE_R_TO_TEXTURE;
using gl::GL_COMPARE_R_TO_TEXTURE_ARB;
using gl::GL_TEXTURE_CUBE_MAP_SEAMLESS;
using gl::GL_OFFSET_PROJECTIVE_TEXTURE_2D_NV;
using gl::GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_NV;
using gl::GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_NV;
using gl::GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_NV;
using gl::GL_OFFSET_HILO_TEXTURE_2D_NV;
using gl::GL_OFFSET_HILO_TEXTURE_RECTANGLE_NV;
using gl::GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_NV;
using gl::GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_NV;
using gl::GL_DEPENDENT_HILO_TEXTURE_2D_NV;
using gl::GL_DEPENDENT_RGB_TEXTURE_3D_NV;
using gl::GL_DEPENDENT_RGB_TEXTURE_CUBE_MAP_NV;
using gl::GL_DOT_PRODUCT_PASS_THROUGH_NV;
using gl::GL_DOT_PRODUCT_TEXTURE_1D_NV;
using gl::GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_NV;
using gl::GL_HILO8_NV;
using gl::GL_SIGNED_HILO8_NV;
using gl::GL_FORCE_BLUE_TO_ONE_NV;
using gl::GL_POINT_SPRITE;
using gl::GL_POINT_SPRITE_ARB;
using gl::GL_POINT_SPRITE_NV;
using gl::GL_COORD_REPLACE;
using gl::GL_COORD_REPLACE_ARB;
using gl::GL_COORD_REPLACE_NV;
using gl::GL_POINT_SPRITE_R_MODE_NV;
using gl::GL_PIXEL_COUNTER_BITS_NV;
using gl::GL_QUERY_COUNTER_BITS;
using gl::GL_QUERY_COUNTER_BITS_ARB;
using gl::GL_CURRENT_OCCLUSION_QUERY_ID_NV;
using gl::GL_CURRENT_QUERY;
using gl::GL_CURRENT_QUERY_ARB;
using gl::GL_PIXEL_COUNT_NV;
using gl::GL_QUERY_RESULT;
using gl::GL_QUERY_RESULT_ARB;
using gl::GL_PIXEL_COUNT_AVAILABLE_NV;
using gl::GL_QUERY_RESULT_AVAILABLE;
using gl::GL_QUERY_RESULT_AVAILABLE_ARB;
using gl::GL_MAX_FRAGMENT_PROGRAM_LOCAL_PARAMETERS_NV;
using gl::GL_MAX_VERTEX_ATTRIBS;
using gl::GL_MAX_VERTEX_ATTRIBS_ARB;
using gl::GL_VERTEX_ATTRIB_ARRAY_NORMALIZED;
using gl::GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB;
using gl::GL_MAX_TESS_CONTROL_INPUT_COMPONENTS;
using gl::GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS;
using gl::GL_DEPTH_STENCIL_TO_RGBA_NV;
using gl::GL_DEPTH_STENCIL_TO_BGRA_NV;
using gl::GL_FRAGMENT_PROGRAM_NV;
using gl::GL_MAX_TEXTURE_COORDS;
using gl::GL_MAX_TEXTURE_COORDS_ARB;
using gl::GL_MAX_TEXTURE_COORDS_NV;
using gl::GL_MAX_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_TEXTURE_IMAGE_UNITS_ARB;
using gl::GL_MAX_TEXTURE_IMAGE_UNITS_NV;
using gl::GL_FRAGMENT_PROGRAM_BINDING_NV;
using gl::GL_PROGRAM_ERROR_STRING_ARB;
using gl::GL_PROGRAM_ERROR_STRING_NV;
using gl::GL_PROGRAM_FORMAT_ASCII_ARB;
using gl::GL_PROGRAM_FORMAT_ARB;
using gl::GL_WRITE_PIXEL_DATA_RANGE_NV;
using gl::GL_READ_PIXEL_DATA_RANGE_NV;
using gl::GL_WRITE_PIXEL_DATA_RANGE_LENGTH_NV;
using gl::GL_READ_PIXEL_DATA_RANGE_LENGTH_NV;
using gl::GL_WRITE_PIXEL_DATA_RANGE_POINTER_NV;
using gl::GL_READ_PIXEL_DATA_RANGE_POINTER_NV;
using gl::GL_GEOMETRY_SHADER_INVOCATIONS;
using gl::GL_FLOAT_R_NV;
using gl::GL_FLOAT_RG_NV;
using gl::GL_FLOAT_RGB_NV;
using gl::GL_FLOAT_RGBA_NV;
using gl::GL_FLOAT_R16_NV;
using gl::GL_FLOAT_R32_NV;
using gl::GL_FLOAT_RG16_NV;
using gl::GL_FLOAT_RG32_NV;
using gl::GL_FLOAT_RGB16_NV;
using gl::GL_FLOAT_RGB32_NV;
using gl::GL_FLOAT_RGBA16_NV;
using gl::GL_FLOAT_RGBA32_NV;
using gl::GL_TEXTURE_FLOAT_COMPONENTS_NV;
using gl::GL_FLOAT_CLEAR_COLOR_VALUE_NV;
using gl::GL_FLOAT_RGBA_MODE_NV;
using gl::GL_TEXTURE_UNSIGNED_REMAP_MODE_NV;
using gl::GL_DEPTH_BOUNDS_TEST_EXT;
using gl::GL_DEPTH_BOUNDS_EXT;
using gl::GL_ARRAY_BUFFER;
using gl::GL_ARRAY_BUFFER_ARB;
using gl::GL_ELEMENT_ARRAY_BUFFER;
using gl::GL_ELEMENT_ARRAY_BUFFER_ARB;
using gl::GL_ARRAY_BUFFER_BINDING;
using gl::GL_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_ELEMENT_ARRAY_BUFFER_BINDING;
using gl::GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_VERTEX_ARRAY_BUFFER_BINDING;
using gl::GL_VERTEX_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_NORMAL_ARRAY_BUFFER_BINDING;
using gl::GL_NORMAL_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_COLOR_ARRAY_BUFFER_BINDING;
using gl::GL_COLOR_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_INDEX_ARRAY_BUFFER_BINDING;
using gl::GL_INDEX_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING;
using gl::GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_EDGE_FLAG_ARRAY_BUFFER_BINDING;
using gl::GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING;
using gl::GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING;
using gl::GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_FOG_COORD_ARRAY_BUFFER_BINDING;
using gl::GL_WEIGHT_ARRAY_BUFFER_BINDING;
using gl::GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING;
using gl::GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB;
using gl::GL_PROGRAM_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB;
using gl::GL_PROGRAM_TEMPORARIES_ARB;
using gl::GL_MAX_PROGRAM_TEMPORARIES_ARB;
using gl::GL_PROGRAM_NATIVE_TEMPORARIES_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB;
using gl::GL_PROGRAM_PARAMETERS_ARB;
using gl::GL_MAX_PROGRAM_PARAMETERS_ARB;
using gl::GL_PROGRAM_NATIVE_PARAMETERS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB;
using gl::GL_PROGRAM_ATTRIBS_ARB;
using gl::GL_MAX_PROGRAM_ATTRIBS_ARB;
using gl::GL_PROGRAM_NATIVE_ATTRIBS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB;
using gl::GL_PROGRAM_ADDRESS_REGISTERS_ARB;
using gl::GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB;
using gl::GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB;
using gl::GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB;
using gl::GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB;
using gl::GL_MAX_PROGRAM_ENV_PARAMETERS_ARB;
using gl::GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB;
using gl::GL_TRANSPOSE_CURRENT_MATRIX_ARB;
using gl::GL_READ_ONLY;
using gl::GL_READ_ONLY_ARB;
using gl::GL_WRITE_ONLY;
using gl::GL_WRITE_ONLY_ARB;
using gl::GL_READ_WRITE;
using gl::GL_READ_WRITE_ARB;
using gl::GL_BUFFER_ACCESS;
using gl::GL_BUFFER_ACCESS_ARB;
using gl::GL_BUFFER_MAPPED;
using gl::GL_BUFFER_MAPPED_ARB;
using gl::GL_BUFFER_MAP_POINTER;
using gl::GL_BUFFER_MAP_POINTER_ARB;
using gl::GL_WRITE_DISCARD_NV;
using gl::GL_TIME_ELAPSED;
using gl::GL_TIME_ELAPSED_EXT;
using gl::GL_MATRIX0_ARB;
using gl::GL_MATRIX1_ARB;
using gl::GL_MATRIX2_ARB;
using gl::GL_MATRIX3_ARB;
using gl::GL_MATRIX4_ARB;
using gl::GL_MATRIX5_ARB;
using gl::GL_MATRIX6_ARB;
using gl::GL_MATRIX7_ARB;
using gl::GL_MATRIX8_ARB;
using gl::GL_MATRIX9_ARB;
using gl::GL_MATRIX10_ARB;
using gl::GL_MATRIX11_ARB;
using gl::GL_MATRIX12_ARB;
using gl::GL_MATRIX13_ARB;
using gl::GL_MATRIX14_ARB;
using gl::GL_MATRIX15_ARB;
using gl::GL_MATRIX16_ARB;
using gl::GL_MATRIX17_ARB;
using gl::GL_MATRIX18_ARB;
using gl::GL_MATRIX19_ARB;
using gl::GL_MATRIX20_ARB;
using gl::GL_MATRIX21_ARB;
using gl::GL_MATRIX22_ARB;
using gl::GL_MATRIX23_ARB;
using gl::GL_MATRIX24_ARB;
using gl::GL_MATRIX25_ARB;
using gl::GL_MATRIX26_ARB;
using gl::GL_MATRIX27_ARB;
using gl::GL_MATRIX28_ARB;
using gl::GL_MATRIX29_ARB;
using gl::GL_MATRIX30_ARB;
using gl::GL_MATRIX31_ARB;
using gl::GL_STREAM_DRAW;
using gl::GL_STREAM_DRAW_ARB;
using gl::GL_STREAM_READ;
using gl::GL_STREAM_READ_ARB;
using gl::GL_STREAM_COPY;
using gl::GL_STREAM_COPY_ARB;
using gl::GL_STATIC_DRAW;
using gl::GL_STATIC_DRAW_ARB;
using gl::GL_STATIC_READ;
using gl::GL_STATIC_READ_ARB;
using gl::GL_STATIC_COPY;
using gl::GL_STATIC_COPY_ARB;
using gl::GL_DYNAMIC_DRAW;
using gl::GL_DYNAMIC_DRAW_ARB;
using gl::GL_DYNAMIC_READ;
using gl::GL_DYNAMIC_READ_ARB;
using gl::GL_DYNAMIC_COPY;
using gl::GL_DYNAMIC_COPY_ARB;
using gl::GL_PIXEL_PACK_BUFFER;
using gl::GL_PIXEL_PACK_BUFFER_ARB;
using gl::GL_PIXEL_PACK_BUFFER_EXT;
using gl::GL_PIXEL_UNPACK_BUFFER;
using gl::GL_PIXEL_UNPACK_BUFFER_ARB;
using gl::GL_PIXEL_UNPACK_BUFFER_EXT;
using gl::GL_PIXEL_PACK_BUFFER_BINDING;
using gl::GL_PIXEL_PACK_BUFFER_BINDING_ARB;
using gl::GL_PIXEL_PACK_BUFFER_BINDING_EXT;
using gl::GL_PIXEL_UNPACK_BUFFER_BINDING;
using gl::GL_PIXEL_UNPACK_BUFFER_BINDING_ARB;
using gl::GL_PIXEL_UNPACK_BUFFER_BINDING_EXT;
using gl::GL_DEPTH24_STENCIL8;
using gl::GL_DEPTH24_STENCIL8_EXT;
using gl::GL_TEXTURE_STENCIL_SIZE;
using gl::GL_TEXTURE_STENCIL_SIZE_EXT;
using gl::GL_STENCIL_TAG_BITS_EXT;
using gl::GL_STENCIL_CLEAR_TAG_VALUE_EXT;
using gl::GL_MAX_PROGRAM_EXEC_INSTRUCTIONS_NV;
using gl::GL_MAX_PROGRAM_CALL_DEPTH_NV;
using gl::GL_MAX_PROGRAM_IF_DEPTH_NV;
using gl::GL_MAX_PROGRAM_LOOP_DEPTH_NV;
using gl::GL_MAX_PROGRAM_LOOP_COUNT_NV;
using gl::GL_SRC1_COLOR;
using gl::GL_ONE_MINUS_SRC1_COLOR;
using gl::GL_ONE_MINUS_SRC1_ALPHA;
using gl::GL_MAX_DUAL_SOURCE_DRAW_BUFFERS;
using gl::GL_VERTEX_ATTRIB_ARRAY_INTEGER;
using gl::GL_VERTEX_ATTRIB_ARRAY_INTEGER_EXT;
using gl::GL_VERTEX_ATTRIB_ARRAY_INTEGER_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_DIVISOR;
using gl::GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB;
using gl::GL_MAX_ARRAY_TEXTURE_LAYERS;
using gl::GL_MAX_ARRAY_TEXTURE_LAYERS_EXT;
using gl::GL_MIN_PROGRAM_TEXEL_OFFSET;
using gl::GL_MIN_PROGRAM_TEXEL_OFFSET_EXT;
using gl::GL_MIN_PROGRAM_TEXEL_OFFSET_NV;
using gl::GL_MAX_PROGRAM_TEXEL_OFFSET;
using gl::GL_MAX_PROGRAM_TEXEL_OFFSET_EXT;
using gl::GL_MAX_PROGRAM_TEXEL_OFFSET_NV;
using gl::GL_PROGRAM_ATTRIB_COMPONENTS_NV;
using gl::GL_PROGRAM_RESULT_COMPONENTS_NV;
using gl::GL_MAX_PROGRAM_ATTRIB_COMPONENTS_NV;
using gl::GL_MAX_PROGRAM_RESULT_COMPONENTS_NV;
using gl::GL_STENCIL_TEST_TWO_SIDE_EXT;
using gl::GL_ACTIVE_STENCIL_FACE_EXT;
using gl::GL_MIRROR_CLAMP_TO_BORDER_EXT;
using gl::GL_SAMPLES_PASSED;
using gl::GL_SAMPLES_PASSED_ARB;
using gl::GL_GEOMETRY_VERTICES_OUT;
using gl::GL_GEOMETRY_INPUT_TYPE;
using gl::GL_GEOMETRY_OUTPUT_TYPE;
using gl::GL_SAMPLER_BINDING;
using gl::GL_CLAMP_VERTEX_COLOR;
using gl::GL_CLAMP_VERTEX_COLOR_ARB;
using gl::GL_CLAMP_FRAGMENT_COLOR;
using gl::GL_CLAMP_FRAGMENT_COLOR_ARB;
using gl::GL_CLAMP_READ_COLOR;
using gl::GL_CLAMP_READ_COLOR_ARB;
using gl::GL_FIXED_ONLY;
using gl::GL_FIXED_ONLY_ARB;
using gl::GL_TESS_CONTROL_PROGRAM_NV;
using gl::GL_TESS_EVALUATION_PROGRAM_NV;
using gl::GL_FRAGMENT_SHADER_ATI;
using gl::GL_REG_0_ATI;
using gl::GL_REG_1_ATI;
using gl::GL_REG_2_ATI;
using gl::GL_REG_3_ATI;
using gl::GL_REG_4_ATI;
using gl::GL_REG_5_ATI;
using gl::GL_REG_6_ATI;
using gl::GL_REG_7_ATI;
using gl::GL_REG_8_ATI;
using gl::GL_REG_9_ATI;
using gl::GL_REG_10_ATI;
using gl::GL_REG_11_ATI;
using gl::GL_REG_12_ATI;
using gl::GL_REG_13_ATI;
using gl::GL_REG_14_ATI;
using gl::GL_REG_15_ATI;
using gl::GL_REG_16_ATI;
using gl::GL_REG_17_ATI;
using gl::GL_REG_18_ATI;
using gl::GL_REG_19_ATI;
using gl::GL_REG_20_ATI;
using gl::GL_REG_21_ATI;
using gl::GL_REG_22_ATI;
using gl::GL_REG_23_ATI;
using gl::GL_REG_24_ATI;
using gl::GL_REG_25_ATI;
using gl::GL_REG_26_ATI;
using gl::GL_REG_27_ATI;
using gl::GL_REG_28_ATI;
using gl::GL_REG_29_ATI;
using gl::GL_REG_30_ATI;
using gl::GL_REG_31_ATI;
using gl::GL_CON_0_ATI;
using gl::GL_CON_1_ATI;
using gl::GL_CON_2_ATI;
using gl::GL_CON_3_ATI;
using gl::GL_CON_4_ATI;
using gl::GL_CON_5_ATI;
using gl::GL_CON_6_ATI;
using gl::GL_CON_7_ATI;
using gl::GL_CON_8_ATI;
using gl::GL_CON_9_ATI;
using gl::GL_CON_10_ATI;
using gl::GL_CON_11_ATI;
using gl::GL_CON_12_ATI;
using gl::GL_CON_13_ATI;
using gl::GL_CON_14_ATI;
using gl::GL_CON_15_ATI;
using gl::GL_CON_16_ATI;
using gl::GL_CON_17_ATI;
using gl::GL_CON_18_ATI;
using gl::GL_CON_19_ATI;
using gl::GL_CON_20_ATI;
using gl::GL_CON_21_ATI;
using gl::GL_CON_22_ATI;
using gl::GL_CON_23_ATI;
using gl::GL_CON_24_ATI;
using gl::GL_CON_25_ATI;
using gl::GL_CON_26_ATI;
using gl::GL_CON_27_ATI;
using gl::GL_CON_28_ATI;
using gl::GL_CON_29_ATI;
using gl::GL_CON_30_ATI;
using gl::GL_CON_31_ATI;
using gl::GL_MOV_ATI;
using gl::GL_ADD_ATI;
using gl::GL_MUL_ATI;
using gl::GL_SUB_ATI;
using gl::GL_DOT3_ATI;
using gl::GL_DOT4_ATI;
using gl::GL_MAD_ATI;
using gl::GL_LERP_ATI;
using gl::GL_CND_ATI;
using gl::GL_CND0_ATI;
using gl::GL_DOT2_ADD_ATI;
using gl::GL_SECONDARY_INTERPOLATOR_ATI;
using gl::GL_NUM_FRAGMENT_REGISTERS_ATI;
using gl::GL_NUM_FRAGMENT_CONSTANTS_ATI;
using gl::GL_NUM_PASSES_ATI;
using gl::GL_NUM_INSTRUCTIONS_PER_PASS_ATI;
using gl::GL_NUM_INSTRUCTIONS_TOTAL_ATI;
using gl::GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI;
using gl::GL_NUM_LOOPBACK_COMPONENTS_ATI;
using gl::GL_COLOR_ALPHA_PAIRING_ATI;
using gl::GL_SWIZZLE_STR_ATI;
using gl::GL_SWIZZLE_STQ_ATI;
using gl::GL_SWIZZLE_STR_DR_ATI;
using gl::GL_SWIZZLE_STQ_DQ_ATI;
using gl::GL_SWIZZLE_STRQ_ATI;
using gl::GL_SWIZZLE_STRQ_DQ_ATI;
using gl::GL_INTERLACE_OML;
using gl::GL_INTERLACE_READ_OML;
using gl::GL_FORMAT_SUBSAMPLE_24_24_OML;
using gl::GL_FORMAT_SUBSAMPLE_244_244_OML;
using gl::GL_RESAMPLE_REPLICATE_OML;
using gl::GL_RESAMPLE_ZERO_FILL_OML;
using gl::GL_RESAMPLE_AVERAGE_OML;
using gl::GL_RESAMPLE_DECIMATE_OML;
using gl::GL_VERTEX_ATTRIB_MAP1_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP2_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP1_SIZE_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP1_COEFF_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP1_ORDER_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP1_DOMAIN_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP2_SIZE_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP2_COEFF_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP2_ORDER_APPLE;
using gl::GL_VERTEX_ATTRIB_MAP2_DOMAIN_APPLE;
using gl::GL_DRAW_PIXELS_APPLE;
using gl::GL_FENCE_APPLE;
using gl::GL_ELEMENT_ARRAY_APPLE;
using gl::GL_ELEMENT_ARRAY_TYPE_APPLE;
using gl::GL_ELEMENT_ARRAY_POINTER_APPLE;
using gl::GL_COLOR_FLOAT_APPLE;
using gl::GL_UNIFORM_BUFFER;
using gl::GL_BUFFER_SERIALIZED_MODIFY_APPLE;
using gl::GL_BUFFER_FLUSHING_UNMAP_APPLE;
using gl::GL_AUX_DEPTH_STENCIL_APPLE;
using gl::GL_PACK_ROW_BYTES_APPLE;
using gl::GL_UNPACK_ROW_BYTES_APPLE;
using gl::GL_RELEASED_APPLE;
using gl::GL_VOLATILE_APPLE;
using gl::GL_RETAINED_APPLE;
using gl::GL_UNDEFINED_APPLE;
using gl::GL_PURGEABLE_APPLE;
using gl::GL_RGB_422_APPLE;
using gl::GL_UNIFORM_BUFFER_BINDING;
using gl::GL_UNIFORM_BUFFER_START;
using gl::GL_UNIFORM_BUFFER_SIZE;
using gl::GL_MAX_VERTEX_UNIFORM_BLOCKS;
using gl::GL_MAX_GEOMETRY_UNIFORM_BLOCKS;
using gl::GL_MAX_FRAGMENT_UNIFORM_BLOCKS;
using gl::GL_MAX_COMBINED_UNIFORM_BLOCKS;
using gl::GL_MAX_UNIFORM_BUFFER_BINDINGS;
using gl::GL_MAX_UNIFORM_BLOCK_SIZE;
using gl::GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS;
using gl::GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS;
using gl::GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS;
using gl::GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT;
using gl::GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH;
using gl::GL_ACTIVE_UNIFORM_BLOCKS;
using gl::GL_UNIFORM_TYPE;
using gl::GL_UNIFORM_SIZE;
using gl::GL_UNIFORM_NAME_LENGTH;
using gl::GL_UNIFORM_BLOCK_INDEX;
using gl::GL_UNIFORM_OFFSET;
using gl::GL_UNIFORM_ARRAY_STRIDE;
using gl::GL_UNIFORM_MATRIX_STRIDE;
using gl::GL_UNIFORM_IS_ROW_MAJOR;
using gl::GL_UNIFORM_BLOCK_BINDING;
using gl::GL_UNIFORM_BLOCK_DATA_SIZE;
using gl::GL_UNIFORM_BLOCK_NAME_LENGTH;
using gl::GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS;
using gl::GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER;
using gl::GL_TEXTURE_SRGB_DECODE_EXT;
using gl::GL_DECODE_EXT;
using gl::GL_SKIP_DECODE_EXT;
using gl::GL_PROGRAM_PIPELINE_OBJECT_EXT;
using gl::GL_RGB_RAW_422_APPLE;
using gl::GL_FRAGMENT_SHADER;
using gl::GL_FRAGMENT_SHADER_ARB;
using gl::GL_VERTEX_SHADER;
using gl::GL_VERTEX_SHADER_ARB;
using gl::GL_PROGRAM_OBJECT_ARB;
using gl::GL_PROGRAM_OBJECT_EXT;
using gl::GL_SHADER_OBJECT_ARB;
using gl::GL_SHADER_OBJECT_EXT;
using gl::GL_MAX_FRAGMENT_UNIFORM_COMPONENTS;
using gl::GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB;
using gl::GL_MAX_VERTEX_UNIFORM_COMPONENTS;
using gl::GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB;
using gl::GL_MAX_VARYING_COMPONENTS;
using gl::GL_MAX_VARYING_COMPONENTS_EXT;
using gl::GL_MAX_VARYING_FLOATS;
using gl::GL_MAX_VARYING_FLOATS_ARB;
using gl::GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB;
using gl::GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB;
using gl::GL_OBJECT_TYPE_ARB;
using gl::GL_OBJECT_SUBTYPE_ARB;
using gl::GL_SHADER_TYPE;
using gl::GL_FLOAT_VEC2;
using gl::GL_FLOAT_VEC2_ARB;
using gl::GL_FLOAT_VEC3;
using gl::GL_FLOAT_VEC3_ARB;
using gl::GL_FLOAT_VEC4;
using gl::GL_FLOAT_VEC4_ARB;
using gl::GL_INT_VEC2;
using gl::GL_INT_VEC2_ARB;
using gl::GL_INT_VEC3;
using gl::GL_INT_VEC3_ARB;
using gl::GL_INT_VEC4;
using gl::GL_INT_VEC4_ARB;
using gl::GL_BOOL;
using gl::GL_BOOL_ARB;
using gl::GL_BOOL_VEC2;
using gl::GL_BOOL_VEC2_ARB;
using gl::GL_BOOL_VEC3;
using gl::GL_BOOL_VEC3_ARB;
using gl::GL_BOOL_VEC4;
using gl::GL_BOOL_VEC4_ARB;
using gl::GL_FLOAT_MAT2;
using gl::GL_FLOAT_MAT2_ARB;
using gl::GL_FLOAT_MAT3;
using gl::GL_FLOAT_MAT3_ARB;
using gl::GL_FLOAT_MAT4;
using gl::GL_FLOAT_MAT4_ARB;
using gl::GL_SAMPLER_1D;
using gl::GL_SAMPLER_1D_ARB;
using gl::GL_SAMPLER_2D;
using gl::GL_SAMPLER_2D_ARB;
using gl::GL_SAMPLER_3D;
using gl::GL_SAMPLER_3D_ARB;
using gl::GL_SAMPLER_CUBE;
using gl::GL_SAMPLER_CUBE_ARB;
using gl::GL_SAMPLER_1D_SHADOW;
using gl::GL_SAMPLER_1D_SHADOW_ARB;
using gl::GL_SAMPLER_2D_SHADOW;
using gl::GL_SAMPLER_2D_SHADOW_ARB;
using gl::GL_SAMPLER_2D_RECT;
using gl::GL_SAMPLER_2D_RECT_ARB;
using gl::GL_SAMPLER_2D_RECT_SHADOW;
using gl::GL_SAMPLER_2D_RECT_SHADOW_ARB;
using gl::GL_FLOAT_MAT2x3;
using gl::GL_FLOAT_MAT2x4;
using gl::GL_FLOAT_MAT3x2;
using gl::GL_FLOAT_MAT3x4;
using gl::GL_FLOAT_MAT4x2;
using gl::GL_FLOAT_MAT4x3;
using gl::GL_DELETE_STATUS;
using gl::GL_OBJECT_DELETE_STATUS_ARB;
using gl::GL_COMPILE_STATUS;
using gl::GL_OBJECT_COMPILE_STATUS_ARB;
using gl::GL_LINK_STATUS;
using gl::GL_OBJECT_LINK_STATUS_ARB;
using gl::GL_OBJECT_VALIDATE_STATUS_ARB;
using gl::GL_VALIDATE_STATUS;
using gl::GL_INFO_LOG_LENGTH;
using gl::GL_OBJECT_INFO_LOG_LENGTH_ARB;
using gl::GL_ATTACHED_SHADERS;
using gl::GL_OBJECT_ATTACHED_OBJECTS_ARB;
using gl::GL_ACTIVE_UNIFORMS;
using gl::GL_OBJECT_ACTIVE_UNIFORMS_ARB;
using gl::GL_ACTIVE_UNIFORM_MAX_LENGTH;
using gl::GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB;
using gl::GL_OBJECT_SHADER_SOURCE_LENGTH_ARB;
using gl::GL_SHADER_SOURCE_LENGTH;
using gl::GL_ACTIVE_ATTRIBUTES;
using gl::GL_OBJECT_ACTIVE_ATTRIBUTES_ARB;
using gl::GL_ACTIVE_ATTRIBUTE_MAX_LENGTH;
using gl::GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB;
using gl::GL_SHADING_LANGUAGE_VERSION;
using gl::GL_SHADING_LANGUAGE_VERSION_ARB;
using gl::GL_ACTIVE_PROGRAM_EXT;
using gl::GL_CURRENT_PROGRAM;
using gl::GL_PALETTE4_RGB8_OES;
using gl::GL_PALETTE4_RGBA8_OES;
using gl::GL_PALETTE4_R5_G6_B5_OES;
using gl::GL_PALETTE4_RGBA4_OES;
using gl::GL_PALETTE4_RGB5_A1_OES;
using gl::GL_PALETTE8_RGB8_OES;
using gl::GL_PALETTE8_RGBA8_OES;
using gl::GL_PALETTE8_R5_G6_B5_OES;
using gl::GL_PALETTE8_RGBA4_OES;
using gl::GL_PALETTE8_RGB5_A1_OES;
using gl::GL_IMPLEMENTATION_COLOR_READ_TYPE;
using gl::GL_IMPLEMENTATION_COLOR_READ_TYPE_OES;
using gl::GL_IMPLEMENTATION_COLOR_READ_FORMAT;
using gl::GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES;
using gl::GL_COUNTER_TYPE_AMD;
using gl::GL_COUNTER_RANGE_AMD;
using gl::GL_UNSIGNED_INT64_AMD;
using gl::GL_PERCENTAGE_AMD;
using gl::GL_PERFMON_RESULT_AVAILABLE_AMD;
using gl::GL_PERFMON_RESULT_SIZE_AMD;
using gl::GL_PERFMON_RESULT_AMD;
using gl::GL_TEXTURE_RED_TYPE;
using gl::GL_TEXTURE_RED_TYPE_ARB;
using gl::GL_TEXTURE_GREEN_TYPE;
using gl::GL_TEXTURE_GREEN_TYPE_ARB;
using gl::GL_TEXTURE_BLUE_TYPE;
using gl::GL_TEXTURE_BLUE_TYPE_ARB;
using gl::GL_TEXTURE_ALPHA_TYPE;
using gl::GL_TEXTURE_ALPHA_TYPE_ARB;
using gl::GL_TEXTURE_LUMINANCE_TYPE;
using gl::GL_TEXTURE_LUMINANCE_TYPE_ARB;
using gl::GL_TEXTURE_INTENSITY_TYPE;
using gl::GL_TEXTURE_INTENSITY_TYPE_ARB;
using gl::GL_TEXTURE_DEPTH_TYPE;
using gl::GL_TEXTURE_DEPTH_TYPE_ARB;
using gl::GL_UNSIGNED_NORMALIZED;
using gl::GL_UNSIGNED_NORMALIZED_ARB;
using gl::GL_TEXTURE_1D_ARRAY;
using gl::GL_TEXTURE_1D_ARRAY_EXT;
using gl::GL_PROXY_TEXTURE_1D_ARRAY;
using gl::GL_PROXY_TEXTURE_1D_ARRAY_EXT;
using gl::GL_TEXTURE_2D_ARRAY;
using gl::GL_TEXTURE_2D_ARRAY_EXT;
using gl::GL_PROXY_TEXTURE_2D_ARRAY;
using gl::GL_PROXY_TEXTURE_2D_ARRAY_EXT;
using gl::GL_TEXTURE_BINDING_1D_ARRAY;
using gl::GL_TEXTURE_BINDING_1D_ARRAY_EXT;
using gl::GL_TEXTURE_BINDING_2D_ARRAY;
using gl::GL_TEXTURE_BINDING_2D_ARRAY_EXT;
using gl::GL_GEOMETRY_PROGRAM_NV;
using gl::GL_MAX_PROGRAM_OUTPUT_VERTICES_NV;
using gl::GL_MAX_PROGRAM_TOTAL_OUTPUT_COMPONENTS_NV;
using gl::GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_ARB;
using gl::GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT;
using gl::GL_TEXTURE_BUFFER;
using gl::GL_TEXTURE_BUFFER_ARB;
using gl::GL_TEXTURE_BUFFER_BINDING;
using gl::GL_TEXTURE_BUFFER_EXT;
using gl::GL_MAX_TEXTURE_BUFFER_SIZE;
using gl::GL_MAX_TEXTURE_BUFFER_SIZE_ARB;
using gl::GL_MAX_TEXTURE_BUFFER_SIZE_EXT;
using gl::GL_TEXTURE_BINDING_BUFFER;
using gl::GL_TEXTURE_BINDING_BUFFER_ARB;
using gl::GL_TEXTURE_BINDING_BUFFER_EXT;
using gl::GL_TEXTURE_BUFFER_DATA_STORE_BINDING;
using gl::GL_TEXTURE_BUFFER_DATA_STORE_BINDING_ARB;
using gl::GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT;
using gl::GL_TEXTURE_BUFFER_FORMAT_ARB;
using gl::GL_TEXTURE_BUFFER_FORMAT_EXT;
using gl::GL_ANY_SAMPLES_PASSED;
using gl::GL_SAMPLE_SHADING;
using gl::GL_SAMPLE_SHADING_ARB;
using gl::GL_MIN_SAMPLE_SHADING_VALUE;
using gl::GL_MIN_SAMPLE_SHADING_VALUE_ARB;
using gl::GL_R11F_G11F_B10F;
using gl::GL_R11F_G11F_B10F_EXT;
using gl::GL_UNSIGNED_INT_10F_11F_11F_REV;
using gl::GL_UNSIGNED_INT_10F_11F_11F_REV_EXT;
using gl::GL_RGBA_SIGNED_COMPONENTS_EXT;
using gl::GL_RGB9_E5;
using gl::GL_RGB9_E5_EXT;
using gl::GL_UNSIGNED_INT_5_9_9_9_REV;
using gl::GL_UNSIGNED_INT_5_9_9_9_REV_EXT;
using gl::GL_TEXTURE_SHARED_SIZE;
using gl::GL_TEXTURE_SHARED_SIZE_EXT;
using gl::GL_SRGB;
using gl::GL_SRGB_EXT;
using gl::GL_SRGB8;
using gl::GL_SRGB8_EXT;
using gl::GL_SRGB_ALPHA;
using gl::GL_SRGB_ALPHA_EXT;
using gl::GL_SRGB8_ALPHA8;
using gl::GL_SRGB8_ALPHA8_EXT;
using gl::GL_SLUMINANCE_ALPHA;
using gl::GL_SLUMINANCE_ALPHA_EXT;
using gl::GL_SLUMINANCE8_ALPHA8;
using gl::GL_SLUMINANCE8_ALPHA8_EXT;
using gl::GL_SLUMINANCE;
using gl::GL_SLUMINANCE_EXT;
using gl::GL_SLUMINANCE8;
using gl::GL_SLUMINANCE8_EXT;
using gl::GL_COMPRESSED_SRGB;
using gl::GL_COMPRESSED_SRGB_EXT;
using gl::GL_COMPRESSED_SRGB_ALPHA;
using gl::GL_COMPRESSED_SRGB_ALPHA_EXT;
using gl::GL_COMPRESSED_SLUMINANCE;
using gl::GL_COMPRESSED_SLUMINANCE_EXT;
using gl::GL_COMPRESSED_SLUMINANCE_ALPHA;
using gl::GL_COMPRESSED_SLUMINANCE_ALPHA_EXT;
using gl::GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
using gl::GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
using gl::GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
using gl::GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
using gl::GL_COMPRESSED_LUMINANCE_LATC1_EXT;
using gl::GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT;
using gl::GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
using gl::GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT;
using gl::GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH;
using gl::GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH_EXT;
using gl::GL_BACK_PRIMARY_COLOR_NV;
using gl::GL_BACK_SECONDARY_COLOR_NV;
using gl::GL_TEXTURE_COORD_NV;
using gl::GL_CLIP_DISTANCE_NV;
using gl::GL_VERTEX_ID_NV;
using gl::GL_PRIMITIVE_ID_NV;
using gl::GL_GENERIC_ATTRIB_NV;
using gl::GL_TRANSFORM_FEEDBACK_ATTRIBS_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_MODE;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_MODE_EXT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_MODE_NV;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_EXT;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_NV;
using gl::GL_ACTIVE_VARYINGS_NV;
using gl::GL_ACTIVE_VARYING_MAX_LENGTH_NV;
using gl::GL_TRANSFORM_FEEDBACK_VARYINGS;
using gl::GL_TRANSFORM_FEEDBACK_VARYINGS_EXT;
using gl::GL_TRANSFORM_FEEDBACK_VARYINGS_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_START;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_START_EXT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_START_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_SIZE;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_EXT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_NV;
using gl::GL_TRANSFORM_FEEDBACK_RECORD_NV;
using gl::GL_PRIMITIVES_GENERATED;
using gl::GL_PRIMITIVES_GENERATED_EXT;
using gl::GL_PRIMITIVES_GENERATED_NV;
using gl::GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
using gl::GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_EXT;
using gl::GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_NV;
using gl::GL_RASTERIZER_DISCARD;
using gl::GL_RASTERIZER_DISCARD_EXT;
using gl::GL_RASTERIZER_DISCARD_NV;
using gl::GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS;
using gl::GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_EXT;
using gl::GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_NV;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_EXT;
using gl::GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_NV;
using gl::GL_INTERLEAVED_ATTRIBS;
using gl::GL_INTERLEAVED_ATTRIBS_EXT;
using gl::GL_INTERLEAVED_ATTRIBS_NV;
using gl::GL_SEPARATE_ATTRIBS;
using gl::GL_SEPARATE_ATTRIBS_EXT;
using gl::GL_SEPARATE_ATTRIBS_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_EXT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_EXT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV;
using gl::GL_POINT_SPRITE_COORD_ORIGIN;
using gl::GL_LOWER_LEFT;
using gl::GL_UPPER_LEFT;
using gl::GL_STENCIL_BACK_REF;
using gl::GL_STENCIL_BACK_VALUE_MASK;
using gl::GL_STENCIL_BACK_WRITEMASK;
using gl::GL_DRAW_FRAMEBUFFER_BINDING;
using gl::GL_DRAW_FRAMEBUFFER_BINDING_EXT;
using gl::GL_FRAMEBUFFER_BINDING;
using gl::GL_FRAMEBUFFER_BINDING_EXT;
using gl::GL_RENDERBUFFER_BINDING;
using gl::GL_RENDERBUFFER_BINDING_EXT;
using gl::GL_READ_FRAMEBUFFER;
using gl::GL_READ_FRAMEBUFFER_EXT;
using gl::GL_DRAW_FRAMEBUFFER;
using gl::GL_DRAW_FRAMEBUFFER_EXT;
using gl::GL_READ_FRAMEBUFFER_BINDING;
using gl::GL_READ_FRAMEBUFFER_BINDING_EXT;
using gl::GL_RENDERBUFFER_COVERAGE_SAMPLES_NV;
using gl::GL_RENDERBUFFER_SAMPLES;
using gl::GL_RENDERBUFFER_SAMPLES_EXT;
using gl::GL_DEPTH_COMPONENT32F;
using gl::GL_DEPTH32F_STENCIL8;
using gl::GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT;
using gl::GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME;
using gl::GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_EXT;
using gl::GL_FRAMEBUFFER_COMPLETE;
using gl::GL_FRAMEBUFFER_COMPLETE_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER;
using gl::GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER;
using gl::GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT;
using gl::GL_FRAMEBUFFER_UNSUPPORTED;
using gl::GL_FRAMEBUFFER_UNSUPPORTED_EXT;
using gl::GL_MAX_COLOR_ATTACHMENTS;
using gl::GL_MAX_COLOR_ATTACHMENTS_EXT;
using gl::GL_COLOR_ATTACHMENT0;
using gl::GL_COLOR_ATTACHMENT0_EXT;
using gl::GL_COLOR_ATTACHMENT1;
using gl::GL_COLOR_ATTACHMENT1_EXT;
using gl::GL_COLOR_ATTACHMENT2;
using gl::GL_COLOR_ATTACHMENT2_EXT;
using gl::GL_COLOR_ATTACHMENT3;
using gl::GL_COLOR_ATTACHMENT3_EXT;
using gl::GL_COLOR_ATTACHMENT4;
using gl::GL_COLOR_ATTACHMENT4_EXT;
using gl::GL_COLOR_ATTACHMENT5;
using gl::GL_COLOR_ATTACHMENT5_EXT;
using gl::GL_COLOR_ATTACHMENT6;
using gl::GL_COLOR_ATTACHMENT6_EXT;
using gl::GL_COLOR_ATTACHMENT7;
using gl::GL_COLOR_ATTACHMENT7_EXT;
using gl::GL_COLOR_ATTACHMENT8;
using gl::GL_COLOR_ATTACHMENT8_EXT;
using gl::GL_COLOR_ATTACHMENT9;
using gl::GL_COLOR_ATTACHMENT9_EXT;
using gl::GL_COLOR_ATTACHMENT10;
using gl::GL_COLOR_ATTACHMENT10_EXT;
using gl::GL_COLOR_ATTACHMENT11;
using gl::GL_COLOR_ATTACHMENT11_EXT;
using gl::GL_COLOR_ATTACHMENT12;
using gl::GL_COLOR_ATTACHMENT12_EXT;
using gl::GL_COLOR_ATTACHMENT13;
using gl::GL_COLOR_ATTACHMENT13_EXT;
using gl::GL_COLOR_ATTACHMENT14;
using gl::GL_COLOR_ATTACHMENT14_EXT;
using gl::GL_COLOR_ATTACHMENT15;
using gl::GL_COLOR_ATTACHMENT15_EXT;
using gl::GL_COLOR_ATTACHMENT16;
using gl::GL_COLOR_ATTACHMENT17;
using gl::GL_COLOR_ATTACHMENT18;
using gl::GL_COLOR_ATTACHMENT19;
using gl::GL_COLOR_ATTACHMENT20;
using gl::GL_COLOR_ATTACHMENT21;
using gl::GL_COLOR_ATTACHMENT22;
using gl::GL_COLOR_ATTACHMENT23;
using gl::GL_COLOR_ATTACHMENT24;
using gl::GL_COLOR_ATTACHMENT25;
using gl::GL_COLOR_ATTACHMENT26;
using gl::GL_COLOR_ATTACHMENT27;
using gl::GL_COLOR_ATTACHMENT28;
using gl::GL_COLOR_ATTACHMENT29;
using gl::GL_COLOR_ATTACHMENT30;
using gl::GL_COLOR_ATTACHMENT31;
using gl::GL_DEPTH_ATTACHMENT;
using gl::GL_DEPTH_ATTACHMENT_EXT;
using gl::GL_STENCIL_ATTACHMENT;
using gl::GL_STENCIL_ATTACHMENT_EXT;
using gl::GL_FRAMEBUFFER;
using gl::GL_FRAMEBUFFER_EXT;
using gl::GL_RENDERBUFFER;
using gl::GL_RENDERBUFFER_EXT;
using gl::GL_RENDERBUFFER_WIDTH;
using gl::GL_RENDERBUFFER_WIDTH_EXT;
using gl::GL_RENDERBUFFER_HEIGHT;
using gl::GL_RENDERBUFFER_HEIGHT_EXT;
using gl::GL_RENDERBUFFER_INTERNAL_FORMAT;
using gl::GL_RENDERBUFFER_INTERNAL_FORMAT_EXT;
using gl::GL_STENCIL_INDEX1;
using gl::GL_STENCIL_INDEX1_EXT;
using gl::GL_STENCIL_INDEX4;
using gl::GL_STENCIL_INDEX4_EXT;
using gl::GL_STENCIL_INDEX8;
using gl::GL_STENCIL_INDEX8_EXT;
using gl::GL_STENCIL_INDEX16;
using gl::GL_STENCIL_INDEX16_EXT;
using gl::GL_RENDERBUFFER_RED_SIZE;
using gl::GL_RENDERBUFFER_RED_SIZE_EXT;
using gl::GL_RENDERBUFFER_GREEN_SIZE;
using gl::GL_RENDERBUFFER_GREEN_SIZE_EXT;
using gl::GL_RENDERBUFFER_BLUE_SIZE;
using gl::GL_RENDERBUFFER_BLUE_SIZE_EXT;
using gl::GL_RENDERBUFFER_ALPHA_SIZE;
using gl::GL_RENDERBUFFER_ALPHA_SIZE_EXT;
using gl::GL_RENDERBUFFER_DEPTH_SIZE;
using gl::GL_RENDERBUFFER_DEPTH_SIZE_EXT;
using gl::GL_RENDERBUFFER_STENCIL_SIZE;
using gl::GL_RENDERBUFFER_STENCIL_SIZE_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
using gl::GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT;
using gl::GL_MAX_SAMPLES;
using gl::GL_MAX_SAMPLES_EXT;
using gl::GL_RGB565;
using gl::GL_PRIMITIVE_RESTART_FIXED_INDEX;
using gl::GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
using gl::GL_MAX_ELEMENT_INDEX;
using gl::GL_RGBA32UI;
using gl::GL_RGBA32UI_EXT;
using gl::GL_RGB32UI;
using gl::GL_RGB32UI_EXT;
using gl::GL_ALPHA32UI_EXT;
using gl::GL_INTENSITY32UI_EXT;
using gl::GL_LUMINANCE32UI_EXT;
using gl::GL_LUMINANCE_ALPHA32UI_EXT;
using gl::GL_RGBA16UI;
using gl::GL_RGBA16UI_EXT;
using gl::GL_RGB16UI;
using gl::GL_RGB16UI_EXT;
using gl::GL_ALPHA16UI_EXT;
using gl::GL_INTENSITY16UI_EXT;
using gl::GL_LUMINANCE16UI_EXT;
using gl::GL_LUMINANCE_ALPHA16UI_EXT;
using gl::GL_RGBA8UI;
using gl::GL_RGBA8UI_EXT;
using gl::GL_RGB8UI;
using gl::GL_RGB8UI_EXT;
using gl::GL_ALPHA8UI_EXT;
using gl::GL_INTENSITY8UI_EXT;
using gl::GL_LUMINANCE8UI_EXT;
using gl::GL_LUMINANCE_ALPHA8UI_EXT;
using gl::GL_RGBA32I;
using gl::GL_RGBA32I_EXT;
using gl::GL_RGB32I;
using gl::GL_RGB32I_EXT;
using gl::GL_ALPHA32I_EXT;
using gl::GL_INTENSITY32I_EXT;
using gl::GL_LUMINANCE32I_EXT;
using gl::GL_LUMINANCE_ALPHA32I_EXT;
using gl::GL_RGBA16I;
using gl::GL_RGBA16I_EXT;
using gl::GL_RGB16I;
using gl::GL_RGB16I_EXT;
using gl::GL_ALPHA16I_EXT;
using gl::GL_INTENSITY16I_EXT;
using gl::GL_LUMINANCE16I_EXT;
using gl::GL_LUMINANCE_ALPHA16I_EXT;
using gl::GL_RGBA8I;
using gl::GL_RGBA8I_EXT;
using gl::GL_RGB8I;
using gl::GL_RGB8I_EXT;
using gl::GL_ALPHA8I_EXT;
using gl::GL_INTENSITY8I_EXT;
using gl::GL_LUMINANCE8I_EXT;
using gl::GL_LUMINANCE_ALPHA8I_EXT;
using gl::GL_RED_INTEGER;
using gl::GL_RED_INTEGER_EXT;
using gl::GL_GREEN_INTEGER;
using gl::GL_GREEN_INTEGER_EXT;
using gl::GL_BLUE_INTEGER;
using gl::GL_BLUE_INTEGER_EXT;
using gl::GL_ALPHA_INTEGER;
using gl::GL_ALPHA_INTEGER_EXT;
using gl::GL_RGB_INTEGER;
using gl::GL_RGB_INTEGER_EXT;
using gl::GL_RGBA_INTEGER;
using gl::GL_RGBA_INTEGER_EXT;
using gl::GL_BGR_INTEGER;
using gl::GL_BGR_INTEGER_EXT;
using gl::GL_BGRA_INTEGER;
using gl::GL_BGRA_INTEGER_EXT;
using gl::GL_LUMINANCE_INTEGER_EXT;
using gl::GL_LUMINANCE_ALPHA_INTEGER_EXT;
using gl::GL_RGBA_INTEGER_MODE_EXT;
using gl::GL_INT_2_10_10_10_REV;
using gl::GL_MAX_PROGRAM_PARAMETER_BUFFER_BINDINGS_NV;
using gl::GL_MAX_PROGRAM_PARAMETER_BUFFER_SIZE_NV;
using gl::GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_MAX_PROGRAM_GENERIC_ATTRIBS_NV;
using gl::GL_MAX_PROGRAM_GENERIC_RESULTS_NV;
using gl::GL_FRAMEBUFFER_ATTACHMENT_LAYERED;
using gl::GL_FRAMEBUFFER_ATTACHMENT_LAYERED_ARB;
using gl::GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS;
using gl::GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_ARB;
using gl::GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT;
using gl::GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_ARB;
using gl::GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT;
using gl::GL_LAYER_NV;
using gl::GL_DEPTH_COMPONENT32F_NV;
using gl::GL_DEPTH32F_STENCIL8_NV;
using gl::GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
using gl::GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV;
using gl::GL_SHADER_INCLUDE_ARB;
using gl::GL_DEPTH_BUFFER_FLOAT_MODE_NV;
using gl::GL_FRAMEBUFFER_SRGB;
using gl::GL_FRAMEBUFFER_SRGB_EXT;
using gl::GL_FRAMEBUFFER_SRGB_CAPABLE_EXT;
using gl::GL_COMPRESSED_RED_RGTC1;
using gl::GL_COMPRESSED_RED_RGTC1_EXT;
using gl::GL_COMPRESSED_SIGNED_RED_RGTC1;
using gl::GL_COMPRESSED_SIGNED_RED_RGTC1_EXT;
using gl::GL_COMPRESSED_RED_GREEN_RGTC2_EXT;
using gl::GL_COMPRESSED_RG_RGTC2;
using gl::GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT;
using gl::GL_COMPRESSED_SIGNED_RG_RGTC2;
using gl::GL_SAMPLER_1D_ARRAY;
using gl::GL_SAMPLER_1D_ARRAY_EXT;
using gl::GL_SAMPLER_2D_ARRAY;
using gl::GL_SAMPLER_2D_ARRAY_EXT;
using gl::GL_SAMPLER_BUFFER;
using gl::GL_SAMPLER_BUFFER_EXT;
using gl::GL_SAMPLER_1D_ARRAY_SHADOW;
using gl::GL_SAMPLER_1D_ARRAY_SHADOW_EXT;
using gl::GL_SAMPLER_2D_ARRAY_SHADOW;
using gl::GL_SAMPLER_2D_ARRAY_SHADOW_EXT;
using gl::GL_SAMPLER_CUBE_SHADOW;
using gl::GL_SAMPLER_CUBE_SHADOW_EXT;
using gl::GL_UNSIGNED_INT_VEC2;
using gl::GL_UNSIGNED_INT_VEC2_EXT;
using gl::GL_UNSIGNED_INT_VEC3;
using gl::GL_UNSIGNED_INT_VEC3_EXT;
using gl::GL_UNSIGNED_INT_VEC4;
using gl::GL_UNSIGNED_INT_VEC4_EXT;
using gl::GL_INT_SAMPLER_1D;
using gl::GL_INT_SAMPLER_1D_EXT;
using gl::GL_INT_SAMPLER_2D;
using gl::GL_INT_SAMPLER_2D_EXT;
using gl::GL_INT_SAMPLER_3D;
using gl::GL_INT_SAMPLER_3D_EXT;
using gl::GL_INT_SAMPLER_CUBE;
using gl::GL_INT_SAMPLER_CUBE_EXT;
using gl::GL_INT_SAMPLER_2D_RECT;
using gl::GL_INT_SAMPLER_2D_RECT_EXT;
using gl::GL_INT_SAMPLER_1D_ARRAY;
using gl::GL_INT_SAMPLER_1D_ARRAY_EXT;
using gl::GL_INT_SAMPLER_2D_ARRAY;
using gl::GL_INT_SAMPLER_2D_ARRAY_EXT;
using gl::GL_INT_SAMPLER_BUFFER;
using gl::GL_INT_SAMPLER_BUFFER_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_1D;
using gl::GL_UNSIGNED_INT_SAMPLER_1D_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_2D;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_3D;
using gl::GL_UNSIGNED_INT_SAMPLER_3D_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_CUBE;
using gl::GL_UNSIGNED_INT_SAMPLER_CUBE_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_RECT;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_RECT_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_1D_ARRAY;
using gl::GL_UNSIGNED_INT_SAMPLER_1D_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_ARRAY;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_SAMPLER_BUFFER;
using gl::GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT;
using gl::GL_GEOMETRY_SHADER;
using gl::GL_GEOMETRY_SHADER_ARB;
using gl::GL_GEOMETRY_SHADER_EXT;
using gl::GL_GEOMETRY_VERTICES_OUT_ARB;
using gl::GL_GEOMETRY_VERTICES_OUT_EXT;
using gl::GL_GEOMETRY_INPUT_TYPE_ARB;
using gl::GL_GEOMETRY_INPUT_TYPE_EXT;
using gl::GL_GEOMETRY_OUTPUT_TYPE_ARB;
using gl::GL_GEOMETRY_OUTPUT_TYPE_EXT;
using gl::GL_MAX_GEOMETRY_VARYING_COMPONENTS_ARB;
using gl::GL_MAX_GEOMETRY_VARYING_COMPONENTS_EXT;
using gl::GL_MAX_VERTEX_VARYING_COMPONENTS_ARB;
using gl::GL_MAX_VERTEX_VARYING_COMPONENTS_EXT;
using gl::GL_MAX_GEOMETRY_UNIFORM_COMPONENTS;
using gl::GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB;
using gl::GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT;
using gl::GL_MAX_GEOMETRY_OUTPUT_VERTICES;
using gl::GL_MAX_GEOMETRY_OUTPUT_VERTICES_ARB;
using gl::GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT;
using gl::GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS;
using gl::GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB;
using gl::GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT;
using gl::GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT;
using gl::GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT;
using gl::GL_MAX_GEOMETRY_BINDABLE_UNIFORMS_EXT;
using gl::GL_ACTIVE_SUBROUTINES;
using gl::GL_ACTIVE_SUBROUTINE_UNIFORMS;
using gl::GL_MAX_SUBROUTINES;
using gl::GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS;
using gl::GL_NAMED_STRING_LENGTH_ARB;
using gl::GL_NAMED_STRING_TYPE_ARB;
using gl::GL_MAX_BINDABLE_UNIFORM_SIZE_EXT;
using gl::GL_UNIFORM_BUFFER_EXT;
using gl::GL_UNIFORM_BUFFER_BINDING_EXT;
using gl::GL_LOW_FLOAT;
using gl::GL_MEDIUM_FLOAT;
using gl::GL_HIGH_FLOAT;
using gl::GL_LOW_INT;
using gl::GL_MEDIUM_INT;
using gl::GL_HIGH_INT;
using gl::GL_SHADER_BINARY_FORMATS;
using gl::GL_NUM_SHADER_BINARY_FORMATS;
using gl::GL_SHADER_COMPILER;
using gl::GL_MAX_VERTEX_UNIFORM_VECTORS;
using gl::GL_MAX_VARYING_VECTORS;
using gl::GL_MAX_FRAGMENT_UNIFORM_VECTORS;
using gl::GL_RENDERBUFFER_COLOR_SAMPLES_NV;
using gl::GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV;
using gl::GL_MULTISAMPLE_COVERAGE_MODES_NV;
using gl::GL_QUERY_WAIT;
using gl::GL_QUERY_WAIT_NV;
using gl::GL_QUERY_NO_WAIT;
using gl::GL_QUERY_NO_WAIT_NV;
using gl::GL_QUERY_BY_REGION_WAIT;
using gl::GL_QUERY_BY_REGION_WAIT_NV;
using gl::GL_QUERY_BY_REGION_NO_WAIT;
using gl::GL_QUERY_BY_REGION_NO_WAIT_NV;
using gl::GL_QUERY_WAIT_INVERTED;
using gl::GL_QUERY_NO_WAIT_INVERTED;
using gl::GL_QUERY_BY_REGION_WAIT_INVERTED;
using gl::GL_QUERY_BY_REGION_NO_WAIT_INVERTED;
using gl::GL_POLYGON_OFFSET_CLAMP_EXT;
using gl::GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS;
using gl::GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS;
using gl::GL_COLOR_SAMPLES_NV;
using gl::GL_TRANSFORM_FEEDBACK;
using gl::GL_TRANSFORM_FEEDBACK_NV;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED_NV;
using gl::GL_TRANSFORM_FEEDBACK_PAUSED;
using gl::GL_TRANSFORM_FEEDBACK_ACTIVE;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE_NV;
using gl::GL_TRANSFORM_FEEDBACK_BINDING;
using gl::GL_TRANSFORM_FEEDBACK_BINDING_NV;
using gl::GL_FRAME_NV;
using gl::GL_FIELDS_NV;
using gl::GL_CURRENT_TIME_NV;
using gl::GL_TIMESTAMP;
using gl::GL_NUM_FILL_STREAMS_NV;
using gl::GL_PRESENT_TIME_NV;
using gl::GL_PRESENT_DURATION_NV;
using gl::GL_PROGRAM_MATRIX_EXT;
using gl::GL_TRANSPOSE_PROGRAM_MATRIX_EXT;
using gl::GL_PROGRAM_MATRIX_STACK_DEPTH_EXT;
using gl::GL_TEXTURE_SWIZZLE_R;
using gl::GL_TEXTURE_SWIZZLE_R_EXT;
using gl::GL_TEXTURE_SWIZZLE_G;
using gl::GL_TEXTURE_SWIZZLE_G_EXT;
using gl::GL_TEXTURE_SWIZZLE_B;
using gl::GL_TEXTURE_SWIZZLE_B_EXT;
using gl::GL_TEXTURE_SWIZZLE_A;
using gl::GL_TEXTURE_SWIZZLE_A_EXT;
using gl::GL_TEXTURE_SWIZZLE_RGBA;
using gl::GL_TEXTURE_SWIZZLE_RGBA_EXT;
using gl::GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS;
using gl::GL_ACTIVE_SUBROUTINE_MAX_LENGTH;
using gl::GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH;
using gl::GL_NUM_COMPATIBLE_SUBROUTINES;
using gl::GL_COMPATIBLE_SUBROUTINES;
using gl::GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION;
using gl::GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION_EXT;
using gl::GL_FIRST_VERTEX_CONVENTION;
using gl::GL_FIRST_VERTEX_CONVENTION_EXT;
using gl::GL_LAST_VERTEX_CONVENTION;
using gl::GL_LAST_VERTEX_CONVENTION_EXT;
using gl::GL_PROVOKING_VERTEX;
using gl::GL_PROVOKING_VERTEX_EXT;
using gl::GL_SAMPLE_LOCATION_ARB;
using gl::GL_SAMPLE_LOCATION_NV;
using gl::GL_SAMPLE_POSITION;
using gl::GL_SAMPLE_POSITION_NV;
using gl::GL_SAMPLE_MASK;
using gl::GL_SAMPLE_MASK_NV;
using gl::GL_SAMPLE_MASK_VALUE;
using gl::GL_SAMPLE_MASK_VALUE_NV;
using gl::GL_TEXTURE_BINDING_RENDERBUFFER_NV;
using gl::GL_TEXTURE_RENDERBUFFER_DATA_STORE_BINDING_NV;
using gl::GL_TEXTURE_RENDERBUFFER_NV;
using gl::GL_SAMPLER_RENDERBUFFER_NV;
using gl::GL_INT_SAMPLER_RENDERBUFFER_NV;
using gl::GL_UNSIGNED_INT_SAMPLER_RENDERBUFFER_NV;
using gl::GL_MAX_SAMPLE_MASK_WORDS;
using gl::GL_MAX_SAMPLE_MASK_WORDS_NV;
using gl::GL_MAX_GEOMETRY_PROGRAM_INVOCATIONS_NV;
using gl::GL_MAX_GEOMETRY_SHADER_INVOCATIONS;
using gl::GL_MIN_FRAGMENT_INTERPOLATION_OFFSET;
using gl::GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_NV;
using gl::GL_MAX_FRAGMENT_INTERPOLATION_OFFSET;
using gl::GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_NV;
using gl::GL_FRAGMENT_INTERPOLATION_OFFSET_BITS;
using gl::GL_FRAGMENT_PROGRAM_INTERPOLATION_OFFSET_BITS_NV;
using gl::GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET;
using gl::GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET_ARB;
using gl::GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET_NV;
using gl::GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET;
using gl::GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET_ARB;
using gl::GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET_NV;
using gl::GL_MAX_TRANSFORM_FEEDBACK_BUFFERS;
using gl::GL_MAX_VERTEX_STREAMS;
using gl::GL_PATCH_VERTICES;
using gl::GL_PATCH_DEFAULT_INNER_LEVEL;
using gl::GL_PATCH_DEFAULT_OUTER_LEVEL;
using gl::GL_TESS_CONTROL_OUTPUT_VERTICES;
using gl::GL_TESS_GEN_MODE;
using gl::GL_TESS_GEN_SPACING;
using gl::GL_TESS_GEN_VERTEX_ORDER;
using gl::GL_TESS_GEN_POINT_MODE;
using gl::GL_ISOLINES;
using gl::GL_FRACTIONAL_ODD;
using gl::GL_FRACTIONAL_EVEN;
using gl::GL_MAX_PATCH_VERTICES;
using gl::GL_MAX_TESS_GEN_LEVEL;
using gl::GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS;
using gl::GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS;
using gl::GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS;
using gl::GL_MAX_TESS_PATCH_COMPONENTS;
using gl::GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS;
using gl::GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS;
using gl::GL_TESS_EVALUATION_SHADER;
using gl::GL_TESS_CONTROL_SHADER;
using gl::GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS;
using gl::GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS;
using gl::GL_COMPRESSED_RGBA_BPTC_UNORM;
using gl::GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
using gl::GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
using gl::GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
using gl::GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
using gl::GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB;
using gl::GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
using gl::GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
using gl::GL_BUFFER_GPU_ADDRESS_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV;
using gl::GL_ELEMENT_ARRAY_UNIFIED_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV;
using gl::GL_VERTEX_ARRAY_ADDRESS_NV;
using gl::GL_NORMAL_ARRAY_ADDRESS_NV;
using gl::GL_COLOR_ARRAY_ADDRESS_NV;
using gl::GL_INDEX_ARRAY_ADDRESS_NV;
using gl::GL_TEXTURE_COORD_ARRAY_ADDRESS_NV;
using gl::GL_EDGE_FLAG_ARRAY_ADDRESS_NV;
using gl::GL_SECONDARY_COLOR_ARRAY_ADDRESS_NV;
using gl::GL_FOG_COORD_ARRAY_ADDRESS_NV;
using gl::GL_ELEMENT_ARRAY_ADDRESS_NV;
using gl::GL_VERTEX_ATTRIB_ARRAY_LENGTH_NV;
using gl::GL_VERTEX_ARRAY_LENGTH_NV;
using gl::GL_NORMAL_ARRAY_LENGTH_NV;
using gl::GL_COLOR_ARRAY_LENGTH_NV;
using gl::GL_INDEX_ARRAY_LENGTH_NV;
using gl::GL_TEXTURE_COORD_ARRAY_LENGTH_NV;
using gl::GL_EDGE_FLAG_ARRAY_LENGTH_NV;
using gl::GL_SECONDARY_COLOR_ARRAY_LENGTH_NV;
using gl::GL_FOG_COORD_ARRAY_LENGTH_NV;
using gl::GL_ELEMENT_ARRAY_LENGTH_NV;
using gl::GL_GPU_ADDRESS_NV;
using gl::GL_MAX_SHADER_BUFFER_ADDRESS_NV;
using gl::GL_COPY_READ_BUFFER;
using gl::GL_COPY_READ_BUFFER_BINDING;
using gl::GL_COPY_WRITE_BUFFER;
using gl::GL_COPY_WRITE_BUFFER_BINDING;
using gl::GL_MAX_IMAGE_UNITS;
using gl::GL_MAX_IMAGE_UNITS_EXT;
using gl::GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS;
using gl::GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS_EXT;
using gl::GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES;
using gl::GL_IMAGE_BINDING_NAME;
using gl::GL_IMAGE_BINDING_NAME_EXT;
using gl::GL_IMAGE_BINDING_LEVEL;
using gl::GL_IMAGE_BINDING_LEVEL_EXT;
using gl::GL_IMAGE_BINDING_LAYERED;
using gl::GL_IMAGE_BINDING_LAYERED_EXT;
using gl::GL_IMAGE_BINDING_LAYER;
using gl::GL_IMAGE_BINDING_LAYER_EXT;
using gl::GL_IMAGE_BINDING_ACCESS;
using gl::GL_IMAGE_BINDING_ACCESS_EXT;
using gl::GL_DRAW_INDIRECT_BUFFER;
using gl::GL_DRAW_INDIRECT_UNIFIED_NV;
using gl::GL_DRAW_INDIRECT_ADDRESS_NV;
using gl::GL_DRAW_INDIRECT_LENGTH_NV;
using gl::GL_DRAW_INDIRECT_BUFFER_BINDING;
using gl::GL_MAX_PROGRAM_SUBROUTINE_PARAMETERS_NV;
using gl::GL_MAX_PROGRAM_SUBROUTINE_NUM_NV;
using gl::GL_DOUBLE_MAT2;
using gl::GL_DOUBLE_MAT2_EXT;
using gl::GL_DOUBLE_MAT3;
using gl::GL_DOUBLE_MAT3_EXT;
using gl::GL_DOUBLE_MAT4;
using gl::GL_DOUBLE_MAT4_EXT;
using gl::GL_DOUBLE_MAT2x3;
using gl::GL_DOUBLE_MAT2x3_EXT;
using gl::GL_DOUBLE_MAT2x4;
using gl::GL_DOUBLE_MAT2x4_EXT;
using gl::GL_DOUBLE_MAT3x2;
using gl::GL_DOUBLE_MAT3x2_EXT;
using gl::GL_DOUBLE_MAT3x4;
using gl::GL_DOUBLE_MAT3x4_EXT;
using gl::GL_DOUBLE_MAT4x2;
using gl::GL_DOUBLE_MAT4x2_EXT;
using gl::GL_DOUBLE_MAT4x3;
using gl::GL_DOUBLE_MAT4x3_EXT;
using gl::GL_VERTEX_BINDING_BUFFER;
using gl::GL_RED_SNORM;
using gl::GL_RG_SNORM;
using gl::GL_RGB_SNORM;
using gl::GL_RGBA_SNORM;
using gl::GL_R8_SNORM;
using gl::GL_RG8_SNORM;
using gl::GL_RGB8_SNORM;
using gl::GL_RGBA8_SNORM;
using gl::GL_R16_SNORM;
using gl::GL_RG16_SNORM;
using gl::GL_RGB16_SNORM;
using gl::GL_RGBA16_SNORM;
using gl::GL_SIGNED_NORMALIZED;
using gl::GL_PRIMITIVE_RESTART;
using gl::GL_PRIMITIVE_RESTART_INDEX;
using gl::GL_MAX_PROGRAM_TEXTURE_GATHER_COMPONENTS_ARB;
using gl::GL_INT8_NV;
using gl::GL_INT8_VEC2_NV;
using gl::GL_INT8_VEC3_NV;
using gl::GL_INT8_VEC4_NV;
using gl::GL_INT16_NV;
using gl::GL_INT16_VEC2_NV;
using gl::GL_INT16_VEC3_NV;
using gl::GL_INT16_VEC4_NV;
using gl::GL_INT64_VEC2_ARB;
using gl::GL_INT64_VEC2_NV;
using gl::GL_INT64_VEC3_ARB;
using gl::GL_INT64_VEC3_NV;
using gl::GL_INT64_VEC4_ARB;
using gl::GL_INT64_VEC4_NV;
using gl::GL_UNSIGNED_INT8_NV;
using gl::GL_UNSIGNED_INT8_VEC2_NV;
using gl::GL_UNSIGNED_INT8_VEC3_NV;
using gl::GL_UNSIGNED_INT8_VEC4_NV;
using gl::GL_UNSIGNED_INT16_NV;
using gl::GL_UNSIGNED_INT16_VEC2_NV;
using gl::GL_UNSIGNED_INT16_VEC3_NV;
using gl::GL_UNSIGNED_INT16_VEC4_NV;
using gl::GL_UNSIGNED_INT64_VEC2_ARB;
using gl::GL_UNSIGNED_INT64_VEC2_NV;
using gl::GL_UNSIGNED_INT64_VEC3_ARB;
using gl::GL_UNSIGNED_INT64_VEC3_NV;
using gl::GL_UNSIGNED_INT64_VEC4_ARB;
using gl::GL_UNSIGNED_INT64_VEC4_NV;
using gl::GL_FLOAT16_NV;
using gl::GL_FLOAT16_VEC2_NV;
using gl::GL_FLOAT16_VEC3_NV;
using gl::GL_FLOAT16_VEC4_NV;
using gl::GL_DOUBLE_VEC2;
using gl::GL_DOUBLE_VEC2_EXT;
using gl::GL_DOUBLE_VEC3;
using gl::GL_DOUBLE_VEC3_EXT;
using gl::GL_DOUBLE_VEC4;
using gl::GL_DOUBLE_VEC4_EXT;
using gl::GL_SAMPLER_BUFFER_AMD;
using gl::GL_INT_SAMPLER_BUFFER_AMD;
using gl::GL_UNSIGNED_INT_SAMPLER_BUFFER_AMD;
using gl::GL_TESSELLATION_MODE_AMD;
using gl::GL_TESSELLATION_FACTOR_AMD;
using gl::GL_DISCRETE_AMD;
using gl::GL_CONTINUOUS_AMD;
using gl::GL_TEXTURE_CUBE_MAP_ARRAY;
using gl::GL_TEXTURE_CUBE_MAP_ARRAY_ARB;
using gl::GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
using gl::GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_ARB;
using gl::GL_PROXY_TEXTURE_CUBE_MAP_ARRAY;
using gl::GL_PROXY_TEXTURE_CUBE_MAP_ARRAY_ARB;
using gl::GL_SAMPLER_CUBE_MAP_ARRAY;
using gl::GL_SAMPLER_CUBE_MAP_ARRAY_ARB;
using gl::GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW;
using gl::GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_ARB;
using gl::GL_INT_SAMPLER_CUBE_MAP_ARRAY;
using gl::GL_INT_SAMPLER_CUBE_MAP_ARRAY_ARB;
using gl::GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY;
using gl::GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_ARB;
using gl::GL_ALPHA_SNORM;
using gl::GL_LUMINANCE_SNORM;
using gl::GL_LUMINANCE_ALPHA_SNORM;
using gl::GL_INTENSITY_SNORM;
using gl::GL_ALPHA8_SNORM;
using gl::GL_LUMINANCE8_SNORM;
using gl::GL_LUMINANCE8_ALPHA8_SNORM;
using gl::GL_INTENSITY8_SNORM;
using gl::GL_ALPHA16_SNORM;
using gl::GL_LUMINANCE16_SNORM;
using gl::GL_LUMINANCE16_ALPHA16_SNORM;
using gl::GL_INTENSITY16_SNORM;
using gl::GL_FACTOR_MIN_AMD;
using gl::GL_FACTOR_MAX_AMD;
using gl::GL_DEPTH_CLAMP_NEAR_AMD;
using gl::GL_DEPTH_CLAMP_FAR_AMD;
using gl::GL_VIDEO_BUFFER_NV;
using gl::GL_VIDEO_BUFFER_BINDING_NV;
using gl::GL_FIELD_UPPER_NV;
using gl::GL_FIELD_LOWER_NV;
using gl::GL_NUM_VIDEO_CAPTURE_STREAMS_NV;
using gl::GL_NEXT_VIDEO_CAPTURE_BUFFER_STATUS_NV;
using gl::GL_VIDEO_CAPTURE_TO_422_SUPPORTED_NV;
using gl::GL_LAST_VIDEO_CAPTURE_STATUS_NV;
using gl::GL_VIDEO_BUFFER_PITCH_NV;
using gl::GL_VIDEO_COLOR_CONVERSION_MATRIX_NV;
using gl::GL_VIDEO_COLOR_CONVERSION_MAX_NV;
using gl::GL_VIDEO_COLOR_CONVERSION_MIN_NV;
using gl::GL_VIDEO_COLOR_CONVERSION_OFFSET_NV;
using gl::GL_VIDEO_BUFFER_INTERNAL_FORMAT_NV;
using gl::GL_PARTIAL_SUCCESS_NV;
using gl::GL_SUCCESS_NV;
using gl::GL_FAILURE_NV;
using gl::GL_YCBYCR8_422_NV;
using gl::GL_YCBAYCR8A_4224_NV;
using gl::GL_Z6Y10Z6CB10Z6Y10Z6CR10_422_NV;
using gl::GL_Z6Y10Z6CB10Z6A10Z6Y10Z6CR10Z6A10_4224_NV;
using gl::GL_Z4Y12Z4CB12Z4Y12Z4CR12_422_NV;
using gl::GL_Z4Y12Z4CB12Z4A12Z4Y12Z4CR12Z4A12_4224_NV;
using gl::GL_Z4Y12Z4CB12Z4CR12_444_NV;
using gl::GL_VIDEO_CAPTURE_FRAME_WIDTH_NV;
using gl::GL_VIDEO_CAPTURE_FRAME_HEIGHT_NV;
using gl::GL_VIDEO_CAPTURE_FIELD_UPPER_HEIGHT_NV;
using gl::GL_VIDEO_CAPTURE_FIELD_LOWER_HEIGHT_NV;
using gl::GL_VIDEO_CAPTURE_SURFACE_ORIGIN_NV;
using gl::GL_TEXTURE_COVERAGE_SAMPLES_NV;
using gl::GL_TEXTURE_COLOR_SAMPLES_NV;
using gl::GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX;
using gl::GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX;
using gl::GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX;
using gl::GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX;
using gl::GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX;
using gl::GL_IMAGE_1D;
using gl::GL_IMAGE_1D_EXT;
using gl::GL_IMAGE_2D;
using gl::GL_IMAGE_2D_EXT;
using gl::GL_IMAGE_3D;
using gl::GL_IMAGE_3D_EXT;
using gl::GL_IMAGE_2D_RECT;
using gl::GL_IMAGE_2D_RECT_EXT;
using gl::GL_IMAGE_CUBE;
using gl::GL_IMAGE_CUBE_EXT;
using gl::GL_IMAGE_BUFFER;
using gl::GL_IMAGE_BUFFER_EXT;
using gl::GL_IMAGE_1D_ARRAY;
using gl::GL_IMAGE_1D_ARRAY_EXT;
using gl::GL_IMAGE_2D_ARRAY;
using gl::GL_IMAGE_2D_ARRAY_EXT;
using gl::GL_IMAGE_CUBE_MAP_ARRAY;
using gl::GL_IMAGE_CUBE_MAP_ARRAY_EXT;
using gl::GL_IMAGE_2D_MULTISAMPLE;
using gl::GL_IMAGE_2D_MULTISAMPLE_EXT;
using gl::GL_IMAGE_2D_MULTISAMPLE_ARRAY;
using gl::GL_IMAGE_2D_MULTISAMPLE_ARRAY_EXT;
using gl::GL_INT_IMAGE_1D;
using gl::GL_INT_IMAGE_1D_EXT;
using gl::GL_INT_IMAGE_2D;
using gl::GL_INT_IMAGE_2D_EXT;
using gl::GL_INT_IMAGE_3D;
using gl::GL_INT_IMAGE_3D_EXT;
using gl::GL_INT_IMAGE_2D_RECT;
using gl::GL_INT_IMAGE_2D_RECT_EXT;
using gl::GL_INT_IMAGE_CUBE;
using gl::GL_INT_IMAGE_CUBE_EXT;
using gl::GL_INT_IMAGE_BUFFER;
using gl::GL_INT_IMAGE_BUFFER_EXT;
using gl::GL_INT_IMAGE_1D_ARRAY;
using gl::GL_INT_IMAGE_1D_ARRAY_EXT;
using gl::GL_INT_IMAGE_2D_ARRAY;
using gl::GL_INT_IMAGE_2D_ARRAY_EXT;
using gl::GL_INT_IMAGE_CUBE_MAP_ARRAY;
using gl::GL_INT_IMAGE_CUBE_MAP_ARRAY_EXT;
using gl::GL_INT_IMAGE_2D_MULTISAMPLE;
using gl::GL_INT_IMAGE_2D_MULTISAMPLE_EXT;
using gl::GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY;
using gl::GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_1D;
using gl::GL_UNSIGNED_INT_IMAGE_1D_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_2D;
using gl::GL_UNSIGNED_INT_IMAGE_2D_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_3D;
using gl::GL_UNSIGNED_INT_IMAGE_3D_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_2D_RECT;
using gl::GL_UNSIGNED_INT_IMAGE_2D_RECT_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_CUBE;
using gl::GL_UNSIGNED_INT_IMAGE_CUBE_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_BUFFER;
using gl::GL_UNSIGNED_INT_IMAGE_BUFFER_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_1D_ARRAY;
using gl::GL_UNSIGNED_INT_IMAGE_1D_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_2D_ARRAY;
using gl::GL_UNSIGNED_INT_IMAGE_2D_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY;
using gl::GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE;
using gl::GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_EXT;
using gl::GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY;
using gl::GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT;
using gl::GL_MAX_IMAGE_SAMPLES;
using gl::GL_MAX_IMAGE_SAMPLES_EXT;
using gl::GL_IMAGE_BINDING_FORMAT;
using gl::GL_IMAGE_BINDING_FORMAT_EXT;
using gl::GL_RGB10_A2UI;
using gl::GL_PATH_FORMAT_SVG_NV;
using gl::GL_PATH_FORMAT_PS_NV;
using gl::GL_STANDARD_FONT_NAME_NV;
using gl::GL_SYSTEM_FONT_NAME_NV;
using gl::GL_FILE_NAME_NV;
using gl::GL_PATH_STROKE_WIDTH_NV;
using gl::GL_PATH_END_CAPS_NV;
using gl::GL_PATH_INITIAL_END_CAP_NV;
using gl::GL_PATH_TERMINAL_END_CAP_NV;
using gl::GL_PATH_JOIN_STYLE_NV;
using gl::GL_PATH_MITER_LIMIT_NV;
using gl::GL_PATH_DASH_CAPS_NV;
using gl::GL_PATH_INITIAL_DASH_CAP_NV;
using gl::GL_PATH_TERMINAL_DASH_CAP_NV;
using gl::GL_PATH_DASH_OFFSET_NV;
using gl::GL_PATH_CLIENT_LENGTH_NV;
using gl::GL_PATH_FILL_MODE_NV;
using gl::GL_PATH_FILL_MASK_NV;
using gl::GL_PATH_FILL_COVER_MODE_NV;
using gl::GL_PATH_STROKE_COVER_MODE_NV;
using gl::GL_PATH_STROKE_MASK_NV;
using gl::GL_COUNT_UP_NV;
using gl::GL_COUNT_DOWN_NV;
using gl::GL_PATH_OBJECT_BOUNDING_BOX_NV;
using gl::GL_CONVEX_HULL_NV;
using gl::GL_BOUNDING_BOX_NV;
using gl::GL_TRANSLATE_X_NV;
using gl::GL_TRANSLATE_Y_NV;
using gl::GL_TRANSLATE_2D_NV;
using gl::GL_TRANSLATE_3D_NV;
using gl::GL_AFFINE_2D_NV;
using gl::GL_AFFINE_3D_NV;
using gl::GL_TRANSPOSE_AFFINE_2D_NV;
using gl::GL_TRANSPOSE_AFFINE_3D_NV;
using gl::GL_UTF8_NV;
using gl::GL_UTF16_NV;
using gl::GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV;
using gl::GL_PATH_COMMAND_COUNT_NV;
using gl::GL_PATH_COORD_COUNT_NV;
using gl::GL_PATH_DASH_ARRAY_COUNT_NV;
using gl::GL_PATH_COMPUTED_LENGTH_NV;
using gl::GL_PATH_FILL_BOUNDING_BOX_NV;
using gl::GL_PATH_STROKE_BOUNDING_BOX_NV;
using gl::GL_SQUARE_NV;
using gl::GL_ROUND_NV;
using gl::GL_TRIANGULAR_NV;
using gl::GL_BEVEL_NV;
using gl::GL_MITER_REVERT_NV;
using gl::GL_MITER_TRUNCATE_NV;
using gl::GL_SKIP_MISSING_GLYPH_NV;
using gl::GL_USE_MISSING_GLYPH_NV;
using gl::GL_PATH_ERROR_POSITION_NV;
using gl::GL_PATH_FOG_GEN_MODE_NV;
using gl::GL_ACCUM_ADJACENT_PAIRS_NV;
using gl::GL_ADJACENT_PAIRS_NV;
using gl::GL_FIRST_TO_REST_NV;
using gl::GL_PATH_GEN_MODE_NV;
using gl::GL_PATH_GEN_COEFF_NV;
using gl::GL_PATH_GEN_COLOR_FORMAT_NV;
using gl::GL_PATH_GEN_COMPONENTS_NV;
using gl::GL_PATH_DASH_OFFSET_RESET_NV;
using gl::GL_MOVE_TO_RESETS_NV;
using gl::GL_MOVE_TO_CONTINUES_NV;
using gl::GL_PATH_STENCIL_FUNC_NV;
using gl::GL_PATH_STENCIL_REF_NV;
using gl::GL_PATH_STENCIL_VALUE_MASK_NV;
using gl::GL_SCALED_RESOLVE_FASTEST_EXT;
using gl::GL_SCALED_RESOLVE_NICEST_EXT;
using gl::GL_MIN_MAP_BUFFER_ALIGNMENT;
using gl::GL_PATH_STENCIL_DEPTH_OFFSET_FACTOR_NV;
using gl::GL_PATH_STENCIL_DEPTH_OFFSET_UNITS_NV;
using gl::GL_PATH_COVER_DEPTH_FUNC_NV;
using gl::GL_IMAGE_FORMAT_COMPATIBILITY_TYPE;
using gl::GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE;
using gl::GL_IMAGE_FORMAT_COMPATIBILITY_BY_CLASS;
using gl::GL_MAX_VERTEX_IMAGE_UNIFORMS;
using gl::GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS;
using gl::GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS;
using gl::GL_MAX_GEOMETRY_IMAGE_UNIFORMS;
using gl::GL_MAX_FRAGMENT_IMAGE_UNIFORMS;
using gl::GL_MAX_COMBINED_IMAGE_UNIFORMS;
using gl::GL_MAX_DEEP_3D_TEXTURE_WIDTH_HEIGHT_NV;
using gl::GL_MAX_DEEP_3D_TEXTURE_DEPTH_NV;
using gl::GL_SHADER_STORAGE_BUFFER;
using gl::GL_SHADER_STORAGE_BUFFER_BINDING;
using gl::GL_SHADER_STORAGE_BUFFER_START;
using gl::GL_SHADER_STORAGE_BUFFER_SIZE;
using gl::GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS;
using gl::GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS;
using gl::GL_MAX_SHADER_STORAGE_BLOCK_SIZE;
using gl::GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT;
using gl::GL_SYNC_X11_FENCE_EXT;
using gl::GL_DEPTH_STENCIL_TEXTURE_MODE;
using gl::GL_MAX_COMPUTE_FIXED_GROUP_INVOCATIONS_ARB;
using gl::GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS;
using gl::GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER;
using gl::GL_DISPATCH_INDIRECT_BUFFER;
using gl::GL_DISPATCH_INDIRECT_BUFFER_BINDING;
using gl::GL_CONTEXT_ROBUST_ACCESS;
using gl::GL_COMPUTE_PROGRAM_NV;
using gl::GL_COMPUTE_PROGRAM_PARAMETER_BUFFER_NV;
using gl::GL_TEXTURE_2D_MULTISAMPLE;
using gl::GL_PROXY_TEXTURE_2D_MULTISAMPLE;
using gl::GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
using gl::GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY;
using gl::GL_TEXTURE_BINDING_2D_MULTISAMPLE;
using gl::GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
using gl::GL_TEXTURE_SAMPLES;
using gl::GL_TEXTURE_FIXED_SAMPLE_LOCATIONS;
using gl::GL_SAMPLER_2D_MULTISAMPLE;
using gl::GL_INT_SAMPLER_2D_MULTISAMPLE;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE;
using gl::GL_SAMPLER_2D_MULTISAMPLE_ARRAY;
using gl::GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;
using gl::GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;
using gl::GL_MAX_COLOR_TEXTURE_SAMPLES;
using gl::GL_MAX_DEPTH_TEXTURE_SAMPLES;
using gl::GL_MAX_INTEGER_SAMPLES;
using gl::GL_MAX_SERVER_WAIT_TIMEOUT;
using gl::GL_OBJECT_TYPE;
using gl::GL_SYNC_CONDITION;
using gl::GL_SYNC_STATUS;
using gl::GL_SYNC_FLAGS;
using gl::GL_SYNC_FENCE;
using gl::GL_SYNC_GPU_COMMANDS_COMPLETE;
using gl::GL_UNSIGNALED;
using gl::GL_SIGNALED;
using gl::GL_ALREADY_SIGNALED;
using gl::GL_TIMEOUT_EXPIRED;
using gl::GL_CONDITION_SATISFIED;
using gl::GL_WAIT_FAILED;
using gl::GL_BUFFER_ACCESS_FLAGS;
using gl::GL_BUFFER_MAP_LENGTH;
using gl::GL_BUFFER_MAP_OFFSET;
using gl::GL_MAX_VERTEX_OUTPUT_COMPONENTS;
using gl::GL_MAX_GEOMETRY_INPUT_COMPONENTS;
using gl::GL_MAX_GEOMETRY_OUTPUT_COMPONENTS;
using gl::GL_MAX_FRAGMENT_INPUT_COMPONENTS;
using gl::GL_CONTEXT_PROFILE_MASK;
using gl::GL_UNPACK_COMPRESSED_BLOCK_WIDTH;
using gl::GL_UNPACK_COMPRESSED_BLOCK_HEIGHT;
using gl::GL_UNPACK_COMPRESSED_BLOCK_DEPTH;
using gl::GL_UNPACK_COMPRESSED_BLOCK_SIZE;
using gl::GL_PACK_COMPRESSED_BLOCK_WIDTH;
using gl::GL_PACK_COMPRESSED_BLOCK_HEIGHT;
using gl::GL_PACK_COMPRESSED_BLOCK_DEPTH;
using gl::GL_PACK_COMPRESSED_BLOCK_SIZE;
using gl::GL_TEXTURE_IMMUTABLE_FORMAT;
using gl::GL_MAX_DEBUG_MESSAGE_LENGTH;
using gl::GL_MAX_DEBUG_MESSAGE_LENGTH_AMD;
using gl::GL_MAX_DEBUG_MESSAGE_LENGTH_ARB;
using gl::GL_MAX_DEBUG_LOGGED_MESSAGES;
using gl::GL_MAX_DEBUG_LOGGED_MESSAGES_AMD;
using gl::GL_MAX_DEBUG_LOGGED_MESSAGES_ARB;
using gl::GL_DEBUG_LOGGED_MESSAGES;
using gl::GL_DEBUG_LOGGED_MESSAGES_AMD;
using gl::GL_DEBUG_LOGGED_MESSAGES_ARB;
using gl::GL_DEBUG_SEVERITY_HIGH;
using gl::GL_DEBUG_SEVERITY_HIGH_AMD;
using gl::GL_DEBUG_SEVERITY_HIGH_ARB;
using gl::GL_DEBUG_SEVERITY_MEDIUM;
using gl::GL_DEBUG_SEVERITY_MEDIUM_AMD;
using gl::GL_DEBUG_SEVERITY_MEDIUM_ARB;
using gl::GL_DEBUG_SEVERITY_LOW;
using gl::GL_DEBUG_SEVERITY_LOW_AMD;
using gl::GL_DEBUG_SEVERITY_LOW_ARB;
using gl::GL_DEBUG_CATEGORY_API_ERROR_AMD;
using gl::GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD;
using gl::GL_DEBUG_CATEGORY_DEPRECATION_AMD;
using gl::GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD;
using gl::GL_DEBUG_CATEGORY_PERFORMANCE_AMD;
using gl::GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD;
using gl::GL_DEBUG_CATEGORY_APPLICATION_AMD;
using gl::GL_DEBUG_CATEGORY_OTHER_AMD;
using gl::GL_BUFFER_OBJECT_EXT;
using gl::GL_DATA_BUFFER_AMD;
using gl::GL_PERFORMANCE_MONITOR_AMD;
using gl::GL_QUERY_OBJECT_AMD;
using gl::GL_QUERY_OBJECT_EXT;
using gl::GL_VERTEX_ARRAY_OBJECT_AMD;
using gl::GL_VERTEX_ARRAY_OBJECT_EXT;
using gl::GL_SAMPLER_OBJECT_AMD;
using gl::GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD;
using gl::GL_QUERY_BUFFER;
using gl::GL_QUERY_BUFFER_AMD;
using gl::GL_QUERY_BUFFER_BINDING;
using gl::GL_QUERY_BUFFER_BINDING_AMD;
using gl::GL_QUERY_RESULT_NO_WAIT;
using gl::GL_QUERY_RESULT_NO_WAIT_AMD;
using gl::GL_VIRTUAL_PAGE_SIZE_X_AMD;
using gl::GL_VIRTUAL_PAGE_SIZE_X_ARB;
using gl::GL_VIRTUAL_PAGE_SIZE_Y_AMD;
using gl::GL_VIRTUAL_PAGE_SIZE_Y_ARB;
using gl::GL_VIRTUAL_PAGE_SIZE_Z_AMD;
using gl::GL_VIRTUAL_PAGE_SIZE_Z_ARB;
using gl::GL_MAX_SPARSE_TEXTURE_SIZE_AMD;
using gl::GL_MAX_SPARSE_TEXTURE_SIZE_ARB;
using gl::GL_MAX_SPARSE_3D_TEXTURE_SIZE_AMD;
using gl::GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB;
using gl::GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS;
using gl::GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB;
using gl::GL_MIN_SPARSE_LEVEL_AMD;
using gl::GL_MIN_LOD_WARNING_AMD;
using gl::GL_TEXTURE_BUFFER_OFFSET;
using gl::GL_TEXTURE_BUFFER_SIZE;
using gl::GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT;
using gl::GL_STREAM_RASTERIZATION_AMD;
using gl::GL_VERTEX_ELEMENT_SWIZZLE_AMD;
using gl::GL_VERTEX_ID_SWIZZLE_AMD;
using gl::GL_TEXTURE_SPARSE_ARB;
using gl::GL_VIRTUAL_PAGE_SIZE_INDEX_ARB;
using gl::GL_NUM_VIRTUAL_PAGE_SIZES_ARB;
using gl::GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB;
using gl::GL_NUM_SPARSE_LEVELS_ARB;
using gl::GL_MAX_SHADER_COMPILER_THREADS_ARB;
using gl::GL_COMPLETION_STATUS_ARB;
using gl::GL_COMPUTE_SHADER;
using gl::GL_MAX_COMPUTE_UNIFORM_BLOCKS;
using gl::GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS;
using gl::GL_MAX_COMPUTE_IMAGE_UNIFORMS;
using gl::GL_MAX_COMPUTE_WORK_GROUP_COUNT;
using gl::GL_MAX_COMPUTE_FIXED_GROUP_SIZE_ARB;
using gl::GL_MAX_COMPUTE_WORK_GROUP_SIZE;
using gl::GL_COMPRESSED_R11_EAC;
using gl::GL_COMPRESSED_SIGNED_R11_EAC;
using gl::GL_COMPRESSED_RG11_EAC;
using gl::GL_COMPRESSED_SIGNED_RG11_EAC;
using gl::GL_COMPRESSED_RGB8_ETC2;
using gl::GL_COMPRESSED_SRGB8_ETC2;
using gl::GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
using gl::GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
using gl::GL_COMPRESSED_RGBA8_ETC2_EAC;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
using gl::GL_BLEND_PREMULTIPLIED_SRC_NV;
using gl::GL_BLEND_OVERLAP_NV;
using gl::GL_UNCORRELATED_NV;
using gl::GL_DISJOINT_NV;
using gl::GL_CONJOINT_NV;
using gl::GL_BLEND_ADVANCED_COHERENT_KHR;
using gl::GL_BLEND_ADVANCED_COHERENT_NV;
using gl::GL_SRC_NV;
using gl::GL_DST_NV;
using gl::GL_SRC_OVER_NV;
using gl::GL_DST_OVER_NV;
using gl::GL_SRC_IN_NV;
using gl::GL_DST_IN_NV;
using gl::GL_SRC_OUT_NV;
using gl::GL_DST_OUT_NV;
using gl::GL_SRC_ATOP_NV;
using gl::GL_DST_ATOP_NV;
using gl::GL_PLUS_NV;
using gl::GL_PLUS_DARKER_NV;
using gl::GL_MULTIPLY_KHR;
using gl::GL_MULTIPLY_NV;
using gl::GL_SCREEN_KHR;
using gl::GL_SCREEN_NV;
using gl::GL_OVERLAY_KHR;
using gl::GL_OVERLAY_NV;
using gl::GL_DARKEN_KHR;
using gl::GL_DARKEN_NV;
using gl::GL_LIGHTEN_KHR;
using gl::GL_LIGHTEN_NV;
using gl::GL_COLORDODGE_KHR;
using gl::GL_COLORDODGE_NV;
using gl::GL_COLORBURN_KHR;
using gl::GL_COLORBURN_NV;
using gl::GL_HARDLIGHT_KHR;
using gl::GL_HARDLIGHT_NV;
using gl::GL_SOFTLIGHT_KHR;
using gl::GL_SOFTLIGHT_NV;
using gl::GL_DIFFERENCE_KHR;
using gl::GL_DIFFERENCE_NV;
using gl::GL_MINUS_NV;
using gl::GL_EXCLUSION_KHR;
using gl::GL_EXCLUSION_NV;
using gl::GL_CONTRAST_NV;
using gl::GL_INVERT_RGB_NV;
using gl::GL_LINEARDODGE_NV;
using gl::GL_LINEARBURN_NV;
using gl::GL_VIVIDLIGHT_NV;
using gl::GL_LINEARLIGHT_NV;
using gl::GL_PINLIGHT_NV;
using gl::GL_HARDMIX_NV;
using gl::GL_HSL_HUE_KHR;
using gl::GL_HSL_HUE_NV;
using gl::GL_HSL_SATURATION_KHR;
using gl::GL_HSL_SATURATION_NV;
using gl::GL_HSL_COLOR_KHR;
using gl::GL_HSL_COLOR_NV;
using gl::GL_HSL_LUMINOSITY_KHR;
using gl::GL_HSL_LUMINOSITY_NV;
using gl::GL_PLUS_CLAMPED_NV;
using gl::GL_PLUS_CLAMPED_ALPHA_NV;
using gl::GL_MINUS_CLAMPED_NV;
using gl::GL_INVERT_OVG_NV;
using gl::GL_PRIMITIVE_BOUNDING_BOX_ARB;
using gl::GL_ATOMIC_COUNTER_BUFFER;
using gl::GL_ATOMIC_COUNTER_BUFFER_BINDING;
using gl::GL_ATOMIC_COUNTER_BUFFER_START;
using gl::GL_ATOMIC_COUNTER_BUFFER_SIZE;
using gl::GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE;
using gl::GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS;
using gl::GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER;
using gl::GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER;
using gl::GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS;
using gl::GL_MAX_VERTEX_ATOMIC_COUNTERS;
using gl::GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS;
using gl::GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS;
using gl::GL_MAX_GEOMETRY_ATOMIC_COUNTERS;
using gl::GL_MAX_FRAGMENT_ATOMIC_COUNTERS;
using gl::GL_MAX_COMBINED_ATOMIC_COUNTERS;
using gl::GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE;
using gl::GL_ACTIVE_ATOMIC_COUNTER_BUFFERS;
using gl::GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX;
using gl::GL_UNSIGNED_INT_ATOMIC_COUNTER;
using gl::GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS;
using gl::GL_FRAGMENT_COVERAGE_TO_COLOR_NV;
using gl::GL_FRAGMENT_COVERAGE_COLOR_NV;
using gl::GL_DEBUG_OUTPUT;
using gl::GL_UNIFORM;
using gl::GL_UNIFORM_BLOCK;
using gl::GL_PROGRAM_INPUT;
using gl::GL_PROGRAM_OUTPUT;
using gl::GL_BUFFER_VARIABLE;
using gl::GL_SHADER_STORAGE_BLOCK;
using gl::GL_IS_PER_PATCH;
using gl::GL_VERTEX_SUBROUTINE;
using gl::GL_TESS_CONTROL_SUBROUTINE;
using gl::GL_TESS_EVALUATION_SUBROUTINE;
using gl::GL_GEOMETRY_SUBROUTINE;
using gl::GL_FRAGMENT_SUBROUTINE;
using gl::GL_COMPUTE_SUBROUTINE;
using gl::GL_VERTEX_SUBROUTINE_UNIFORM;
using gl::GL_TESS_CONTROL_SUBROUTINE_UNIFORM;
using gl::GL_TESS_EVALUATION_SUBROUTINE_UNIFORM;
using gl::GL_GEOMETRY_SUBROUTINE_UNIFORM;
using gl::GL_FRAGMENT_SUBROUTINE_UNIFORM;
using gl::GL_COMPUTE_SUBROUTINE_UNIFORM;
using gl::GL_TRANSFORM_FEEDBACK_VARYING;
using gl::GL_ACTIVE_RESOURCES;
using gl::GL_MAX_NAME_LENGTH;
using gl::GL_MAX_NUM_ACTIVE_VARIABLES;
using gl::GL_MAX_NUM_COMPATIBLE_SUBROUTINES;
using gl::GL_NAME_LENGTH;
using gl::GL_TYPE;
using gl::GL_ARRAY_SIZE;
using gl::GL_OFFSET;
using gl::GL_BLOCK_INDEX;
using gl::GL_ARRAY_STRIDE;
using gl::GL_MATRIX_STRIDE;
using gl::GL_IS_ROW_MAJOR;
using gl::GL_ATOMIC_COUNTER_BUFFER_INDEX;
using gl::GL_BUFFER_BINDING;
using gl::GL_BUFFER_DATA_SIZE;
using gl::GL_NUM_ACTIVE_VARIABLES;
using gl::GL_ACTIVE_VARIABLES;
using gl::GL_REFERENCED_BY_VERTEX_SHADER;
using gl::GL_REFERENCED_BY_TESS_CONTROL_SHADER;
using gl::GL_REFERENCED_BY_TESS_EVALUATION_SHADER;
using gl::GL_REFERENCED_BY_GEOMETRY_SHADER;
using gl::GL_REFERENCED_BY_FRAGMENT_SHADER;
using gl::GL_REFERENCED_BY_COMPUTE_SHADER;
using gl::GL_TOP_LEVEL_ARRAY_SIZE;
using gl::GL_TOP_LEVEL_ARRAY_STRIDE;
using gl::GL_LOCATION;
using gl::GL_LOCATION_INDEX;
using gl::GL_FRAMEBUFFER_DEFAULT_WIDTH;
using gl::GL_FRAMEBUFFER_DEFAULT_HEIGHT;
using gl::GL_FRAMEBUFFER_DEFAULT_LAYERS;
using gl::GL_FRAMEBUFFER_DEFAULT_SAMPLES;
using gl::GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS;
using gl::GL_MAX_FRAMEBUFFER_WIDTH;
using gl::GL_MAX_FRAMEBUFFER_HEIGHT;
using gl::GL_MAX_FRAMEBUFFER_LAYERS;
using gl::GL_MAX_FRAMEBUFFER_SAMPLES;
using gl::GL_RASTER_MULTISAMPLE_EXT;
using gl::GL_RASTER_SAMPLES_EXT;
using gl::GL_MAX_RASTER_SAMPLES_EXT;
using gl::GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT;
using gl::GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT;
using gl::GL_EFFECTIVE_RASTER_SAMPLES_EXT;
using gl::GL_DEPTH_SAMPLES_NV;
using gl::GL_STENCIL_SAMPLES_NV;
using gl::GL_MIXED_DEPTH_SAMPLES_SUPPORTED_NV;
using gl::GL_MIXED_STENCIL_SAMPLES_SUPPORTED_NV;
using gl::GL_COVERAGE_MODULATION_TABLE_NV;
using gl::GL_COVERAGE_MODULATION_NV;
using gl::GL_COVERAGE_MODULATION_TABLE_SIZE_NV;
using gl::GL_WARP_SIZE_NV;
using gl::GL_WARPS_PER_SM_NV;
using gl::GL_SM_COUNT_NV;
using gl::GL_FILL_RECTANGLE_NV;
using gl::GL_SAMPLE_LOCATION_SUBPIXEL_BITS_ARB;
using gl::GL_SAMPLE_LOCATION_SUBPIXEL_BITS_NV;
using gl::GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_ARB;
using gl::GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_NV;
using gl::GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_ARB;
using gl::GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_NV;
using gl::GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_ARB;
using gl::GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV;
using gl::GL_PROGRAMMABLE_SAMPLE_LOCATION_ARB;
using gl::GL_PROGRAMMABLE_SAMPLE_LOCATION_NV;
using gl::GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_ARB;
using gl::GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV;
using gl::GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_ARB;
using gl::GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_NV;
using gl::GL_MAX_COMPUTE_VARIABLE_GROUP_INVOCATIONS_ARB;
using gl::GL_MAX_COMPUTE_VARIABLE_GROUP_SIZE_ARB;
using gl::GL_CONSERVATIVE_RASTERIZATION_NV;
using gl::GL_SUBPIXEL_PRECISION_BIAS_X_BITS_NV;
using gl::GL_SUBPIXEL_PRECISION_BIAS_Y_BITS_NV;
using gl::GL_MAX_SUBPIXEL_PRECISION_BIAS_BITS_NV;
using gl::GL_LOCATION_COMPONENT;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_INDEX;
using gl::GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE;
using gl::GL_CLIP_ORIGIN;
using gl::GL_CLIP_DEPTH_MODE;
using gl::GL_NEGATIVE_ONE_TO_ONE;
using gl::GL_ZERO_TO_ONE;
using gl::GL_CLEAR_TEXTURE;
using gl::GL_TEXTURE_REDUCTION_MODE_ARB;
using gl::GL_WEIGHTED_AVERAGE_ARB;
using gl::GL_FONT_GLYPHS_AVAILABLE_NV;
using gl::GL_FONT_TARGET_UNAVAILABLE_NV;
using gl::GL_FONT_UNAVAILABLE_NV;
using gl::GL_FONT_UNINTELLIGIBLE_NV;
using gl::GL_STANDARD_FONT_FORMAT_NV;
using gl::GL_FRAGMENT_INPUT_NV;
using gl::GL_UNIFORM_BUFFER_UNIFIED_NV;
using gl::GL_UNIFORM_BUFFER_ADDRESS_NV;
using gl::GL_UNIFORM_BUFFER_LENGTH_NV;
using gl::GL_MULTISAMPLES_NV;
using gl::GL_SUPERSAMPLE_SCALE_X_NV;
using gl::GL_SUPERSAMPLE_SCALE_Y_NV;
using gl::GL_CONFORMANT_NV;
using gl::GL_CONSERVATIVE_RASTER_DILATE_NV;
using gl::GL_CONSERVATIVE_RASTER_DILATE_RANGE_NV;
using gl::GL_CONSERVATIVE_RASTER_DILATE_GRANULARITY_NV;
using gl::GL_NUM_SAMPLE_COUNTS;
using gl::GL_MULTISAMPLE_LINE_WIDTH_RANGE_ARB;
using gl::GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY_ARB;
using gl::GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
using gl::GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR;
using gl::GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;
using gl::GL_PERFQUERY_COUNTER_EVENT_INTEL;
using gl::GL_PERFQUERY_COUNTER_DURATION_NORM_INTEL;
using gl::GL_PERFQUERY_COUNTER_DURATION_RAW_INTEL;
using gl::GL_PERFQUERY_COUNTER_THROUGHPUT_INTEL;
using gl::GL_PERFQUERY_COUNTER_RAW_INTEL;
using gl::GL_PERFQUERY_COUNTER_TIMESTAMP_INTEL;
using gl::GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL;
using gl::GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL;
using gl::GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL;
using gl::GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL;
using gl::GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL;
using gl::GL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL;
using gl::GL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL;
using gl::GL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL;
using gl::GL_PERFQUERY_GPA_EXTENDED_COUNTERS_INTEL;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR;
using gl::GL_MAX_VIEWS_OVR;
using gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR;
using gl::GL_SHARED_EDGE_NV;
using gl::GL_ROUNDED_RECT_NV;
using gl::GL_RELATIVE_ROUNDED_RECT_NV;
using gl::GL_ROUNDED_RECT2_NV;
using gl::GL_RELATIVE_ROUNDED_RECT2_NV;
using gl::GL_ROUNDED_RECT4_NV;
using gl::GL_RELATIVE_ROUNDED_RECT4_NV;
using gl::GL_ROUNDED_RECT8_NV;
using gl::GL_RELATIVE_ROUNDED_RECT8_NV;
using gl::GL_RESTART_PATH_NV;
using gl::GL_DUP_FIRST_CUBIC_CURVE_TO_NV;
using gl::GL_DUP_LAST_CUBIC_CURVE_TO_NV;
using gl::GL_RECT_NV;
using gl::GL_RELATIVE_RECT_NV;
using gl::GL_CIRCULAR_CCW_ARC_TO_NV;
using gl::GL_CIRCULAR_CW_ARC_TO_NV;
using gl::GL_CIRCULAR_TANGENT_ARC_TO_NV;
using gl::GL_ARC_TO_NV;
using gl::GL_RELATIVE_ARC_TO_NV;
using gl::GL_CULL_VERTEX_IBM;
using gl::GL_ALL_STATIC_DATA_IBM;
using gl::GL_STATIC_VERTEX_ARRAY_IBM;
using gl::GL_VERTEX_ARRAY_LIST_IBM;
using gl::GL_NORMAL_ARRAY_LIST_IBM;
using gl::GL_COLOR_ARRAY_LIST_IBM;
using gl::GL_INDEX_ARRAY_LIST_IBM;
using gl::GL_TEXTURE_COORD_ARRAY_LIST_IBM;
using gl::GL_EDGE_FLAG_ARRAY_LIST_IBM;
using gl::GL_FOG_COORDINATE_ARRAY_LIST_IBM;
using gl::GL_SECONDARY_COLOR_ARRAY_LIST_IBM;
using gl::GL_VERTEX_ARRAY_LIST_STRIDE_IBM;
using gl::GL_NORMAL_ARRAY_LIST_STRIDE_IBM;
using gl::GL_COLOR_ARRAY_LIST_STRIDE_IBM;
using gl::GL_INDEX_ARRAY_LIST_STRIDE_IBM;
using gl::GL_TEXTURE_COORD_ARRAY_LIST_STRIDE_IBM;
using gl::GL_EDGE_FLAG_ARRAY_LIST_STRIDE_IBM;
using gl::GL_FOG_COORDINATE_ARRAY_LIST_STRIDE_IBM;
using gl::GL_SECONDARY_COLOR_ARRAY_LIST_STRIDE_IBM;

} // namespace gl10ext
