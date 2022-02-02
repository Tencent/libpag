/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cinttypes>
#include <memory>

namespace GL {
#undef FALSE
#undef TRUE
// The following constants consist of the intersection of GL constants
// exported by GLES 1.0, GLES 2.0, and desktop GL required by the system.

static constexpr unsigned DEPTH_BUFFER_BIT = 0x00000100;
static constexpr unsigned STENCIL_BUFFER_BIT = 0x00000400;
static constexpr unsigned COLOR_BUFFER_BIT = 0x00004000;

// Boolean
static constexpr unsigned FALSE = 0;
static constexpr unsigned TRUE = 1;

// BeginMode
static constexpr unsigned POINTS = 0x0000;
static constexpr unsigned LINES = 0x0001;
static constexpr unsigned LINE_LOOP = 0x0002;
static constexpr unsigned LINE_STRIP = 0x0003;
static constexpr unsigned TRIANGLES = 0x0004;
static constexpr unsigned TRIANGLE_STRIP = 0x0005;
static constexpr unsigned TRIANGLE_FAN = 0x0006;

// Basic OpenGL blend equations
static constexpr unsigned FUNC_ADD = 0x8006;
static constexpr unsigned BLEND_EQUATION = 0x8009;
static constexpr unsigned BLEND_EQUATION_RGB = 0x8009; /* same as BLEND_EQUATION */
static constexpr unsigned BLEND_EQUATION_ALPHA = 0x883D;
static constexpr unsigned FUNC_SUBTRACT = 0x800A;
static constexpr unsigned FUNC_REVERSE_SUBTRACT = 0x800B;

// KHR_blend_equation_advanced
static constexpr unsigned SCREEN = 0x9295;
static constexpr unsigned OVERLAY = 0x9296;
static constexpr unsigned DARKEN = 0x9297;
static constexpr unsigned LIGHTEN = 0x9298;
static constexpr unsigned COLORDODGE = 0x9299;
static constexpr unsigned COLORBURN = 0x929A;
static constexpr unsigned HARDLIGHT = 0x929B;
static constexpr unsigned SOFTLIGHT = 0x929C;
static constexpr unsigned DIFFERENCE = 0x929E;
static constexpr unsigned EXCLUSION = 0x92A0;
static constexpr unsigned MULTIPLY = 0x9294;
static constexpr unsigned HSL_HUE = 0x92AD;
static constexpr unsigned HSL_SATURATION = 0x92AE;
static constexpr unsigned HSL_COLOR = 0x92AF;
static constexpr unsigned HSL_LUMINOSITY = 0x92B0;

// BlendingFactorDest
static constexpr unsigned ZERO = 0;
static constexpr unsigned ONE = 1;
static constexpr unsigned SRC_COLOR = 0x0300;
static constexpr unsigned ONE_MINUS_SRC_COLOR = 0x0301;
static constexpr unsigned SRC_ALPHA = 0x0302;
static constexpr unsigned ONE_MINUS_SRC_ALPHA = 0x0303;
static constexpr unsigned DST_ALPHA = 0x0304;
static constexpr unsigned ONE_MINUS_DST_ALPHA = 0x0305;

// BlendingFactorSrc
static constexpr unsigned DST_COLOR = 0x0306;
static constexpr unsigned ONE_MINUS_DST_COLOR = 0x0307;
static constexpr unsigned SRC_ALPHA_SATURATE = 0x0308;

// ExtendedBlendFactors
static constexpr unsigned SRC1_COLOR = 0x88F9;
static constexpr unsigned ONE_MINUS_SRC1_COLOR = 0x88FA;
//      SRC1_ALPHA
static constexpr unsigned ONE_MINUS_SRC1_ALPHA = 0x88FB;

// Separate Blend Functions
static constexpr unsigned BLEND_DST_RGB = 0x80C8;
static constexpr unsigned BLEND_SRC_RGB = 0x80C9;
static constexpr unsigned BLEND_DST_ALPHA = 0x80CA;
static constexpr unsigned BLEND_SRC_ALPHA = 0x80CB;
static constexpr unsigned CONSTANT_COLOR = 0x8001;
static constexpr unsigned ONE_MINUS_CONSTANT_COLOR = 0x8002;
static constexpr unsigned CONSTANT_ALPHA = 0x8003;
static constexpr unsigned ONE_MINUS_CONSTANT_ALPHA = 0x8004;
static constexpr unsigned BLEND_COLOR = 0x8005;

// Buffer Objects
static constexpr unsigned ARRAY_BUFFER = 0x8892;
static constexpr unsigned ELEMENT_ARRAY_BUFFER = 0x8893;
static constexpr unsigned DRAW_INDIRECT_BUFFER = 0x8F3F;
static constexpr unsigned TEXTURE_BUFFER = 0x8C2A;
static constexpr unsigned ARRAY_BUFFER_BINDING = 0x8894;
static constexpr unsigned ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
static constexpr unsigned DRAW_INDIRECT_BUFFER_BINDING = 0x8F43;
static constexpr unsigned VERTEX_ARRAY_BINDING = 0x85B5;
static constexpr unsigned PIXEL_PACK_BUFFER = 0x88EB;
static constexpr unsigned PIXEL_UNPACK_BUFFER = 0x88EC;

static constexpr unsigned PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM = 0x78EC;
static constexpr unsigned PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM = 0x78ED;

static constexpr unsigned STREAM_DRAW = 0x88E0;
static constexpr unsigned STREAM_READ = 0x88E1;
static constexpr unsigned STATIC_DRAW = 0x88E4;
static constexpr unsigned STATIC_READ = 0x88E5;
static constexpr unsigned DYNAMIC_DRAW = 0x88E8;
static constexpr unsigned DYNAMIC_READ = 0x88E9;

static constexpr unsigned BUFFER_SIZE = 0x8764;
static constexpr unsigned BUFFER_USAGE = 0x8765;

static constexpr unsigned CURRENT_VERTEX_ATTRIB = 0x8626;

// CullFaceMode
static constexpr unsigned FRONT = 0x0404;
static constexpr unsigned BACK = 0x0405;
static constexpr unsigned FRONT_AND_BACK = 0x0408;

// EnableCap
static constexpr unsigned TEXTURE_2D = 0x0DE1;
static constexpr unsigned TEXTURE_RECTANGLE = 0x84F5;
static constexpr unsigned CULL_FACE = 0x0B44;
static constexpr unsigned BLEND = 0x0BE2;
static constexpr unsigned DITHER = 0x0BD0;
static constexpr unsigned STENCIL_TEST = 0x0B90;
static constexpr unsigned DEPTH_TEST = 0x0B71;
static constexpr unsigned SCISSOR_TEST = 0x0C11;
static constexpr unsigned POLYGON_OFFSET_FILL = 0x8037;
static constexpr unsigned SAMPLE_ALPHA_TO_COVERAGE = 0x809E;
static constexpr unsigned SAMPLE_COVERAGE = 0x80A0;
static constexpr unsigned POLYGON_SMOOTH = 0x0B41;
static constexpr unsigned POLYGON_STIPPLE = 0x0B42;
static constexpr unsigned COLOR_LOGIC_OP = 0x0BF2;
static constexpr unsigned COLOR_TABLE = 0x80D0;
static constexpr unsigned INDEX_LOGIC_OP = 0x0BF1;
static constexpr unsigned VERTEX_PROGRAM_POINT_SIZE = 0x8642;
static constexpr unsigned FRAMEBUFFER_SRGB = 0x8DB9;
static constexpr unsigned SHADER_PIXEL_LOCAL_STORAGE = 0x8F64;
static constexpr unsigned SAMPLE_SHADING = 0x8C36;

// ErrorCode
static constexpr unsigned NO_ERROR = 0;
static constexpr unsigned INVALID_ENUM = 0x0500;
static constexpr unsigned INVALID_VALUE = 0x0501;
static constexpr unsigned INVALID_OPERATION = 0x0502;
static constexpr unsigned OUT_OF_MEMORY = 0x0505;
static constexpr unsigned CONTEXT_LOST = 0x300E;

// FrontFaceDirection
static constexpr unsigned CW = 0x0900;
static constexpr unsigned CCW = 0x0901;

// GetPName
static constexpr unsigned ALIASED_POINT_SIZE_RANGE = 0x846D;
static constexpr unsigned ALIASED_LINE_WIDTH_RANGE = 0x846E;
static constexpr unsigned CULL_FACE_MODE = 0x0B45;
static constexpr unsigned FRONT_FACE = 0x0B46;
static constexpr unsigned DEPTH_RANGE = 0x0B70;
static constexpr unsigned DEPTH_WRITEMASK = 0x0B72;
static constexpr unsigned DEPTH_CLEAR_VALUE = 0x0B73;
static constexpr unsigned DEPTH_FUNC = 0x0B74;
static constexpr unsigned STENCIL_CLEAR_VALUE = 0x0B91;
static constexpr unsigned STENCIL_FUNC = 0x0B92;
static constexpr unsigned STENCIL_FAIL = 0x0B94;
static constexpr unsigned STENCIL_PASS_DEPTH_FAIL = 0x0B95;
static constexpr unsigned STENCIL_PASS_DEPTH_PASS = 0x0B96;
static constexpr unsigned STENCIL_REF = 0x0B97;
static constexpr unsigned STENCIL_VALUE_MASK = 0x0B93;
static constexpr unsigned STENCIL_WRITEMASK = 0x0B98;
static constexpr unsigned STENCIL_BACK_FUNC = 0x8800;
static constexpr unsigned STENCIL_BACK_FAIL = 0x8801;
static constexpr unsigned STENCIL_BACK_PASS_DEPTH_FAIL = 0x8802;
static constexpr unsigned STENCIL_BACK_PASS_DEPTH_PASS = 0x8803;
static constexpr unsigned STENCIL_BACK_REF = 0x8CA3;
static constexpr unsigned STENCIL_BACK_VALUE_MASK = 0x8CA4;
static constexpr unsigned STENCIL_BACK_WRITEMASK = 0x8CA5;
static constexpr unsigned VIEWPORT = 0x0BA2;
static constexpr unsigned SCISSOR_BOX = 0x0C10;
static constexpr unsigned COLOR_CLEAR_VALUE = 0x0C22;
static constexpr unsigned COLOR_WRITEMASK = 0x0C23;
static constexpr unsigned UNPACK_ALIGNMENT = 0x0CF5;
static constexpr unsigned PACK_ALIGNMENT = 0x0D05;
static constexpr unsigned PACK_REVERSE_ROW_ORDER = 0x93A4;
static constexpr unsigned MAX_TEXTURE_SIZE = 0x0D33;
static constexpr unsigned TEXTURE_MIN_LOD = 0x813A;
static constexpr unsigned TEXTURE_MAX_LOD = 0x813B;
static constexpr unsigned TEXTURE_BASE_LEVEL = 0x813C;
static constexpr unsigned TEXTURE_MAX_LEVEL = 0x813D;
static constexpr unsigned MAX_VIEWPORT_DIMS = 0x0D3A;
static constexpr unsigned SUBPIXEL_BITS = 0x0D50;
static constexpr unsigned RED_BITS = 0x0D52;
static constexpr unsigned GREEN_BITS = 0x0D53;
static constexpr unsigned BLUE_BITS = 0x0D54;
static constexpr unsigned ALPHA_BITS = 0x0D55;
static constexpr unsigned DEPTH_BITS = 0x0D56;
static constexpr unsigned STENCIL_BITS = 0x0D57;
static constexpr unsigned POLYGON_OFFSET_UNITS = 0x2A00;
static constexpr unsigned POLYGON_OFFSET_FACTOR = 0x8038;
static constexpr unsigned TEXTURE_BINDING_2D = 0x8069;
static constexpr unsigned TEXTURE_BINDING_RECTANGLE = 0x84F6;
static constexpr unsigned SAMPLE_BUFFERS = 0x80A8;
static constexpr unsigned SAMPLES = 0x80A9;
static constexpr unsigned SAMPLE_COVERAGE_VALUE = 0x80AA;
static constexpr unsigned SAMPLE_COVERAGE_INVERT = 0x80AB;
static constexpr unsigned RENDERBUFFER_COVERAGE_SAMPLES = 0x8CAB;
static constexpr unsigned RENDERBUFFER_COLOR_SAMPLES = 0x8E10;
static constexpr unsigned MAX_MULTISAMPLE_COVERAGE_MODES = 0x8E11;
static constexpr unsigned MULTISAMPLE_COVERAGE_MODES = 0x8E12;
static constexpr unsigned MAX_TEXTURE_BUFFER_SIZE = 0x8C2B;

static constexpr unsigned NUM_COMPRESSED_TEXTURE_FORMATS = 0x86A2;
static constexpr unsigned COMPRESSED_TEXTURE_FORMATS = 0x86A3;

// Compressed Texture Formats
static constexpr unsigned COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0;
static constexpr unsigned COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1;
static constexpr unsigned COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2;
static constexpr unsigned COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3;

static constexpr unsigned COMPRESSED_RGB_PVRTC_4BPPV1_IMG = 0x8C00;
static constexpr unsigned COMPRESSED_RGB_PVRTC_2BPPV1_IMG = 0x8C01;
static constexpr unsigned COMPRESSED_RGBA_PVRTC_4BPPV1_IMG = 0x8C02;
static constexpr unsigned COMPRESSED_RGBA_PVRTC_2BPPV1_IMG = 0x8C03;

static constexpr unsigned COMPRESSED_RGBA_PVRTC_2BPPV2_IMG = 0x9137;
static constexpr unsigned COMPRESSED_RGBA_PVRTC_4BPPV2_IMG = 0x9138;

static constexpr unsigned COMPRESSED_ETC1_RGB8 = 0x8D64;

static constexpr unsigned COMPRESSED_R11_EAC = 0x9270;
static constexpr unsigned COMPRESSED_SIGNED_R11_EAC = 0x9271;
static constexpr unsigned COMPRESSED_RG11_EAC = 0x9272;
static constexpr unsigned COMPRESSED_SIGNED_RG11_EAC = 0x9273;

static constexpr unsigned COMPRESSED_RGB8_ETC2 = 0x9274;
static constexpr unsigned COMPRESSED_SRGB8 = 0x9275;
static constexpr unsigned COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1 = 0x9276;
static constexpr unsigned COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1 = 0x9277;
static constexpr unsigned COMPRESSED_RGBA8_ETC2 = 0x9278;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ETC2 = 0x9279;

static constexpr unsigned COMPRESSED_LUMINANCE_LATC1 = 0x8C70;
static constexpr unsigned COMPRESSED_SIGNED_LUMINANCE_LATC1 = 0x8C71;
static constexpr unsigned COMPRESSED_LUMINANCE_ALPHA_LATC2 = 0x8C72;
static constexpr unsigned COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2 = 0x8C73;

static constexpr unsigned COMPRESSED_RED_RGTC1 = 0x8DBB;
static constexpr unsigned COMPRESSED_SIGNED_RED_RGTC1 = 0x8DBC;
static constexpr unsigned COMPRESSED_RED_GREEN_RGTC2 = 0x8DBD;
static constexpr unsigned COMPRESSED_SIGNED_RED_GREEN_RGTC2 = 0x8DBE;

static constexpr unsigned COMPRESSED_3DC_X = 0x87F9;
static constexpr unsigned COMPRESSED_3DC_XY = 0x87FA;

static constexpr unsigned COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
static constexpr unsigned COMPRESSED_SRGB_ALPHA_BPTC_UNORM = 0x8E8D;
static constexpr unsigned COMPRESSED_RGB_BPTC_SIGNED_FLOAT = 0x8E8E;
static constexpr unsigned COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT = 0x8E8F;

static constexpr unsigned COMPRESSED_RGBA_ASTC_4x4 = 0x93B0;
static constexpr unsigned COMPRESSED_RGBA_ASTC_5x4 = 0x93B1;
static constexpr unsigned COMPRESSED_RGBA_ASTC_5x5 = 0x93B2;
static constexpr unsigned COMPRESSED_RGBA_ASTC_6x5 = 0x93B3;
static constexpr unsigned COMPRESSED_RGBA_ASTC_6x6 = 0x93B4;
static constexpr unsigned COMPRESSED_RGBA_ASTC_8x5 = 0x93B5;
static constexpr unsigned COMPRESSED_RGBA_ASTC_8x6 = 0x93B6;
static constexpr unsigned COMPRESSED_RGBA_ASTC_8x8 = 0x93B7;
static constexpr unsigned COMPRESSED_RGBA_ASTC_10x5 = 0x93B8;
static constexpr unsigned COMPRESSED_RGBA_ASTC_10x6 = 0x93B9;
static constexpr unsigned COMPRESSED_RGBA_ASTC_10x8 = 0x93BA;
static constexpr unsigned COMPRESSED_RGBA_ASTC_10x10 = 0x93BB;
static constexpr unsigned COMPRESSED_RGBA_ASTC_12x10 = 0x93BC;
static constexpr unsigned COMPRESSED_RGBA_ASTC_12x12 = 0x93BD;

static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 = 0x93D0;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_5x4 = 0x93D1;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_5x5 = 0x93D2;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_6x5 = 0x93D3;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_6x6 = 0x93D4;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_8x5 = 0x93D5;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_8x6 = 0x93D6;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_8x8 = 0x93D7;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_10x5 = 0x93D8;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_10x6 = 0x93D9;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_10x8 = 0x93DA;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_10x10 = 0x93DB;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_12x10 = 0x93DC;
static constexpr unsigned COMPRESSED_SRGB8_ALPHA8_ASTC_12x12 = 0x93DD;

// HintMode
static constexpr unsigned DONT_CARE = 0x1100;
static constexpr unsigned FASTEST = 0x1101;
static constexpr unsigned NICEST = 0x1102;

// HintTarget
static constexpr unsigned GENERATE_MIPMAP_HINT = 0x8192;

// DataType
static constexpr unsigned BYTE = 0x1400;
static constexpr unsigned UNSIGNED_BYTE = 0x1401;
static constexpr unsigned SHORT = 0x1402;
static constexpr unsigned UNSIGNED_SHORT = 0x1403;
static constexpr unsigned INT = 0x1404;
static constexpr unsigned UNSIGNED_INT = 0x1405;
static constexpr unsigned FLOAT = 0x1406;
static constexpr unsigned HALF_FLOAT = 0x140B;
static constexpr unsigned FIXED = 0x140C;
static constexpr unsigned HALF_FLOAT_OES = 0x8D61;

// Lighting
static constexpr unsigned LIGHTING = 0x0B50;
static constexpr unsigned LIGHT0 = 0x4000;
static constexpr unsigned LIGHT1 = 0x4001;
static constexpr unsigned LIGHT2 = 0x4002;
static constexpr unsigned LIGHT3 = 0x4003;
static constexpr unsigned LIGHT4 = 0x4004;
static constexpr unsigned LIGHT5 = 0x4005;
static constexpr unsigned LIGHT6 = 0x4006;
static constexpr unsigned LIGHT7 = 0x4007;
static constexpr unsigned SPOT_EXPONENT = 0x1205;
static constexpr unsigned SPOT_CUTOFF = 0x1206;
static constexpr unsigned CONSTANT_ATTENUATION = 0x1207;
static constexpr unsigned LINEAR_ATTENUATION = 0x1208;
static constexpr unsigned QUADRATIC_ATTENUATION = 0x1209;
static constexpr unsigned AMBIENT = 0x1200;
static constexpr unsigned DIFFUSE = 0x1201;
static constexpr unsigned SPECULAR = 0x1202;
static constexpr unsigned SHININESS = 0x1601;
static constexpr unsigned EMISSION = 0x1600;
static constexpr unsigned POSITION = 0x1203;
static constexpr unsigned SPOT_DIRECTION = 0x1204;
static constexpr unsigned AMBIENT_AND_DIFFUSE = 0x1602;
static constexpr unsigned COLOR_INDEXES = 0x1603;
static constexpr unsigned LIGHT_MODEL_TWO_SIDE = 0x0B52;
static constexpr unsigned LIGHT_MODEL_LOCAL_VIEWER = 0x0B51;
static constexpr unsigned LIGHT_MODEL_AMBIENT = 0x0B53;
static constexpr unsigned SHADE_MODEL = 0x0B54;
static constexpr unsigned FLAT = 0x1D00;
static constexpr unsigned SMOOTH = 0x1D01;
static constexpr unsigned COLOR_MATERIAL = 0x0B57;
static constexpr unsigned COLOR_MATERIAL_FACE = 0x0B55;
static constexpr unsigned COLOR_MATERIAL_PARAMETER = 0x0B56;
static constexpr unsigned NORMALIZE = 0x0BA1;

// Matrix Mode
static constexpr unsigned MATRIX_MODE = 0x0BA0;
static constexpr unsigned MODELVIEW = 0x1700;
static constexpr unsigned PROJECTION = 0x1701;

// multisample
static constexpr unsigned MULTISAMPLE = 0x809D;
static constexpr unsigned SAMPLE_POSITION = 0x8E50;

// Points
static constexpr unsigned POINT_SMOOTH = 0x0B10;
static constexpr unsigned POINT_SIZE = 0x0B11;
static constexpr unsigned POINT_SIZE_GRANULARITY = 0x0B13;
static constexpr unsigned POINT_SIZE_RANGE = 0x0B12;

// Lines
static constexpr unsigned LINE_SMOOTH = 0x0B20;
static constexpr unsigned LINE_STIPPLE = 0x0B24;
static constexpr unsigned LINE_STIPPLE_PATTERN = 0x0B25;
static constexpr unsigned LINE_STIPPLE_REPEAT = 0x0B26;
static constexpr unsigned LINE_WIDTH = 0x0B21;
static constexpr unsigned LINE_WIDTH_GRANULARITY = 0x0B23;
static constexpr unsigned LINE_WIDTH_RANGE = 0x0B22;

// PolygonMode
static constexpr unsigned POINT = 0x1B00;
static constexpr unsigned LINE = 0x1B01;
static constexpr unsigned FILL = 0x1B02;

// Unsized formats
static constexpr unsigned STENCIL_INDEX = 0x1901;
static constexpr unsigned DEPTH_COMPONENT = 0x1902;
static constexpr unsigned DEPTH_STENCIL = 0x84F9;
static constexpr unsigned RED = 0x1903;
static constexpr unsigned RED_INTEGER = 0x8D94;
static constexpr unsigned GREEN = 0x1904;
static constexpr unsigned BLUE = 0x1905;
static constexpr unsigned ALPHA = 0x1906;
static constexpr unsigned LUMINANCE = 0x1909;
static constexpr unsigned LUMINANCE_ALPHA = 0x190A;
static constexpr unsigned RG_INTEGER = 0x8228;
static constexpr unsigned RGB = 0x1907;
static constexpr unsigned RGB_INTEGER = 0x8D98;
static constexpr unsigned SRGB = 0x8C40;
static constexpr unsigned RGBA = 0x1908;
static constexpr unsigned RG = 0x8227;
static constexpr unsigned SRGB_ALPHA = 0x8C42;
static constexpr unsigned RGBA_INTEGER = 0x8D99;
static constexpr unsigned BGRA = 0x80E1;

// Stencil index sized formats
static constexpr unsigned STENCIL_INDEX4 = 0x8D47;
static constexpr unsigned STENCIL_INDEX8 = 0x8D48;
static constexpr unsigned STENCIL_INDEX16 = 0x8D49;

// Depth component sized formats
static constexpr unsigned DEPTH_COMPONENT16 = 0x81A5;

// Depth stencil sized formats
static constexpr unsigned DEPTH24_STENCIL8 = 0x88F0;

// Red sized formats
static constexpr unsigned R8 = 0x8229;
static constexpr unsigned R16 = 0x822A;
static constexpr unsigned R16F = 0x822D;
static constexpr unsigned R32F = 0x822E;

// Red integer sized formats
static constexpr unsigned R8I = 0x8231;
static constexpr unsigned R8UI = 0x8232;
static constexpr unsigned R16I = 0x8233;
static constexpr unsigned R16UI = 0x8234;
static constexpr unsigned R32I = 0x8235;
static constexpr unsigned R32UI = 0x8236;

// Luminance sized formats
static constexpr unsigned LUMINANCE8 = 0x8040;
static constexpr unsigned LUMINANCE8_ALPHA8 = 0x8045;

// Alpha sized formats
static constexpr unsigned ALPHA8 = 0x803C;
static constexpr unsigned ALPHA16 = 0x803E;
static constexpr unsigned ALPHA16F = 0x881C;
static constexpr unsigned ALPHA32F = 0x8816;

// Alpha integer sized formats
static constexpr unsigned ALPHA8I = 0x8D90;
static constexpr unsigned ALPHA8UI = 0x8D7E;
static constexpr unsigned ALPHA16I = 0x8D8A;
static constexpr unsigned ALPHA16UI = 0x8D78;
static constexpr unsigned ALPHA32I = 0x8D84;
static constexpr unsigned ALPHA32UI = 0x8D72;

// RG sized formats
static constexpr unsigned RG8 = 0x822B;
static constexpr unsigned RG16 = 0x822C;

// RG sized integer formats
static constexpr unsigned RG8I = 0x8237;
static constexpr unsigned RG8UI = 0x8238;
static constexpr unsigned RG16I = 0x8239;
static constexpr unsigned RG16UI = 0x823A;
static constexpr unsigned RG32I = 0x823B;
static constexpr unsigned RG32UI = 0x823C;

// RGB sized formats
static constexpr unsigned RGB5 = 0x8050;
static constexpr unsigned RGB565 = 0x8D62;
static constexpr unsigned RGB8 = 0x8051;
static constexpr unsigned SRGB8 = 0x8C41;

// RGB integer sized formats
static constexpr unsigned RGB8I = 0x8D8F;
static constexpr unsigned RGB8UI = 0x8D7D;
static constexpr unsigned RGB16I = 0x8D89;
static constexpr unsigned RGB16UI = 0x8D77;
static constexpr unsigned RGB32I = 0x8D83;
static constexpr unsigned RGB32UI = 0x8D71;

// RGBA sized formats
static constexpr unsigned RGBA4 = 0x8056;
static constexpr unsigned RGB5_A1 = 0x8057;
static constexpr unsigned RGBA8 = 0x8058;
static constexpr unsigned RGB10_A2 = 0x8059;
static constexpr unsigned SRGB8_ALPHA8 = 0x8C43;
static constexpr unsigned RGBA16F = 0x881A;
static constexpr unsigned RGBA32F = 0x8814;
static constexpr unsigned RG32F = 0x8230;

// RGBA integer sized formats
static constexpr unsigned RGBA8I = 0x8D8E;
static constexpr unsigned RGBA8UI = 0x8D7C;
static constexpr unsigned RGBA16I = 0x8D88;
static constexpr unsigned RGBA16UI = 0x8D76;
static constexpr unsigned RGBA32I = 0x8D82;
static constexpr unsigned RGBA32UI = 0x8D70;

// BGRA sized formats
static constexpr unsigned BGRA8 = 0x93A1;

// PixelType
static constexpr unsigned UNSIGNED_SHORT_4_4_4_4 = 0x8033;
static constexpr unsigned UNSIGNED_SHORT_5_5_5_1 = 0x8034;
static constexpr unsigned UNSIGNED_SHORT_5_6_5 = 0x8363;
static constexpr unsigned UNSIGNED_INT_2_10_10_10_REV = 0x8368;

// Shaders
static constexpr unsigned FRAGMENT_SHADER = 0x8B30;
static constexpr unsigned VERTEX_SHADER = 0x8B31;
static constexpr unsigned GEOMETRY_SHADER = 0x8DD9;
static constexpr unsigned MAX_VERTEX_ATTRIBS = 0x8869;
static constexpr unsigned MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB;
static constexpr unsigned MAX_VARYING_VECTORS = 0x8DFC;
static constexpr unsigned MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D;
static constexpr unsigned MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C;
static constexpr unsigned MAX_GEOMETRY_TEXTURE_IMAGE_UNITS = 0x8C29;
static constexpr unsigned MAX_TEXTURE_IMAGE_UNITS = 0x8872;
static constexpr unsigned MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD;
static constexpr unsigned SHADER_TYPE = 0x8B4F;
static constexpr unsigned DELETE_STATUS = 0x8B80;
static constexpr unsigned LINK_STATUS = 0x8B82;
static constexpr unsigned VALIDATE_STATUS = 0x8B83;
static constexpr unsigned ATTACHED_SHADERS = 0x8B85;
static constexpr unsigned ACTIVE_UNIFORMS = 0x8B86;
static constexpr unsigned ACTIVE_UNIFORM_MAX_LENGTH = 0x8B87;
static constexpr unsigned ACTIVE_ATTRIBUTES = 0x8B89;
static constexpr unsigned ACTIVE_ATTRIBUTE_MAX_LENGTH = 0x8B8A;
static constexpr unsigned SHADING_LANGUAGE_VERSION = 0x8B8C;
static constexpr unsigned CURRENT_PROGRAM = 0x8B8D;
static constexpr unsigned MAX_FRAGMENT_UNIFORM_COMPONENTS = 0x8B49;
static constexpr unsigned MAX_VERTEX_UNIFORM_COMPONENTS = 0x8B4A;
static constexpr unsigned MAX_SHADER_PIXEL_LOCAL_STORAGE_FAST_SIZE = 0x8F63;
static constexpr unsigned SHADER_BINARY_FORMATS = 0x8DF8;

// StencilFunction
static constexpr unsigned NEVER = 0x0200;
static constexpr unsigned LESS = 0x0201;
static constexpr unsigned EQUAL = 0x0202;
static constexpr unsigned LEQUAL = 0x0203;
static constexpr unsigned GREATER = 0x0204;
static constexpr unsigned NOTEQUAL = 0x0205;
static constexpr unsigned GEQUAL = 0x0206;
static constexpr unsigned ALWAYS = 0x0207;

// StencilOp
static constexpr unsigned KEEP = 0x1E00;
static constexpr unsigned REPLACE = 0x1E01;
static constexpr unsigned INCR = 0x1E02;
static constexpr unsigned DECR = 0x1E03;
static constexpr unsigned INVERT = 0x150A;
static constexpr unsigned INCR_WRAP = 0x8507;
static constexpr unsigned DECR_WRAP = 0x8508;

// StringName
static constexpr unsigned VENDOR = 0x1F00;
static constexpr unsigned RENDERER = 0x1F01;
static constexpr unsigned VERSION = 0x1F02;
static constexpr unsigned EXTENSIONS = 0x1F03;

// StringCounts
static constexpr unsigned NUM_EXTENSIONS = 0x821D;

// Pixel Mode / Transfer
static constexpr unsigned UNPACK_ROW_LENGTH = 0x0CF2;
static constexpr unsigned PACK_ROW_LENGTH = 0x0D02;

// TextureMagFilter
static constexpr unsigned NEAREST = 0x2600;
static constexpr unsigned LINEAR = 0x2601;

// TextureMinFilter
static constexpr unsigned NEAREST_MIPMAP_NEAREST = 0x2700;
static constexpr unsigned LINEAR_MIPMAP_NEAREST = 0x2701;
static constexpr unsigned NEAREST_MIPMAP_LINEAR = 0x2702;
static constexpr unsigned LINEAR_MIPMAP_LINEAR = 0x2703;

// TextureUsage
static constexpr unsigned FRAMEBUFFER_ATTACHMENT = 0x93A3;

// TextureParameterName
static constexpr unsigned TEXTURE_MAG_FILTER = 0x2800;
static constexpr unsigned TEXTURE_MIN_FILTER = 0x2801;
static constexpr unsigned TEXTURE_WRAP_S = 0x2802;
static constexpr unsigned TEXTURE_WRAP_T = 0x2803;
static constexpr unsigned TEXTURE_USAGE = 0x93A2;

// TextureTarget
static constexpr unsigned TEXTURE = 0x1702;
static constexpr unsigned TEXTURE_CUBE_MAP = 0x8513;
static constexpr unsigned TEXTURE_BINDING_CUBE_MAP = 0x8514;
static constexpr unsigned TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515;
static constexpr unsigned TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516;
static constexpr unsigned TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517;
static constexpr unsigned TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518;
static constexpr unsigned TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519;
static constexpr unsigned TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A;
static constexpr unsigned MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C;

// TextureUnit
static constexpr unsigned TEXTURE0 = 0x84C0;
static constexpr unsigned TEXTURE1 = 0x84C1;
static constexpr unsigned TEXTURE2 = 0x84C2;
static constexpr unsigned TEXTURE3 = 0x84C3;
static constexpr unsigned TEXTURE4 = 0x84C4;
static constexpr unsigned TEXTURE5 = 0x84C5;
static constexpr unsigned TEXTURE6 = 0x84C6;
static constexpr unsigned TEXTURE7 = 0x84C7;
static constexpr unsigned TEXTURE8 = 0x84C8;
static constexpr unsigned TEXTURE9 = 0x84C9;
static constexpr unsigned TEXTURE10 = 0x84CA;
static constexpr unsigned TEXTURE11 = 0x84CB;
static constexpr unsigned TEXTURE12 = 0x84CC;
static constexpr unsigned TEXTURE13 = 0x84CD;
static constexpr unsigned TEXTURE14 = 0x84CE;
static constexpr unsigned TEXTURE15 = 0x84CF;
static constexpr unsigned TEXTURE16 = 0x84D0;
static constexpr unsigned TEXTURE17 = 0x84D1;
static constexpr unsigned TEXTURE18 = 0x84D2;
static constexpr unsigned TEXTURE19 = 0x84D3;
static constexpr unsigned TEXTURE20 = 0x84D4;
static constexpr unsigned TEXTURE21 = 0x84D5;
static constexpr unsigned TEXTURE22 = 0x84D6;
static constexpr unsigned TEXTURE23 = 0x84D7;
static constexpr unsigned TEXTURE24 = 0x84D8;
static constexpr unsigned TEXTURE25 = 0x84D9;
static constexpr unsigned TEXTURE26 = 0x84DA;
static constexpr unsigned TEXTURE27 = 0x84DB;
static constexpr unsigned TEXTURE28 = 0x84DC;
static constexpr unsigned TEXTURE29 = 0x84DD;
static constexpr unsigned TEXTURE30 = 0x84DE;
static constexpr unsigned TEXTURE31 = 0x84DF;
static constexpr unsigned ACTIVE_TEXTURE = 0x84E0;
static constexpr unsigned MAX_TEXTURE_UNITS = 0x84E2;
static constexpr unsigned MAX_TEXTURE_COORDS = 0x8871;

// TextureWrapMode
static constexpr unsigned REPEAT = 0x2901;
static constexpr unsigned CLAMP_TO_EDGE = 0x812F;
static constexpr unsigned MIRRORED_REPEAT = 0x8370;
static constexpr unsigned CLAMP_TO_BORDER = 0x812D;

// Texture Swizzle
static constexpr unsigned TEXTURE_SWIZZLE_R = 0x8E42;
static constexpr unsigned TEXTURE_SWIZZLE_G = 0x8E43;
static constexpr unsigned TEXTURE_SWIZZLE_B = 0x8E44;
static constexpr unsigned TEXTURE_SWIZZLE_A = 0x8E45;
static constexpr unsigned TEXTURE_SWIZZLE_RGBA = 0x8E46;

// Texture mapping
static constexpr unsigned TEXTURE_ENV = 0x2300;
static constexpr unsigned TEXTURE_ENV_MODE = 0x2200;
static constexpr unsigned TEXTURE_1D = 0x0DE0;
static constexpr unsigned TEXTURE_ENV_COLOR = 0x2201;
static constexpr unsigned TEXTURE_GEN_S = 0x0C60;
static constexpr unsigned TEXTURE_GEN_T = 0x0C61;
static constexpr unsigned TEXTURE_GEN_R = 0x0C62;
static constexpr unsigned TEXTURE_GEN_Q = 0x0C63;
static constexpr unsigned TEXTURE_GEN_MODE = 0x2500;
static constexpr unsigned TEXTURE_BORDER_COLOR = 0x1004;
static constexpr unsigned TEXTURE_WIDTH = 0x1000;
static constexpr unsigned TEXTURE_HEIGHT = 0x1001;
static constexpr unsigned TEXTURE_BORDER = 0x1005;
static constexpr unsigned TEXTURE_COMPONENTS = 0x1003;
static constexpr unsigned TEXTURE_RED_SIZE = 0x805C;
static constexpr unsigned TEXTURE_GREEN_SIZE = 0x805D;
static constexpr unsigned TEXTURE_BLUE_SIZE = 0x805E;
static constexpr unsigned TEXTURE_ALPHA_SIZE = 0x805F;
static constexpr unsigned TEXTURE_LUMINANCE_SIZE = 0x8060;
static constexpr unsigned TEXTURE_INTENSITY_SIZE = 0x8061;
static constexpr unsigned TEXTURE_INTERNAL_FORMAT = 0x1003;
static constexpr unsigned OBJECT_LINEAR = 0x2401;
static constexpr unsigned OBJECT_PLANE = 0x2501;
static constexpr unsigned EYE_LINEAR = 0x2400;
static constexpr unsigned EYE_PLANE = 0x2502;
static constexpr unsigned SPHERE_MAP = 0x2402;
static constexpr unsigned DECAL = 0x2101;
static constexpr unsigned MODULATE = 0x2100;
static constexpr unsigned CLAMP = 0x2900;
static constexpr unsigned S = 0x2000;
static constexpr unsigned T = 0x2001;
static constexpr unsigned R = 0x2002;
static constexpr unsigned Q = 0x2003;

// texture_env_combine
static constexpr unsigned COMBINE = 0x8570;
static constexpr unsigned COMBINE_RGB = 0x8571;
static constexpr unsigned COMBINE_ALPHA = 0x8572;
static constexpr unsigned SOURCE0_RGB = 0x8580;
static constexpr unsigned SOURCE1_RGB = 0x8581;
static constexpr unsigned SOURCE2_RGB = 0x8582;
static constexpr unsigned SOURCE0_ALPHA = 0x8588;
static constexpr unsigned SOURCE1_ALPHA = 0x8589;
static constexpr unsigned SOURCE2_ALPHA = 0x858A;
static constexpr unsigned OPERAND0_RGB = 0x8590;
static constexpr unsigned OPERAND1_RGB = 0x8591;
static constexpr unsigned OPERAND2_RGB = 0x8592;
static constexpr unsigned OPERAND0_ALPHA = 0x8598;
static constexpr unsigned OPERAND1_ALPHA = 0x8599;
static constexpr unsigned OPERAND2_ALPHA = 0x859A;
static constexpr unsigned RGB_SCALE = 0x8573;
static constexpr unsigned ADD_SIGNED = 0x8574;
static constexpr unsigned INTERPOLATE = 0x8575;
static constexpr unsigned SUBTRACT = 0x84E7;
static constexpr unsigned CONSTANT = 0x8576;
static constexpr unsigned PRIMARY_COLOR = 0x8577;
static constexpr unsigned PREVIOUS = 0x8578;
static constexpr unsigned SRC0_RGB = 0x8580;
static constexpr unsigned SRC1_RGB = 0x8581;
static constexpr unsigned SRC2_RGB = 0x8582;
static constexpr unsigned SRC0_ALPHA = 0x8588;
static constexpr unsigned SRC1_ALPHA = 0x8589;
static constexpr unsigned SRC2_ALPHA = 0x858A;

// Uniform Types
static constexpr unsigned FLOAT_VEC2 = 0x8B50;
static constexpr unsigned FLOAT_VEC3 = 0x8B51;
static constexpr unsigned FLOAT_VEC4 = 0x8B52;
static constexpr unsigned INT_VEC2 = 0x8B53;
static constexpr unsigned INT_VEC3 = 0x8B54;
static constexpr unsigned INT_VEC4 = 0x8B55;
static constexpr unsigned BOOL = 0x8B56;
static constexpr unsigned BOOL_VEC2 = 0x8B57;
static constexpr unsigned BOOL_VEC3 = 0x8B58;
static constexpr unsigned BOOL_VEC4 = 0x8B59;
static constexpr unsigned FLOAT_MAT2 = 0x8B5A;
static constexpr unsigned FLOAT_MAT3 = 0x8B5B;
static constexpr unsigned FLOAT_MAT4 = 0x8B5C;
static constexpr unsigned SAMPLER_2D = 0x8B5E;
static constexpr unsigned SAMPLER_CUBE = 0x8B60;

// Vertex Arrays
static constexpr unsigned VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_SIZE = 0x8623;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_STRIDE = 0x8624;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_TYPE = 0x8625;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_NORMALIZED = 0x886A;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_POINTER = 0x8645;
static constexpr unsigned VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;
static constexpr unsigned VERTEX_ARRAY = 0x8074;
static constexpr unsigned NORMAL_ARRAY = 0x8075;
static constexpr unsigned COLOR_ARRAY = 0x8076;
static constexpr unsigned SECONDARY_COLOR_ARRAY = 0x845E;
static constexpr unsigned INDEX_ARRAY = 0x8077;
static constexpr unsigned TEXTURE_COORD_ARRAY = 0x8078;
static constexpr unsigned EDGE_FLAG_ARRAY = 0x8079;
static constexpr unsigned VERTEX_ARRAY_SIZE = 0x807A;
static constexpr unsigned VERTEX_ARRAY_TYPE = 0x807B;
static constexpr unsigned VERTEX_ARRAY_STRIDE = 0x807C;
static constexpr unsigned NORMAL_ARRAY_TYPE = 0x807E;
static constexpr unsigned NORMAL_ARRAY_STRIDE = 0x807F;
static constexpr unsigned COLOR_ARRAY_SIZE = 0x8081;
static constexpr unsigned COLOR_ARRAY_TYPE = 0x8082;
static constexpr unsigned COLOR_ARRAY_STRIDE = 0x8083;
static constexpr unsigned INDEX_ARRAY_TYPE = 0x8085;
static constexpr unsigned INDEX_ARRAY_STRIDE = 0x8086;
static constexpr unsigned TEXTURE_COORD_ARRAY_SIZE = 0x8088;
static constexpr unsigned TEXTURE_COORD_ARRAY_TYPE = 0x8089;
static constexpr unsigned TEXTURE_COORD_ARRAY_STRIDE = 0x808A;
static constexpr unsigned EDGE_FLAG_ARRAY_STRIDE = 0x808C;
static constexpr unsigned VERTEX_ARRAY_POINTER = 0x808E;
static constexpr unsigned NORMAL_ARRAY_POINTER = 0x808F;
static constexpr unsigned COLOR_ARRAY_POINTER = 0x8090;
static constexpr unsigned INDEX_ARRAY_POINTER = 0x8091;
static constexpr unsigned TEXTURE_COORD_ARRAY_POINTER = 0x8092;
static constexpr unsigned EDGE_FLAG_ARRAY_POINTER = 0x8093;
static constexpr unsigned V2F = 0x2A20;
static constexpr unsigned V3F = 0x2A21;
static constexpr unsigned C4UB_V2F = 0x2A22;
static constexpr unsigned C4UB_V3F = 0x2A23;
static constexpr unsigned C3F_V3F = 0x2A24;
static constexpr unsigned N3F_V3F = 0x2A25;
static constexpr unsigned C4F_N3F_V3F = 0x2A26;
static constexpr unsigned T2F_V3F = 0x2A27;
static constexpr unsigned T4F_V4F = 0x2A28;
static constexpr unsigned T2F_C4UB_V3F = 0x2A29;
static constexpr unsigned T2F_C3F_V3F = 0x2A2A;
static constexpr unsigned T2F_N3F_V3F = 0x2A2B;
static constexpr unsigned T2F_C4F_N3F_V3F = 0x2A2C;
static constexpr unsigned T4F_C4F_N3F_V4F = 0x2A2D;
static constexpr unsigned PRIMITIVE_RESTART_FIXED_INDEX = 0x8D69;

// Buffer Object
static constexpr unsigned READ_ONLY = 0x88B8;
static constexpr unsigned WRITE_ONLY = 0x88B9;
static constexpr unsigned READ_WRITE = 0x88BA;
static constexpr unsigned BUFFER_MAPPED = 0x88BC;

static constexpr unsigned MAP_READ_BIT = 0x0001;
static constexpr unsigned MAP_WRITE_BIT = 0x0002;
static constexpr unsigned MAP_INVALIDATE_RANGE_BIT = 0x0004;
static constexpr unsigned MAP_INVALIDATE_BUFFER_BIT = 0x0008;
static constexpr unsigned MAP_FLUSH_EXPLICIT_BIT = 0x0010;
static constexpr unsigned MAP_UNSYNCHRONIZED_BIT = 0x0020;

// Read Format
static constexpr unsigned IMPLEMENTATION_COLOR_READ_TYPE = 0x8B9A;
static constexpr unsigned IMPLEMENTATION_COLOR_READ_FORMAT = 0x8B9B;

// Shader Source
static constexpr unsigned COMPILE_STATUS = 0x8B81;
static constexpr unsigned INFO_LOG_LENGTH = 0x8B84;
static constexpr unsigned SHADER_SOURCE_LENGTH = 0x8B88;
static constexpr unsigned SHADER_COMPILER = 0x8DFA;

// Shader Binary
static constexpr unsigned NUM_SHADER_BINARY_FORMATS = 0x8DF9;

// Program Binary
static constexpr unsigned NUM_PROGRAM_BINARY_FORMATS = 0x87FE;

// Shader Precision-Specified Types
static constexpr unsigned LOW_FLOAT = 0x8DF0;
static constexpr unsigned MEDIUM_FLOAT = 0x8DF1;
static constexpr unsigned HIGH_FLOAT = 0x8DF2;
static constexpr unsigned LOW_INT = 0x8DF3;
static constexpr unsigned MEDIUM_INT = 0x8DF4;
static constexpr unsigned HIGH_INT = 0x8DF5;

// Queries
static constexpr unsigned QUERY_COUNTER_BITS = 0x8864;
static constexpr unsigned CURRENT_QUERY = 0x8865;
static constexpr unsigned QUERY_RESULT = 0x8866;
static constexpr unsigned QUERY_RESULT_AVAILABLE = 0x8867;
static constexpr unsigned SAMPLES_PASSED = 0x8914;
static constexpr unsigned ANY_SAMPLES_PASSED = 0x8C2F;
static constexpr unsigned TIME_ELAPSED = 0x88BF;
static constexpr unsigned TIMESTAMP = 0x8E28;
static constexpr unsigned PRIMITIVES_GENERATED = 0x8C87;
static constexpr unsigned TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88;

// Framebuffer Object.
static constexpr unsigned FRAMEBUFFER = 0x8D40;
static constexpr unsigned DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
static constexpr unsigned READ_FRAMEBUFFER = 0x8CA8;
static constexpr unsigned DRAW_FRAMEBUFFER = 0x8CA9;
static constexpr unsigned READ_FRAMEBUFFER_BINDING = 0x8CAA;

static constexpr unsigned RENDERBUFFER = 0x8D41;

static constexpr unsigned MAX_SAMPLES = 0x8D57;
static constexpr unsigned MAX_SAMPLES_IMG = 0x9135;

static constexpr unsigned RENDERBUFFER_WIDTH = 0x8D42;
static constexpr unsigned RENDERBUFFER_HEIGHT = 0x8D43;
static constexpr unsigned RENDERBUFFER_INTERNAL_FORMAT = 0x8D44;
static constexpr unsigned RENDERBUFFER_RED_SIZE = 0x8D50;
static constexpr unsigned RENDERBUFFER_GREEN_SIZE = 0x8D51;
static constexpr unsigned RENDERBUFFER_BLUE_SIZE = 0x8D52;
static constexpr unsigned RENDERBUFFER_ALPHA_SIZE = 0x8D53;
static constexpr unsigned RENDERBUFFER_DEPTH_SIZE = 0x8D54;
static constexpr unsigned RENDERBUFFER_STENCIL_SIZE = 0x8D55;

static constexpr unsigned FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL = 0x8CD2;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE = 0x8CD3;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER = 0x8CD4;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING = 0x8210;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_RED_SIZE = 0x8212;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_GREEN_SIZE = 0x8213;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_BLUE_SIZE = 0x8214;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE = 0x8215;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216;
static constexpr unsigned FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217;

static constexpr unsigned COLOR_ATTACHMENT0 = 0x8CE0;
static constexpr unsigned DEPTH_ATTACHMENT = 0x8D00;
static constexpr unsigned STENCIL_ATTACHMENT = 0x8D20;

// EXT_discard_framebuffer
static constexpr unsigned COLOR = 0x1800;
static constexpr unsigned DEPTH = 0x1801;
static constexpr unsigned STENCIL = 0x1802;

static constexpr unsigned NONE = 0;
static constexpr unsigned FRAMEBUFFER_DEFAULT = 0x8218;

static constexpr unsigned FRAMEBUFFER_COMPLETE = 0x8CD5;
static constexpr unsigned FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6;
static constexpr unsigned FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7;
static constexpr unsigned FRAMEBUFFER_INCOMPLETE_DIMENSIONS = 0x8CD9;
static constexpr unsigned FRAMEBUFFER_UNSUPPORTED = 0x8CDD;

static constexpr unsigned FRAMEBUFFER_BINDING = 0x8CA6;
static constexpr unsigned RENDERBUFFER_BINDING = 0x8CA7;
static constexpr unsigned MAX_RENDERBUFFER_SIZE = 0x84E8;

static constexpr unsigned INVALID_FRAMEBUFFER_OPERATION = 0x0506;

// OES
static constexpr unsigned TEXTURE_EXTERNAL_OES = 0x8D65;
static constexpr unsigned TEXTURE_BINDING_EXTERNAL_OES = 0x8d67;
static constexpr unsigned DEPTH_STENCIL_OES = 0x84F9;
static constexpr unsigned UNSIGNED_INT_24_8_OES = 0x84FA;
static constexpr unsigned DEPTH24_STENCIL8_OES = 0x88F0;

/* ARB_internalformat_query */
static constexpr unsigned NUM_SAMPLE_COUNTS = 0x9380;

/* ARM specific define for MSAA support on framebuffer fetch */
static constexpr unsigned FETCH_PER_SAMPLE_ARM = 0x8F65;

static constexpr unsigned SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
static constexpr uint64_t TIMEOUT_IGNORED = 0xFFFFFFFFFFFFFFFFull;
}  // namespace GL
