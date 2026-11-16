#!/usr/bin/env node
/**
 * freeze.js — build the "frozen" twin of a PAGX document: every animated channel evaluated at one
 * instant and written back as a plain node attribute, with <Animations> removed.
 *
 * Why: `pagx render` of the frozen twin at t=0 and a browser screenshot of the *static* HTML export
 * of that same twin compare the two renderers on one identical document state with no keyframe
 * machinery on either side. The gap between that pair (staticSSIM) and the animated pair
 * (animSSIM) is what the animation writer contributes, which is how a font-shape difference gets
 * told apart from an exporter bug that is present even on the static path.
 *
 * The evaluation mirrors the runtime, not an approximation of it:
 *   - playback mapping    src/pagx/PAGAnimation.cpp (apply): Once clamps to the offset window,
 *                         Loop/PingPong wrap over one period, the offset is in frames.
 *   - sample position     src/pagx/animation/Channel.cpp: framePosition = us * frameRate / 1e6.
 *   - key interpolation   src/pagx/runtime/KeyframeEvaluatorImpl.h (EvaluateKeyframeSequence): the
 *                         segment start key owns `interpolation`; None/Hold return the left value;
 *                         Bezier eases with the left bezierOut and the right bezierIn; else lerp.
 *   - bezier easing       src/pagx/runtime/BezierEasing.cpp (Newton + bisection, exact port).
 *   - color lerp          src/pagx/utils/ColorSpaceUtils.cpp (LerpColor): per-component lerp in the
 *                         stored color space, each component clamped to [0, 1].
 *   - bool / int / string src/pagx/runtime/KeyframeEvaluatorImpl.h: their lerp returns the left
 *                         value, i.e. they are inherently hold channels.
 *
 * Matrix / Matrix3D channels interpolate through decompose-and-mix, which is not ported here: they
 * hold the left key value. Such a document is reported as `exact: false` so the caller drops the
 * attribution instead of trusting a state that may be wrong.
 */
'use strict';

// ---------------------------------------------------------------------------------------------
// Minimal XML scan. PAGX is attribute-only (no mixed content) and machine written, so a tag scanner
// that records character spans is enough — and it lets the rewrite keep every byte it does not
// touch, so a diff of a frozen document only shows the baked attributes.
// ---------------------------------------------------------------------------------------------

const ATTR_RE = /\s([A-Za-z_][\w.:-]*)\s*=\s*"([^"]*)"/g;

function findTagEnd(text, from) {
  let quote = false;
  for (let i = from; i < text.length; i++) {
    const ch = text[i];
    if (ch === '"') quote = !quote;
    else if (ch === '>' && !quote) return i + 1;
  }
  return text.length;
}

function parseTagAttributes(text, tagStart, tagEnd) {
  const body = text.slice(tagStart, tagEnd);
  const attrs = [];
  ATTR_RE.lastIndex = 0;
  let m;
  while ((m = ATTR_RE.exec(body)) !== null) {
    const valueStart = tagStart + m.index + m[0].length - m[2].length - 1;
    attrs.push({ name: m[1], value: m[2], valueStart, valueEnd: valueStart + m[2].length });
  }
  return attrs;
}

function attrOf(rec, name) {
  for (const a of rec.attrs) {
    if (a.name === name) return a.value;
  }
  return null;
}

// Records every element with its character span and parent, so callers can find the enclosing
// <Animations> of an <Animation>, the node a channel targets, and cut whole subtrees out.
function scanDocument(text) {
  const tags = [];
  const stack = [];
  let i = 0;
  while (i < text.length) {
    const lt = text.indexOf('<', i);
    if (lt < 0) break;
    if (text.startsWith('<!--', lt)) {
      const close = text.indexOf('-->', lt);
      i = close < 0 ? text.length : close + 3;
      continue;
    }
    if (text.startsWith('<![CDATA[', lt)) {
      const close = text.indexOf(']]>', lt);
      i = close < 0 ? text.length : close + 3;
      continue;
    }
    if (text.startsWith('<?', lt) || text.startsWith('<!', lt)) {
      const close = text.indexOf('>', lt);
      i = close < 0 ? text.length : close + 1;
      continue;
    }
    if (text.startsWith('</', lt)) {
      const close = text.indexOf('>', lt);
      const rec = stack.pop();
      if (rec) {
        rec.closeStart = lt;
        rec.bodyEnd = close < 0 ? text.length : close + 1;
      }
      i = close < 0 ? text.length : close + 1;
      continue;
    }
    const end = findTagEnd(text, lt);
    const raw = text.slice(lt, end);
    const nameMatch = raw.match(/^<([A-Za-z_][\w.:-]*)/);
    if (!nameMatch) {
      i = end;
      continue;
    }
    const selfClosing = /\/\s*>$/.test(raw);
    const rec = {
      name: nameMatch[1],
      start: lt,
      end,
      selfClosing,
      attrs: parseTagAttributes(text, lt, end),
      parent: stack.length ? stack[stack.length - 1] : null,
      depth: stack.length,
      closeStart: selfClosing ? end : -1,
      bodyEnd: selfClosing ? end : -1,
    };
    tags.push(rec);
    if (!selfClosing) stack.push(rec);
    i = end;
  }
  return tags;
}

function childrenOf(tags, parent) {
  return tags.filter((tag) => tag.parent === parent);
}

// ---------------------------------------------------------------------------------------------
// Value evaluation
// ---------------------------------------------------------------------------------------------

const LOOP_ONCE = 'once';
const LOOP_LOOP = 'loop';
const LOOP_PING_PONG = 'pingPong';

// Mirrors FramesToUs in src/pagx/PAGStateMachineRegion.h.
function framesToUs(frames, frameRate) {
  if (!(frameRate > 0)) return 0;
  return Math.trunc((frames / frameRate) * 1e6);
}

// Mirrors WrapTime in src/pagx/PAGAnimation.cpp.
function wrapTime(time, duration, loop) {
  if (duration <= 0) return time;
  if (loop === LOOP_ONCE) return Math.min(Math.max(time, 0), duration);
  if (loop === LOOP_LOOP) {
    let pos = time % duration;
    if (pos < 0) pos += duration;
    return pos;
  }
  const period = duration * 2;
  let pos = time % period;
  if (pos < 0) pos += period;
  return pos < duration ? pos : period - pos;
}

// Mirrors the evaluationTimeUs block of PAGAnimation::apply.
function evaluationTimeUs(elapsedUs, durationUs, loop, offsetUs) {
  if (durationUs <= 0) return elapsedUs - offsetUs;
  if (loop === LOOP_ONCE) {
    return Math.min(Math.max(elapsedUs, offsetUs), offsetUs + durationUs) - offsetUs;
  }
  const period = loop === LOOP_PING_PONG ? durationUs * 2 : durationUs;
  return wrapTime((elapsedUs % period) - offsetUs, durationUs, loop);
}

function bezierCoefficients(p1, p2) {
  const cx0 = 3 * p1;
  const cx1 = 3 * (p2 - p1) - cx0;
  const cx2 = 1 - cx0 - cx1;
  return { cx0, cx1, cx2 };
}

function sampleCurve(p1, p2, t) {
  const c = bezierCoefficients(p1, p2);
  return ((c.cx2 * t + c.cx1) * t + c.cx0) * t;
}

function sampleDerivative(p1, p2, t) {
  const c = bezierCoefficients(p1, p2);
  return (3 * c.cx2 * t + 2 * c.cx1) * t + c.cx0;
}

function solveCurveX(p1x, p2x, input) {
  const NEWTON_ITERATIONS = 8;
  const MAX_BISECTION_ITERATIONS = 60;
  const EPSILON = 1.0e-7;
  let t = input;
  for (let i = 0; i < NEWTON_ITERATIONS; i++) {
    const currentX = sampleCurve(p1x, p2x, t) - input;
    if (Math.abs(currentX) < EPSILON) return t;
    const derivative = sampleDerivative(p1x, p2x, t);
    if (Math.abs(derivative) < EPSILON) break;
    t -= currentX / derivative;
  }
  let low = 0;
  let high = 1;
  t = input;
  for (let i = 0; i < MAX_BISECTION_ITERATIONS && low < high; i++) {
    const currentX = sampleCurve(p1x, p2x, t);
    if (Math.abs(currentX - input) < EPSILON) return t;
    if (input > currentX) low = t;
    else high = t;
    t = (low + high) * 0.5;
  }
  return t;
}

// Mirrors SolveBezierEasing in src/pagx/runtime/BezierEasing.cpp, including the identity shortcut
// that makes a bezier segment without control handles behave like a linear one.
function solveBezierEasing(p1x, p1y, p2x, p2y, input) {
  if (input <= 0) return 0;
  if (input >= 1) return 1;
  if (p1x === p1y && p2x === p2y) return input;
  return sampleCurve(p1y, p2y, solveCurveX(p1x, p2x, input));
}

function parsePoint(text) {
  if (!text) return { x: 0, y: 0 };
  const parts = String(text).split(',');
  return { x: parseFloat(parts[0]) || 0, y: parseFloat(parts[1]) || 0 };
}

function parseHexColor(text) {
  let hex = String(text || '').trim().replace(/^#/, '');
  if (hex.length === 3) hex = hex.split('').map((c) => c + c).join('');
  if (hex.length !== 6 && hex.length !== 8) return null;
  const r = parseInt(hex.slice(0, 2), 16);
  const g = parseInt(hex.slice(2, 4), 16);
  const b = parseInt(hex.slice(4, 6), 16);
  const a = hex.length === 8 ? parseInt(hex.slice(6, 8), 16) : 255;
  if (![r, g, b, a].every((v) => Number.isFinite(v))) return null;
  return { r, g, b, a };
}

function clamp01(value) {
  return Math.min(1, Math.max(0, value));
}

// Reads the <Key> children of a <Channel> record out of the scanned document.
function collectKeys(tags, channelRec) {
  const keys = [];
  for (const key of childrenOf(tags, channelRec)) {
    if (key.name !== 'Key') continue;
    keys.push({
      time: parseFloat(attrOf(key, 'time') || '0') || 0,
      value: attrOf(key, 'value'),
      interpolation: attrOf(key, 'interpolation'),
      bezierOut: parsePoint(attrOf(key, 'bezierOut') || attrOf(key, 'bezier-out')),
      bezierIn: parsePoint(attrOf(key, 'bezierIn') || attrOf(key, 'bezier-in')),
    });
  }
  keys.sort((a, b) => a.time - b.time);
  return keys;
}

// Mirrors EvaluateKeyframeSequence: clamp at both ends, then pick the segment by upper_bound.
function keySegment(keys, framePosition) {
  if (keys.length === 0) return null;
  if (framePosition <= keys[0].time) return { left: keys[0], right: null, rawT: 0 };
  const last = keys[keys.length - 1];
  if (framePosition >= last.time) return { left: last, right: null, rawT: 0 };
  let rightIdx = keys.length - 1;
  for (let i = 1; i < keys.length; i++) {
    if (keys[i].time > framePosition) {
      rightIdx = i;
      break;
    }
  }
  const left = keys[rightIdx - 1];
  const right = keys[rightIdx];
  const span = right.time - left.time;
  if (span <= 0) return { left, right: null, rawT: 0 };
  return { left, right, rawT: (framePosition - left.time) / span };
}

// The eased fraction of a segment: null means "hold the left value".
function segmentFactor(segment) {
  if (!segment.right) return null;
  const interp = segment.left.interpolation || 'linear';
  if (interp === 'hold' || interp === 'none') return null;
  if (interp === 'bezier') {
    return solveBezierEasing(
      segment.left.bezierOut.x, segment.left.bezierOut.y,
      segment.right.bezierIn.x, segment.right.bezierIn.y,
      segment.rawT,
    );
  }
  return segment.rawT;
}

// Evaluates one channel at the given frame position. `exact` is false when the value type cannot be
// interpolated here (matrix / matrix3D), in which case the left key value is held instead.
function evaluateChannel(type, keys, framePosition) {
  const segment = keySegment(keys, framePosition);
  if (segment == null) return { value: null, exact: false };
  if (type === 'color') {
    const start = parseHexColor(segment.left.value);
    if (!start) return { value: null, exact: false };
    const end = segment.right ? parseHexColor(segment.right.value) : null;
    const factor = segmentFactor(segment);
    if (factor === null || !end) return { value: start, exact: true };
    const lerp = (a, b) => Math.round(clamp01(a / 255 + ((b - a) / 255) * factor) * 255);
    return {
      value: { r: lerp(start.r, end.r), g: lerp(start.g, end.g), b: lerp(start.b, end.b), a: lerp(start.a, end.a) },
      exact: true,
    };
  }
  if (type === 'float') {
    const start = parseFloat(segment.left.value);
    if (!Number.isFinite(start)) return { value: null, exact: false };
    const factor = segmentFactor(segment);
    if (factor === null || !segment.right) return { value: start, exact: true };
    const end = parseFloat(segment.right.value);
    if (!Number.isFinite(end)) return { value: start, exact: true };
    return { value: start + (end - start) * factor, exact: true };
  }
  if (type === 'bool') {
    return { value: String(segment.left.value).trim() === 'true', exact: true };
  }
  if (type === 'int') {
    const value = parseInt(segment.left.value, 10);
    return { value: Number.isFinite(value) ? value : 0, exact: true };
  }
  if (type === 'string') {
    return { value: segment.left.value == null ? '' : String(segment.left.value), exact: true };
  }
  return { value: segment.left.value, exact: false };
}

// ---------------------------------------------------------------------------------------------
// Channel name -> where the evaluated value has to land. PAGX stores a sub-channel as a component of
// a comma-separated attribute (position="x,y"), except on Layer, which keeps one camelCase
// attribute per component.
// ---------------------------------------------------------------------------------------------

const LAYER_SUB_ATTRIBUTES = {
  'rotation.x': 'rotationX',
  'rotation.y': 'rotationY',
  'rotation.z': 'rotationZ',
  'scale.x': 'scaleX',
  'scale.y': 'scaleY',
  'scale.z': 'scaleZ',
  'skew.x': 'skewX',
  'skew.y': 'skewY',
};

// Nodes whose x / y channel drives the first / second component of `position` instead of a plain
// attribute of that name.
const POSITION_PAIR_NODES = {
  Text: { x: 0, y: 1 },
  TextBox: { x: 0, y: 1 },
};

const COMPONENT_INDEX = { x: 0, y: 1, z: 2, width: 0, height: 1 };

// A missing pair attribute falls back to these components; only scale differs from zero.
const PAIR_DEFAULTS = { scale: [1, 1, 1] };

// Shape nodes that own neither a color nor a corner radius: the runtime resolves those channels to
// the sibling paint / corner node inside the same layer.
const PAINT_TARGETS = new Set(['Rectangle', 'Ellipse', 'Polystar', 'Path', 'TrimPath', 'RoundCorner']);

// Enum-valued int channels: the attribute holds the enum name, not the ordinal.
const BLEND_MODE_NAMES = [
  'normal', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'colorDodge', 'colorBurn',
  'hardLight', 'softLight', 'difference', 'exclusion', 'hue', 'saturation', 'color', 'luminosity',
  'plusLighter', 'plusDarker',
];

function formatNumber(value) {
  if (!Number.isFinite(value)) return '0';
  const rounded = Math.round(value * 1e6) / 1e6;
  return Number.isInteger(rounded) ? String(rounded) : String(rounded);
}

function formatColor(color) {
  const hex = (v) => v.toString(16).toUpperCase().padStart(2, '0');
  const rgb = `#${hex(color.r)}${hex(color.g)}${hex(color.b)}`;
  return color.a === 255 ? rgb : `${rgb}${hex(color.a)}`;
}

function formatChannelValue(type, name, value) {
  if (type === 'color') return formatColor(value);
  if (type === 'bool') return value ? 'true' : 'false';
  if (type === 'int') return name === 'blendMode' ? (BLEND_MODE_NAMES[value] || 'normal') : String(value);
  if (type === 'float') return formatNumber(value);
  return String(value);
}

function findSibling(tags, target, name) {
  return childrenOf(tags, target.parent).find((tag) => tag !== target && tag.name === name) || null;
}

// Resolves where one channel write lands: a plain attribute, a component of a pair attribute, or the
// sibling node a shape-level paint / corner channel is routed to.
function resolveChannel(tags, target, channelName) {
  if (target.name === 'Path' && /^point[1-9]\.(x|y)$/.test(channelName)) {
    // The runtime rebuilds a contour Path from the animated components alone, so the authored
    // <Path data> is not the state it renders. No attribute write reproduces that; the caller
    // marks the case unavailable instead of baking a state the runtime never shows.
    return null;
  }
  if (PAINT_TARGETS.has(target.name) && channelName === 'color') {
    // The PAGX runtime does not drive a shape-level `color`: rendering the source at any time
    // paints it with the layer's own Fill. Leaving the node untouched therefore keeps the frozen
    // document on the state the runtime shows, so this is a skip, not an approximation.
    return { kind: 'skip' };
  }
  if (PAINT_TARGETS.has(target.name) && channelName === 'radius' && target.name !== 'RoundCorner') {
    // A shape's `radius` channel is its corner rounding, which the sibling RoundCorner node owns.
    const corner = findSibling(tags, target, 'RoundCorner');
    if (!corner) return null;
    return { kind: 'scalar', rec: corner, attribute: 'radius' };
  }
  if (target.name === 'Layer') {
    const camel = LAYER_SUB_ATTRIBUTES[channelName];
    if (camel) return { kind: 'scalar', rec: target, attribute: camel };
    if (channelName.startsWith('anchor.')) {
      return { kind: 'pair', rec: target, attribute: 'anchor', component: COMPONENT_INDEX[channelName.split('.')[1]] };
    }
    if (channelName.includes('.')) {
      const [base, component] = channelName.split('.');
      const index = COMPONENT_INDEX[component];
      if (index === undefined) return null;
      return { kind: 'pair', rec: target, attribute: base, component: index };
    }
    return { kind: 'scalar', rec: target, attribute: channelName };
  }
  const positionPair = POSITION_PAIR_NODES[target.name];
  if (positionPair && channelName in positionPair) {
    return { kind: 'pair', rec: target, attribute: 'position', component: positionPair[channelName] };
  }
  if (channelName.includes('.')) {
    const [base, component] = channelName.split('.');
    const index = COMPONENT_INDEX[component];
    if (index === undefined) return null;
    return { kind: 'pair', rec: target, attribute: base, component: index };
  }
  return { kind: 'scalar', rec: target, attribute: channelName };
}

// Rebuilds a comma-separated attribute with one component replaced, keeping the components the
// animation does not drive.
function pairWithComponent(attribute, current, component, value) {
  const defaults = PAIR_DEFAULTS[attribute] || [0, 0, 0];
  const parts = current == null ? [] : String(current).split(',');
  const arity = Math.max(2, parts.length, component + 1);
  const components = [];
  for (let i = 0; i < arity; i++) {
    const parsed = parseFloat(parts[i]);
    components.push(Number.isFinite(parsed) ? parsed : defaults[i]);
  }
  components[component] = value;
  return components.map(formatNumber).join(',');
}

// ---------------------------------------------------------------------------------------------
// Freezing
// ---------------------------------------------------------------------------------------------

function escapeAttr(value) {
  return String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;');
}

function rebuildTag(text, rec, writes) {
  let out = `<${rec.name}`;
  const seen = new Set();
  for (const attr of rec.attrs) {
    const override = writes.get(attr.name);
    const raw = text.slice(attr.valueStart, attr.valueEnd);
    out += ` ${attr.name}="${escapeAttr(override === undefined ? raw : override)}"`;
    seen.add(attr.name);
  }
  for (const [name, value] of writes) {
    if (seen.has(name)) continue;
    out += ` ${name}="${escapeAttr(value)}"`;
  }
  return out + (rec.selfClosing ? '/>' : '>');
}

/**
 * Evaluates every animated channel of `xml` at `timeSec` and returns the same document with the
 * values baked into node attributes and every <Animations> block removed.
 *
 * @param xml source PAGX text.
 * @param timeSec playback time in seconds.
 * @return {{xml: string, exact: boolean, notes: string[]}} the frozen document, whether every
 *         channel was evaluated exactly, and human-readable notes for anything skipped.
 */
function freezeToState(xml, timeSec) {
  const tags = scanDocument(xml);
  const byId = new Map();
  for (const rec of tags) {
    const id = attrOf(rec, 'id');
    if (id != null && !byId.has(id)) byId.set(id, rec);
  }

  const notes = [];
  let exact = true;
  const elapsedUs = Math.round(timeSec * 1e6);

  const mountTags = tags.filter((rec) => rec.name === 'Animation' && attrOf(rec, 'ref') != null);
  const animationBlocks = tags.filter((rec) => rec.name === 'Animations');
  const removed = [];
  const scalarWrites = new Map();  // tag -> Map(attribute -> value)
  const pairWrites = new Map();    // tag -> Map(attribute -> Map(component -> value))

  const declaredIds = new Set();
  for (const block of animationBlocks) {
    const rootLevel = block.parent != null && block.parent.name === 'pagx';
    for (const anim of childrenOf(tags, block)) {
      if (anim.name !== 'Animation') continue;
      const id = attrOf(anim, 'id');
      if (id == null) continue;
      declaredIds.add(id);
      const frameRate = parseFloat(attrOf(anim, 'frameRate') || '60') || 60;
      const duration = parseFloat(attrOf(anim, 'duration') || '0') || 0;
      const loop = attrOf(anim, 'loop') || LOOP_ONCE;
      const durationUs = framesToUs(duration, frameRate);

      const instances = [];
      if (rootLevel) {
        instances.push({ offsetFrames: 0, playing: true });
      } else {
        for (const mount of mountTags) {
          const ref = attrOf(mount, 'ref');
          if (ref == null || ref.replace(/^@/, '') !== id) continue;
          instances.push({
            offsetFrames: parseFloat(attrOf(mount, 'evaluationOffset') || '0') || 0,
            playing: attrOf(mount, 'playing') !== 'false',
          });
        }
        if (instances.length === 0) {
          notes.push(`unmounted:${id}`);
          continue;
        }
      }

      const framePositions = [];
      for (const instance of instances) {
        if (!instance.playing) continue;
        const offsetUs = framesToUs(instance.offsetFrames, frameRate);
        const evalUs = evaluationTimeUs(elapsedUs, durationUs, loop, offsetUs);
        framePositions.push(frameRate > 0 ? (evalUs * frameRate) / 1e6 : 0);
      }
      if (framePositions.length === 0) continue;  // every mount is paused: the values never apply
      if (framePositions.some((p) => Math.abs(p - framePositions[0]) > 1e-9)) {
        // Two slots mount one keyframe set at different phases; no single baked value carries both.
        notes.push(`multi-phase:${id}`);
        exact = false;
        continue;
      }

      const framePosition = framePositions[0];
      for (const object of childrenOf(tags, anim)) {
        if (object.name !== 'Object') continue;
        const targetId = attrOf(object, 'target');
        const target = targetId == null ? null : byId.get(targetId);
        if (target == null) {
          notes.push(`missing-target:${targetId}`);
          exact = false;
          continue;
        }
        for (const channel of childrenOf(tags, object)) {
          if (channel.name !== 'Channel') continue;
          const channelName = attrOf(channel, 'name');
          const type = attrOf(channel, 'type') || 'float';
          const destination = resolveChannel(tags, target, channelName);
          if (destination == null) {
            notes.push(`unsupported:${target.name}.${channelName}`);
            exact = false;
            continue;
          }
          if (destination.kind === 'skip') {
            notes.push(`skipped:${target.name}.${channelName}`);
            continue;
          }
          const evaluated = evaluateChannel(type, collectKeys(tags, channel), framePosition);
          if (evaluated.value == null) {
            notes.push(`unsupported-value:${target.name}.${channelName}`);
            exact = false;
            continue;
          }
          if (!evaluated.exact) {
            notes.push(`approximate:${target.name}.${channelName}`);
            exact = false;
          }
          const rec = destination.rec;
          if (destination.kind === 'scalar') {
            if (!scalarWrites.has(rec)) scalarWrites.set(rec, new Map());
            scalarWrites.get(rec).set(destination.attribute, formatChannelValue(type, channelName, evaluated.value));
          } else {
            if (!pairWrites.has(rec)) pairWrites.set(rec, new Map());
            const byAttribute = pairWrites.get(rec);
            if (!byAttribute.has(destination.attribute)) byAttribute.set(destination.attribute, new Map());
            byAttribute.get(destination.attribute).set(destination.component, evaluated.value);
          }
        }
      }
    }
    removed.push([block.start, block.bodyEnd]);
  }

  // A <Timelines> whose every mount pointed at a removed animation would keep a dangling ref, so
  // drop it wholesale. A timeline holding anything this script cannot classify stays untouched.
  for (const timelines of tags) {
    if (timelines.name !== 'Timelines') continue;
    const mounts = childrenOf(tags, timelines);
    if (mounts.length === 0) continue;
    const allRemoved = mounts.every((mount) => {
      if (mount.name !== 'Animation') return false;
      const ref = attrOf(mount, 'ref');
      return ref != null && declaredIds.has(ref.replace(/^@/, ''));
    });
    if (allRemoved) removed.push([timelines.start, timelines.bodyEnd]);
    else notes.push('timelines-kept');
  }

  const spans = [];
  const recs = new Set([...scalarWrites.keys(), ...pairWrites.keys()]);
  for (const rec of recs) {
    const writes = new Map();
    for (const [attribute, value] of scalarWrites.get(rec) || []) writes.set(attribute, value);
    for (const [attribute, components] of pairWrites.get(rec) || []) {
      let merged = attrOf(rec, attribute);
      if (merged == null) merged = (PAIR_DEFAULTS[attribute] || [0, 0]).join(',');
      for (const [component, value] of components) {
        merged = pairWithComponent(attribute, merged, component, value);
      }
      writes.set(attribute, merged);
    }
    if (writes.size) spans.push({ start: rec.start, end: rec.end, text: rebuildTag(xml, rec, writes) });
  }
  for (const [start, end] of removed) spans.push({ start, end, text: '' });
  spans.sort((a, b) => a.start - b.start);

  let out = '';
  let cursor = 0;
  for (const span of spans) {
    if (span.start < cursor) continue;  // a removal already swallowed this span
    out += xml.slice(cursor, span.start) + span.text;
    cursor = span.end;
  }
  out += xml.slice(cursor);

  return { xml: out, exact, notes };
}

module.exports = {
  freezeToState,
  evaluateChannel,
  evaluationTimeUs,
  wrapTime,
  solveBezierEasing,
  resolveChannel,
  parseHexColor,
  formatColor,
};
