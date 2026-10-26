// Shared by the snapshot, baseline screenshot and viewport guard. This function is also shipped
// verbatim into the browser bundle, so keep all dependencies inside its body.
export function measureBodyCanvas(
  maxHeight = Number.POSITIVE_INFINITY,
  visibleExtent?: () => { right: number; bottom: number },
): { width: number; height: number } {
  const body = document.body;
  if (!body) return { width: 0, height: 0 };
  const style = body.style;
  const position = { value: style.getPropertyValue('position'), priority: style.getPropertyPriority('position') };
  // Snapshot measurement temporarily gives body a containing-block role. Its canvas must still be
  // measured with the authored position, exactly as the unmodified baseline is measured.
  const authored = (body as HTMLElement & {
    __pagxAuthoredPosition?: { value: string; priority: string };
  }).__pagxAuthoredPosition;
  try {
    if (authored) style.setProperty('position', authored.value, authored.priority);
    const rect = body.getBoundingClientRect();
    const root = document.scrollingElement || document.documentElement;
    const scrollX = root?.scrollLeft || 0;
    const scrollY = root?.scrollTop || 0;
    const x = rect.left + scrollX;
    const y = rect.top + scrollY;
    // Measure far edges from the document origin, including a leading collapsed margin. A
    // trailing margin outside body remains outside this body-based capture. Never use the root
    // scrollHeight: it inflates a small fixed canvas to the viewport's minimum size.
    let width = Math.max(0, x + rect.width, x + body.scrollWidth);
    let height = Math.max(0, y + rect.height, y + body.scrollHeight);
    if (rect.width <= 0 || rect.height <= 0) {
      let extent = { right: 0, bottom: 0 };
      if (visibleExtent) {
        extent = visibleExtent();
      } else {
        for (const element of body.querySelectorAll('*')) {
          const cs = getComputedStyle(element);
          if (cs.display === 'none' || cs.visibility === 'hidden' || Number(cs.opacity) === 0) continue;
          const box = element.getBoundingClientRect();
          if (box.width <= 0 || box.height <= 0) continue;
          extent.right = Math.max(extent.right, box.right);
          extent.bottom = Math.max(extent.bottom, box.bottom);
        }
      }
      if (rect.width <= 0) width = Math.max(width, extent.right + scrollX);
      if (rect.height <= 0) height = Math.max(height, extent.bottom + scrollY);
    }
    return { width: Math.ceil(width), height: Math.min(maxHeight, Math.ceil(height)) };
  } finally {
    if (authored) style.setProperty('position', position.value, position.priority);
  }
}

export const MEASURE_BODY_CANVAS_SRC = measureBodyCanvas.toString();
