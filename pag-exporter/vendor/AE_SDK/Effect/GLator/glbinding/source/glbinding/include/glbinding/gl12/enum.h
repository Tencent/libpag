#pragma once

#include <glbinding/nogl.h>

#include <glbinding/gl/enum.h>


namespace gl12
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

// BlendingFactorDest

using gl::GL_ZERO;
using gl::GL_SRC_COLOR;
using gl::GL_ONE_MINUS_SRC_COLOR;
using gl::GL_SRC_ALPHA;
using gl::GL_ONE_MINUS_SRC_ALPHA;
using gl::GL_DST_ALPHA;
using gl::GL_ONE_MINUS_DST_ALPHA;
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
using gl::GL_POLYGON_OFFSET_FILL;
using gl::GL_VERTEX_ARRAY;
using gl::GL_NORMAL_ARRAY;
using gl::GL_COLOR_ARRAY;
using gl::GL_INDEX_ARRAY;
using gl::GL_TEXTURE_COORD_ARRAY;
using gl::GL_EDGE_FLAG_ARRAY;

// ErrorCode

using gl::GL_NO_ERROR;
using gl::GL_INVALID_ENUM;
using gl::GL_INVALID_VALUE;
using gl::GL_INVALID_OPERATION;
using gl::GL_STACK_OVERFLOW;
using gl::GL_STACK_UNDERFLOW;
using gl::GL_OUT_OF_MEMORY;

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

// FogCoordinatePointerType

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FogMode

using gl::GL_EXP;
using gl::GL_EXP2;
using gl::GL_LINEAR;

// FogParameter

using gl::GL_FOG_INDEX;
using gl::GL_FOG_DENSITY;
using gl::GL_FOG_START;
using gl::GL_FOG_END;
using gl::GL_FOG_MODE;
using gl::GL_FOG_COLOR;

// FogPointerTypeEXT

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FogPointerTypeIBM

// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// FrontFaceDirection

using gl::GL_CW;
using gl::GL_CCW;

// GetMapQuery

using gl::GL_COEFF;
using gl::GL_ORDER;
using gl::GL_DOMAIN;

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
using gl::GL_MODELVIEW_STACK_DEPTH;
using gl::GL_PROJECTION_STACK_DEPTH;
using gl::GL_TEXTURE_STACK_DEPTH;
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
// using gl::GL_POLYGON_OFFSET_FILL; // reuse EnableCap
using gl::GL_POLYGON_OFFSET_FACTOR;
using gl::GL_TEXTURE_BINDING_1D;
using gl::GL_TEXTURE_BINDING_2D;
using gl::GL_TEXTURE_BINDING_3D;
// using gl::GL_VERTEX_ARRAY; // reuse EnableCap
// using gl::GL_NORMAL_ARRAY; // reuse EnableCap
// using gl::GL_COLOR_ARRAY; // reuse EnableCap
// using gl::GL_INDEX_ARRAY; // reuse EnableCap
// using gl::GL_TEXTURE_COORD_ARRAY; // reuse EnableCap
// using gl::GL_EDGE_FLAG_ARRAY; // reuse EnableCap
using gl::GL_VERTEX_ARRAY_SIZE;
using gl::GL_VERTEX_ARRAY_TYPE;
using gl::GL_VERTEX_ARRAY_STRIDE;
using gl::GL_NORMAL_ARRAY_TYPE;
using gl::GL_NORMAL_ARRAY_STRIDE;
using gl::GL_COLOR_ARRAY_SIZE;
using gl::GL_COLOR_ARRAY_TYPE;
using gl::GL_COLOR_ARRAY_STRIDE;
using gl::GL_INDEX_ARRAY_TYPE;
using gl::GL_INDEX_ARRAY_STRIDE;
using gl::GL_TEXTURE_COORD_ARRAY_SIZE;
using gl::GL_TEXTURE_COORD_ARRAY_TYPE;
using gl::GL_TEXTURE_COORD_ARRAY_STRIDE;
using gl::GL_EDGE_FLAG_ARRAY_STRIDE;
using gl::GL_LIGHT_MODEL_COLOR_CONTROL;
using gl::GL_ALIASED_POINT_SIZE_RANGE;
using gl::GL_ALIASED_LINE_WIDTH_RANGE;

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
using gl::GL_NORMAL_ARRAY_POINTER;
using gl::GL_COLOR_ARRAY_POINTER;
using gl::GL_INDEX_ARRAY_POINTER;
using gl::GL_TEXTURE_COORD_ARRAY_POINTER;
using gl::GL_EDGE_FLAG_ARRAY_POINTER;

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

// LightEnvModeSGIX

// using gl::GL_ADD; // reuse AccumOp
using gl::GL_REPLACE;
using gl::GL_MODULATE;

// LightModelColorControl

using gl::GL_SINGLE_COLOR;
using gl::GL_SEPARATE_SPECULAR_COLOR;

// LightModelParameter

// using gl::GL_LIGHT_MODEL_LOCAL_VIEWER; // reuse GetPName
// using gl::GL_LIGHT_MODEL_TWO_SIDE; // reuse GetPName
// using gl::GL_LIGHT_MODEL_AMBIENT; // reuse GetPName
// using gl::GL_LIGHT_MODEL_COLOR_CONTROL; // reuse GetPName

// LightName

// using gl::GL_LIGHT0; // reuse EnableCap
// using gl::GL_LIGHT1; // reuse EnableCap
// using gl::GL_LIGHT2; // reuse EnableCap
// using gl::GL_LIGHT3; // reuse EnableCap
// using gl::GL_LIGHT4; // reuse EnableCap
// using gl::GL_LIGHT5; // reuse EnableCap
// using gl::GL_LIGHT6; // reuse EnableCap
// using gl::GL_LIGHT7; // reuse EnableCap

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
using gl::GL_PROJECTION;
using gl::GL_TEXTURE;

// MeshMode1

using gl::GL_POINT;
using gl::GL_LINE;

// MeshMode2

// using gl::GL_POINT; // reuse MeshMode1
// using gl::GL_LINE; // reuse MeshMode1
using gl::GL_FILL;

// NormalPointerType

// using gl::GL_BYTE; // reuse ColorPointerType
// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

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
using gl::GL_PACK_IMAGE_HEIGHT;
using gl::GL_UNPACK_SKIP_IMAGES;
using gl::GL_UNPACK_IMAGE_HEIGHT;

// PixelTexGenMode

// using gl::GL_NONE; // reuse DrawBufferMode
// using gl::GL_RGB; // reuse PixelFormat
// using gl::GL_RGBA; // reuse PixelFormat
// using gl::GL_LUMINANCE; // reuse PixelFormat
// using gl::GL_LUMINANCE_ALPHA; // reuse PixelFormat

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
using gl::GL_UNSIGNED_SHORT_4_4_4_4;
using gl::GL_UNSIGNED_SHORT_5_5_5_1;
using gl::GL_UNSIGNED_INT_8_8_8_8;
using gl::GL_UNSIGNED_INT_10_10_10_2;

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

// TextureEnvParameter

using gl::GL_TEXTURE_ENV_MODE;
using gl::GL_TEXTURE_ENV_COLOR;

// TextureEnvTarget

using gl::GL_TEXTURE_ENV;

// TextureGenMode

using gl::GL_EYE_LINEAR;
using gl::GL_OBJECT_LINEAR;
using gl::GL_SPHERE_MAP;

// TextureGenParameter

using gl::GL_TEXTURE_GEN_MODE;
using gl::GL_OBJECT_PLANE;
using gl::GL_EYE_PLANE;

// TextureMagFilter

using gl::GL_NEAREST;
// using gl::GL_LINEAR; // reuse FogMode

// TextureMinFilter

// using gl::GL_NEAREST; // reuse TextureMagFilter
// using gl::GL_LINEAR; // reuse FogMode
using gl::GL_NEAREST_MIPMAP_NEAREST;
using gl::GL_LINEAR_MIPMAP_NEAREST;
using gl::GL_NEAREST_MIPMAP_LINEAR;
using gl::GL_LINEAR_MIPMAP_LINEAR;

// TextureParameterName

// using gl::GL_TEXTURE_BORDER_COLOR; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MAG_FILTER; // reuse GetTextureParameter
// using gl::GL_TEXTURE_MIN_FILTER; // reuse GetTextureParameter
// using gl::GL_TEXTURE_WRAP_S; // reuse GetTextureParameter
// using gl::GL_TEXTURE_WRAP_T; // reuse GetTextureParameter
// using gl::GL_TEXTURE_PRIORITY; // reuse GetTextureParameter
using gl::GL_TEXTURE_WRAP_R;

// TextureTarget

// using gl::GL_TEXTURE_1D; // reuse EnableCap
// using gl::GL_TEXTURE_2D; // reuse EnableCap
using gl::GL_PROXY_TEXTURE_1D;
using gl::GL_PROXY_TEXTURE_2D;
using gl::GL_TEXTURE_3D;
using gl::GL_PROXY_TEXTURE_3D;
using gl::GL_TEXTURE_MIN_LOD;
using gl::GL_TEXTURE_MAX_LOD;
using gl::GL_TEXTURE_BASE_LEVEL;
using gl::GL_TEXTURE_MAX_LEVEL;

// TextureWrapMode

using gl::GL_CLAMP;
using gl::GL_REPEAT;
using gl::GL_CLAMP_TO_EDGE;

// VertexPointerType

// using gl::GL_SHORT; // reuse ColorPointerType
// using gl::GL_INT; // reuse ColorPointerType
// using gl::GL_FLOAT; // reuse ColorPointerType
// using gl::GL_DOUBLE; // reuse ColorPointerType

// __UNGROUPED__

using gl::GL_RESCALE_NORMAL;
using gl::GL_TEXTURE_DEPTH;
using gl::GL_MAX_3D_TEXTURE_SIZE;
using gl::GL_BGR;
using gl::GL_BGRA;
using gl::GL_MAX_ELEMENTS_VERTICES;
using gl::GL_MAX_ELEMENTS_INDICES;
using gl::GL_UNSIGNED_BYTE_2_3_3_REV;
using gl::GL_UNSIGNED_SHORT_5_6_5;
using gl::GL_UNSIGNED_SHORT_5_6_5_REV;
using gl::GL_UNSIGNED_SHORT_4_4_4_4_REV;
using gl::GL_UNSIGNED_SHORT_1_5_5_5_REV;
using gl::GL_UNSIGNED_INT_8_8_8_8_REV;
using gl::GL_UNSIGNED_INT_2_10_10_10_REV;

} // namespace gl12
