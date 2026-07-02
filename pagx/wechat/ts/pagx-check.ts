/* eslint-disable @typescript-eslint/no-explicit-any */
import { getSystemInfo } from './wx-system-info';
declare const wx: any;

/**
 * PAGX render-risk evaluator for mini-program (WeChat) target.
 *
 * Risk model: "max-risk-path" scoring (May 2026 calibration, high-end device as baseline).
 *
 * Five independent failure paths:
 *   A. "BgBlur × uncacheable below"    bg_count × (inner + blur + grad/10)
 *   B. "Path geometry overload"        path_data_bytes (MB)
 *   C. "Big canvas × element density"  (pix_M/100) × (imgPat + layer/30 + grad/20)
 *   D. "BgBlur count"                  bg_count
 *   E. "Layer XML count"               raw <Layer> count in source XML
 *
 * Rendering recommendation (decided by runtime platform):
 *   - Android: score >= 65 renders smoothly, otherwise it may stutter
 *   - Other platforms (iOS, etc.): score >= 75 renders smoothly, otherwise it may stutter
 *
 * Thresholds are adjusted dynamically by device performance level (benchmarkLevel):
 *   - High-end devices: use the default thresholds (calibration baseline)
 *   - Mid-range devices: tighten the thresholds
 *   - Low-end devices: tighten the thresholds further with stricter limits
 */

// ============================================================================
// Types
// ============================================================================

type DeviceTier = 'high' | 'mid' | 'low' | 'unknown';
type Platform = 'ios' | 'android' | 'other';

/** Full result returned by checkPagx. */
export interface PagxCheckResult {
  /** Score (0-100), higher means smoother; Android >= 65 renders normally, other platforms >= 75. */
  score: number;
  /** Device performance level (raw value returned by the WeChat API, -1 means unknown). */
  benchmarkLevel: number;
  /** Device tier. */
  deviceTier: DeviceTier;
  /** Platform. */
  platform: Platform;
  /** Device brand. */
  brand?: string;
  /** Device model. */
  model?: string;
  /** GPU RENDERER string (collected on Android, for diagnostics only). */
  gpuRenderer?: string;
}

// ============================================================================
// Device Info Cache
// ============================================================================

interface DeviceInfoCache {
  tier: DeviceTier;
  benchmarkLevel: number;
  platform: Platform;
  brand: string;
  model: string;
  /** WebGL RENDERER string, e.g. "Mali-G925-Immortalis MP12". Only collected on Android. */
  gpuRenderer: string;
}

/**
 * Mali GPU families with a WebGL2 driver-level memory defect that causes WASM heap
 * to bloat by 2GB during rendering, triggering LMK:
 *   Mali-G925   → Dimensity 9400 / 9400+
 *   Mali-G1-Ultra → Dimensity 9500
 * Dimensity 9300 (Mali-G720) is unaffected.
 */
const DIMENSITY_GPU_RENDERER_PATTERN = /\b(Mali-G925|Mali-G1-Ultra)\b/;

let cachedDeviceInfo: DeviceInfoCache | null = null;

/**
 * Determine the device tier from benchmarkLevel and platform.
 *
 * Android platform:
 *   - High tier: ≥30 (Xiaomi 15, OPPO Find X8)
 *   - Mid tier: 23-29 (HONOR 200, REDMI K40)
 *   - Low tier: ≤22 (HONOR Play 20, VIVO Y52s)
 *
 * iOS platform:
 *   - High tier: ≥36 (iPhone 15/16/17)
 *   - Mid tier: 30-35 (iPhone 11/12)
 *   - Low tier: ≤29 (iPhone 7/8/X)
 */
function getDeviceTier(benchmarkLevel: number, platform: string): DeviceTier {
  // benchmarkLevel = -1 means performance is unknown.
  if (benchmarkLevel < 0) return 'unknown';

  if (platform === 'ios') {
    if (benchmarkLevel >= 36) return 'high';
    if (benchmarkLevel >= 30) return 'mid';
    return 'low';
  } else {
    // Android or other platforms.
    if (benchmarkLevel >= 30) return 'high';
    if (benchmarkLevel >= 23) return 'mid';
    return 'low';
  }
}

/**
 * Fetch device info asynchronously (calls wx.getDeviceBenchmarkInfo).
 */
async function fetchDeviceInfoAsync(): Promise<DeviceInfoCache> {
  // Return directly if already cached.
  if (cachedDeviceInfo !== null) {
    return cachedDeviceInfo;
  }

  try {
    const systemInfo = await getSystemInfo();
    const rawPlatform = systemInfo.platform || 'android';
    const platform: Platform = rawPlatform === 'ios' ? 'ios' : rawPlatform === 'android' ? 'android' : 'other';
    const benchmarkLevel = systemInfo.benchmarkLevel;
    const tier = getDeviceTier(benchmarkLevel, rawPlatform);
    const brand = String(systemInfo.brand ?? '');
    const model = String(systemInfo.model ?? '');

    // Obtain the real GPU name via the WEBGL_debug_renderer_info extension (the standard
    // WeChat RENDERER returns a generic string). Fall back to precise model matching for
    // Dimensity chips when the extension is unavailable.
    let gpuRenderer = '';
    if (platform === 'android') {
      try {
        const glCanvas = wx.createOffscreenCanvas({ type: 'webgl', width: 1, height: 1 });
        const gl = glCanvas.getContext('webgl');
        if (gl) {
          const debugExt = gl.getExtension('WEBGL_debug_renderer_info') as any;
          if (debugExt) {
            gpuRenderer = String(gl.getParameter(debugExt.UNMASKED_RENDERER_WEBGL) ?? '');
          }
          if (!gpuRenderer) {
            gpuRenderer = String(gl.getParameter(0x1F01) ?? '');
          }
        }
      } catch {
        // ignore
      }
    }

    cachedDeviceInfo = { tier, benchmarkLevel, platform, brand, model, gpuRenderer };
  } catch {
    // Fetch failed, fall back to a conservative unknown.
    cachedDeviceInfo = { tier: 'unknown', benchmarkLevel: -1, platform: 'other', brand: '', model: '', gpuRenderer: '' };
  }

  return cachedDeviceInfo;
}

// ============================================================================
// Risk Path Configuration
// ============================================================================

/**
 * Threshold multipliers for the different device tiers.
 * - High-end: baseline (1.0x)
 * - Mid-range: tightened to 0.77x
 * - Low-end: tightened to 0.54x
 * - Unknown: conservative, uses 0.62x
 *
 * Note: the original calibration data was based on mid-range devices; it now uses
 * high-end devices as the baseline.
 * Original multipliers: high=1.3, mid=1.0, low=0.7, unknown=0.8
 * After conversion: high=1.0, mid=0.77, low=0.54, unknown=0.62
 */
const TIER_MULTIPLIERS: Record<DeviceTier, number> = {
  high: 1.0,
  mid: 0.77,
  low: 0.54,
  unknown: 0.62,
};

/**
 * Platform multipliers: iOS mini-program rendering performs worse than Android, so it needs stricter thresholds.
 *
 * Root-cause analysis:
 * - The WebGL implementation of the iOS mini-program WebView differs from Android.
 * - iOS handles complex layer compositing less efficiently.
 * - iOS mini-programs manage memory more strictly and trigger GC more easily.
 *
 * Multiplier notes (applied to the thresholds; a smaller multiplier lowers the threshold and makes scoring stricter):
 * - iOS: 0.6 (threshold lowered to 60%, stricter)
 * - Android: 1.0 (baseline)
 * - Other: 0.8 (conservative)
 */
const PLATFORM_MULTIPLIERS: Record<Platform, number> = {
  ios: 0.6,
  android: 1.0,
  other: 0.8,
};

interface RiskPathConfig {
  yellow: number;
  red: number;
}

const BASE_RISK_PATHS: Record<string, RiskPathConfig> = {
  A_bg_x_uncacheable: { yellow: 2000, red: 4000 },
  B_path_data_MB: { yellow: 3, red: 25 },
  C_canvas_x_density: { yellow: 1500, red: 5000 },
  bg_blur_count: { yellow: 200, red: 400 },
  layer_xml: { yellow: 15000, red: 30000 },
};

const LOCAL_RISK_PATHS = {
  bgBlurAreaRadius: { yellow: 25, red: 120 },
  imageLoadMP: { yellow: 80, red: 220 },
  imageRuntimeRisk: { yellow: 45, red: 95 },
};
const NEUTRAL_SDK_BG_UNCACHEABLE_RISK = BASE_RISK_PATHS.A_bg_x_uncacheable;
const BARELY_USABLE_SCORE = 50;
const NEUTRAL_SDK_GREEN_SCORE = 75;
const SMALL_AREA_BLUR_RISK = 25;
const LARGE_DOWNSCALE_RATIO = 16;

interface LocalRiskRaw {
  bgBlurAreaRadius: number;
  imageDecodeMP: number;
  imageDownscaleMP: number;
  sdkBgUncacheableRisk: number;
  docMP: number;
  layerCount: number;
  textCount: number;
  innerShadowCount: number;
}

interface ImageRiskState {
  uniqueOriginalPixels: Map<string, number>;
  downscaleOriginalPixels: Map<string, number>;
  patternIndex: number;
}

interface LayerScanState {
  maxShapeArea: number;
  bgBlurRadii: number[];
}

/**
 * Adjust the thresholds by device tier and platform.
 * Final multiplier = device-tier multiplier × platform multiplier.
 */
function getAdjustedRiskPaths(tier: DeviceTier, platform: Platform): Record<string, RiskPathConfig> {
  const tierMultiplier = TIER_MULTIPLIERS[tier];
  const platformMultiplier = PLATFORM_MULTIPLIERS[platform];
  const finalMultiplier = tierMultiplier * platformMultiplier;

  const adjusted: Record<string, RiskPathConfig> = {};

  for (const [key, cfg] of Object.entries(BASE_RISK_PATHS)) {
    adjusted[key] = {
      yellow: cfg.yellow * finalMultiplier,
      red: cfg.red * finalMultiplier,
    };
  }

  return adjusted;
}

// ============================================================================
// XML Parser (lightweight)
// ============================================================================

interface XmlNode {
  tag: string;
  attribs: Record<string, string>;
  children: XmlNode[];
}

const ENTITY_MAP: Record<string, string> = {
  amp: '&',
  lt: '<',
  gt: '>',
  quot: '"',
  apos: "'",
};

function decodeEntities(value: string): string {
  if (value.indexOf('&') === -1) return value;
  return value.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+);/g, (raw, body: string) => {
    if (body.charAt(0) === '#') {
      const codePoint =
        body.charAt(1) === 'x' ? parseInt(body.substring(2), 16) : parseInt(body.substring(1), 10);
      if (!Number.isFinite(codePoint) || codePoint < 0) return raw;
      try {
        return String.fromCodePoint(codePoint);
      } catch {
        return raw;
      }
    }
    const replacement = ENTITY_MAP[body];
    return replacement !== undefined ? replacement : raw;
  });
}

// Skips constructs whose payload may contain unbalanced '>' (e.g. DOCTYPE internal subsets).
// Returns the index right after the closing '>'.
function skipBracketedBlock(xmlString: string, start: number): number {
  const len = xmlString.length;
  let depth = 0;
  for (let j = start; j < len; j++) {
    const ch = xmlString[j];
    if (ch === '[') {
      depth++;
    } else if (ch === ']') {
      if (depth > 0) depth--;
    } else if (ch === '>' && depth === 0) {
      return j + 1;
    }
  }
  return len;
}

function parseXml(xmlString: string): XmlNode | null {
  const stack: XmlNode[] = [];
  let root: XmlNode | null = null;
  let i = 0;
  const len = xmlString.length;

  if (xmlString.startsWith('<?xml')) {
    const endDecl = xmlString.indexOf('?>');
    if (endDecl !== -1) i = endDecl + 2;
  }

  while (i < len) {
    while (i < len && /\s/.test(xmlString[i])) i++;
    if (i >= len) break;

    if (xmlString[i] === '<') {
      if (xmlString[i + 1] === '/') {
        const endTag = xmlString.indexOf('>', i);
        if (endTag === -1) break;
        stack.pop();
        i = endTag + 1;
      } else if (xmlString[i + 1] === '!') {
        if (xmlString.substring(i, i + 4) === '<!--') {
          const endComment = xmlString.indexOf('-->', i);
          i = endComment === -1 ? len : endComment + 3;
        } else if (xmlString.substring(i, i + 9) === '<![CDATA[') {
          const endCdata = xmlString.indexOf(']]>', i);
          i = endCdata === -1 ? len : endCdata + 3;
        } else {
          // Handles <!DOCTYPE ...>, <!ENTITY ...> and other declarations whose internal
          // subsets may contain '>' inside '[...]'.
          i = skipBracketedBlock(xmlString, i + 2);
        }
      } else if (xmlString[i + 1] === '?') {
        const endPI = xmlString.indexOf('?>', i);
        i = endPI === -1 ? len : endPI + 2;
      } else {
        const tagEnd = xmlString.indexOf('>', i);
        if (tagEnd === -1) break;

        const tagContent = xmlString.substring(i + 1, tagEnd);
        const selfClosing = tagContent.endsWith('/');
        const cleanContent = selfClosing
          ? tagContent.slice(0, -1).trim()
          : tagContent.trim();

        const spaceIdx = cleanContent.search(/\s/);
        const tagName =
          spaceIdx === -1 ? cleanContent : cleanContent.substring(0, spaceIdx);
        const attribStr = spaceIdx === -1 ? '' : cleanContent.substring(spaceIdx);

        const attribs: Record<string, string> = {};
        // Accepts both double-quoted and single-quoted attribute values, and decodes
        // standard XML entities (&amp; &lt; &gt; &quot; &apos; plus numeric refs).
        const attrRegex = /([\w:-]+)\s*=\s*("([^"]*)"|'([^']*)')/g;
        let match;
        while ((match = attrRegex.exec(attribStr)) !== null) {
          const rawValue = match[3] !== undefined ? match[3] : match[4];
          attribs[match[1]] = decodeEntities(rawValue);
        }

        const node: XmlNode = { tag: tagName, attribs, children: [] };

        if (stack.length > 0) {
          stack[stack.length - 1].children.push(node);
        } else {
          root = node;
        }

        if (!selfClosing) stack.push(node);
        i = tagEnd + 1;
      }
    } else {
      const nextTag = xmlString.indexOf('<', i);
      i = nextTag === -1 ? len : nextTag;
    }
  }

  return root;
}

function collectNodes(node: XmlNode, results: XmlNode[]): void {
  results.push(node);
  for (const child of node.children) {
    collectNodes(child, results);
  }
}

function iterNodes(node: XmlNode): XmlNode[] {
  const results: XmlNode[] = [];
  collectNodes(node, results);
  return results;
}

function findAllByTag(node: XmlNode, tagName: string): XmlNode[] {
  return iterNodes(node).filter((n) => n.tag === tagName);
}

// ============================================================================
// Composition Expansion (for accurate risk calculation)
// ============================================================================

/**
 * Build the Composition lookup table.
 * Extracts all Composition definitions from the Resources node.
 */
function buildCompositionLookup(root: XmlNode): Map<string, XmlNode> {
  const lookup = new Map<string, XmlNode>();

  // Find the Resources node.
  const resources = root.children.find((c) => c.tag === 'Resources');
  if (!resources) return lookup;

  // Collect all Composition definitions.
  for (const comp of findAllByTag(resources, 'Composition')) {
    const id = comp.attribs.id;
    if (id) {
      lookup.set(id, comp);
    }
  }

  return lookup;
}

/**
 * Count the tags inside a single Composition (cached, guards against circular references).
 * Returns the tag counts inside the Composition definition itself (without expanding nested references).
 */
function getCompositionTagCounts(
  compId: string,
  compLookup: Map<string, XmlNode>,
  cache: Map<string, Map<string, number>>,
): Map<string, number> {
  // Return directly if already cached.
  if (cache.has(compId)) {
    return cache.get(compId)!;
  }

  const comp = compLookup.get(compId);
  if (!comp) {
    const empty = new Map<string, number>();
    cache.set(compId, empty);
    return empty;
  }

  // Guard against circular references: set an empty Map first.
  const counts = new Map<string, number>();
  cache.set(compId, counts);

  // Recursively count the tags inside the Composition (expands nested Composition references).
  countTagsInNodeInternal(comp, counts, compLookup, cache);

  return counts;
}

/**
 * Recursively count the tags inside a node (internal helper).
 * Expands Composition references when encountered.
 */
function countTagsInNodeInternal(
  node: XmlNode,
  counts: Map<string, number>,
  compLookup: Map<string, XmlNode>,
  cache: Map<string, Map<string, number>>,
): void {
  // Count the current node.
  counts.set(node.tag, (counts.get(node.tag) || 0) + 1);

  // If it is a Layer that references a Composition, fetch and accumulate the Composition tag counts.
  if (node.tag === 'Layer') {
    const compRef = node.attribs.composition;
    if (compRef && compRef.startsWith('@')) {
      const refId = compRef.slice(1);
      const refCounts = getCompositionTagCounts(refId, compLookup, cache);
      // Merge the referenced Composition tag counts (accumulate into the current counts).
      for (const [tag, count] of refCounts) {
        counts.set(tag, (counts.get(tag) || 0) + count);
      }
    }
  }

  // Recurse into child nodes.
  for (const child of node.children) {
    countTagsInNodeInternal(child, counts, compLookup, cache);
  }
}

/**
 * Count the tags across the whole document (expanding Composition references).
 * Counts only the body part (excluding Resources), but expands Composition references.
 */
function countTagsWithExpansion(root: XmlNode): Map<string, number> {
  const compLookup = buildCompositionLookup(root);
  const cache = new Map<string, Map<string, number>>();
  const totalCounts = new Map<string, number>();

  // Traverse only the body nodes (excluding Resources).
  for (const child of root.children) {
    if (child.tag === 'Resources') continue;
    countTagsInNodeInternal(child, totalCounts, compLookup, cache);
  }

  return totalCounts;
}

/**
 * Count the raw tags in the XML (without expanding Composition).
 * Used by the layer_xml risk path, kept consistent with the Python version.
 */
function countTagsRaw(root: XmlNode): Map<string, number> {
  const counts = new Map<string, number>();
  for (const node of iterNodes(root)) {
    counts.set(node.tag, (counts.get(node.tag) || 0) + 1);
  }
  return counts;
}

/**
 * Compute the total Path data byte count after expansion.
 */
function countPathDataWithExpansion(root: XmlNode): number {
  const compLookup = buildCompositionLookup(root);
  const pathDataCache = new Map<string, number>();

  // Compute the Path data byte count for a single Composition (cached).
  function getCompPathData(compId: string): number {
    if (pathDataCache.has(compId)) {
      return pathDataCache.get(compId)!;
    }

    const comp = compLookup.get(compId);
    if (!comp) {
      pathDataCache.set(compId, 0);
      return 0;
    }

    // Guard against circular references: set 0 first.
    pathDataCache.set(compId, 0);
    const bytes = countPathDataInNode(comp);
    pathDataCache.set(compId, bytes);
    return bytes;
  }

  // Recursively compute the Path data of a node.
  function countPathDataInNode(node: XmlNode): number {
    let total = 0;

    // If it is a Path, accumulate the data length.
    if (node.tag === 'Path') {
      const d = node.attribs.data || '';
      if (d) total += d.length;
    }

    // If it is a Layer that references a Composition.
    if (node.tag === 'Layer') {
      const compRef = node.attribs.composition;
      if (compRef && compRef.startsWith('@')) {
        total += getCompPathData(compRef.slice(1));
      }
    }

    // Recurse into child nodes.
    for (const child of node.children) {
      total += countPathDataInNode(child);
    }

    return total;
  }

  // Count only the body part.
  let totalBytes = 0;
  for (const child of root.children) {
    if (child.tag === 'Resources') continue;
    totalBytes += countPathDataInNode(child);
  }

  return totalBytes;
}

// ============================================================================
// Core Functions
// ============================================================================

function normalize(value: number, yellow: number, red: number): number {
  if (value <= 0) return 100.0;
  if (value <= yellow) return 100.0 - (value / yellow) * 19.0;
  if (value <= red) return 81.0 - ((value - yellow) / (red - yellow)) * 62.0;
  return (19.0 * red) / value;
}

function readNumericAttr(attribs: Record<string, string>, name: string): number {
  const value = Number(attribs[name]);
  return Number.isFinite(value) ? value : 0;
}

function readShapeArea(node: XmlNode): number {
  const size = node.attribs.size;
  if (!size) return 0;
  const [widthText, heightText] = size.split(',');
  const width = Number(widthText);
  const height = Number(heightText);
  if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) return 0;
  return width * height;
}

function currentLayerState(stack: LayerScanState[]): LayerScanState | null {
  return stack.length > 0 ? stack[stack.length - 1] : null;
}

function addImageRisk(node: XmlNode, state: ImageRiskState): void {
  const origWidth = readNumericAttr(node.attribs, 'data-orig-image-width');
  const origHeight = readNumericAttr(node.attribs, 'data-orig-image-height');
  const nodeWidth = readNumericAttr(node.attribs, 'data-node-width');
  const nodeHeight = readNumericAttr(node.attribs, 'data-node-height');
  const originalPixels = Math.max(0, origWidth) * Math.max(0, origHeight);
  const displayPixels = Math.max(0, nodeWidth) * Math.max(0, nodeHeight);
  if (originalPixels <= 0) return;

  const imageKey = node.attribs.image || `__inline_${state.patternIndex}`;
  state.patternIndex += 1;
  state.uniqueOriginalPixels.set(
    imageKey,
    Math.max(state.uniqueOriginalPixels.get(imageKey) ?? 0, originalPixels),
  );

  if (displayPixels > 0 && originalPixels / displayPixels >= LARGE_DOWNSCALE_RATIO) {
    state.downscaleOriginalPixels.set(
      imageKey,
      Math.max(state.downscaleOriginalPixels.get(imageKey) ?? 0, originalPixels),
    );
  }
}

function scanLocalRisk(root: XmlNode): LocalRiskRaw {
  const docWidth = readNumericAttr(root.attribs, 'width');
  const docHeight = readNumericAttr(root.attribs, 'height');
  const raw: LocalRiskRaw = {
    bgBlurAreaRadius: 0,
    imageDecodeMP: 0,
    imageDownscaleMP: 0,
    sdkBgUncacheableRisk: 0,
    docMP: Math.max(0, docWidth) * Math.max(0, docHeight) / 1_000_000,
    layerCount: 0,
    textCount: 0,
    innerShadowCount: 0,
  };
  const layerStack: LayerScanState[] = [];
  const imageState: ImageRiskState = {
    uniqueOriginalPixels: new Map<string, number>(),
    downscaleOriginalPixels: new Map<string, number>(),
    patternIndex: 0,
  };
  let bgBlurCount = 0;
  let innerShadowCount = 0;
  let blurFilterCount = 0;
  let gradientCount = 0;

  function visit(node: XmlNode): void {
    let layerState: LayerScanState | null = null;
    if (node.tag === 'Layer') {
      raw.layerCount += 1;
      layerState = { maxShapeArea: 0, bgBlurRadii: [] };
      layerStack.push(layerState);
    }

    if (node.tag === 'Text' || node.tag === 'GlyphRun') raw.textCount += 1;
    if (node.tag === 'Rectangle' || node.tag === 'Ellipse') {
      const currentLayer = currentLayerState(layerStack);
      const area = readShapeArea(node);
      if (currentLayer && area > currentLayer.maxShapeArea) currentLayer.maxShapeArea = area;
    }
    if (node.tag === 'BackgroundBlurStyle') {
      bgBlurCount += 1;
      const currentLayer = currentLayerState(layerStack);
      if (currentLayer) {
        currentLayer.bgBlurRadii.push(
          Math.max(readNumericAttr(node.attribs, 'blurX'), readNumericAttr(node.attribs, 'blurY'), 0),
        );
      }
    }
    if (node.tag === 'InnerShadowStyle') {
      innerShadowCount += 1;
      raw.innerShadowCount += 1;
    }
    if (node.tag === 'BlurFilter') blurFilterCount += 1;
    if (node.tag === 'LinearGradient' || node.tag === 'RadialGradient' || node.tag === 'SweepGradient') {
      gradientCount += 1;
    }
    if (node.tag === 'ImagePattern') addImageRisk(node, imageState);

    for (const child of node.children) visit(child);

    if (node.tag === 'Layer' && layerState) {
      const areaMP = layerState.maxShapeArea / 1_000_000;
      for (const radius of layerState.bgBlurRadii) raw.bgBlurAreaRadius += areaMP * radius;
      layerStack.pop();
      const parentLayer = currentLayerState(layerStack);
      if (parentLayer && layerState.maxShapeArea > parentLayer.maxShapeArea) {
        parentLayer.maxShapeArea = layerState.maxShapeArea;
      }
    }
  }

  visit(root);
  raw.imageDecodeMP = Array.from(imageState.uniqueOriginalPixels.values())
    .reduce((sum, pixels) => sum + pixels, 0) / 1_000_000;
  raw.imageDownscaleMP = Array.from(imageState.downscaleOriginalPixels.values())
    .reduce((sum, pixels) => sum + pixels, 0) / 1_000_000;
  raw.sdkBgUncacheableRisk = bgBlurCount * (innerShadowCount + blurFilterCount + gradientCount / 10);
  return raw;
}

function scoreBgBlurAreaRadius(value: number): number {
  if (value <= 0) return 100;
  return Math.max(
    normalize(value, LOCAL_RISK_PATHS.bgBlurAreaRadius.yellow, LOCAL_RISK_PATHS.bgBlurAreaRadius.red),
    BARELY_USABLE_SCORE,
  );
}

function getImageRuntimeRisk(raw: LocalRiskRaw): number {
  const effectiveDownscaleMP = Math.max(0, raw.imageDownscaleMP - 10);
  if (effectiveDownscaleMP <= 0) return 0;

  const contentComplexity = raw.layerCount / 1500 + raw.textCount / 400 + raw.innerShadowCount / 10;
  const bgBlurInteraction = raw.bgBlurAreaRadius > 0
    && raw.bgBlurAreaRadius <= LOCAL_RISK_PATHS.bgBlurAreaRadius.red
    ? (raw.bgBlurAreaRadius / 60) * contentComplexity
    : 0;
  const layoutInteraction = raw.imageDownscaleMP >= LOCAL_RISK_PATHS.imageLoadMP.yellow
    ? raw.layerCount / 6000 + raw.docMP / 100
    : 0;
  return effectiveDownscaleMP * (bgBlurInteraction + layoutInteraction);
}

function scoreImageLoadRisk(raw: LocalRiskRaw): number {
  return normalize(
    Math.max(raw.imageDecodeMP, raw.imageDownscaleMP),
    LOCAL_RISK_PATHS.imageLoadMP.yellow,
    LOCAL_RISK_PATHS.imageLoadMP.red,
  );
}

function scoreImageRuntimeRisk(raw: LocalRiskRaw): number {
  return normalize(
    getImageRuntimeRisk(raw),
    LOCAL_RISK_PATHS.imageRuntimeRisk.yellow,
    LOCAL_RISK_PATHS.imageRuntimeRisk.red,
  );
}

function scoreLocalRisk(raw: LocalRiskRaw): number {
  return Math.min(
    scoreBgBlurAreaRadius(raw.bgBlurAreaRadius),
    scoreImageLoadRisk(raw),
    scoreImageRuntimeRisk(raw),
  );
}

function scoreNeutralSdkRisk(raw: LocalRiskRaw): number {
  return normalize(
    raw.sdkBgUncacheableRisk,
    NEUTRAL_SDK_BG_UNCACHEABLE_RISK.yellow,
    NEUTRAL_SDK_BG_UNCACHEABLE_RISK.red,
  );
}

function protectDeviceSpread(baseScore: number, localScore: number, neutralScore: number, raw: LocalRiskRaw): number {
  const isSmallAreaBlur = raw.bgBlurAreaRadius > 0 && raw.bgBlurAreaRadius <= SMALL_AREA_BLUR_RISK;
  if (baseScore < BARELY_USABLE_SCORE
      && localScore >= BARELY_USABLE_SCORE
      && (neutralScore >= NEUTRAL_SDK_GREEN_SCORE || isSmallAreaBlur)) {
    return BARELY_USABLE_SCORE;
  }
  return baseScore;
}

function uint8ArrayToString(data: Uint8Array): string {
  if (typeof TextDecoder !== 'undefined') {
    return new TextDecoder('utf-8').decode(data);
  }
  const chunkSize = 8192;
  let result = '';
  for (let i = 0; i < data.length; i += chunkSize) {
    const chunk = data.subarray(i, Math.min(i + chunkSize, data.length));
    result += String.fromCharCode.apply(null, Array.from(chunk));
  }
  try {
    return decodeURIComponent(escape(result));
  } catch {
    return result;
  }
}

// ============================================================================
// Mali GPU Driver Defect Detection (Dimensity 9500 / 9400+ / 9400)
// ============================================================================
//
// The WeChat WebGL RENDERER returns a generic string. Use the WEBGL_debug_renderer_info
// extension to obtain the real GPU name from UNMASKED_RENDERER: Mali-G925 (Dimensity 9400)
// / Mali-G1-Ultra (Dimensity 9500). Dimensity 9300 (Mali-G720) is unaffected.

interface DimensityDetectionResult {
  isUnsupported: boolean;
  /** Detection source, for logging. */
  gpuRenderer: string;
}

function detectDimensityUnsupported(gpuRenderer: string): DimensityDetectionResult | null {
  if (gpuRenderer && DIMENSITY_GPU_RENDERER_PATTERN.test(gpuRenderer)) {
    return { isUnsupported: true, gpuRenderer };
  }
  return null;
}

/**
 * Dimensity 9500 / 9400+ / 9400 devices are unsupported; returns this special value.
 * Consumers check score === DIMENSITY_UNSUPPORTED_SCORE.
 */
export const DIMENSITY_UNSUPPORTED_SCORE = -1;

// ============================================================================
// Main Export Function
// ============================================================================

/**
 * Evaluate the render-stutter risk of a PAGX file; a higher score means smoother rendering.
 *
 * Rendering recommendation (decided by runtime platform):
 * - Android: score >= 65 renders normally.
 * - Other platforms (iOS, etc.): score >= 75 renders normally.
 *
 * It automatically fetches device performance info and adjusts the thresholds by device tier:
 * - High-end (benchmarkLevel >=30/36): use the default thresholds (calibration baseline).
 * - Mid-range (benchmarkLevel 23-29/30-35): tightened thresholds.
 * - Low-end (benchmarkLevel <=22/29): thresholds tightened further.
 *
 * @param pagxData - Binary data of the PAGX file (Uint8Array).
 * @returns Promise<PagxCheckResult> - contains the score, device performance level and device tier.
 *
 * @example
 * ```typescript
 * import { CheckPagx } from './pagx-check';
 *
 * const fs = wx.getFileSystemManager();
 * const data = fs.readFileSync(filePath);
 * const result = await CheckPagx(new Uint8Array(data));
 *
 * const minScore = result.platform === 'android' ? 65 : 75;
 * if (result.score < minScore) {
 *   wx.showToast({ title: 'This file may cause stutter', icon: 'none' });
 * }
 * ```
 */
export async function CheckPagx(pagxData: Uint8Array): Promise<PagxCheckResult> {
  // Fetch device info first (device info must be returned even if parsing fails).
  const deviceInfo = await fetchDeviceInfoAsync();

  // Dimensity 9400 / 9400+ / 9500 (Mali-G925 GPU) have a driver-level memory defect; block immediately.
  const dimensityCheck = detectDimensityUnsupported(deviceInfo.gpuRenderer);
  if (dimensityCheck?.isUnsupported) {
    return {
      score: DIMENSITY_UNSUPPORTED_SCORE,
      benchmarkLevel: deviceInfo.benchmarkLevel,
      deviceTier: deviceInfo.tier,
      platform: deviceInfo.platform,
      brand: deviceInfo.brand,
      model: deviceInfo.model,
      gpuRenderer: deviceInfo.gpuRenderer,
    };
  }

  // Default return value (used when parsing fails).
  const defaultResult: PagxCheckResult = {
    score: 0,
    benchmarkLevel: deviceInfo.benchmarkLevel,
    deviceTier: deviceInfo.tier,
    platform: deviceInfo.platform,
    gpuRenderer: deviceInfo.gpuRenderer,
  };

  // Convert the Uint8Array to a string.
  let pagxContent: string;
  try {
    pagxContent = uint8ArrayToString(pagxData);
  } catch {
    return defaultResult;
  }

  // Parse the XML.
  const root = parseXml(pagxContent);
  if (!root) return defaultResult;

  // Adjust the thresholds by device tier and platform.
  const riskPaths = getAdjustedRiskPaths(deviceInfo.tier, deviceInfo.platform);

  // Read the document dimensions (use readNumericAttr like scanLocalRisk for consistency).
  const docWidth = readNumericAttr(root.attribs, 'width');
  const docHeight = readNumericAttr(root.attribs, 'height');

  // Count the raw XML tags (not expanded, used by the layer_xml risk path).
  const rawTagCounts = countTagsRaw(root);

  // Count the tags after expanding Composition (used for the actual rendering-cost calculation).
  const expandedTagCounts = countTagsWithExpansion(root);

  // Key metrics (use the expanded counts to reflect the real rendering cost).
  const bgBlurCount = expandedTagCounts.get('BackgroundBlurStyle') || 0;
  const imagePattern = expandedTagCounts.get('ImagePattern') || 0;
  const innerShadowCount = expandedTagCounts.get('InnerShadowStyle') || 0;
  const blurFilterCount = expandedTagCounts.get('BlurFilter') || 0;
  const gradientCount =
    (expandedTagCounts.get('LinearGradient') || 0) +
    (expandedTagCounts.get('RadialGradient') || 0) +
    (expandedTagCounts.get('SweepGradient') || 0);

  // Compute the total Path data byte count (expanding Composition references).
  const pathDataBytes = countPathDataWithExpansion(root);

  // layer_xml uses the raw XML count (consistent with the Python version).
  const layerXml = rawTagCounts.get('Layer') || 0;

  // The expanded Layer count is used for the density calculation of path C.
  const expandedLayerCount = expandedTagCounts.get('Layer') || 0;

  const docPixels = Math.floor(docWidth * docHeight);
  const pixM = docPixels / 1_000_000;

  // Compute the raw values of the five risk paths.
  const riskRaw: Record<string, number> = {
    // Path A: BgBlur × uncacheable elements (uses the expanded counts).
    A_bg_x_uncacheable:
      bgBlurCount * (innerShadowCount + blurFilterCount + gradientCount / 10),
    // Path B: Path data volume (uses the expanded counts).
    B_path_data_MB: pathDataBytes / (1024 * 1024),
    // Path C: canvas × element density (uses the expanded Layer count).
    C_canvas_x_density:
      (pixM / 100) * (imagePattern + expandedLayerCount / 30 + gradientCount / 20),
    // Path D: BgBlur count (uses the expanded counts).
    bg_blur_count: bgBlurCount,
    // Path E: Layer XML count (uses the raw XML count, consistent with the Python version).
    layer_xml: layerXml,
  };

  // Compute the contribution score of each path and take the minimum.
  let minScore = 100;
  for (const [key, cfg] of Object.entries(riskPaths)) {
    const score = normalize(riskRaw[key], cfg.yellow, cfg.red);
    if (score < minScore) minScore = score;
  }

  // Supplementary calibration from real-device business samples:
  // - Large-area bgBlur bottoms out at 50 (openable but not recommended).
  // - Image pressure only counts as a runtime FPS risk when combined with bgBlur / layers / text / canvas complexity.
  // - Small-area but high-count blur is kept from being wrongly killed by the SDK A path.
  const localRiskRaw = scanLocalRisk(root);
  const localScore = scoreLocalRisk(localRiskRaw);
  const neutralScore = scoreNeutralSdkRisk(localRiskRaw);
  const protectedBaseScore = protectDeviceSpread(minScore, localScore, neutralScore, localRiskRaw);

  // Return the full result.
  return {
    score: Math.min(protectedBaseScore, localScore),
    benchmarkLevel: deviceInfo.benchmarkLevel,
    deviceTier: deviceInfo.tier,
    platform: deviceInfo.platform,
    brand: deviceInfo.brand,
    model: deviceInfo.model,
    gpuRenderer: deviceInfo.gpuRenderer,
  };
}
