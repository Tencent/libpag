/* eslint-disable @typescript-eslint/no-explicit-any */
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
 * 渲染建议（按运行平台判定）：
 *   - Android：score >= 65 可正常渲染，否则可能卡顿
 *   - 其他平台（iOS 等）：score >= 75 可正常渲染，否则可能卡顿
 *
 * 根据设备性能等级 (benchmarkLevel) 动态调整阈值：
 *   - 高端机：使用默认阈值（校准基准）
 *   - 中端机：阈值收紧
 *   - 低端机：阈值进一步收紧，更严格的限制
 */

// ============================================================================
// Types
// ============================================================================

type DeviceTier = 'high' | 'mid' | 'low' | 'unknown';
type Platform = 'ios' | 'android' | 'other';

/** checkPagx 返回的完整结果 */
export interface PagxCheckResult {
  /** 评分（0-100），分数越高越流畅；Android 平台 >= 65 可正常渲染，其他平台 >= 75 可正常渲染 */
  score: number;
  /** 设备性能等级（微信 API 返回的原始值，-1 表示未知） */
  benchmarkLevel: number;
  /** 设备档位 */
  deviceTier: DeviceTier;
  /** 平台 */
  platform: Platform;
}

// ============================================================================
// Device Info Cache
// ============================================================================

interface DeviceInfoCache {
  tier: DeviceTier;
  benchmarkLevel: number;
  platform: Platform;
}

let cachedDeviceInfo: DeviceInfoCache | null = null;

/**
 * 根据 benchmarkLevel 和平台判断设备档位
 *
 * Android 平台：
 *   - 高档机: ≥30 (小米15、OPPO Find X8)
 *   - 中档机: 23-29 (HONOR 200、REDMI K40)
 *   - 低档机: ≤22 (HONOR Play 20、VIVO Y52s)
 *
 * iOS 平台：
 *   - 高档机: ≥36 (iPhone 15/16/17)
 *   - 中档机: 30-35 (iPhone 11/12)
 *   - 低档机: ≤29 (iPhone 7/8/X)
 */
function getDeviceTier(benchmarkLevel: number, platform: string): DeviceTier {
  // benchmarkLevel = -1 表示性能未知
  if (benchmarkLevel < 0) return 'unknown';

  if (platform === 'ios') {
    if (benchmarkLevel >= 36) return 'high';
    if (benchmarkLevel >= 30) return 'mid';
    return 'low';
  } else {
    // Android 或其他平台
    if (benchmarkLevel >= 30) return 'high';
    if (benchmarkLevel >= 23) return 'mid';
    return 'low';
  }
}

/**
 * 异步获取设备信息（调用 wx.getDeviceBenchmarkInfo）
 */
async function fetchDeviceInfoAsync(): Promise<DeviceInfoCache> {
  // 如果已缓存，直接返回
  if (cachedDeviceInfo !== null) {
    return cachedDeviceInfo;
  }

  try {
    // 同步获取平台信息
    const systemInfo = wx.getSystemInfoSync();
    const rawPlatform = systemInfo.platform || 'android';
    const platform: Platform = rawPlatform === 'ios' ? 'ios' : rawPlatform === 'android' ? 'android' : 'other';

    // 异步获取设备性能等级
    const benchmarkLevel = await new Promise<number>((resolve) => {
      wx.getDeviceBenchmarkInfo({
        success(res: { benchmarkLevel: number }) {
          resolve(res.benchmarkLevel);
        },
        fail() {
          resolve(-1);
        },
      });
    });
    const tier = getDeviceTier(benchmarkLevel, rawPlatform);
    cachedDeviceInfo = { tier, benchmarkLevel, platform };
  } catch {
    // 获取失败，使用保守的 unknown
    cachedDeviceInfo = { tier: 'unknown', benchmarkLevel: -1, platform: 'other' };
  }

  return cachedDeviceInfo;
}

// ============================================================================
// Risk Path Configuration
// ============================================================================

/**
 * 不同设备档位的阈值系数
 * - 高端机：基准（1.0 倍）
 * - 中端机：阈值收紧到 0.77 倍
 * - 低端机：阈值收紧到 0.54 倍
 * - 未知：保守处理，使用 0.62 倍
 *
 * 注：原校准数据基于中端机，现改为高端机为基准
 * 原系数：high=1.3, mid=1.0, low=0.7, unknown=0.8
 * 换算后：high=1.0, mid=0.77, low=0.54, unknown=0.62
 */
const TIER_MULTIPLIERS: Record<DeviceTier, number> = {
  high: 1.0,
  mid: 0.77,
  low: 0.54,
  unknown: 0.62,
};

/**
 * 平台系数：iOS 小程序渲染性能比 Android 差，需要更严格的阈值
 *
 * 原因分析：
 * - iOS 小程序 WebView 的 WebGL 实现与 Android 有差异
 * - iOS 对复杂图层合成的处理效率较低
 * - iOS 小程序的内存管理更严格，容易触发 GC
 *
 * 系数说明（乘到阈值上，系数越小阈值越低，评分越严格）：
 * - iOS: 0.6 (阈值降低到 60%，更严格)
 * - Android: 1.0 (基准)
 * - Other: 0.8 (保守处理)
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

/**
 * 根据设备档位和平台调整阈值
 * 最终系数 = 设备档位系数 × 平台系数
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
 * 构建 Composition 查找表
 * 从 Resources 节点中提取所有 Composition 定义
 */
function buildCompositionLookup(root: XmlNode): Map<string, XmlNode> {
  const lookup = new Map<string, XmlNode>();

  // 查找 Resources 节点
  const resources = root.children.find((c) => c.tag === 'Resources');
  if (!resources) return lookup;

  // 收集所有 Composition 定义
  for (const comp of findAllByTag(resources, 'Composition')) {
    const id = comp.attribs.id;
    if (id) {
      lookup.set(id, comp);
    }
  }

  return lookup;
}

/**
 * 统计单个 Composition 内的标签数量（带缓存，防止循环引用）
 * 返回的是 Composition 定义内部的标签数量（不包含嵌套引用展开）
 */
function getCompositionTagCounts(
  compId: string,
  compLookup: Map<string, XmlNode>,
  cache: Map<string, Map<string, number>>,
): Map<string, number> {
  // 已缓存则直接返回
  if (cache.has(compId)) {
    return cache.get(compId)!;
  }

  const comp = compLookup.get(compId);
  if (!comp) {
    const empty = new Map<string, number>();
    cache.set(compId, empty);
    return empty;
  }

  // 防止循环引用：先设置空 Map
  const counts = new Map<string, number>();
  cache.set(compId, counts);

  // 递归统计 Composition 内部的标签（会展开嵌套的 Composition 引用）
  countTagsInNodeInternal(comp, counts, compLookup, cache);

  return counts;
}

/**
 * 递归统计节点内的标签数量（内部函数）
 * 遇到 Composition 引用时会展开
 */
function countTagsInNodeInternal(
  node: XmlNode,
  counts: Map<string, number>,
  compLookup: Map<string, XmlNode>,
  cache: Map<string, Map<string, number>>,
): void {
  // 统计当前节点
  counts.set(node.tag, (counts.get(node.tag) || 0) + 1);

  // 如果是 Layer 且引用了 Composition，获取并累加 Composition 的标签计数
  if (node.tag === 'Layer') {
    const compRef = node.attribs.composition;
    if (compRef && compRef.startsWith('@')) {
      const refId = compRef.slice(1);
      const refCounts = getCompositionTagCounts(refId, compLookup, cache);
      // 合并引用的 Composition 的标签计数（累加到当前 counts）
      for (const [tag, count] of refCounts) {
        counts.set(tag, (counts.get(tag) || 0) + count);
      }
    }
  }

  // 递归处理子节点
  for (const child of node.children) {
    countTagsInNodeInternal(child, counts, compLookup, cache);
  }
}

/**
 * 统计整个文档的标签数量（展开 Composition 引用）
 * 只统计主体部分（排除 Resources），但会展开 Composition 引用
 */
function countTagsWithExpansion(root: XmlNode): Map<string, number> {
  const compLookup = buildCompositionLookup(root);
  const cache = new Map<string, Map<string, number>>();
  const totalCounts = new Map<string, number>();

  // 只遍历主体节点（排除 Resources）
  for (const child of root.children) {
    if (child.tag === 'Resources') continue;
    countTagsInNodeInternal(child, totalCounts, compLookup, cache);
  }

  return totalCounts;
}

/**
 * 统计 XML 中原始的标签数量（不展开 Composition）
 * 用于 layer_xml 风险路径，与 Python 版保持一致
 */
function countTagsRaw(root: XmlNode): Map<string, number> {
  const counts = new Map<string, number>();
  for (const node of iterNodes(root)) {
    counts.set(node.tag, (counts.get(node.tag) || 0) + 1);
  }
  return counts;
}

/**
 * 计算展开后的 Path data 总字节数
 */
function countPathDataWithExpansion(root: XmlNode): number {
  const compLookup = buildCompositionLookup(root);
  const pathDataCache = new Map<string, number>();

  // 计算单个 Composition 的 Path data 字节数（带缓存）
  function getCompPathData(compId: string): number {
    if (pathDataCache.has(compId)) {
      return pathDataCache.get(compId)!;
    }

    const comp = compLookup.get(compId);
    if (!comp) {
      pathDataCache.set(compId, 0);
      return 0;
    }

    // 防止循环引用：先设置 0
    pathDataCache.set(compId, 0);
    const bytes = countPathDataInNode(comp);
    pathDataCache.set(compId, bytes);
    return bytes;
  }

  // 递归计算节点的 Path data
  function countPathDataInNode(node: XmlNode): number {
    let total = 0;

    // 如果是 Path，累加 data 长度
    if (node.tag === 'Path') {
      const d = node.attribs.data || '';
      if (d) total += d.length;
    }

    // 如果是 Layer 且引用了 Composition
    if (node.tag === 'Layer') {
      const compRef = node.attribs.composition;
      if (compRef && compRef.startsWith('@')) {
        total += getCompPathData(compRef.slice(1));
      }
    }

    // 递归子节点
    for (const child of node.children) {
      total += countPathDataInNode(child);
    }

    return total;
  }

  // 只统计主体部分
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
// Main Export Function
// ============================================================================

/**
 * 评估 PAGX 文件的渲染卡顿风险，分数越高代表越流畅
 *
 * 渲染建议（按运行平台判定）：
 * - Android：score >= 65 可正常渲染
 * - 其他平台（iOS 等）：score >= 75 可正常渲染
 *
 * 内部会自动获取设备性能信息，根据设备档位动态调整阈值：
 * - 高端机 (benchmarkLevel ≥30/36)：使用默认阈值（校准基准）
 * - 中端机 (benchmarkLevel 23-29/30-35)：阈值收紧
 * - 低端机 (benchmarkLevel ≤22/29)：阈值进一步收紧
 *
 * @param pagxData - PAGX 文件的二进制数据 (Uint8Array)
 * @returns Promise<PagxCheckResult> - 包含评分、设备性能等级和设备档位
 *
 * @example
 * ```typescript
 * import { checkPagx } from './pagx-check';
 *
 * const fs = wx.getFileSystemManager();
 * const data = fs.readFileSync(filePath);
 * const result = await checkPagx(new Uint8Array(data));
 *
 * const minScore = result.platform === 'android' ? 65 : 75;
 * if (result.score < minScore) {
 *   wx.showToast({ title: '该文件可能导致卡顿', icon: 'none' });
 * }
 * ```
 */
export async function CheckPagx(pagxData: Uint8Array): Promise<PagxCheckResult> {
  // 先获取设备信息（即使解析失败也需要返回设备信息）
  const deviceInfo = await fetchDeviceInfoAsync();

  // 默认返回值（解析失败时使用）
  const defaultResult: PagxCheckResult = {
    score: 0,
    benchmarkLevel: deviceInfo.benchmarkLevel,
    deviceTier: deviceInfo.tier,
    platform: deviceInfo.platform,
  };

  // 将 Uint8Array 转换为字符串
  let pagxContent: string;
  try {
    pagxContent = uint8ArrayToString(pagxData);
  } catch {
    return defaultResult;
  }

  // 解析 XML
  const root = parseXml(pagxContent);
  if (!root) return defaultResult;

  // 根据设备档位和平台调整阈值
  const riskPaths = getAdjustedRiskPaths(deviceInfo.tier, deviceInfo.platform);

  // 获取文档尺寸
  const docWidth = parseFloat(root.attribs.width || '0') || 0;
  const docHeight = parseFloat(root.attribs.height || '0') || 0;

  // 统计 XML 原始标签数量（不展开，用于 layer_xml 风险路径）
  const rawTagCounts = countTagsRaw(root);

  // 统计展开 Composition 后的标签数量（用于实际渲染开销计算）
  const expandedTagCounts = countTagsWithExpansion(root);

  // 关键指标（使用展开后的数量，反映真实渲染开销）
  const bgBlurCount = expandedTagCounts.get('BackgroundBlurStyle') || 0;
  const imagePattern = expandedTagCounts.get('ImagePattern') || 0;
  const innerShadowCount = expandedTagCounts.get('InnerShadowStyle') || 0;
  const blurFilterCount = expandedTagCounts.get('BlurFilter') || 0;
  const gradientCount =
    (expandedTagCounts.get('LinearGradient') || 0) +
    (expandedTagCounts.get('RadialGradient') || 0) +
    (expandedTagCounts.get('SweepGradient') || 0);

  // 计算 Path data 总字节数（展开 Composition 引用）
  const pathDataBytes = countPathDataWithExpansion(root);

  // layer_xml 使用 XML 原始数量（与 Python 版一致）
  const layerXml = rawTagCounts.get('Layer') || 0;

  // 展开后的 Layer 数量用于路径 C 的密度计算
  const expandedLayerCount = expandedTagCounts.get('Layer') || 0;

  const docPixels = Math.floor(docWidth * docHeight);
  const pixM = docPixels / 1_000_000;

  // 计算五条风险路径的原始值
  const riskRaw: Record<string, number> = {
    // 路径 A：BgBlur × 不可缓存元素（使用展开后数量）
    A_bg_x_uncacheable:
      bgBlurCount * (innerShadowCount + blurFilterCount + gradientCount / 10),
    // 路径 B：Path 数据量（使用展开后数量）
    B_path_data_MB: pathDataBytes / (1024 * 1024),
    // 路径 C：画布 × 元素密度（使用展开后的 Layer 数量）
    C_canvas_x_density:
      (pixM / 100) * (imagePattern + expandedLayerCount / 30 + gradientCount / 20),
    // 路径 D：BgBlur 数量（使用展开后数量）
    bg_blur_count: bgBlurCount,
    // 路径 E：Layer XML 数量（使用原始 XML 数量，与 Python 版一致）
    layer_xml: layerXml,
  };

  // 计算各路径的贡献分数，取最低分
  let minScore = 100;
  for (const [key, cfg] of Object.entries(riskPaths)) {
    const score = normalize(riskRaw[key], cfg.yellow, cfg.red);
    if (score < minScore) minScore = score;
  }

  // 返回完整结果
  return {
    score: minScore,
    benchmarkLevel: deviceInfo.benchmarkLevel,
    deviceTier: deviceInfo.tier,
    platform: deviceInfo.platform,
  };
}
