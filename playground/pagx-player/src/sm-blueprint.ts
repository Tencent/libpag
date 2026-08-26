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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { TimelineTreeNode } from './pagx-view-types';
import { iconUrl } from './playback-bar';

/** Player capabilities the blueprint + chip bar need. Injected by PAGXPlayer so the components
 *  stay backend-agnostic. */
export interface SMBlueprintHost {
  getTimelineTree(): TimelineTreeNode[];
  getSMCurrentStates(): Record<string, string>;
  getSelectedTimelineUnit(): { kind: string; id: string } | null;
  selectTimelineUnit(kind: string, id: string): boolean;
  /** Toggles play/pause of the SM itself (no timed-timeline guards - a default SM reports no
   *  duration). Only meaningful when no preview is active. */
  togglePlayback(): void;
  /** Whether the underlying view is currently playing; drives the panel play-button icon. */
  isPlaying(): boolean;
  /** While previewing, this "parks" the preview: the SM resets and starts playing, the preview
   *  animation resets to frame 0, and the playback bar dims + disables all its controls. The
   *  chip stays highlighted (see getParkedPreviewId) so the user can un-park by re-clicking
   *  that chip or double-clicking a state. */
  parkPreview(): void;
  /** The preview id that is "parked" (SM playing while a preview stays marked in the chip
   *  bar as active). Null when no preview is parked. Used by PreviewChipBar to keep the chip
   *  active even though view.getSelectedTimelineUnit() returned to null. */
  getParkedPreviewId(): string | null;
}

const NODE_W = 128;
const NODE_H = 50;
// Horizontal graph layout: columns are transition depth (BFS from initial state), rows are
// the vertical stacking within a column when several states share the same depth. GAP_X is
// deliberately wide (about the size of a full node) so multi-line label backdrops fit
// between adjacent columns without spilling onto the next node.
const GAP_X = 140;
const GAP_Y = 20;
const PADDING_X = 20;
const PADDING_Y = 20;
const POLL_INTERVAL_MS = 250;

interface SMRegionData {
  name: string;
  initial: string;
  states: { name: string; animationId: string; durationUs: number; previewSupported: boolean }[];
  transitions: { from: string; to: string; fromAny: boolean; conditions: string }[];
}

/**
 * State-machine blueprint panel for a default state-machine timeline, docked in the top-left
 * corner of the player. The panel header carries a play button that mirrors the playback bar's
 * primary button: while no solo preview is active it plays/pauses the whole SM; while previewing
 * it shows the play glyph and clicking it exits the preview (the SM resumes from its initial
 * states). The graph area stacks each region's states vertically in declaration order, and
 * multiple regions sit side-by-side. The graph never shows scrollbars; the mouse wheel pans
 * content. Double-clicking a state solo-previews that state's animation. Hovering a state shows
 * a tooltip with its details.
 *
 * Companion: {@link PreviewChipBar}. Chips are NOT part of this panel - they live in their own
 * bar that floats above the playback bar, by design (the chips are an operations row, not a
 * graph feature). The two components share the same preview-selection state via the host.
 */
export class SMBlueprint {
  private host: SMBlueprintHost;
  private root: HTMLElement;
  private playBtn: HTMLButtonElement;
  private playBtnImg: HTMLImageElement;
  private titleEl: HTMLElement;
  private viewport: HTMLElement;
  private content: HTMLElement;
  private tooltip: HTMLElement;
  private pollTimer: number | null = null;
  private regions: SMRegionData[] = [];
  private smTitle = '';
  private nodeEls = new Map<string, HTMLElement>();
  private animationEls = new Map<string, HTMLElement[]>();
  private currentStates: Record<string, string> = {};
  private visible = true;
  private iconBaseUrl: string;
  private lastPlayIcon = 'play.png';
  private lastPreviewId: string | null = null;
  private contentX = 0;
  private contentY = 0;
  private markerCounter = 0;
  // Cached 2D canvas used to measure label widths without depending on SVG layout (getBBox
  // returns zeros when the panel is still display:none during refresh()). Created lazily so
  // SSR / non-DOM contexts don't crash.
  private textMetrics: CanvasRenderingContext2D | null = null;

  constructor(host: SMBlueprintHost, iconBaseUrl: string) {
    this.host = host;
    this.iconBaseUrl = iconBaseUrl;
    this.root = document.createElement('div');
    this.root.className = 'sm-blueprint';
    this.root.style.display = 'none';

    const header = document.createElement('div');
    header.className = 'sm-header';
    this.playBtn = document.createElement('button');
    this.playBtn.className = 'sm-play-btn';
    this.playBtn.type = 'button';
    this.playBtn.title = 'Play / Pause';
    this.playBtnImg = document.createElement('img');
    this.playBtnImg.src = iconUrl(iconBaseUrl, 'play.png');
    this.playBtnImg.alt = '';
    this.playBtn.appendChild(this.playBtnImg);
    this.playBtn.addEventListener('click', () => {
      this.playBtn.blur();
      const selection = this.host.getSelectedTimelineUnit();
      const activePreview = selection != null && selection.kind === 'animation';
      if (activePreview) {
        // Preview is on stage: pressing play here parks it (SM resets and runs, preview
        // rewinds and its progress bar dims). Chip stays highlighted via getParkedPreviewId.
        this.host.parkPreview();
      } else {
        // Either no preview ever, or already parked. Both mean the SM is (or should be)
        // driving the render loop, so just toggle its play state.
        this.host.togglePlayback();
      }
      this.updatePlayIcon();
    });
    this.titleEl = document.createElement('span');
    this.titleEl.className = 'sm-title';
    header.appendChild(this.playBtn);
    header.appendChild(this.titleEl);

    this.viewport = document.createElement('div');
    this.viewport.className = 'sm-viewport';
    this.content = document.createElement('div');
    this.content.className = 'sm-content';
    this.viewport.appendChild(this.content);
    this.viewport.addEventListener('wheel', (event) => {
      event.preventDefault();
      this.contentX -= event.deltaX;
      this.contentY -= event.deltaY;
      this.applyContentOffset();
    }, { passive: false });

    this.tooltip = document.createElement('div');
    this.tooltip.className = 'sm-tooltip';
    this.tooltip.style.display = 'none';

    this.root.appendChild(header);
    this.root.appendChild(this.viewport);
    this.root.appendChild(this.tooltip);
  }

  attach(parent: HTMLElement): void {
    parent.appendChild(this.root);
  }

  setVisible(visible: boolean): void {
    this.visible = visible;
    this.updateVisibility();
  }

  private updateVisibility(): void {
    this.root.style.display = this.visible && this.regions.length > 0 ? '' : 'none';
  }

  /** Rebuilds the blueprint from the timeline tree. Returns whether a default state machine was
   *  found (the caller can use this to switch the surrounding player into SM mode). */
  refresh(): boolean {
    const tree = this.host.getTimelineTree();
    const smNode = tree.find((node) => node.kind === 'stateMachine' && node.isDefault) ?? null;
    this.regions = [];
    this.smTitle = '';
    this.nodeEls.clear();
    this.animationEls.clear();
    this.currentStates = {};
    this.contentX = 0;
    this.contentY = 0;
    this.applyContentOffset();
    this.content.innerHTML = '';
    if (smNode == null || smNode.regions == null || smNode.regions.length === 0) {
      this.updateVisibility();
      return false;
    }
    this.smTitle = smNode.name || smNode.id;
    this.titleEl.textContent = this.smTitle;
    for (const region of smNode.regions) {
      this.regions.push({
        name: region.name,
        initial: region.initial,
        states: region.states.map((state) => ({
          name: state.name,
          animationId: state.animationId,
          durationUs: state.durationUs,
          previewSupported: state.previewSupported === true,
        })),
        transitions: (region.transitions ?? []).map((transition) => ({
          from: transition.from,
          to: transition.to,
          fromAny: transition.fromAny === true,
          conditions: transition.conditions,
        })),
      });
    }
    // Regions stack vertically; each region is its own left-to-right flowchart. Panel width
    // is dominated by the widest region, so multiple regions read as separate subgraphs
    // stacked one on top of the other (Rive / Unity Mecanim / Stately convention).
    this.content.style.flexDirection = 'column';
    this.regions.forEach((region, index) => {
      const regionEl = this.buildRegionElement(region);
      if (index > 0) {
        regionEl.style.marginTop = `${GAP_Y}px`;
      }
      this.content.appendChild(regionEl);
    });
    this.updateVisibility();
    this.lastPreviewId = this.refreshSelectionState();
    this.refreshHighlight(true);
    this.updatePlayIcon();
    return true;
  }

  startPolling(): void {
    this.stopPolling();
    this.pollTimer = window.setInterval(this.onPollTick, POLL_INTERVAL_MS);
  }

  stopPolling(): void {
    if (this.pollTimer != null) {
      window.clearInterval(this.pollTimer);
      this.pollTimer = null;
    }
  }

  destroy(): void {
    this.stopPolling();
    if (this.root.parentElement != null) {
      this.root.parentElement.removeChild(this.root);
    }
  }

  private isPreviewing(): boolean {
    const selection = this.host.getSelectedTimelineUnit();
    if (selection != null && selection.kind === 'animation') {
      return true;
    }
    // A "parked" preview counts as previewing for the panel play button: pressing it while
    // parked would toggle the SM instead, which is exactly the same visual state so the icon
    // stays a play glyph either way.
    return this.host.getParkedPreviewId() != null;
  }

  private applyContentOffset(): void {
    if (this.contentX > 0) {
      this.contentX = 0;
    }
    if (this.contentY > 0) {
      this.contentY = 0;
    }
    const maxX = this.viewport.clientWidth - this.content.offsetWidth;
    const maxY = this.viewport.clientHeight - this.content.offsetHeight;
    if (this.contentX < maxX) {
      this.contentX = Math.min(0, maxX);
    }
    if (this.contentY < maxY) {
      this.contentY = Math.min(0, maxY);
    }
    this.content.style.transform = `translate(${this.contentX}px, ${this.contentY}px)`;
  }

  private onPollTick = (): void => {
    const previewId = this.refreshSelectionState();
    if (previewId !== this.lastPreviewId) {
      this.lastPreviewId = previewId;
      this.refreshHighlight(true);
    } else {
      this.refreshHighlight(false);
    }
    this.updatePlayIcon();
  };

  private buildRegionElement(region: SMRegionData): HTMLElement {
    const wrapper = document.createElement('div');
    wrapper.className = 'sm-region';
    const label = document.createElement('div');
    label.className = 'sm-region-label';
    label.textContent = region.name;
    wrapper.appendChild(label);

    const canvas = document.createElement('div');
    canvas.className = 'sm-region-canvas';

    // BFS layout: columns are transition depth from the initial state, rows are stacked within
    // each column. Standard "left-to-right flowchart" convention (Rive, Unity, XState, Stately).
    const layout = this.computeLayout(region);
    // Detect back edges and self-loops so we can reserve room below/above the node grid for
    // their arcs; without this padding they would render outside the SVG viewBox and clip.
    let hasBackEdge = false;
    let hasSelfLoop = false;
    for (const transition of region.transitions) {
      const fromCell = transition.fromAny ? null : layout.cells.get(transition.from);
      const toCell = layout.cells.get(transition.to);
      if (fromCell != null && toCell != null) {
        if (fromCell.col === toCell.col && fromCell.row === toCell.row) {
          hasSelfLoop = true;
        } else if (toCell.col < fromCell.col) {
          hasBackEdge = true;
        }
      }
    }
    const bottomPad = hasBackEdge ? 64 : 0;
    const topPad = hasSelfLoop ? 48 : 0;
    const canvasWidth = PADDING_X * 2 + layout.cols * NODE_W +
      Math.max(0, layout.cols - 1) * GAP_X;
    const canvasHeight = PADDING_Y * 2 + layout.rows * NODE_H +
      Math.max(0, layout.rows - 1) * GAP_Y + topPad + bottomPad;
    canvas.style.width = `${canvasWidth}px`;
    canvas.style.height = `${canvasHeight}px`;

    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('class', 'sm-edges');
    svg.setAttribute('width', String(canvasWidth));
    svg.setAttribute('height', String(canvasHeight));
    const markerId = `sm-arrow-${this.markerCounter++}`;
    svg.appendChild(this.createArrowMarker(markerId));
    canvas.appendChild(svg);

    const positions = new Map<string, { x: number; y: number }>();
    for (const [name, cell] of layout.cells) {
      const x = PADDING_X + cell.col * (NODE_W + GAP_X);
      const y = PADDING_Y + topPad + cell.row * (NODE_H + GAP_Y);
      positions.set(name, { x, y });
      const state = region.states.find((s) => s.name === name);
      if (state == null) {
        continue;
      }
      const stateEl = this.buildStateElement(region, state);
      stateEl.style.left = `${x}px`;
      stateEl.style.top = `${y}px`;
      canvas.appendChild(stateEl);
      this.nodeEls.set(`${region.name}/${state.name}`, stateEl);
      if (state.animationId) {
        const list = this.animationEls.get(state.animationId) ?? [];
        list.push(stateEl);
        this.animationEls.set(state.animationId, list);
      }
      if (state.name === region.initial) {
        const dot = document.createElement('div');
        dot.className = 'sm-entry-dot';
        dot.style.left = `${x - 10}px`;
        dot.style.top = `${y + NODE_H / 2}px`;
        canvas.appendChild(dot);
      }
    }
    // Multi-edges between the same (from, to) pair get vertically fanned so their paths and
    // labels never overlap. parallelIndex tells drawEdge which slot to occupy inside the fan;
    // parallelCount says how many total edges share this pair. Grouping key includes fromAny
    // so any-state edges fan separately from their concrete counterparts.
    const parallelGroups = new Map<string, number>();
    const parallelKeys: string[] = [];
    for (const transition of region.transitions) {
      const key = `${transition.fromAny ? '__any__' : transition.from}|${transition.to}`;
      parallelKeys.push(key);
      parallelGroups.set(key, (parallelGroups.get(key) ?? 0) + 1);
    }
    const parallelSeen = new Map<string, number>();
    region.transitions.forEach((transition, index) => {
      const key = parallelKeys[index];
      const parallelIndex = parallelSeen.get(key) ?? 0;
      parallelSeen.set(key, parallelIndex + 1);
      this.drawEdge(svg, positions, transition, markerId,
        parallelIndex, parallelGroups.get(key) ?? 1);
    });
    wrapper.appendChild(canvas);
    return wrapper;
  }

  /** Computes a BFS-based left-to-right layout for a region's state graph. Column = transition
   *  depth from the initial state; states sharing the same depth stack vertically. Detached
   *  states (unreachable from initial) get appended to the last column. */
  private computeLayout(region: SMRegionData):
    { cells: Map<string, { col: number; row: number }>; cols: number; rows: number } {
    const cells = new Map<string, { col: number; row: number }>();
    if (region.states.length === 0) {
      return { cells, cols: 0, rows: 0 };
    }
    // Adjacency list: from -> [to...]. Any-state transitions don't participate in layout depth
    // (they short-circuit; we render them as a common source node when present).
    const outgoing = new Map<string, string[]>();
    for (const state of region.states) {
      outgoing.set(state.name, []);
    }
    for (const transition of region.transitions) {
      if (transition.fromAny || !outgoing.has(transition.from)) {
        continue;
      }
      const list = outgoing.get(transition.from);
      if (list != null && outgoing.has(transition.to)) {
        list.push(transition.to);
      }
    }
    // BFS from the initial state to assign column indices.
    const depth = new Map<string, number>();
    const seed = region.initial && outgoing.has(region.initial)
      ? region.initial : region.states[0].name;
    depth.set(seed, 0);
    const queue = [seed];
    while (queue.length > 0) {
      const cursor = queue.shift() as string;
      const cursorDepth = depth.get(cursor) ?? 0;
      for (const next of outgoing.get(cursor) ?? []) {
        if (depth.has(next)) {
          continue;
        }
        depth.set(next, cursorDepth + 1);
        queue.push(next);
      }
    }
    // Detached states (not reached by BFS) go to depth = maxDepth + 1 so they don't overlap
    // the main flow but stay visible.
    let maxDepth = 0;
    for (const d of depth.values()) {
      if (d > maxDepth) maxDepth = d;
    }
    for (const state of region.states) {
      if (!depth.has(state.name)) {
        depth.set(state.name, maxDepth + 1);
      }
    }
    // Group by column; row order preserves declaration order for stable output.
    const columns = new Map<number, string[]>();
    for (const state of region.states) {
      const col = depth.get(state.name) as number;
      const bucket = columns.get(col) ?? [];
      bucket.push(state.name);
      columns.set(col, bucket);
    }
    let cols = 0;
    let rows = 0;
    for (const [col, names] of columns) {
      cols = Math.max(cols, col + 1);
      rows = Math.max(rows, names.length);
      names.forEach((name, row) => {
        cells.set(name, { col, row });
      });
    }
    return { cells, cols, rows };
  }

  private buildStateElement(region: SMRegionData,
                            state: { name: string; animationId: string; durationUs: number;
                                     previewSupported: boolean }): HTMLElement {
    const el = document.createElement('div');
    el.className = 'sm-state';
    if (!state.previewSupported) {
      el.classList.add('sm-state-disabled');
    }
    const nameEl = document.createElement('span');
    nameEl.className = 'sm-state-name';
    nameEl.textContent = state.name;
    el.appendChild(nameEl);
    const subEl = document.createElement('span');
    subEl.className = 'sm-state-sub';
    subEl.textContent = state.animationId || '(no animation)';
    el.appendChild(subEl);
    el.addEventListener('mouseenter', () => {
      this.showTooltip(el, state, region.name);
    });
    el.addEventListener('mouseleave', () => {
      this.tooltip.style.display = 'none';
    });
    el.addEventListener('dblclick', (event) => {
      event.stopPropagation();
      this.tooltip.style.display = 'none';
      if (!state.animationId || !state.previewSupported) {
        return;
      }
      const selection = this.host.getSelectedTimelineUnit();
      if (selection != null && selection.kind === 'animation' &&
          selection.id === state.animationId) {
        this.host.selectTimelineUnit('', '');
        return;
      }
      this.host.selectTimelineUnit('animation', state.animationId);
    });
    return el;
  }

  private showTooltip(stateEl: HTMLElement,
                      state: { name: string; animationId: string; durationUs: number },
                      regionName: string): void {
    const durationText = state.durationUs > 0 ? `${(state.durationUs / 1000000).toFixed(2)}s`
      : state.durationUs === 0 ? '0s' : 'unknown';
    const lines = [
      `Region: ${regionName}`,
      `State: ${state.name}`,
      state.animationId ? `Animation: ${state.animationId}` : 'Animation: (none)',
      `Duration: ${durationText}`,
    ];
    this.tooltip.textContent = lines.join('\n');
    this.tooltip.style.display = 'block';
    const rootRect = this.root.getBoundingClientRect();
    const stateRect = stateEl.getBoundingClientRect();
    const tooltipLeft = Math.max(0, stateRect.right - rootRect.left + 6);
    const tooltipTop = Math.max(0, stateRect.top - rootRect.top - 6);
    this.tooltip.style.left = `${tooltipLeft}px`;
    this.tooltip.style.top = `${tooltipTop}px`;
  }

  private drawEdge(svg: SVGElement,
                   positions: Map<string, { x: number; y: number }>,
                   transition: { from: string; fromAny: boolean; to: string; conditions: string },
                   markerId: string,
                   parallelIndex: number,
                   parallelCount: number): void {
    const fromPos = transition.fromAny ? positions.get('__any__') : positions.get(transition.from);
    const toPos = positions.get(transition.to);
    if (fromPos == null || toPos == null) {
      return;
    }
    // Vertical (or horizontal for back edges) offset applied to fan out multi-edges between
    // the same pair of nodes. The centered slot is 0; siblings step out symmetrically. Step
    // size is wider than the label line-height so parallel labels get their own row instead
    // of visually merging with the neighbor.
    const slot = parallelIndex - (parallelCount - 1) / 2;
    const step = 24;
    const offset = slot * step;
    const group = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'sm-edge');
    path.setAttribute('marker-end', `url(#${markerId})`);
    let midX = 0;
    let midY = 0;
    const sameSpot = fromPos.x === toPos.x && fromPos.y === toPos.y;
    const sameColumn = fromPos.x === toPos.x;
    const forward = toPos.x > fromPos.x;
    if (sameSpot) {
      // Self-loop: an arc sitting above the node, radius grows with parallelIndex so multiple
      // self-loops stack outward without overlapping.
      const bump = 32 + parallelIndex * 20;
      const x1 = fromPos.x + NODE_W * 0.3;
      const x2 = fromPos.x + NODE_W * 0.7;
      const y = fromPos.y;
      path.setAttribute('d', `M ${x1} ${y} C ${x1} ${y - bump} ${x2} ${y - bump} ${x2} ${y}`);
      midX = (x1 + x2) / 2;
      // Label sits inside the arc, well clear of the curve itself.
      midY = y - bump + 10;
    } else if (forward) {
      // Standard left-to-right forward edge: exit source's right edge, enter target's left
      // edge. Multi-edges fan vertically so parallel siblings occupy distinct y offsets both
      // at the ports and along the curve; the label sits ABOVE its own curve (offset - 6px)
      // so it never overlaps the path or the sibling below.
      const x1 = fromPos.x + NODE_W;
      const x2 = toPos.x;
      const y1 = fromPos.y + NODE_H / 2 + offset;
      const y2 = toPos.y + NODE_H / 2 + offset;
      const cx1 = x1 + GAP_X * 0.5;
      const cx2 = x2 - GAP_X * 0.5;
      path.setAttribute('d', `M ${x1} ${y1} C ${cx1} ${y1} ${cx2} ${y2} ${x2} ${y2}`);
      midX = (x1 + x2) / 2;
      midY = (y1 + y2) / 2 - 6;
    } else if (sameColumn) {
      // Same-column transition: a straight vertical line between the closer edges. The
      // parallel offset shifts it horizontally so siblings don't overlap.
      const x = fromPos.x + NODE_W / 2 + offset;
      const goingDown = toPos.y > fromPos.y;
      const y1 = goingDown ? fromPos.y + NODE_H : fromPos.y;
      const y2 = goingDown ? toPos.y : toPos.y + NODE_H;
      path.setAttribute('d', `M ${x} ${y1} L ${x} ${y2}`);
      midX = x + 8;
      midY = (y1 + y2) / 2;
    } else {
      // Back edge (target left of source): loop UNDERNEATH both nodes. Enter the target from
      // straight below (a short vertical segment at the tip) so the arrow points UP into the
      // node's bottom edge — this reads as an entry arrow instead of a diagonal slash across
      // the corner. The loop is deep enough for the label to sit clear of the curve.
      const x1 = fromPos.x + NODE_W * 0.35;
      const x2 = toPos.x + NODE_W * 0.65;
      const y1 = fromPos.y + NODE_H;
      const y2 = toPos.y + NODE_H;
      const drop = 44 + Math.abs(offset);
      const cy = Math.max(y1, y2) + drop;
      // Use a two-segment path (curve + short vertical straight) so the arrow head at the tip
      // is aligned with the vertical direction of entry.
      path.setAttribute('d',
        `M ${x1} ${y1} C ${x1} ${cy} ${x2} ${cy} ${x2} ${y2 + 10} L ${x2} ${y2}`);
      midX = (x1 + x2) / 2;
      // Place the label above the curve (near the lowest point but offset up), never on it.
      midY = cy - 10;
    }
    group.appendChild(path);
    if (transition.conditions && transition.conditions !== 'always') {
      // Label sits on top of the edge path; without a solid backdrop the line would visually
      // slice through the glyphs. Measure the label with a canvas 2D context (not getBBox,
      // which returns zeros when the panel is still display:none during refresh()) so the
      // backdrop is exactly the width the rendered text will occupy.
      let label = transition.conditions;
      if (label.length > 18) {
        label = `${label.slice(0, 17)}\u2026`;
      }
      const textWidth = this.measureLabelWidth(label);
      const textHeight = 12;
      const paddingX = 6;
      const paddingY = 3;
      const backdrop = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      backdrop.setAttribute('class', 'sm-edge-label-bg');
      backdrop.setAttribute('x', String(midX - textWidth / 2 - paddingX));
      backdrop.setAttribute('y', String(midY - textHeight / 2 - paddingY));
      backdrop.setAttribute('width', String(textWidth + paddingX * 2));
      backdrop.setAttribute('height', String(textHeight + paddingY * 2));
      backdrop.setAttribute('rx', '3');
      group.appendChild(backdrop);
      const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      text.setAttribute('class', 'sm-edge-label');
      text.setAttribute('x', String(midX));
      text.setAttribute('y', String(midY));
      text.textContent = label;
      group.appendChild(text);
    }
    svg.appendChild(group);
  }

  private measureLabelWidth(label: string): number {
    if (this.textMetrics == null) {
      const canvas = document.createElement('canvas');
      const ctx = canvas.getContext('2d');
      if (ctx != null) {
        // Match .sm-edge-label CSS. Font family follows the parent SVG (no explicit font
        // stack), so use the same default UI system font that renders the graph.
        ctx.font = '10px -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif';
        this.textMetrics = ctx;
      }
    }
    if (this.textMetrics == null) {
      // Fallback: assume 6px per glyph. Only reached when canvas 2D is unavailable.
      return label.length * 6;
    }
    return this.textMetrics.measureText(label).width;
  }

  private createArrowMarker(markerId: string): SVGDefsElement {
    const defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
    const marker = document.createElementNS('http://www.w3.org/2000/svg', 'marker');
    marker.setAttribute('id', markerId);
    marker.setAttribute('viewBox', '0 0 10 10');
    marker.setAttribute('refX', '9');
    marker.setAttribute('refY', '5');
    marker.setAttribute('markerWidth', '7');
    marker.setAttribute('markerHeight', '7');
    marker.setAttribute('orient', 'auto-start-reverse');
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('d', 'M 0 1 L 9 5 L 0 9 z');
    path.setAttribute('class', 'sm-edge-arrow');
    marker.appendChild(path);
    defs.appendChild(marker);
    return defs;
  }

  /** Syncs the chip state from the live selection and returns the animation id being previewed
   *  (null when the SM itself is running). The actual chip DOM is owned by PreviewChipBar; the
   *  blueprint just mirrors the state for its highlight logic. */
  private refreshSelectionState(): string | null {
    const selection = this.host.getSelectedTimelineUnit();
    if (selection != null && selection.kind === 'animation') {
      return selection.id;
    }
    return null;
  }

  private updatePlayIcon(): void {
    const selection = this.host.getSelectedTimelineUnit();
    const activePreview = selection != null && selection.kind === 'animation';
    if (activePreview) {
      // Preview on stage: the panel play button parks it, so its glyph is always ▶.
      this.setPlayIcon('play.png');
      this.playBtn.title = 'Play the state machine (park the preview)';
      return;
    }
    // No active preview (fresh SM or parked preview): the button toggles SM play/pause, so
    // the glyph tracks the view's live playing state.
    this.playBtn.title = 'Play / Pause';
    this.setPlayIcon(this.host.isPlaying() ? 'pause.png' : 'play.png');
  }

  private setPlayIcon(icon: string): void {
    if (this.lastPlayIcon === icon) {
      return;
    }
    this.lastPlayIcon = icon;
    this.playBtnImg.src = iconUrl(this.iconBaseUrl, icon);
  }

  private refreshHighlight(force: boolean): void {
    const next = this.host.getSMCurrentStates();
    let changed = force;
    for (const region of this.regions) {
      const nextName = next[region.name] ?? '';
      if ((this.currentStates[region.name] ?? '') !== nextName) {
        changed = true;
      }
      this.currentStates[region.name] = nextName;
    }
    if (!changed) {
      return;
    }
    this.nodeEls.forEach((el) => el.classList.remove('sm-state-current'));
    // Active preview: highlight the previewed state; SM (fresh or parked) highlights each
    // region's live current state. A parked preview does NOT highlight the parked animation:
    // the SM is running the show, so the graph tracks its live regions instead.
    const selection = this.host.getSelectedTimelineUnit();
    const previewId = selection != null && selection.kind === 'animation' ? selection.id : null;
    if (previewId != null) {
      const els = this.animationEls.get(previewId);
      if (els != null) {
        for (const el of els) {
          el.classList.add('sm-state-current');
        }
      }
      return;
    }
    for (const region of this.regions) {
      const currentName = this.currentStates[region.name] ?? '';
      if (!currentName) {
        continue;
      }
      const el = this.nodeEls.get(`${region.name}/${currentName}`);
      if (el != null) {
        el.classList.add('sm-state-current');
      }
    }
  }
}

/**
 * Floating operations row that sits just above the playback bar, holding one chip per animation
 * that has been previewed. Clicking a chip switches the preview to its animation; the chip's
 * close button removes it (and exits the preview when removing the active one). The bar is NOT
 * part of the blueprint panel - it is a sibling element that lives inside the player root,
 * positioned independently. It shows only when at least one chip exists, otherwise it is hidden.
 */
export class PreviewChipBar {
  private host: SMBlueprintHost;
  private root: HTMLElement;
  private pollTimer: number | null = null;
  private chips: string[] = [];
  private activeChip: string | null = null;
  private visible = false;
  // Reference to the playback bar DOM so we can mirror its width; the strip is meant to look
  // like it's glued on top of the bar, so any width change (window resize, mode switch) must
  // propagate here on the same frame.
  private barAnchor: HTMLElement | null = null;
  private resizeObserver: ResizeObserver | null = null;

  constructor(host: SMBlueprintHost) {
    this.host = host;
    this.root = document.createElement('div');
    this.root.className = 'sm-chip-bar';
    this.root.style.display = 'none';
  }

  attach(parent: HTMLElement): void {
    parent.appendChild(this.root);
  }

  /** Wires the chip strip's width to the playback bar's live width so the two rectangles
   *  always line up. Called by PAGXPlayer once its playback bar is built. */
  setBarAnchor(bar: HTMLElement): void {
    this.barAnchor = bar;
    if (typeof ResizeObserver !== 'undefined') {
      this.resizeObserver?.disconnect();
      this.resizeObserver = new ResizeObserver(() => this.syncWidth());
      this.resizeObserver.observe(bar);
    }
    this.syncWidth();
  }

  private syncWidth(): void {
    if (this.barAnchor == null) {
      return;
    }
    const width = this.barAnchor.offsetWidth;
    if (width > 0) {
      this.root.style.width = `${width}px`;
    }
    // Deliberately sink the strip's bottom edge INSIDE the playback bar so the two share a
    // seamless top border (no sub-pixel gap between them). The bar has a higher z-index than
    // the strip (see .sm-chip-bar z-index in styles), so the overlap is hidden behind the
    // bar's own background instead of double-drawing.
    const barHeight = this.barAnchor.offsetHeight;
    if (barHeight > 0) {
      const barBottomPx = parseFloat(getComputedStyle(this.barAnchor).bottom) || 16;
      const overlap = 12;
      this.root.style.bottom = `${barBottomPx + barHeight - overlap}px`;
    }
  }

  setVisible(visible: boolean): void {
    this.visible = visible;
    this.refreshVisibility();
  }

  startPolling(): void {
    this.stopPolling();
    this.pollTimer = window.setInterval(this.onPollTick, POLL_INTERVAL_MS);
  }

  stopPolling(): void {
    if (this.pollTimer != null) {
      window.clearInterval(this.pollTimer);
      this.pollTimer = null;
    }
  }

  destroy(): void {
    this.stopPolling();
    this.resizeObserver?.disconnect();
    this.resizeObserver = null;
    if (this.root.parentElement != null) {
      this.root.parentElement.removeChild(this.root);
    }
  }

  /** Wipes chip history. Called when the player loads a new document so chips from the
   *  previous file don't linger. */
  clear(): void {
    this.chips = [];
    this.activeChip = null;
    this.render();
  }

  private refreshVisibility(): void {
    // Only show when there is at least one chip AND a preview is currently in play (either
    // actively rendered or parked). Without a live preview + playback bar to attach to, the
    // strip would float above nothing.
    const selection = this.host.getSelectedTimelineUnit();
    const activePreview = selection != null && selection.kind === 'animation';
    const parked = this.host.getParkedPreviewId() != null;
    const inPreview = activePreview || parked;
    this.root.style.display = this.visible && inPreview && this.chips.length > 0 ? '' : 'none';
  }

  private onPollTick = (): void => {
    this.refresh();
  };

  private refresh(): void {
    const selection = this.host.getSelectedTimelineUnit();
    const parked = this.host.getParkedPreviewId();
    if (selection != null && selection.kind === 'animation') {
      // Fresh preview: chip is the live selection.
      this.activeChip = selection.id;
      if (!this.chips.includes(selection.id)) {
        this.chips.push(selection.id);
      }
    } else if (parked != null) {
      // Preview was parked (SM is running, playback bar is dimmed): keep the chip marked as
      // active so the user still sees which one they can un-park with a single click.
      this.activeChip = parked;
    } else {
      this.activeChip = null;
    }
    this.render();
  }

  private render(): void {
    this.root.innerHTML = '';
    for (const animationId of this.chips) {
      this.root.appendChild(this.buildChip(animationId));
    }
    this.refreshVisibility();
  }

  private buildChip(animationId: string): HTMLElement {
    const chip = document.createElement('div');
    chip.className = 'sm-chip';
    if (animationId === this.activeChip) {
      chip.classList.add('sm-chip-active');
    }
    const label = document.createElement('span');
    label.className = 'sm-chip-label';
    label.textContent = animationId;
    label.title = 'Preview this animation';
    label.addEventListener('click', () => {
      // Two cases share the "select this animation" path:
      //   1. clicking an inactive chip switches the preview to it (engine resets the previous
      //      preview and starts this one from frame 0);
      //   2. clicking the active chip while the preview is *parked* un-parks it (the engine
      //      selectTimelineUnit path resets and plays this animation from frame 0).
      // Clicking an already-active, non-parked chip is a no-op: the animation is already
      // playing.
      const selection = this.host.getSelectedTimelineUnit();
      const activePreview = selection != null && selection.kind === 'animation';
      if (activePreview && selection.id === animationId) {
        return;
      }
      this.host.selectTimelineUnit('animation', animationId);
      // Refresh the strip immediately so the click target flips to "active" without waiting
      // for the next poll tick (which runs at POLL_INTERVAL_MS cadence).
      this.refresh();
    });
    const closeButton = document.createElement('button');
    closeButton.className = 'sm-chip-close';
    closeButton.type = 'button';
    closeButton.textContent = '\u00d7';
    closeButton.title = 'Close';
    closeButton.addEventListener('click', () => {
      // Browser-tab semantics: closing the active chip advances to its right neighbor (or
      // falls back to the left one when it was the rightmost). Closing an inactive chip has
      // no effect on the current preview. Closing the last remaining chip clears the preview
      // entirely - the SM resumes from its initial states, playback bar goes away.
      const index = this.chips.indexOf(animationId);
      const wasActive = this.activeChip === animationId;
      this.chips = this.chips.filter((id) => id !== animationId);
      if (wasActive) {
        if (this.chips.length === 0) {
          this.host.selectTimelineUnit('', '');
        } else {
          const nextIndex = Math.min(index, this.chips.length - 1);
          this.host.selectTimelineUnit('animation', this.chips[nextIndex]);
        }
      }
      // Force an immediate render even if the selection change hasn't propagated through the
      // poll tick yet, so the removed chip disappears on the same event.
      this.render();
    });
    chip.appendChild(label);
    chip.appendChild(closeButton);
    return chip;
  }
}
