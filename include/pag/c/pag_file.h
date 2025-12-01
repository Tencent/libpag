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

#include "pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

/**
 * Load a pag file from byte data, return null if the bytes is empty, or it's not a valid pag
 * file. Release it by pag_release.
 */
PAG_EXPORT pag_file* pag_file_load(const void* bytes, size_t length, const char* filePath);

PAG_EXPORT void pag_file_set_duration(pag_file* file, int64_t duration);

PAG_EXPORT int pag_file_get_num_texts(pag_file* file);

PAG_EXPORT int pag_file_get_num_images(pag_file* file);

PAG_EXPORT int pag_file_get_num_videos(pag_file* file);

PAG_EXPORT pag_time_stretch_mode pag_file_get_time_stretch_mode(pag_file* file);

PAG_EXPORT void pag_file_set_time_stretch_mode(pag_file* file, pag_time_stretch_mode mode);

/**
 * Get a text data of the specified index. The index ranges from 0 to numTexts - 1. Release it by
 * pag_release.
 * Note: It always returns the default text data.
 */
PAG_EXPORT pag_text_document* pag_file_get_text_data(pag_file* file, int editableTextIndex);

PAG_EXPORT void pag_file_replace_text(pag_file* file, int editableTextIndex, pag_text_document* text);

PAG_EXPORT void pag_file_replace_image(pag_file* file, int editableImageIndex, pag_image* image);

/**
 * Return an array of layers by specified editable index and layer type.
 * Release them by pag_release.
 */
PAG_EXPORT pag_layer** pag_file_get_layers_by_editable_index(pag_file* file, int editableIndex,
                                                          pag_layer_type layerType,
                                                          size_t* numLayers);

PAG_EXPORT int* pag_file_get_editable_indices(pag_file* file, pag_layer_type layerType,
                                           size_t* numIndexes);

PAG_C_PLUS_PLUS_END_GUARD
