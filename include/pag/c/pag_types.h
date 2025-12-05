/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <cstddef>
#include <cstdint>

#if !defined(PAG_EXPORT)
#if defined(PAG_DLL)
#if defined(_MSC_VER)
#define PAG_EXPORT __declspec(dllexport)
#else
#define PAG_EXPORT __attribute__((visibility("default")))
#endif
#else
#define PAG_EXPORT
#endif
#endif

#ifdef __cplusplus
#define PAG_C_PLUS_PLUS_BEGIN_GUARD extern "C" {
#define PAG_C_PLUS_PLUS_END_GUARD }
#else
#define PAG_C_PLUS_PLUS_BEGIN_GUARD
#define PAG_C_PLUS_PLUS_END_GUARD
#endif

PAG_C_PLUS_PLUS_BEGIN_GUARD

typedef enum {
  pag_time_stretch_mode_none,
  pag_time_stretch_mode_scale,
  pag_time_stretch_mode_repeat,
  pag_time_stretch_mode_repeat_inverted,
} pag_time_stretch_mode;

typedef enum {
  pag_color_type_unknown,
  pag_color_type_alpha_8,
  pag_color_type_rgba_8888,
  pag_color_type_bgra_8888,
  pag_color_type_rgb_565,
  pag_color_type_gray_8,
  pag_color_type_rgba_f16,
  pag_color_type_rgba_101012,
} pag_color_type;

typedef enum {
  pag_alpha_type_unknown,
  pag_alpha_type_opaque,
  pag_alpha_type_premultiplied,
  pag_alpha_type_unpremultiplied,
} pag_alpha_type;

typedef enum {
  pag_scale_mode_none,
  pag_scale_mode_stretch,
  pag_scale_mode_letter_box,
  pag_scale_mode_zoom,
} pag_scale_mode;

typedef enum {
  pag_layer_type_unknown,
  pag_layer_type_null,
  pag_layer_type_solid,
  pag_layer_type_text,
  pag_layer_type_shape,
  pag_layer_type_image,
  pag_layer_type_pre_compose,
  pag_layer_type_camera,
} pag_layer_type;

typedef struct pag_gl_texture_info {
  unsigned id = 0;
  unsigned target = 0x0DE1;  // GL_TEXTURE_2D;
  unsigned format = 0x8058;  // GL_RGBA8;
} pag_gl_texture_info;

typedef struct pag_vk_image_info {
  void* image = nullptr;
} pag_vk_image_info;

typedef struct pag_mtl_texture_info {
  void* texture = nullptr;
} pag_mtl_texture_info;

typedef struct pag_point {
  float x;
  float y;
} pag_point;

typedef struct pag_color {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} pag_color;

typedef enum {
  pag_paragraph_justification_left_justify,
  pag_paragraph_justification_center_justify,
  pag_paragraph_justification_right_justify,
  pag_paragraph_justification_full_justify_last_line_left,
  pag_paragraph_justification_full_justify_last_line_right,
  pag_paragraph_justification_full_justify_last_line_center,
  pag_paragraph_justification_full_justify_last_line_full,
} pag_paragraph_justification;

typedef struct pag_display_link_functions {
  void* (*create)(void* user, void (*callback)(void* user));
  void (*start)(void* displayLink);
  void (*stop)(void* displayLink);
  void (*release)(void* displayLink);
} pag_display_link_functions;

typedef struct pag_egl_globals {
  void* display = nullptr;
} egl_globals;

typedef const void* pag_object;
typedef struct pag_byte_data pag_byte_data;
typedef struct pag_text_document pag_text_document;
typedef struct pag_layer pag_layer;
typedef pag_layer pag_solid_layer;
typedef pag_layer pag_composition;
typedef pag_composition pag_file;
typedef struct pag_player pag_player;
typedef struct pag_surface pag_surface;
typedef struct pag_image pag_image;
typedef struct pag_font pag_font;
typedef struct pag_backend_texture pag_backend_texture;
typedef struct pag_backend_semaphore pag_backend_semaphore;
typedef struct pag_decoder PAGDecoder;
typedef struct pag_animator pag_animator;

typedef struct pag_animator_listener {
  void (*on_animation_start)(pag_animator* animator, void* user);
  void (*on_animation_end)(pag_animator* animator, void* user);
  void (*on_animation_cancel)(pag_animator* animator, void* user);
  void (*on_animation_repeat)(pag_animator* animator, void* user);
  void (*on_animation_update)(pag_animator* animator, void* user);
} pag_animator_listener;

/**
 * Release a pag_object.
 */
PAG_EXPORT void pag_release(pag_object object);

PAG_EXPORT void pag_display_link_set_functions(pag_display_link_functions* functions);

PAG_C_PLUS_PLUS_END_GUARD
