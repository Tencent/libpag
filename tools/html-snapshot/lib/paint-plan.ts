// CSS painting is a traversal of boxes and stacking contexts, not a sort of DOM siblings.
// These helpers also run in the standalone browser bundle. Keep them as named functions:
// browser-snapshot embeds their source without a module loader.
// @ts-nocheck

const PAINT_SOURCE_ATTR = 'data-pagx-paint-source';
const PAINT_ORDER_ATTR = 'data-pagx-paint-order';
const PAINT_DECORATION_ATTR = 'data-pagx-paint-decoration';
const PAINT_CAPTURE = {
  active: false, nodes: [], byElement: new WeakMap(), warnings: [], animations: new Map(),
};

function paintProperty(style, name) {
  return String(style.getPropertyValue(name) || '').trim();
}

function paintStyleRecord(style, pseudo, flexItem, transformable = true) {
  const position = pseudo ? pseudo.position : style.position;
  const rawZ = String(pseudo ? pseudo.zIndex : style.zIndex).trim();
  const positioned = position !== 'static';
  const zApplies = positioned || flexItem;
  const zIndex = zApplies && rawZ !== 'auto' && /^-?\d+$/.test(rawZ) ? Number(rawZ) : null;
  const transform = transformable && ['transform', 'translate', 'rotate', 'scale', 'perspective'].some(
    name => !['', 'none'].includes(paintProperty(style, name)),
  );
  const filter = ['filter', 'backdrop-filter', '-webkit-backdrop-filter'].some(
    name => !['', 'none'].includes(paintProperty(style, name)),
  );
  const mask = ['clip-path', 'mask-image', '-webkit-mask-image'].some(
    name => !['', 'none'].includes(paintProperty(style, name)),
  );
  const willChange = paintProperty(style, 'will-change').split(',').map(value => value.trim());
  const willTransform = transformable && willChange.some(
    name => ['transform', 'translate', 'rotate', 'scale', 'perspective'].includes(name));
  const willContext = willTransform || willChange.some(name => [
    'opacity', 'filter',
    'backdrop-filter', 'clip-path', 'mask', 'mask-image', 'mix-blend-mode', 'isolation',
  ].includes(name)) || (zApplies && willChange.includes('z-index'));
  const contain = paintProperty(style, 'contain').split(/\s+/);
  const paintContainment = transformable && (contain.some(value =>
    ['paint', 'strict', 'content'].includes(value)) ||
    ['auto', 'hidden'].includes(paintProperty(style, 'content-visibility')));
  const containment = paintContainment || (transformable && contain.includes('layout'));
  const box = style.display !== 'none' && style.display !== 'contents';
  const context = box && (zIndex !== null || position === 'fixed' || position === 'sticky' ||
    Number(style.opacity) < 1 || transform || filter || mask || containment || willContext ||
    paintProperty(style, 'isolation') === 'isolate' ||
    !['', 'normal'].includes(paintProperty(style, 'mix-blend-mode')));
  const containingBlock = transform || filter || containment || willTransform ||
    willChange.some(name => ['filter', 'backdrop-filter'].includes(name));
  return {
    display: style.display, position, positioned, zIndex, context, flexItem, paintContainment,
    floating: !['', 'none'].includes(paintProperty(style, 'float')),
    order: Number(paintProperty(style, 'order')) || 0,
    clips: box && (paintContainment || ['overflow-x', 'overflow-y'].some(name =>
      !['', 'visible'].includes(paintProperty(style, name)))),
    absContainingBlock: box && (positioned || containingBlock),
    fixedContainingBlock: box && containingBlock,
    preserve3D: paintProperty(style, 'transform-style') === 'preserve-3d',
  };
}

function beginPaintCapture(root) {
  PAINT_CAPTURE.active = true;
  PAINT_CAPTURE.nodes = [];
  PAINT_CAPTURE.byElement = new WeakMap();
  PAINT_CAPTURE.warnings = [];
  PAINT_CAPTURE.animations = new Map();
  // The animation capture pass retains these rules even after measurement cancels their
  // live effects. Animated transform/opacity still need one compositing group in the output.
  const animationSheet = document.getElementById('__pagx_anim_keyframes');
  for (const rule of animationSheet && animationSheet.sheet ? animationSheet.sheet.cssRules : []) {
    if (!rule.name || !rule.cssRules) continue;
    const properties = new Set();
    for (const frame of rule.cssRules) {
      for (const name of Array.from(frame.style)) properties.add(name);
    }
    PAINT_CAPTURE.animations.set(rule.name, properties);
  }
  collectPaintSources(root, null, null, null, []);
}

function collectPaintSources(el, parent, absBlock, fixedBlock, inheritedClips) {
  const computed = getComputedStyle(el);
  const parentDisplay = parent ? parent.style.display : '';
  const flexItem = /^(inline-)?(flex|grid)$/.test(parentDisplay) &&
    !['absolute', 'fixed'].includes(computed.position);
  const replaced = ['IMG', 'SVG', 'CANVAS', 'VIDEO', 'IFRAME', 'OBJECT', 'EMBED', 'INPUT', 'SELECT',
    'TEXTAREA', 'BUTTON'].includes(el.tagName.toUpperCase());
  const transformable = replaced ||
    !['inline', 'table-column', 'table-column-group'].includes(computed.display);
  const style = paintStyleRecord(computed, el.__pagxSnapshotPseudoPaintStyle, flexItem, transformable);
  style.replaced = replaced;
  let animatedStacking = false;
  for (const name of paintProperty(computed, 'animation-name').split(',').map(name => name.trim())) {
    const properties = PAINT_CAPTURE.animations.get(name);
    if (!properties) continue;
    if (properties.has('opacity') || properties.has('transform')) style.context = true;
    if (properties.has('transform')) style.absContainingBlock = style.fixedContainingBlock = true;
    if (['z-index', 'position', 'display', 'isolation', 'contain'].some(p => properties.has(p))) {
      animatedStacking = true;
    }
  }
  const source = {
    id: PAINT_CAPTURE.nodes.length, el, style, parent, geometry: null, parentRect: null,
    clips: inheritedClips.slice(), escapes: false,
  };
  if (style.position === 'absolute' || style.position === 'fixed') {
    const block = style.position === 'fixed' ? fixedBlock : absBlock;
    // Overflow between an out-of-flow box and its containing block does not clip that box.
    source.clips = source.clips.filter(clip => {
      for (let ancestor = block; ancestor; ancestor = ancestor.parent) {
        if (ancestor === clip) return true;
      }
      return false;
    });
  }
  PAINT_CAPTURE.nodes.push(source);
  PAINT_CAPTURE.byElement.set(el, source);
  if (style.preserve3D) paintWarning('preserve-3d', source);
  if (animatedStacking) paintWarning('animated-stacking', source);
  if (el.matches && (el.matches(':modal') || el.matches(':popover-open'))) {
    paintWarning('top-layer', source);
  }
  // SVG has its own paint model and is passed through as a single replaced image.
  if (el.namespaceURI === 'http://www.w3.org/2000/svg' || computed.display === 'none') return source;
  const nextParent = computed.display === 'contents' ? parent : source;
  const nextClips = style.clips ? source.clips.concat(source) : source.clips;
  for (const child of el.children) {
    const childSource = collectPaintSources(child, nextParent,
      style.absContainingBlock ? source : absBlock,
      style.fixedContainingBlock ? source : fixedBlock, nextClips);
    if (childSource.style.context || childSource.style.positioned || childSource.style.floating ||
        childSource.escapes) source.escapes = true;
  }
  return source;
}

function paintWarning(kind, source) {
  const message = `html-snapshot: paint:${kind}: <${source.el.tagName.toLowerCase()}>` +
    (source.el.id ? `#${source.el.id}` : '') +
    ' requires a dedicated painting model; the snapshot retains its supported 2D representation';
  PAINT_CAPTURE.warnings.push(message);
}

function paintSource(el) {
  return PAINT_CAPTURE.active ? PAINT_CAPTURE.byElement.get(el) : null;
}

function paintNeedsSeparateChildren(el) {
  const source = paintSource(el);
  return !!source && source.escapes;
}

function capturePaintGeometry(el, parentRect) {
  const source = paintSource(el);
  if (!source) return;
  const rect = el.getBoundingClientRect();
  source.geometry = { left: rect.left, top: rect.top, width: rect.width, height: rect.height };
  source.parentRect = { left: parentRect.left, top: parentRect.top };
}

function markPaintSource(el, html, opts) {
  const source = paintSource(el);
  if (!source || !html || source.style.display === 'contents' || (opts && opts._strippedTransform)) {
    return html;
  }
  const marker = ` ${PAINT_SOURCE_ATTR}="${source.id}"`;
  // Fragmented inline decorations precede the element's actual positioning wrapper.
  const anchor = ' data-pagx-name-anchor';
  if (html.includes(anchor)) return html.replace(anchor, anchor + marker);
  return html.replace(/^<([\w:-]+)/, `<$1${marker}`);
}

function markPaintDecoration(html) {
  if (!PAINT_CAPTURE.active) return html;
  return html.replace(/^<([\w:-]+)/, `<$1 ${PAINT_DECORATION_ATTR}="true"`);
}

function paintParticipant(box) {
  return !!box.source && (box.source.style.context || box.source.style.positioned);
}

function paintPseudoContainer(box) {
  if (!box.source) return false;
  const style = box.source.style;
  return style.positioned || style.floating || style.flexItem || style.replaced ||
    ['inline-block', 'inline-flex', 'inline-grid', 'inline-table'].includes(style.display);
}

function paintInline(box) {
  if (!box.source) return true;
  return /^(inline|ruby)/.test(box.source.style.display);
}

function paintHasDecoration(el) {
  if (el.nodeType !== 1) return false;
  const style = el.style;
  if (style.backgroundClip === 'text' || style.webkitBackgroundClip === 'text') return false;
  return !!(style.background || style.backgroundColor || style.backgroundImage ||
    style.border || style.borderTop || style.borderRight || style.borderBottom || style.borderLeft ||
    style.boxShadow || style.outline);
}

function buildPaintBoxes(el, parent, sequence) {
  const id = el.nodeType === 1 ? el.getAttribute(PAINT_SOURCE_ATTR) : null;
  const source = id === null ? null : PAINT_CAPTURE.nodes[Number(id)];
  const box = { el, source, parent, order: sequence.value++, children: [], atomic: false };
  if (el.nodeType !== 1) {
    box.atomic = true;
    return box;
  }
  el.removeAttribute(PAINT_SOURCE_ATTR);
  box.decoration = el.hasAttribute(PAINT_DECORATION_ATTR);
  el.removeAttribute(PAINT_DECORATION_ATTR);
  if (source && source.style.paintContainment) el.style.overflow = 'hidden';
  // Renderers own generated image/text/SVG subtrees. Reuse them without interpreting their
  // synthetic absolute positioning as authored CSS positioning.
  if (!source && !el.querySelector(`[${PAINT_SOURCE_ATTR}]`)) {
    box.atomic = true;
    return box;
  }
  for (const child of el.childNodes) {
    if (child.nodeType === 3 && !child.nodeValue.trim()) continue;
    box.children.push(buildPaintBoxes(child, box, sequence));
  }
  if (source && /^(inline-)?(flex|grid)$/.test(source.style.display)) {
    box.children.sort((a, b) =>
      (a.source && a.source.style.flexItem ? a.source.style.order : 0) -
      (b.source && b.source.style.flexItem ? b.source.style.order : 0));
  }
  box.preserve = !!source && /^(inline-)?flex$/.test(el.style.display) && !source.escapes;
  return box;
}

function assignPaintOrder(box, sequence) {
  box.order = sequence.value++;
  for (const child of box.children) assignPaintOrder(child, sequence);
}

// A PaintItem is a primitive or a context (an indivisible recursively planned group).
// Pseudo containers have a local flow traversal, but positioned/context descendants are
// registered on the enclosing *real* context. See CSS 2.2 Appendix E, steps 5, 7 and 8.
function planPaintContext(root) {
  const context = { root, negative: [], zero: [], positive: [] };
  const flow = planPaintFlow(root, context, false);
  const compare = (a, b) => a.z - b.z || a.order - b.order;
  context.negative.sort(compare);
  context.zero.sort(compare);
  context.positive.sort(compare);
  return context.negative.flatMap(entry => entry.items).concat(flow,
    context.zero.flatMap(entry => entry.items), context.positive.flatMap(entry => entry.items));
}

function planPaintFlow(root, context, ownDecoration) {
  const flow = { blocks: [], floats: [], inline: [] };
  const own = [];
  if (ownDecoration && paintHasDecoration(root.el)) own.push({ box: root, kind: 'decoration' });
  for (const child of root.children) collectPaintFlow(child, flow, context);
  return own.concat(flow.blocks, flow.floats, flow.inline);
}

function collectPaintFlow(box, flow, context) {
  if (paintParticipant(box)) {
    const style = box.source.style;
    const items = style.context
      ? [{ box, kind: 'context', items: box.preserve ? null : planPaintContext(box) }]
      : box.preserve ? [{ box, kind: 'atom' }] : planPaintFlow(box, context, true);
    const entry = { z: style.zIndex === null ? 0 : style.zIndex, order: box.order, items };
    if (entry.z < 0) context.negative.push(entry);
    else if (entry.z > 0) context.positive.push(entry);
    else context.zero.push(entry);
    return;
  }
  if (box.decoration) {
    flow.blocks.push({ box, kind: 'atom' });
    return;
  }
  if (box.atomic || box.preserve) {
    if (box.preserve && !paintPseudoContainer(box) && paintHasDecoration(box.el)) {
      flow.blocks.push({ box, kind: 'decoration' });
      flow.inline.push({ box, kind: 'flow-content' });
    } else {
      flow.inline.push({ box, kind: 'atom' });
    }
    return;
  }
  if (paintPseudoContainer(box)) {
    const items = planPaintFlow(box, context, true);
    if (box.source.style.floating) flow.floats.push(...items);
    else flow.inline.push(...items);
    return;
  }
  if (paintHasDecoration(box.el)) {
    (paintInline(box) ? flow.inline : flow.blocks).push({ box, kind: 'decoration' });
  }
  for (const child of box.children) collectPaintFlow(child, flow, context);
}

function neutralPaintStyle(el) {
  const textBackground = el.style.backgroundClip === 'text' || el.style.webkitBackgroundClip === 'text';
  for (const property of Array.from(el.style)) {
    if ((!textBackground && property.startsWith('background')) ||
        property === 'box-shadow' || property.startsWith('outline')) {
      el.style.removeProperty(property);
    }
  }
  // Keep transparent borders: removing their widths changes the padding-box origin used by
  // every absolute child, and changes the inner rounding of an overflow clip.
  if (el.style.border || el.style.borderWidth || el.style.borderStyle) el.style.borderColor = 'transparent';
  for (const side of ['Top', 'Right', 'Bottom', 'Left']) {
    if (el.style[`border${side}`] || el.style[`border${side}Width`]) {
      el.style[`border${side}Color`] = 'transparent';
    }
  }
}

function positionPaintClone(clone, source) {
  if (!source || !source.geometry || clone.nodeType !== 1) return;
  // Source flex items may become paint fragments outside their original flex flow.
  // Their browser-measured box already includes margin, alignment and flex sizing.
  if (clone.style.position === 'relative' || !clone.style.position) {
    const rect = source.geometry;
    const parent = source.parentRect;
    clone.style.position = 'absolute';
    clone.style.left = `${rect.left - parent.left}px`;
    clone.style.top = `${rect.top - parent.top}px`;
    clone.style.width = `${rect.width}px`;
    clone.style.height = `${rect.height}px`;
    clone.style.margin = '0';
    clone.style.removeProperty('flex');
  }
}

function clonePaintBox(box, deep, neutral, identities, ownIdentity = true) {
  const clone = box.el.cloneNode(deep);
  if (clone.nodeType !== 1) return clone;
  positionPaintClone(clone, box.source);
  if (neutral) neutralPaintStyle(clone);
  if (!ownIdentity || identities.has(box.el)) {
    clone.removeAttribute('id');
    clone.removeAttribute('name');
  } else {
    identities.add(box.el);
  }
  clone.setAttribute(PAINT_ORDER_ATTR, 'fixed');
  return clone;
}

function paintItemSource(item) {
  for (let box = item.box; box; box = box.parent) {
    if (box.source) return box.source;
  }
  return null;
}

function paintItemClips(item) {
  if (item.clips) return item.clips;
  const source = paintItemSource(item);
  let clips = source ? source.clips.slice() : [];
  // A renderer-generated img/text/SVG primitive is the source element's own content.
  // Unlike that element's decoration, it is subject to the element's overflow clip too.
  if (!item.box.source && !item.box.decoration && source && source.style.clips) clips.push(source);
  if (item.kind === 'context' && item.items !== null) {
    // A context can contain both clipped in-flow content and unclipped absolute content.
    // Only their common clips may wrap the entire compositing group.
    for (const child of item.items) {
      const childClips = paintItemClips(child);
      clips = clips.filter(clip => childClips.includes(clip));
    }
  }
  item.clips = clips;
  return clips;
}

function paintPath(item, root) {
  const path = [];
  if (item.box === root) return path;
  const clips = paintItemClips(item);
  for (let box = item.box.parent; box && box !== root; box = box.parent) {
    // Overflow is a property of the source containing-block chain. An absolute output
    // wrapper must not introduce clipping that was absent on the source positioned box.
    const clip = !box.source || !box.source.style.clips || clips.includes(box.source);
    path.push({ box, clip });
  }
  return path.reverse();
}

function clearPaintEffects(el) {
  for (const property of Array.from(el.style)) {
    if (['opacity', 'filter', 'backdrop-filter', '-webkit-backdrop-filter', 'mix-blend-mode',
      'isolation', 'transform', 'transform-origin', 'mask', 'mask-image', '-webkit-mask-image',
      'clip-path'].includes(property) || property.startsWith('animation')) {
      el.style.removeProperty(property);
    }
  }
}

function paintBorder(el, side) {
  return parseFloat(el.style[`border${side}Width`]) || 0;
}

function insetPaintClip(el) {
  const top = paintBorder(el, 'Top');
  const right = paintBorder(el, 'Right');
  const bottom = paintBorder(el, 'Bottom');
  const left = paintBorder(el, 'Left');
  if (!(top || right || bottom || left)) return;
  const width = parseFloat(el.style.width);
  const height = parseFloat(el.style.height);
  if (!Number.isFinite(width) || !Number.isFinite(height)) return;
  const corners = ['TopLeft', 'TopRight', 'BottomRight', 'BottomLeft'];
  const radii = corners.map(corner => {
    const values = (el.style[`border${corner}Radius`] || '0').split(/\s+/);
    const x = parseFloat(values[0]) || 0;
    const y = parseFloat(values[1] || values[0]) || 0;
    return [values[0].endsWith('%') ? width * x / 100 : x,
      (values[1] || values[0]).endsWith('%') ? height * y / 100 : y];
  });
  const factor = Math.min(1, width / (radii[0][0] + radii[1][0]),
    width / (radii[3][0] + radii[2][0]), height / (radii[0][1] + radii[3][1]),
    height / (radii[1][1] + radii[2][1]));
  // CSS overflow clips at the padding edge. A transparent border still has width, but
  // PAGX's generic overflow clip uses the layer bounds. Emit the actual padding box.
  el.style.left = `${(parseFloat(el.style.left) || 0) + left}px`;
  el.style.top = `${(parseFloat(el.style.top) || 0) + top}px`;
  el.style.width = `${Math.max(0, width - left - right)}px`;
  el.style.height = `${Math.max(0, height - top - bottom)}px`;
  const insets = [[left, top], [right, top], [right, bottom], [left, bottom]];
  for (let i = 0; i < corners.length; i++) {
    el.style[`border${corners[i]}Radius`] = `${Math.max(0, radii[i][0] * factor - insets[i][0])}px ` +
      `${Math.max(0, radii[i][1] * factor - insets[i][1])}px`;
  }
  el.style.border = '0';
}

function makePaintClip(clipBox, root, identities) {
  const clip = clonePaintBox(clipBox, false, true, identities, false);
  clearPaintEffects(clip);
  const rect = clipBox.source.geometry;
  const origin = root.source.geometry || { left: 0, top: 0 };
  const x = rect.left - origin.left - paintBorder(root.el, 'Left');
  const y = rect.top - origin.top - paintBorder(root.el, 'Top');
  Object.assign(clip.style, {
    position: 'absolute', left: `${x}px`, top: `${y}px`,
    width: `${rect.width}px`, height: `${rect.height}px`, overflow: 'hidden', margin: '0',
  });
  // Restore the context's coordinate origin after entering the clipping box.
  const reset = document.createElement('div');
  Object.assign(reset.style, {
    position: 'absolute',
    left: `${-x - paintBorder(clip, 'Left')}px`,
    top: `${-y - paintBorder(clip, 'Top')}px`,
    width: root.el.style.width || '100%', height: root.el.style.height || '100%',
  });
  reset.setAttribute(PAINT_ORDER_ATTR, 'fixed');
  clip.appendChild(reset);
  insetPaintClip(clip);
  return { outer: clip, inner: reset };
}

function emitPaintItems(root, items, target, identities, pendingClips = []) {
  let previous = [];
  let containers = [target];
  for (const item of items) {
    const clips = paintItemClips(item);
    const applied = pendingClips.filter(box => clips.includes(box.source));
    const path = applied.map(box => ({ box, proxy: true })).concat(paintPath(item, root));
    const deferred = pendingClips.filter(box => !applied.includes(box));
    for (const entry of path) {
      if (entry.clip === false && !deferred.includes(entry.box)) deferred.push(entry.box);
    }
    let common = 0;
    while (common < previous.length && common < path.length &&
           previous[common].box === path[common].box && previous[common].clip === path[common].clip &&
           previous[common].proxy === path[common].proxy) common++;
    containers.length = common + 1;
    for (let i = common; i < path.length; i++) {
      const entry = path[i];
      if (entry.proxy) {
        const proxy = makePaintClip(entry.box, root, identities);
        containers[containers.length - 1].appendChild(proxy.outer);
        containers.push(proxy.inner);
        continue;
      }
      const wrapper = clonePaintBox(entry.box, false, true, identities);
      if (!entry.clip) wrapper.style.overflow = 'visible';
      else if (entry.box.source && entry.box.source.style.clips) insetPaintClip(wrapper);
      containers[containers.length - 1].appendChild(wrapper);
      containers.push(wrapper);
    }
    let node;
    if (item.kind === 'context') {
      node = clonePaintBox(item.box, item.items === null, false, identities);
      node.style.isolation = 'isolate';
      if (item.items !== null) {
        let children = item.items;
        if (deferred.some(box => item.box.source.clips.includes(box.source)) &&
            paintHasDecoration(item.box.el)) {
          neutralPaintStyle(node);
          children = [{ box: item.box, kind: 'context-decoration' }].concat(children);
        }
        if (item.box.source.style.clips) {
          node.style.overflow = 'visible';
          deferred.push(item.box);
        }
        emitPaintItems(item.box, children, node, identities, deferred);
      }
    } else {
      node = clonePaintBox(item.box, ['atom', 'flow-content'].includes(item.kind),
        item.kind === 'flow-content', identities,
        item.kind !== 'context-decoration' && !(item.kind === 'decoration' && item.box.children.length));
      if (item.kind === 'context-decoration') {
        clearPaintEffects(node);
        node.style.left = `${-paintBorder(root.el, 'Left')}px`;
        node.style.top = `${-paintBorder(root.el, 'Top')}px`;
        node.style.overflow = 'visible';
      }
    }
    containers[containers.length - 1].appendChild(node);
    previous = path;
  }
}

function compilePaintMarkup(html) {
  const template = document.createElement('template');
  template.innerHTML = html;
  const host = document.createElement('div');
  host.appendChild(template.content);
  const root = { el: host, source: PAINT_CAPTURE.nodes[0], parent: null, order: -1, children: [] };
  const sequence = { value: 0 };
  for (const child of host.childNodes) {
    if (child.nodeType === 3 && !child.nodeValue.trim()) continue;
    root.children.push(buildPaintBoxes(child, root, sequence));
  }
  assignPaintOrder(root, { value: 0 });
  const output = document.createElement('div');
  emitPaintItems(root, planPaintContext(root), output, new Set());
  if (!output.hasChildNodes()) return '';
  // The native importer consumes XML-compatible HTML. innerHTML would turn <img/> into
  // <img>, and serialize non-breaking spaces as the HTML-only &nbsp; entity.
  const serialized = new XMLSerializer().serializeToString(output);
  return serialized.slice(serialized.indexOf('>') + 1, serialized.lastIndexOf('</div>'));
}

function endPaintCapture() {
  const warnings = PAINT_CAPTURE.warnings.slice();
  PAINT_CAPTURE.active = false;
  PAINT_CAPTURE.nodes = [];
  PAINT_CAPTURE.byElement = new WeakMap();
  PAINT_CAPTURE.warnings = [];
  PAINT_CAPTURE.animations = new Map();
  return warnings;
}

const PAINT_PLAN_HELPERS_SRC = [
  paintProperty, paintStyleRecord, beginPaintCapture, collectPaintSources, paintWarning,
  paintSource, paintNeedsSeparateChildren, capturePaintGeometry, markPaintSource,
  markPaintDecoration, paintParticipant, paintPseudoContainer, paintInline,
  paintHasDecoration, buildPaintBoxes, assignPaintOrder, planPaintContext, planPaintFlow, collectPaintFlow,
  neutralPaintStyle, positionPaintClone, clonePaintBox, paintItemSource, paintItemClips, paintPath,
  clearPaintEffects, paintBorder, insetPaintClip, makePaintClip, emitPaintItems, compilePaintMarkup,
  endPaintCapture,
].map(fn => fn.toString()).join('\n\n');

const PAINT_PLAN_CONSTANTS_SRC = `
const PAINT_SOURCE_ATTR = '${PAINT_SOURCE_ATTR}';
const PAINT_ORDER_ATTR = '${PAINT_ORDER_ATTR}';
const PAINT_DECORATION_ATTR = '${PAINT_DECORATION_ATTR}';
const PAINT_CAPTURE = {
  active: false, nodes: [], byElement: new WeakMap(), warnings: [], animations: new Map(),
};
`;

export { PAINT_PLAN_HELPERS_SRC, PAINT_PLAN_CONSTANTS_SRC, paintStyleRecord, planPaintContext };
