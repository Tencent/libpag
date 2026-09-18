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

// Module-level marker id sequence: SVG ids resolve document-wide, so two PAGXPlayer instances on
// the same page must never mint the same "sm-arrow-N" (url(#...) would resolve to whichever
// panel comes first in the document, and its markers vanish when that panel is display:none).
let nextMarkerSeq = 0;

// Layout key for any-source transitions (from="any"): these edges leave no concrete state, so
// the blueprint renders a dedicated "any" pseudo-node as their shared source column.
const ANY_NODE_KEY = '__any__';

// Vertical geometry of the lanes that carry any-source edges over the grid: the first lane sits
// ANY_LANE_GAP above the grid's top edge and each additional edge stacks ANY_LANE_STEP higher.
// The step must exceed a label's box height (text 12 + 2x3 padding = 18) so stacked lanes keep
// their labels legible.
const ANY_LANE_GAP = 14;
const ANY_LANE_STEP = 24;

// Same idea for back edges (target left of source): they travel in horizontal lanes BELOW the
// grid, one lane per edge, so the run back to the left never crosses the node rows.
const BACK_LANE_GAP = 18;
const BACK_LANE_STEP = 26;

// A lane-routed edge leaves its source's RIGHT edge and enters its target's LEFT edge, so one
// starting in the last column has no gutter to its right to descend in, and one ending in the
// first column none to its left to climb in. Bands of this width plus one track per such edge are
// reserved past the respective side of the grid.
const SIDE_BAND_GAP = 26;

// Self-loop arcs bump this far above their node, each additional loop on the same node stacking
// one step higher. The top padding is derived from the same numbers so the outermost arc always
// stays inside the viewBox.
const SELF_LOOP_BUMP = 32;
const SELF_LOOP_STEP = 20;

// Gutter = the node-free corridor of width GAP_X separating two columns. Every edge that has to
// change rows turns inside a gutter, on a vertical "track". Tracks are handed out per gutter so
// no two edges share one, and TRACK_MARGIN keeps the outermost tracks clear of the nodes bordering
// the corridor. With GAP_X = 140 and TRACK_STEP = 22 a gutter fits ~5 tracks before clamping.
const TRACK_MARGIN = 16;
const TRACK_STEP = 22;

// Corner radius for the orthogonal segments of a routed edge.
const EDGE_CORNER = 8;

/** Whether an edge has to be routed through a horizontal lane below the grid, i.e. leave its
 *  source's right edge, travel back leftwards under every node and climb into its target's left
 *  edge. True for back edges (target in an earlier column) and for same-column edges spanning
 *  more than one row — the latter would otherwise be drawn as a straight vertical inside the
 *  column, cutting through every node stacked between the two endpoints. Same-column neighbours
 *  keep the short vertical: it crosses only the row gap, which holds no nodes.
 *
 *  Single source of truth for the predicate: the padding reservation, the track allocator, the
 *  lane assignment and drawEdge all have to agree on which edges take this shape, and an earlier
 *  revision duplicated the test in each of them. */
function NeedsLaneRouting(fromCell: { col: number; row: number },
                          toCell: { col: number; row: number }): boolean {
  if (toCell.col < fromCell.col) {
    return true;
  }
  return toCell.col === fromCell.col && Math.abs(toCell.row - fromCell.row) > 1;
}

/** The vertical tracks one transition uses. Forward/any edges turn once (`turnX`); a lane-routed
 *  edge descends in the corridor to the RIGHT of its source column and climbs in the one LEFT of
 *  its target column (`backDownX` / `backUpX`). Null means "not applicable to this edge's
 *  shape". */
interface EdgeTracks {
  turnX: number | null;
  backDownX: number | null;
  backUpX: number | null;
}

/** Geometry of the node grid an edge is routed around. */
interface GridMetrics {
  /** x of the first column, i.e. the canvas padding plus the reserved left band. */
  left: number;
  /** y of the first row. */
  top: number;
  /** y of the bottom of the last row; back-edge lanes stack below it. */
  bottom: number;
  cols: number;
  /** Widths of the corridors reserved outside the first / last column for lane-routed edges. */
  leftBand: number;
  rightBand: number;
}

/** Allocates every vertical track the region's edges need, grouped by the corridor it lives in,
 *  so that no two verticals share an x. Doing this region-wide up front — rather than letting
 *  each edge pick its own position — is what guarantees an any-source descent, a forward edge's
 *  turn and a lane-routed edge's climb crossing the same corridor can never end up on top of each
 *  other. Corridors hold no nodes by construction, so a track never crosses a node either. A
 *  corridor is identified by the column bordering it on the right: `0` is the reserved band left
 *  of the first column and `cols` the one right of the last. */
function AssignVerticalTracks(
    transitions: { from: string; fromAny: boolean; to: string }[],
    cells: Map<string, { col: number; row: number }>,
    grid: GridMetrics): EdgeTracks[] {
  const result: EdgeTracks[] = transitions.map(() => ({
    turnX: null,
    backDownX: null,
    backUpX: null,
  }));
  const requests = new Map<number, { index: number; kind: 'turn' | 'down' | 'up' }[]>();
  const request = (col: number, index: number, kind: 'turn' | 'down' | 'up'): void => {
    const bucket = requests.get(col) ?? [];
    bucket.push({ index, kind });
    requests.set(col, bucket);
  };
  transitions.forEach((transition, index) => {
    const fromCell = cells.get(transition.fromAny ? ANY_NODE_KEY : transition.from);
    const toCell = cells.get(transition.to);
    if (fromCell == null || toCell == null) {
      return;
    }
    if (NeedsLaneRouting(fromCell, toCell)) {
      // Leaves the source's right edge, so it descends in the corridor to the RIGHT of the source
      // column and climbs in the one left of the target column — keeping exit ports on the right
      // of every node and entry ports on its left.
      request(fromCell.col + 1, index, 'down');
      request(toCell.col, index, 'up');
      return;
    }
    // Forward edge: one turn, in the gutter left of the target column. Same-row forward edges and
    // same-column neighbours run straight and need no track at all.
    if (toCell.col > fromCell.col && (transition.fromAny || toCell.row !== fromCell.row)) {
      request(toCell.col, index, 'turn');
    }
  });
  const gridRight = grid.left + grid.cols * NODE_W + Math.max(0, grid.cols - 1) * GAP_X;
  for (const [col, bucket] of requests) {
    let corridorLeft: number;
    let corridorRight: number;
    if (col <= 0) {
      const band = Math.max(grid.leftBand, SIDE_BAND_GAP + TRACK_STEP);
      corridorLeft = grid.left - band + SIDE_BAND_GAP * 0.3;
      corridorRight = grid.left - SIDE_BAND_GAP * 0.3;
    } else if (col >= grid.cols) {
      const band = Math.max(grid.rightBand, SIDE_BAND_GAP + TRACK_STEP);
      corridorLeft = gridRight + SIDE_BAND_GAP * 0.3;
      corridorRight = gridRight + band - SIDE_BAND_GAP * 0.3;
    } else {
      const columnX = grid.left + col * (NODE_W + GAP_X);
      corridorLeft = columnX - GAP_X + TRACK_MARGIN;
      corridorRight = columnX - TRACK_MARGIN;
    }
    const centre = (corridorLeft + corridorRight) / 2;
    // Wide gutters space tracks at the nominal step; a band narrower than the whole fan spreads
    // its tracks across whatever width it has.
    const step = Math.min(TRACK_STEP, (corridorRight - corridorLeft) / Math.max(1, bucket.length));
    bucket.forEach((req, slot) => {
      const centred = slot - (bucket.length - 1) / 2;
      const x = Math.min(Math.max(centre + centred * step, corridorLeft), corridorRight);
      if (req.kind === 'turn') {
        result[req.index].turnX = x;
      } else if (req.kind === 'down') {
        result[req.index].backDownX = x;
      } else {
        result[req.index].backUpX = x;
      }
    });
  }
  return result;
}

/** Routing slots assigned to one transition, computed once per region so that every edge sharing
 *  a port, a node pair or a corridor with another edge gets its own offset instead of stacking on
 *  top of it. Each index/count pair is used to spread that group symmetrically around its centre
 *  line: slot = index - (count - 1) / 2. */
interface EdgeSlots {
  /** Among edges connecting the same (from, to) pair — fans the whole curve. */
  pairIndex: number;
  pairCount: number;
  /** Among edges leaving the same source — fans the exit ports. */
  exitIndex: number;
  exitCount: number;
  /** Among edges arriving at the same target — fans the entry ports. Any-source edges share this
   *  group with the ordinary ones because they enter through the same left edge. */
  entryIndex: number;
  entryCount: number;
  /** Any-source edges only: which horizontal lane above the grid this edge travels in (unique
   *  per any-edge within the region). Vertical tracks are handled separately by
   *  AssignVerticalTracks, which allocates them across every row-changing edge. */
  laneIndex: number;
  /** Which horizontal lane below the grid this edge travels in (unique per lane-routed edge
   *  within the region), or -1 when the edge is not lane-routed. Doubles as drawEdge's test for
   *  the lane-routed shape, so the NeedsLaneRouting predicate is evaluated exactly once. */
  backLaneIndex: number;
}

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
  // The graph section's body: the flex-compressed visible window. applyContentOffset measures
  // pan range against this element (see the comment there for why not the viewport).
  private sectionBody: HTMLElement | null = null;
  private tooltip: HTMLElement;
  // "Blueprint" collapsible section wrapping the graph viewport, and "Animations" section
  // holding the flat animation list below it. Body elements toggle .sm-section-collapsed
  // when the user clicks the header (which owns its own chevron via closure).
  private blueprintSection: HTMLElement;
  private animationsSection: HTMLElement;
  private animationList: HTMLElement;
  // Rows keyed by animation id so highlight refresh doesn't rebuild the DOM.
  private animationRowEls = new Map<string, HTMLElement>();
  // Ids of animations rendered as top-level list rows (declaration order).
  private animationIds: string[] = [];
  // Last highlight set applied to the animation list; used to diff on poll.
  private lastAnimationHighlight: Set<string> = new Set();
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

    // Collapsible sections: Blueprint (the graph) and Animations (a flat list of top-level
    // animation defs). Both default to expanded; clicking the header toggles collapse. State
    // is not persisted across reloads (per product decision, keep it simple).
    const blueprint = this.buildSection('Blueprint');
    this.blueprintSection = blueprint.section;
    // Marks the graph section as the panel's flexible part: it absorbs the remaining panel
    // height so the viewport inside gets a bounded height (see .sm-section-graph in styles).
    blueprint.section.classList.add('sm-section-graph');
    this.sectionBody = blueprint.body;
    blueprint.body.appendChild(this.viewport);

    const animations = this.buildSection('Animations');
    this.animationsSection = animations.section;
    this.animationList = document.createElement('div');
    this.animationList.className = 'sm-anim-list';
    animations.body.appendChild(this.animationList);

    this.tooltip = document.createElement('div');
    this.tooltip.className = 'sm-tooltip';
    this.tooltip.style.display = 'none';

    this.root.appendChild(header);
    this.root.appendChild(this.blueprintSection);
    this.root.appendChild(this.animationsSection);
    // this.tooltip is intentionally NOT appended here — attach() mounts it as a sibling of the
    // panel so the panel's overflow: hidden cannot clip it.
  }

  /** Builds one collapsible section. Header row (chevron + title) toggles the body's
   *  .sm-section-collapsed class on click; the caller populates the returned body element
   *  with its own content. */
  private buildSection(title: string): { section: HTMLElement; body: HTMLElement } {
    const section = document.createElement('div');
    section.className = 'sm-section';
    const headerEl = document.createElement('div');
    headerEl.className = 'sm-section-header';
    const chevron = document.createElement('span');
    chevron.className = 'sm-section-chevron';
    chevron.textContent = '\u25BE'; // ▾ expanded
    const titleEl = document.createElement('span');
    titleEl.className = 'sm-section-title';
    titleEl.textContent = title;
    headerEl.appendChild(chevron);
    headerEl.appendChild(titleEl);
    const body = document.createElement('div');
    body.className = 'sm-section-body';
    headerEl.addEventListener('click', () => {
      const collapsed = body.classList.toggle('sm-section-collapsed');
      chevron.textContent = collapsed ? '\u25B8' : '\u25BE'; // ▸ / ▾
    });
    section.appendChild(headerEl);
    section.appendChild(body);
    return { section, body };
  }

  attach(parent: HTMLElement): void {
    parent.appendChild(this.root);
    // The tooltip lives OUTSIDE the panel: the panel clips its overflow (overflow: hidden for
    // the max-height clamp), which would cut off a tooltip anchored near the panel's edge.
    // Mounting it as a sibling above the panel lets it overhang onto the canvas instead.
    parent.appendChild(this.tooltip);
  }

  setVisible(visible: boolean): void {
    this.visible = visible;
    this.updateVisibility();
  }

  private updateVisibility(): void {
    const shown = this.visible && this.regions.length > 0;
    this.root.style.display = shown ? '' : 'none';
    // The tooltip is a sibling of the panel, so hiding the panel no longer hides it implicitly.
    if (!shown) {
      this.tooltip.style.display = 'none';
    }
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
    this.animationRowEls.clear();
    this.animationIds = [];
    this.lastAnimationHighlight.clear();
    this.currentStates = {};
    this.contentX = 0;
    this.contentY = 0;
    this.applyContentOffset();
    this.content.innerHTML = '';
    this.animationList.innerHTML = '';
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
    // Populate the flat animation list from the top-level <Animations> definitions. Skip the
    // SM node itself (kind == 'stateMachine') and any non-animation nodes like mount groups.
    for (const node of tree) {
      if (node.kind !== 'animation') {
        continue;
      }
      this.animationIds.push(node.id);
      const row = this.buildAnimationRow(node);
      this.animationRowEls.set(node.id, row);
      this.animationList.appendChild(row);
    }
    this.updateVisibility();
    this.lastPreviewId = this.refreshSelectionState();
    this.refreshHighlight(true);
    this.refreshAnimationHighlight(true);
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
    // The tooltip is mounted next to the panel rather than inside it, so it needs its own
    // teardown; otherwise it would outlive the panel in the host's DOM.
    if (this.tooltip.parentElement != null) {
      this.tooltip.parentElement.removeChild(this.tooltip);
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
    // Clamp against the section body, not the viewport. The body is the flex-compressed visible
    // window (its size is definite via the blueprint max-height -> section -> body min-height:0
    // chain), while the viewport grows with the content: a percentage max-height on the viewport
    // does not resolve against an auto-sized flex parent, so viewport-based measuring always
    // yields a zero pan range. The body's overflow: hidden provides the actual visual clipping.
    const panBounds = this.sectionBody ?? this.viewport;
    const maxX = panBounds.clientWidth - this.content.offsetWidth;
    const maxY = panBounds.clientHeight - this.content.offsetHeight;
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
      this.refreshAnimationHighlight(true);
    } else {
      this.refreshHighlight(false);
      this.refreshAnimationHighlight(false);
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
    // Every shape that leaves the node grid needs its band reserved up front, otherwise it renders
    // outside the viewBox and gets clipped: self-loops arc above their node, any-source edges run
    // in lanes above the grid, lane-routed edges in lanes below it, and the latter also need a
    // descent corridor right of the last column / a climb corridor left of the first one.
    const selfLoopCounts = new Map<string, number>();
    let laneEdgeCount = 0;
    let rightBandTracks = 0;
    let leftBandTracks = 0;
    for (const transition of region.transitions) {
      const fromCell = layout.cells.get(transition.fromAny ? ANY_NODE_KEY : transition.from);
      const toCell = layout.cells.get(transition.to);
      if (fromCell == null || toCell == null) {
        continue;
      }
      if (fromCell.col === toCell.col && fromCell.row === toCell.row) {
        const key = `${transition.from}|${transition.to}`;
        selfLoopCounts.set(key, (selfLoopCounts.get(key) ?? 0) + 1);
      } else if (NeedsLaneRouting(fromCell, toCell)) {
        laneEdgeCount++;
        if (fromCell.col + 1 >= layout.cols) {
          rightBandTracks++;
        }
        if (toCell.col <= 0) {
          leftBandTracks++;
        }
      }
    }
    const bottomPad = laneEdgeCount > 0 ? BACK_LANE_GAP + laneEdgeCount * BACK_LANE_STEP : 0;
    const rightPad = rightBandTracks > 0 ? SIDE_BAND_GAP + rightBandTracks * TRACK_STEP : 0;
    const leftPad = leftBandTracks > 0 ? SIDE_BAND_GAP + leftBandTracks * TRACK_STEP : 0;
    // Any-source edges travel over the grid in their own horizontal lanes (one per edge) so they
    // never cut through the states between the any node and their targets. Reserve a band above
    // the grid for those lanes, stacked on top of the self-loop bump so the two never collide.
    const anyEdgeCount = region.transitions.filter((transition) => transition.fromAny).length;
    const anyLanePad = anyEdgeCount > 0 ? ANY_LANE_GAP + anyEdgeCount * ANY_LANE_STEP : 0;
    // Derived from the same constants drawEdge bumps its arcs by, plus room for the label that
    // sits inside the outermost arc — a fixed reservation was too small once a node carried two
    // self-loops.
    const maxSelfLoops = Math.max(0, ...selfLoopCounts.values());
    const selfLoopPad = maxSelfLoops > 0
      ? SELF_LOOP_BUMP + (maxSelfLoops - 1) * SELF_LOOP_STEP + 14 : 0;
    const topPad = selfLoopPad + anyLanePad;
    const canvasWidth = PADDING_X * 2 + leftPad + rightPad + layout.cols * NODE_W +
      Math.max(0, layout.cols - 1) * GAP_X;
    const canvasHeight = PADDING_Y * 2 + layout.rows * NODE_H +
      Math.max(0, layout.rows - 1) * GAP_Y + topPad + bottomPad;
    canvas.style.width = `${canvasWidth}px`;
    canvas.style.height = `${canvasHeight}px`;

    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('class', 'sm-edges');
    svg.setAttribute('width', String(canvasWidth));
    svg.setAttribute('height', String(canvasHeight));
    const markerId = `sm-arrow-${nextMarkerSeq++}`;
    svg.appendChild(this.createArrowMarker(markerId));
    canvas.appendChild(svg);

    const gridTop = PADDING_Y + topPad;
    // The grid starts after the left band so a lane-routed edge climbing into the first column
    // has a node-free corridor of its own; without it those climbs shared the 20px canvas padding
    // and collapsed onto each other.
    const gridLeft = PADDING_X + leftPad;
    const positions = new Map<string, { x: number; y: number }>();
    for (const [name, cell] of layout.cells) {
      const x = gridLeft + cell.col * (NODE_W + GAP_X);
      const y = gridTop + cell.row * (NODE_H + GAP_Y);
      positions.set(name, { x, y });
      if (name === ANY_NODE_KEY) {
        // The any-source pseudo-node: a visual stand-in for "leaves whatever the current state
        // is". Styled differently (dashed) from real states and carries no preview affordances.
        const anyEl = this.buildAnyNodeElement();
        anyEl.style.left = `${x}px`;
        anyEl.style.top = `${y}px`;
        canvas.appendChild(anyEl);
        continue;
      }
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
    }
    const edgeSlots = this.computeEdgeSlots(region, layout.cells);
    const gridBottom = gridTop + layout.rows * NODE_H + Math.max(0, layout.rows - 1) * GAP_Y;
    const grid: GridMetrics = {
      left: gridLeft,
      top: gridTop,
      bottom: gridBottom,
      cols: layout.cols,
      leftBand: leftPad,
      rightBand: rightPad,
    };
    // Every vertical track the region needs, allocated region-wide so that edges sharing a
    // corridor (an any-source descent, a forward edge's turn and a lane-routed edge's climb) can
    // never pick the same x and cross each other.
    const gutterTracks = AssignVerticalTracks(region.transitions, layout.cells, grid);
    region.transitions.forEach((transition, index) => {
      this.drawEdge(svg, positions, transition, markerId, edgeSlots[index], grid,
        gutterTracks[index]);
    });
    wrapper.appendChild(canvas);
    return wrapper;
  }

  /** Assigns routing slots to every transition of a region: which fan position it takes among the
   *  edges sharing its pair / source port / target port, plus the horizontal lane it travels in
   *  (above the grid for any-source edges, below it for lane-routed ones). Doing this up front
   *  rather than per edge is what lets drawEdge pick a corridor no sibling edge occupies. The
   *  vertical tracks inside those corridors are allocated separately by AssignVerticalTracks. */
  private computeEdgeSlots(region: SMRegionData,
                           cells: Map<string, { col: number; row: number }>): EdgeSlots[] {
    const pairTotals = new Map<string, number>();
    const exitTotals = new Map<string, number>();
    const entryTotals = new Map<string, number>();
    const pairKeys: string[] = [];
    const exitKeys: string[] = [];
    const entryKeys: string[] = [];
    for (const transition of region.transitions) {
      const source = transition.fromAny ? ANY_NODE_KEY : transition.from;
      const pairKey = `${source}|${transition.to}`;
      // All incoming edges — any-source ones included — now enter through the target's LEFT
      // edge, so they form ONE entry group per target. Splitting any-source edges into their own
      // group (as an earlier revision did, back when they entered from the top) made both groups
      // fan around the same centre line and stacked their arrow heads at the same point.
      const entryKey = transition.to;
      pairKeys.push(pairKey);
      exitKeys.push(source);
      entryKeys.push(entryKey);
      pairTotals.set(pairKey, (pairTotals.get(pairKey) ?? 0) + 1);
      exitTotals.set(source, (exitTotals.get(source) ?? 0) + 1);
      entryTotals.set(entryKey, (entryTotals.get(entryKey) ?? 0) + 1);
    }
    const pairSeen = new Map<string, number>();
    const exitSeen = new Map<string, number>();
    const entrySeen = new Map<string, number>();
    let laneCursor = 0;
    let backLaneCursor = 0;
    return region.transitions.map((transition, index) => {
      const pairKey = pairKeys[index];
      const exitKey = exitKeys[index];
      const entryKey = entryKeys[index];
      const pairIndex = pairSeen.get(pairKey) ?? 0;
      const exitIndex = exitSeen.get(exitKey) ?? 0;
      const entryIndex = entrySeen.get(entryKey) ?? 0;
      pairSeen.set(pairKey, pairIndex + 1);
      exitSeen.set(exitKey, exitIndex + 1);
      entrySeen.set(entryKey, entryIndex + 1);
      const fromCell = cells.get(transition.fromAny ? ANY_NODE_KEY : transition.from);
      const toCell = cells.get(transition.to);
      const laneRouted = fromCell != null && toCell != null &&
        !(fromCell.col === toCell.col && fromCell.row === toCell.row) &&
        NeedsLaneRouting(fromCell, toCell);
      return {
        pairIndex,
        pairCount: pairTotals.get(pairKey) ?? 1,
        exitIndex,
        exitCount: exitTotals.get(exitKey) ?? 1,
        entryIndex,
        entryCount: entryTotals.get(entryKey) ?? 1,
        laneIndex: transition.fromAny ? laneCursor++ : 0,
        backLaneIndex: laneRouted ? backLaneCursor++ : -1,
      };
    });
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
    // Any-source transitions (from="any") have no concrete source state. The any node takes a
    // dedicated leftmost column so the graph keeps its left-to-right reading order, and its
    // edges are routed over the grid (see the fromAny branch in drawEdge) instead of straight
    // through the states that sit between it and its targets.
    const hasAnySource = region.transitions.some((transition) => transition.fromAny);
    if (hasAnySource) {
      for (const cell of cells.values()) {
        cell.col += 1;
      }
      cells.set(ANY_NODE_KEY, { col: 0, row: 0 });
      cols += 1;
      rows = Math.max(rows, 1);
    }
    return { cells, cols, rows };
  }

  /** Builds the "any" pseudo-node shown as the shared source of from="any" transitions. No
   *  preview / tooltip affordances: it is not a real state, only an edge anchor. */
  private buildAnyNodeElement(): HTMLElement {
    const el = document.createElement('div');
    el.className = 'sm-state sm-state-any';
    const nameEl = document.createElement('span');
    nameEl.className = 'sm-state-name';
    nameEl.textContent = 'any';
    el.appendChild(nameEl);
    const subEl = document.createElement('span');
    subEl.className = 'sm-state-sub';
    subEl.textContent = 'from any state';
    el.appendChild(subEl);
    return el;
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
    // Positioned against the tooltip's offsetParent (the player root it is mounted into by
    // attach), not the panel: the tooltip is a sibling of the panel so it can overhang onto the
    // canvas. Falls back to the panel rect when there is no positioned ancestor yet.
    const hostRect = (this.tooltip.offsetParent as HTMLElement | null)?.getBoundingClientRect() ??
        this.root.getBoundingClientRect();
    const stateRect = stateEl.getBoundingClientRect();
    const tooltipLeft = Math.max(0, stateRect.right - hostRect.left + 6);
    const tooltipTop = Math.max(0, stateRect.top - hostRect.top - 6);
    this.tooltip.style.left = `${tooltipLeft}px`;
    this.tooltip.style.top = `${tooltipTop}px`;
  }

  private drawEdge(svg: SVGElement,
                   positions: Map<string, { x: number; y: number }>,
                   transition: { from: string; fromAny: boolean; to: string; conditions: string },
                   markerId: string,
                   slots: EdgeSlots,
                   grid: GridMetrics,
                   tracks: EdgeTracks): void {
    const fromPos = positions.get(transition.fromAny ? ANY_NODE_KEY : transition.from);
    const toPos = positions.get(transition.to);
    if (fromPos == null || toPos == null) {
      return;
    }
    // Vertical (or horizontal for back edges) offset applied to fan out multi-edges between
    // the same pair of nodes. The centered slot is 0; siblings step out symmetrically. Step
    // size is wider than the label line-height so parallel labels get their own row instead
    // of visually merging with the neighbor.
    const offset = (slots.pairIndex - (slots.pairCount - 1) / 2) * 24;
    // Exit-port fan for edges sharing a source but heading to different targets: without it
    // they all leave the source's right edge at the same point and overlap until they diverge.
    // Smaller step than the parallel fan since these curves separate on their own further out.
    const sourceOffset = slots.exitCount > 1
      ? (slots.exitIndex - (slots.exitCount - 1) / 2) * 10 : 0;
    // Entry-port fan, mirroring sourceOffset for the arriving end: edges converging on one
    // target from different sources would otherwise stack their arrow heads and labels at the
    // same point on its left edge.
    const targetOffset = slots.entryCount > 1
      ? (slots.entryIndex - (slots.entryCount - 1) / 2) * 14 : 0;
    const group = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'sm-edge');
    path.setAttribute('marker-end', `url(#${markerId})`);
    let midX = 0;
    let midY = 0;
    const sameSpot = fromPos.x === toPos.x && fromPos.y === toPos.y;
    const forward = toPos.x > fromPos.x;
    if (transition.fromAny) {
      // Any-source edge, routed orthogonally through node-free corridors only (the convention
      // used by graph layout engines such as dagre / ELK):
      //   1. leave the any node's right edge,
      //   2. climb into a horizontal lane above the grid that belongs to this edge alone,
      //   3. run right to the GUTTER separating the target's column from the previous one,
      //   4. descend inside that gutter, and
      //   5. turn into the target's left edge, so the arrow reads like every other incoming edge.
      // Descending in the gutter rather than in the target's own column is the essential part:
      // a column holds nodes, so a descent aimed at a lower row would cut straight through every
      // node stacked above the target (the earlier "drop onto the target" routing did exactly
      // that). Gutters hold no nodes by construction, so the descent is always collision-free.
      const laneY = grid.top - ANY_LANE_GAP - slots.laneIndex * ANY_LANE_STEP;
      const x1 = fromPos.x + NODE_W;
      const y1 = fromPos.y + NODE_H / 2 + sourceOffset;
      const entryY = toPos.y + NODE_H / 2 + targetOffset;
      // Descend on the track the region-wide allocator reserved for this edge (see
      // AssignVerticalTracks): the gutter holds no nodes by construction, and no sibling edge was
      // given the same track, so the descent can cross neither a node nor another edge's turn.
      const descendX = tracks.turnX ?? toPos.x - GAP_X / 2;
      // Climb right after the exit port, staggered per lane so sibling any-edges do not share the
      // same vertical, and never placed past the descent line.
      const riseX = Math.min(x1 + 26 + slots.laneIndex * 12, descendX - EDGE_CORNER * 2);
      const corner = Math.max(2, Math.min(EDGE_CORNER, Math.abs(riseX - x1) / 2,
        Math.abs(descendX - riseX) / 2, Math.abs(y1 - laneY) / 2, Math.abs(entryY - laneY) / 2,
        Math.abs(toPos.x - descendX) / 2));
      path.setAttribute('d',
        `M ${x1} ${y1} L ${riseX - corner} ${y1}` +
        ` Q ${riseX} ${y1} ${riseX} ${y1 - corner}` +
        ` L ${riseX} ${laneY + corner}` +
        ` Q ${riseX} ${laneY} ${riseX + corner} ${laneY}` +
        ` L ${descendX - corner} ${laneY}` +
        ` Q ${descendX} ${laneY} ${descendX} ${laneY + corner}` +
        ` L ${descendX} ${entryY - corner}` +
        ` Q ${descendX} ${entryY} ${descendX + corner} ${entryY}` +
        ` L ${toPos.x} ${entryY}`);
      // Label sits on this edge's own lane, so lanes being a label-height apart is enough to
      // keep every any-edge label legible without further biasing.
      midX = (riseX + descendX) / 2;
      midY = laneY;
    } else if (sameSpot) {
      // Self-loop: an arc sitting above the node, radius grows with pairIndex so multiple
      // self-loops stack outward without overlapping. buildRegionElement reserves the top band
      // from the same constants, so the outermost arc is always inside the viewBox.
      const bump = SELF_LOOP_BUMP + slots.pairIndex * SELF_LOOP_STEP;
      const x1 = fromPos.x + NODE_W * 0.3;
      const x2 = fromPos.x + NODE_W * 0.7;
      const y = fromPos.y;
      path.setAttribute('d', `M ${x1} ${y} C ${x1} ${y - bump} ${x2} ${y - bump} ${x2} ${y}`);
      midX = (x1 + x2) / 2;
      // Label sits inside the arc, well clear of the curve itself.
      midY = y - bump + 10;
    } else if (slots.backLaneIndex >= 0) {
      // Lane-routed edge — a back edge, or a same-column edge spanning more than one row (a
      // straight vertical would cut through every node stacked between its endpoints). It uses
      // the same ports as every other edge: OUT of the source's RIGHT edge, IN to the target's
      // LEFT edge. Keeping the two sides dedicated is what makes the graph readable at a glance —
      // an earlier revision left the source on its left edge, so a node's left side carried both
      // its incoming arrows and its outgoing back edge. The path steps right out of the source,
      // descends the corridor right of its column, runs left in a lane BELOW the grid that
      // belongs to this edge alone, climbs the corridor left of the target column, and turns into
      // the target. Every vertical sits in a node-free corridor and the long horizontal below the
      // grid, so the edge crosses neither a node nor another edge's track.
      const x1 = fromPos.x + NODE_W;
      const y1 = fromPos.y + NODE_H / 2 + sourceOffset;
      const entryY = toPos.y + NODE_H / 2 + targetOffset;
      const downX = tracks.backDownX ?? x1 + GAP_X / 2;
      const upX = tracks.backUpX ?? toPos.x - PADDING_X / 2;
      const laneY = grid.bottom + BACK_LANE_GAP + slots.backLaneIndex * BACK_LANE_STEP;
      const corner = Math.max(2, Math.min(EDGE_CORNER, Math.abs(downX - x1) / 2,
        Math.abs(downX - upX) / 2, Math.abs(laneY - y1) / 2, Math.abs(laneY - entryY) / 2,
        Math.abs(toPos.x - upX) / 2));
      path.setAttribute('d',
        `M ${x1} ${y1} L ${downX - corner} ${y1}` +
        ` Q ${downX} ${y1} ${downX} ${y1 + corner}` +
        ` L ${downX} ${laneY - corner}` +
        ` Q ${downX} ${laneY} ${downX - corner} ${laneY}` +
        ` L ${upX + corner} ${laneY}` +
        ` Q ${upX} ${laneY} ${upX} ${laneY - corner}` +
        ` L ${upX} ${entryY + corner}` +
        ` Q ${upX} ${entryY} ${upX + corner} ${entryY}` +
        ` L ${toPos.x} ${entryY}`);
      // Label rides this edge's own lane, centred on the run below the grid.
      midX = (downX + upX) / 2;
      midY = laneY;
    } else if (forward) {
      // Forward edge, routed orthogonally like the any-source ones so the two can share a gutter
      // without crossing: run along the source row, turn down/up inside the gutter on a track no
      // other edge occupies, then run along the target row into its left edge. A single wide
      // bezier (the previous shape) swept across the whole corridor, so it inevitably crossed
      // any descent line living there — no amount of label dodging could fix that, because it is
      // the PATH that overlaps, not just the text.
      const x1 = fromPos.x + NODE_W;
      const x2 = toPos.x;
      const y1 = fromPos.y + NODE_H / 2 + offset + sourceOffset;
      const y2 = toPos.y + NODE_H / 2 + offset + targetOffset;
      if (Math.abs(y1 - y2) < 1) {
        // Same row: a straight horizontal run, no turn needed.
        path.setAttribute('d', `M ${x1} ${y1} L ${x2} ${y2}`);
        midX = (x1 + x2) / 2;
        midY = y1 - 8;
      } else {
        const turnX = tracks.turnX ?? (x1 + x2) / 2;
        const sweep = y2 > y1 ? 1 : -1;
        const corner = Math.max(2, Math.min(EDGE_CORNER, Math.abs(y2 - y1) / 2,
          Math.abs(turnX - x1), Math.abs(x2 - turnX)));
        path.setAttribute('d',
          `M ${x1} ${y1} L ${turnX - corner} ${y1}` +
          ` Q ${turnX} ${y1} ${turnX} ${y1 + sweep * corner}` +
          ` L ${turnX} ${y2 - sweep * corner}` +
          ` Q ${turnX} ${y2} ${turnX + corner} ${y2}` +
          ` L ${x2} ${y2}`);
        // Label rides the horizontal run on the SOURCE side of the turn, where the corridor is
        // free of vertical tracks by construction.
        midX = (x1 + turnX) / 2;
        midY = y1 - 8;
      }
    } else {
      // Same-column neighbours (adjacent rows): a straight vertical line between the facing
      // edges, which only crosses the row gap and therefore no node. Same-column edges spanning
      // more than one row take the lane-routed shape above instead. Being in the same column is
      // asserted by the branch order — a non-forward, non-lane-routed, non-self-loop edge can
      // only be one.
      const x = fromPos.x + NODE_W / 2 + offset;
      const goingDown = toPos.y > fromPos.y;
      const y1 = goingDown ? fromPos.y + NODE_H : fromPos.y;
      const y2 = goingDown ? toPos.y : toPos.y + NODE_H;
      path.setAttribute('d', `M ${x} ${y1} L ${x} ${y2}`);
      midX = x + 8;
      midY = (y1 + y2) / 2;
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

  /** Builds one row in the animations section. Double-clicking a row solo-previews that
   *  animation - same host path used by state node double-click on the blueprint above. */
  private buildAnimationRow(node: TimelineTreeNode): HTMLElement {
    const row = document.createElement('div');
    row.className = 'sm-anim-row';
    const nameEl = document.createElement('span');
    nameEl.className = 'sm-anim-name';
    nameEl.textContent = node.name || node.id;
    row.appendChild(nameEl);
    const metaEl = document.createElement('span');
    metaEl.className = 'sm-anim-meta';
    metaEl.textContent = this.formatAnimationMeta(node);
    row.appendChild(metaEl);
    row.addEventListener('dblclick', (event) => {
      event.stopPropagation();
      const selection = this.host.getSelectedTimelineUnit();
      if (selection != null && selection.kind === 'animation' && selection.id === node.id) {
        // Second dbl-click on the same animation exits the preview - matches the blueprint
        // state node behavior.
        this.host.selectTimelineUnit('', '');
        return;
      }
      this.host.selectTimelineUnit('animation', node.id);
    });
    return row;
  }

  private formatAnimationMeta(node: TimelineTreeNode): string {
    const parts: string[] = [];
    if (node.durationUs != null && node.durationUs > 0) {
      const seconds = node.durationUs / 1_000_000;
      parts.push(`${seconds < 10 ? seconds.toFixed(2) : Math.round(seconds)}s`);
    }
    if (node.loop) {
      parts.push(node.loop);
    }
    return parts.join(' \u00b7 ');
  }

  /** Highlights rows in the animations section that correspond to a currently-playing
   *  animation: the active preview id, or the animations bound to each SM region's live
   *  current state. Parked previews don't count - the SM is driving the render loop then. */
  private refreshAnimationHighlight(force: boolean): void {
    if (this.animationRowEls.size === 0) {
      return;
    }
    const active = new Set<string>();
    const selection = this.host.getSelectedTimelineUnit();
    if (selection != null && selection.kind === 'animation') {
      active.add(selection.id);
    } else {
      for (const region of this.regions) {
        const currentName = this.currentStates[region.name] ?? '';
        if (!currentName) {
          continue;
        }
        const state = region.states.find((s) => s.name === currentName);
        if (state != null && state.animationId) {
          active.add(state.animationId);
        }
      }
    }
    if (!force && setsEqual(active, this.lastAnimationHighlight)) {
      return;
    }
    this.lastAnimationHighlight = active;
    this.animationRowEls.forEach((row, id) => {
      row.classList.toggle('sm-anim-row-current', active.has(id));
    });
  }
}

/** Reference-independent equality check for the small string sets we use for highlight diff. */
function setsEqual(a: Set<string>, b: Set<string>): boolean {
  if (a.size !== b.size) return false;
  for (const value of a) {
    if (!b.has(value)) return false;
  }
  return true;
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
  // Snapshot of what render() last drew. The poll tick calls refresh() every 250ms; without the
  // snapshot each tick would wipe and rebuild the DOM (innerHTML = ''), losing slow clicks that
  // straddle a tick and any keyboard focus inside the strip, and forcing a reflow. render() runs
  // only when the chip sequence or the active chip actually changed since the last draw.
  private lastRenderedChips: string[] | null = null;
  private lastRenderedActive: string | null = null;
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
    this.renderChipsIfChanged();
  }

  private renderChipsIfChanged(): void {
    const chipsUnchanged =
        this.lastRenderedChips != null &&
        this.lastRenderedChips.length === this.chips.length &&
        this.lastRenderedChips.every((id, i) => id === this.chips[i]);
    if (chipsUnchanged && this.lastRenderedActive === this.activeChip) {
      return;
    }
    this.render();
  }

  private render(): void {
    this.root.innerHTML = '';
    for (const animationId of this.chips) {
      this.root.appendChild(this.buildChip(animationId));
    }
    this.lastRenderedChips = [...this.chips];
    this.lastRenderedActive = this.activeChip;
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
