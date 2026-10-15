// Copyright (C) 2026 Tencent. All rights reserved.

import {
  FontMapRule, FontMappingError, FontMappingErrorCode, FontTarget, HtmlFontRequest,
  fontRequestKey, fontWeightNumber, parseFontFamilyStack, validateFontMap,
} from './font-metadata';

/** Applies explicit source-request rules. Unmatched fonts are never substituted implicitly. */
export class FontMatcher {
  private targets = new Map<string, FontTarget>();

  constructor(rules: FontMapRule[]) {
    const errors = validateFontMap(rules);
    if (errors.length) throw new FontMappingError(FontMappingErrorCode.InvalidFontMap, errors.join('; '));
    for (const rule of rules) {
      const key = fontRequestKey({
        families: parseFontFamilyStack(rule.from['font-family']),
        weight: fontWeightNumber(rule.from['font-weight']) as number,
        style: rule.from['font-style'],
      });
      this.targets.set(key, {
        family: rule.to['font-family'],
        style: rule.to['font-weight-style'],
      });
    }
  }

  match(request: HtmlFontRequest): FontTarget | null {
    const target = this.targets.get(fontRequestKey(request));
    return target ? { ...target } : null;
  }
}
