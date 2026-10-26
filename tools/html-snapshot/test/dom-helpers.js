'use strict';

// jsdom supplies DOM mutation/serialization; layout and computed CSS values
// are explicit fixtures because it does not run a browser layout engine.
function computedStyle(element, overrides = {}) {
  const defaults = {
    display: /^(SPAN|A|B|I|EM|IMG)$/.test(element.tagName) ? 'inline' : 'block',
    position: 'static', zIndex: 'auto', opacity: '1',
    visibility: 'visible', appearance: 'auto', transform: 'none',
    rotate: 'none', scale: 'none', translate: 'none', outlineStyle: 'none',
  };
  const camel = name => name.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
  const read = name => overrides[name] ?? overrides[camel(name)] ??
    (element.style?.getPropertyValue(name) || element.style?.[camel(name)] ||
    defaults[camel(name)] || '');
  return new Proxy({}, {
    get: (_target, name) => name === 'getPropertyValue' ? read :
      overrides[name] ?? (element.style?.[name] || defaults[name] || ''),
  });
}

function setRect(element, { left = 0, top = 0, width = 100, height = 20 } = {}) {
  element.getBoundingClientRect = () => ({
    left, top, width, height, right: left + width, bottom: top + height,
  });
  return element;
}

function nodePath(node) {
  const path = [];
  while (node.parentNode) {
    path.unshift(Array.from(node.parentNode.childNodes).indexOf(node));
    node = node.parentNode;
  }
  return path;
}

module.exports = { computedStyle, setRect, nodePath };
