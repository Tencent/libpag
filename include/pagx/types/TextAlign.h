/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

namespace pagx {

/**
 * Text alignment along the inline direction. In horizontal mode, this controls horizontal alignment
 * within each line. In vertical mode, this controls vertical alignment within each column.
 */
enum class TextAlign {
  /**
   * Align text to the start of the inline direction.
   */
  Start,
  /**
   * Align text to the center of the inline direction.
   */
  Center,
  /**
   * Align text to the end of the inline direction.
   */
  End,
  /**
   * Justify text by distributing extra space evenly between characters to fill the available inline
   * dimension. The last line/column uses Start alignment by default.
   */
  Justify
};

}  // namespace pagx
