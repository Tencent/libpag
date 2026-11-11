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

// 仅供两项跨后端文字验证使用；不改变 SDK 默认字体度量，也不补偿三维矩阵。
export async function InstallTextCalibration(module, reference, fontBytes) {
  const compactFamily = reference.family.replaceAll(' ', '');
  const font = new FontFace(reference.style ? compactFamily + ' ' + reference.style : compactFamily, fontBytes);
  await font.load();
  document.fonts.add(font);
  const scaler = module.ScalerContext.prototype;
  const originalAdvance = scaler.getAdvance;
  const originalBounds = scaler.getBounds;
  const stats = { advances: 0, bounds: 0 };
  function matches(instance) {
    return instance.fontName === reference.family && instance.fontStyle === reference.style && instance.size === reference.size;
  }

  function getAdvance(text) {
    const metric = reference.glyphs[text];
    if (matches(this) && metric) {
      stats.advances++;
      return metric.advance;
    }
    return originalAdvance.call(this, text);
  }

  function getBounds(text, bold, italic) {
    const metric = reference.glyphs[text];
    if (matches(this) && metric && !bold && !italic) {
      stats.bounds++;
      const bounds = metric.bounds;
      return { left: bounds[0], top: bounds[1], right: bounds[2], bottom: bounds[3] };
    }
    return originalBounds.call(this, text, bold, italic);
  }

  scaler.getAdvance = getAdvance;
  scaler.getBounds = getBounds;
  return {
    stats,
    restore() {
      scaler.getAdvance = originalAdvance;
      scaler.getBounds = originalBounds;
      document.fonts.delete(font);
    },
  };
}
