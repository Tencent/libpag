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
import { LOOP_SEQUENCE_ICON_SVG, LOOP_SINGLE_ICON_SVG } from './icons';
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
  /** Makes the state machine the thing driving the render loop, before the panel writes an input
   *  into it. A paused SM never advances, so a written input would sit unused, and a solo preview
   *  freezes the SM completely. Both cases restart the machine from its initial states, which is
   *  why the caller has to write the input AFTER calling this - a write before it would be
   *  cleared. */
  ensureSMDriving(): void;
  /** Writes one state-machine input. All three return false when the document's default timeline
   *  is not a state machine or the name/type does not match. */
  setSMInputBool(name: string, value: boolean): boolean;
  setSMInputNumber(name: string, value: number): boolean;
  fireSMInputTrigger(name: string): boolean;
}

// One uniform node height for every node in the graph (Rive / Unity Mecanim convention): a
// single 36px row carrying the loop glyph and the node name. The animation id and duration that
// used to fill a second line moved into the hover tooltip.
const NODE_W = 118;
const NODE_H = 36;
// Horizontal graph layout: columns are transition depth (BFS from initial state), rows are
// the vertical stacking within a column when several states share the same depth. GAP_X is
// deliberately wide (about the size of a full node) so multi-line label backdrops fit
// between adjacent columns without spilling onto the next node.
const GAP_X = 140;
const GAP_Y = 20;
const PADDING_X = 20;
const PADDING_Y = 20;
const POLL_INTERVAL_MS = 250;
// A single click on a state drives the machine into it; a double click solo-previews its bound
// animation. The drive is delayed by this much so a double click can cancel it - driving first and
// previewing second would visibly flash the wrong state on the way into every preview.
const SINGLE_CLICK_DELAY_MS = 220;
// Lifetime of the "this row was just driven" highlight and of the edge-fire flash.
const HIGHLIGHT_DURATION_MS = 1600;
const EDGE_FIRE_DURATION_MS = 700;
// Lifetime of a transient note in the panel header.
const NOTE_DURATION_MS = 2400;

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

// Geometry of the parallel-entry graph (see IsParallelEntryCase): a single Entry node on the
// left fans out to one node per region, all of them the regular NODE_W x NODE_H box. The Entry
// is narrower and vertically centred on the column it feeds, so the fan reads as one source
// splitting into parallel branches rather than a chain.
const ENTRY_NODE_W = 72;
const PARALLEL_GAP_X = 72;
const PARALLEL_GAP_Y = 24;
// Fraction of the gap a fan branch's curve spends bending: the remaining run at each end is
// straight, which is what makes the arrow head leave the Entry and meet its node horizontally.
const ENTRY_EDGE_BEND = 0.45;

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

/** Whether an edge renders as one straight segment with no bends: a forward edge whose endpoints
 *  share a row (BFS bounds a forward edge to one column, so that is a single horizontal run across
 *  a node-free gutter), or a same-column edge between adjacent rows (a vertical crossing only the
 *  row gap). A straight edge has a single y (or x), so the per-port exit/entry fans do not apply to
 *  it — only the pair offset, which shifts both ends equally. AssignVerticalTracks already relies
 *  on exactly this predicate when it skips requesting a turn track, so computeEdgeSlots (fan
 *  groups) and drawEdge (path shape) have to agree with it too; an earlier revision let drawEdge
 *  re-derive straightness from a 1px endpoint comparison instead, which the differently-stepped
 *  fans defeated and bent every fanned same-row edge at its midpoint. */
function IsStraightEdge(fromAny: boolean, fromCell: { col: number; row: number },
                        toCell: { col: number; row: number }): boolean {
  if (fromAny) {
    return false;
  }
  if (toCell.col > fromCell.col) {
    return toCell.row === fromCell.row;
  }
  if (toCell.col === fromCell.col) {
    return Math.abs(toCell.row - fromCell.row) === 1;
  }
  return false;
}

/** Midpoint of an orthogonal "run in -> run across -> run out" path, found by walking its
 *  accumulated length so the point lands on whichever run holds the halfway mark; pinning it to
 *  the first run (the obvious alternative) puts the mark a quarter along the path instead.
 *  Shared by the Entry fan-out edges and the forward edges of the region graph, which are the
 *  same shape. */
function OrthogonalMidpoint(x1: number, y1: number, turnX: number, y2: number, x2: number,
                            sweep: number): { x: number; y: number } {
  const runIn = turnX - x1;
  const runAcross = Math.abs(y2 - y1);
  const runOut = x2 - turnX;
  const half = (runIn + runAcross + runOut) / 2;
  if (half <= runIn) {
    return { x: x1 + half, y: y1 };
  }
  if (half <= runIn + runAcross) {
    return { x: turnX, y: y1 + sweep * (half - runIn) };
  }
  return { x: turnX + (half - runIn - runAcross), y: y2 };
}

/** Whether the whole state machine is a set of independent one-state regions: every region holds
 *  exactly one state and declares no transition, so no state can ever be left and the regions
 *  differ only by which animation they play. Such a document has no graph to lay out - each
 *  region would render as the same lone node - and reads far better as one Entry node fanning
 *  out to one node per region. HTML-imported documents land here routinely: the importer emits
 *  one region per CSS animation, each with a single state and no transitions. */
function IsParallelEntryCase(regions: SMRegionData[]): boolean {
  if (regions.length === 0) {
    return false;
  }
  return regions.every((region) => region.states.length === 1 && region.transitions.length === 0);
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
  /** Whether the edge renders as one straight segment (IsStraightEdge). drawEdge then draws a
   *  single line carrying only the pair offset, and the edge was kept out of the exit/entry fan
   *  groups in the first place. Same single-source-of-truth pattern as backLaneIndex. */
  straight: boolean;
}

/** What drawEdge hands back, so the caller can wire interactions without the drawing code having
 *  to know about them: the path (flashed when its transition fires) and the condition pill
 *  (clickable to drive the transition). `label` is null for an unconditional edge, which has no
 *  input to drive and therefore nothing to click. */
interface DrawnEdge {
  path: SVGPathElement;
  label: SVGGElement | null;
}

/** One state of a region, as the blueprint renders it. `loop` is resolved from the bound
 *  <Animation> definition (the state itself carries no loop mode) and drives the node's badge. */
interface SMStateData {
  name: string;
  animationId: string;
  durationUs: number;
  previewSupported: boolean;
  loop: string;
}

/** One transition of a region, as the blueprint draws and drives it. */
interface SMTransitionData {
  from: string;
  to: string;
  fromAny: boolean;
  /** AND-joined condition summary produced by the C++ side ("speed > 0.5", or "always" when
   *  unconditional). Kept as text because that is what the edge label shows; see ParseConditions
   *  for the reverse trip. */
  conditions: string;
}

interface SMRegionData {
  name: string;
  initial: string;
  states: SMStateData[];
  transitions: SMTransitionData[];
}

/** One parsed <Condition>. The view exports a transition's conditions only as the text summary it
 *  builds for the edge label ("speed > 0.5", AND-joined with " && "), because that is all a
 *  tooltip needs; driving the machine from the graph needs the parts back, so the blueprint parses
 *  the summary. Doing it here rather than asking the view for a second, structured export keeps
 *  the C++ side untouched - the coupling to that summary's exact formatting is confined to
 *  ParseConditions. */
interface ConditionClause {
  input: string;
  kind: 'bool' | 'number' | 'trigger';
  op: string;
  boolValue: boolean;
  numberValue: number;
}

// "<input> <op> [value]", the shape DescribeConditions emits per condition. The trailing value is
// absent for a trigger condition and present for bool ("true" / "false") and number (%g).
const CONDITION_PATTERN = /^(\S+)\s+(==|!=|<=|>=|<|>|trigger)(?:\s+(.+))?$/;

/** Splits a conditions summary back into clauses. A part that does not parse is dropped: an
 *  unparsable clause must never be driven with a guessed value. The input's declared type decides
 *  which control drives it, which is why the type map is threaded through. */
function ParseConditions(text: string, inputTypes: Map<string, string>): ConditionClause[] {
  const clauses: ConditionClause[] = [];
  if (text.length === 0 || text === 'always') {
    return clauses;
  }
  for (const part of text.split(' && ')) {
    const match = CONDITION_PATTERN.exec(part.trim());
    if (match == null) {
      continue;
    }
    const type = inputTypes.get(match[1]);
    const kind: ConditionClause['kind'] =
      type === 'bool' ? 'bool' : type === 'trigger' ? 'trigger' : 'number';
    const raw = match[3];
    clauses.push({
      input: match[1],
      kind,
      op: match[2],
      boolValue: raw === 'true',
      numberValue: raw != null ? parseFloat(raw) : 0,
    });
  }
  return clauses;
}

/** A value satisfying every numeric condition on ONE input: thresholds are stepped just inside
 *  the boundary, so "speed > 0" becomes 1 and "20 <= level < 80" becomes 20. Null when the
 *  conditions cannot all hold at once, which the caller reports instead of writing a value that
 *  would not move the machine. */
function SampleNumberValue(clauses: ConditionClause[]): number | null {
  let lower = Number.NEGATIVE_INFINITY;
  let upper = Number.POSITIVE_INFINITY;
  let exact: number | null = null;
  const excluded: number[] = [];
  for (const clause of clauses) {
    switch (clause.op) {
      case '==':
        exact = clause.numberValue;
        break;
      case '!=':
        excluded.push(clause.numberValue);
        break;
      case '>':
        lower = Math.max(lower, clause.numberValue + 1);
        break;
      case '>=':
        lower = Math.max(lower, clause.numberValue);
        break;
      case '<':
        upper = Math.min(upper, clause.numberValue - 1);
        break;
      case '<=':
        upper = Math.min(upper, clause.numberValue);
        break;
      default:
        break;
    }
  }
  if (exact != null) {
    return exact >= lower && exact <= upper ? exact : null;
  }
  let candidate = lower !== Number.NEGATIVE_INFINITY ? lower
    : upper !== Number.POSITIVE_INFINITY ? upper : 0;
  if (candidate < lower || candidate > upper) {
    return null;
  }
  while (excluded.includes(candidate)) {
    candidate += 1;
  }
  return candidate;
}

/** Identifies one transition's path inside a region, for the transition-pulse highlight. */
function EdgeKey(regionName: string, index: number): string {
  return `${regionName}#${index}`;
}

/**
 * State-machine blueprint panel for a default state-machine timeline, docked in the top-left
 * corner of the player. The panel header carries a play button that mirrors the playback bar's
 * primary button: while no solo preview is active it plays/pauses the whole SM; while previewing
 * it shows the play glyph and clicking it exits the preview (the SM resumes from its initial
 * states). Below the header sit two collapsible sections: the graph (one block per region, or a
 * single Entry fan-out when every region is just one stateless step) and the inputs the machine
 * can be driven by.
 *
 * The graph is a control, not just a picture: clicking an edge's condition pill writes the inputs
 * that condition needs, clicking a state drives its region into that state, and an edge flashes
 * while its region walks it. The graph never shows scrollbars; the mouse wheel pans content.
 * Double-clicking a state solo-previews that state's animation; hovering one shows its details.
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
  // "Blueprint" collapsible section wrapping the graph viewport, and "Inputs" section holding the
  // controls that drive it (one row per declared <Input>). Body elements toggle
  // .sm-section-collapsed when the user clicks the header (which owns its own chevron via
  // closure).
  private blueprintSection: HTMLElement;
  private inputsSection: HTMLElement;
  private inputList: HTMLElement;
  // name -> declared type, which decides which control a condition drives.
  private inputTypes = new Map<string, string>();
  // Row elements and the per-row callback that re-reads an input's mirrored value, keyed by input
  // name. Writes go through the sync callback rather than rebuilding the section.
  private inputRowEls = new Map<string, HTMLElement>();
  private inputSyncFns = new Map<string, () => void>();
  // The panel's own mirror of each input's value. The view offers setters but no getter, so this
  // is what the controls display; an input the panel never wrote reads as false / 0, which equals
  // the declared default only when the document leaves it there.
  private inputValues = new Map<string, boolean | number>();
  // Transition paths per region (EdgeKey -> path), driving the pulse highlight.
  private edgePaths = new Map<string, Map<string, SVGPathElement>>();
  private pollTimer: number | null = null;
  private regions: SMRegionData[] = [];
  private smTitle = '';
  private noteTimer: number | null = null;
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

    // Collapsible sections: Blueprint (the graph) and Inputs (the controls that drive it). Both
    // default to expanded; clicking the header toggles collapse. State is not persisted across
    // reloads (per product decision, keep it simple).
    const blueprint = this.buildSection('Blueprint');
    this.blueprintSection = blueprint.section;
    // Marks the graph section as the panel's flexible part: it absorbs the remaining panel
    // height so the viewport inside gets a bounded height (see .sm-section-graph in styles).
    blueprint.section.classList.add('sm-section-graph');
    this.sectionBody = blueprint.body;
    blueprint.body.appendChild(this.viewport);

    const inputs = this.buildSection('Inputs');
    this.inputsSection = inputs.section;
    this.inputList = document.createElement('div');
    this.inputList.className = 'sm-input-list';
    inputs.body.appendChild(this.inputList);

    this.tooltip = document.createElement('div');
    this.tooltip.className = 'sm-tooltip';
    this.tooltip.style.display = 'none';

    this.root.appendChild(header);
    this.root.appendChild(this.blueprintSection);
    this.root.appendChild(this.inputsSection);
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
    if (this.noteTimer != null) {
      window.clearTimeout(this.noteTimer);
      this.noteTimer = null;
    }
    this.smTitle = '';
    this.nodeEls.clear();
    this.animationEls.clear();
    this.inputRowEls.clear();
    this.inputSyncFns.clear();
    this.inputValues.clear();
    this.inputTypes.clear();
    this.edgePaths.clear();
    this.currentStates = {};
    this.contentX = 0;
    this.contentY = 0;
    this.applyContentOffset();
    this.content.innerHTML = '';
    this.inputList.innerHTML = '';
    if (smNode == null || smNode.regions == null || smNode.regions.length === 0) {
      this.updateVisibility();
      return false;
    }
    this.smTitle = smNode.name || smNode.id;
    this.titleEl.textContent = this.smTitle;
    // Loop mode is declared on the <Animation> definition, not on the state that plays it, so
    // resolve it from the tree's top-level animation nodes: the graph badges each state with how
    // its animation repeats.
    const loopByAnimationId = new Map<string, string>();
    for (const node of tree) {
      if (node.kind === 'animation' && node.loop != null) {
        loopByAnimationId.set(node.id, node.loop);
      }
    }
    for (const region of smNode.regions) {
      this.regions.push({
        name: region.name,
        initial: region.initial,
        states: region.states.map((state) => ({
          name: state.name,
          animationId: state.animationId,
          durationUs: state.durationUs,
          previewSupported: state.previewSupported === true,
          loop: loopByAnimationId.get(state.animationId) ?? '',
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
    if (IsParallelEntryCase(this.regions)) {
      // Nothing to route: the regions only differ by the animation they play, so the usual
      // per-region boxes would repeat the same one-node graph N times - and when the regions
      // reuse a state name (they routinely do), N identical-looking boxes. One Entry fan-out
      // says "these all start together" instead.
      this.content.appendChild(this.buildParallelEntryGraph(this.regions));
    } else {
      this.regions.forEach((region, index) => {
        const regionEl = this.buildRegionElement(region);
        if (index > 0) {
          regionEl.style.marginTop = `${GAP_Y}px`;
        }
        this.content.appendChild(regionEl);
      });
    }
    this.buildInputsList(smNode.inputs ?? []);
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
      const stateEl = this.buildStateElement(region, state, state.name);
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
    const edgePaths = new Map<string, SVGPathElement>();
    this.edgePaths.set(region.name, edgePaths);
    region.transitions.forEach((transition, index) => {
      const drawn = this.drawEdge(svg, positions, transition, markerId, edgeSlots[index], grid,
        gutterTracks[index]);
      if (drawn == null) {
        return;
      }
      edgePaths.set(EdgeKey(region.name, index), drawn.path);
      const label = drawn.label;
      if (label != null) {
        // The pill drives the transition it belongs to; the edge itself stays click-through so
        // dragging on the graph area is never swallowed.
        label.addEventListener('click', () => this.onConditionClick(transition));
        label.addEventListener('mouseenter', () => {
          this.showTooltipAt(label, `${transition.conditions}\nclick to drive this input`);
        });
        label.addEventListener('mouseleave', () => {
          this.tooltip.style.display = 'none';
        });
      }
    });
    wrapper.appendChild(canvas);
    return wrapper;
  }

  /** Renders the whole state machine as one Entry node fanning out to one node per region. Only
   *  used for the IsParallelEntryCase shape, where every region holds a single state and declares
   *  no transition: those regions all start together at the document's start and run in parallel,
   *  which is exactly what one Entry feeding N branches expresses - and it avoids repeating the
   *  same lone-node region box N times. Nodes are labelled by REGION name here, because in this
   *  shape every region tends to reuse one state name ("playing") and the state name therefore
   *  carries no information. */
  private buildParallelEntryGraph(regions: SMRegionData[]): HTMLElement {
    const canvas = document.createElement('div');
    canvas.className = 'sm-region-canvas';
    const columnHeight = regions.length * NODE_H +
      Math.max(0, regions.length - 1) * PARALLEL_GAP_Y;
    const canvasWidth = PADDING_X * 2 + ENTRY_NODE_W + PARALLEL_GAP_X + NODE_W;
    const canvasHeight = PADDING_Y * 2 + columnHeight;
    canvas.style.width = `${canvasWidth}px`;
    canvas.style.height = `${canvasHeight}px`;

    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('class', 'sm-edges');
    svg.setAttribute('width', String(canvasWidth));
    svg.setAttribute('height', String(canvasHeight));
    const markerId = `sm-arrow-${nextMarkerSeq++}`;
    svg.appendChild(this.createArrowMarker(markerId));
    canvas.appendChild(svg);

    const entryX = PADDING_X;
    // Centred on the column it feeds: the fan then reads as one source splitting into parallel
    // branches instead of as the first row of a chain.
    const entryY = PADDING_Y + (columnHeight - NODE_H) / 2;
    const nodeX = PADDING_X + ENTRY_NODE_W + PARALLEL_GAP_X;
    const entryEl = this.buildEntryNodeElement(regions.length);
    entryEl.style.left = `${entryX}px`;
    entryEl.style.top = `${entryY}px`;
    canvas.appendChild(entryEl);

    regions.forEach((region, index) => {
      const state = region.states[0];
      const y = PADDING_Y + index * (NODE_H + PARALLEL_GAP_Y);
      const stateEl = this.buildStateElement(region, state, region.name);
      stateEl.style.left = `${nodeX}px`;
      stateEl.style.top = `${y}px`;
      canvas.appendChild(stateEl);
      // Keyed exactly like the per-region graph so refreshHighlight() and the solo-preview
      // highlight keep working without knowing which shape rendered the node.
      this.nodeEls.set(`${region.name}/${state.name}`, stateEl);
      if (state.animationId) {
        const list = this.animationEls.get(state.animationId) ?? [];
        list.push(stateEl);
        this.animationEls.set(state.animationId, list);
      }
      this.drawEntryEdge(svg, entryX + ENTRY_NODE_W, entryY + NODE_H / 2, nodeX, y + NODE_H / 2,
        markerId);
    });
    return canvas;
  }

  /** The Entry node: where the state machine starts. It is an edge anchor rather than a state -
   *  no preview affordance, and the tooltip only says what it starts. */
  private buildEntryNodeElement(regionCount: number): HTMLElement {
    const el = document.createElement('div');
    el.className = 'sm-state sm-state-entry';
    const nameEl = document.createElement('span');
    nameEl.textContent = 'Entry';
    el.appendChild(nameEl);
    el.addEventListener('mouseenter', () => {
      this.showTooltipAt(el, `Entry\n${regionCount} parallel regions start here`);
    });
    el.addEventListener('mouseleave', () => {
      this.tooltip.style.display = 'none';
    });
    return el;
  }

  /** Draws one branch of the Entry fan-out: a smooth cubic from the Entry's right edge into the
   *  target's left edge. Orthogonal routing would stack every branch's vertical inside the same
   *  narrow gutter and read as a tangle; a curve spreads the branches apart on its own and is how
   *  the reference editor draws a fan leaving one source. The straight head and tail also make the
   *  arrow leave the Entry and meet its node horizontally, with no explicit corner handling. */
  private drawEntryEdge(svg: SVGElement, x0: number, entryY: number, x1: number, targetY: number,
                        markerId: string): void {
    const bend = (x1 - x0) * ENTRY_EDGE_BEND;
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'sm-edge');
    path.setAttribute('marker-end', `url(#${markerId})`);
    path.setAttribute('d',
      `M ${x0} ${entryY} C ${x0 + bend} ${entryY} ${x1 - bend} ${targetY} ${x1} ${targetY}`);
    svg.appendChild(path);
  }

  /** Assigns routing slots to every transition of a region: which fan position it takes among the
   *  edges sharing its pair / source port / target port, plus the horizontal lane it travels in
   *  (above the grid for any-source edges, below it for lane-routed ones). Straight edges
   *  (IsStraightEdge) join only the pair group — they have no bends, so there is no exit or entry
   *  port to fan. Doing this up front rather than per edge is what lets drawEdge pick a corridor
   *  no sibling edge occupies. The vertical tracks inside those corridors are allocated separately
   *  by AssignVerticalTracks. */
  private computeEdgeSlots(region: SMRegionData,
                           cells: Map<string, { col: number; row: number }>): EdgeSlots[] {
    const pairTotals = new Map<string, number>();
    const exitTotals = new Map<string, number>();
    const entryTotals = new Map<string, number>();
    const pairKeys: string[] = [];
    const exitKeys: string[] = [];
    const entryKeys: string[] = [];
    const straightFlags: boolean[] = [];
    for (const transition of region.transitions) {
      const source = transition.fromAny ? ANY_NODE_KEY : transition.from;
      const pairKey = `${source}|${transition.to}`;
      // All incoming edges — any-source ones included — now enter through the target's LEFT
      // edge, so they form ONE entry group per target. Splitting any-source edges into their own
      // group (as an earlier revision did, back when they entered from the top) made both groups
      // fan around the same centre line and stacked their arrow heads at the same point.
      const entryKey = transition.to;
      const fromCell = cells.get(source);
      const toCell = cells.get(transition.to);
      const straight = fromCell != null && toCell != null &&
        IsStraightEdge(transition.fromAny, fromCell, toCell);
      straightFlags.push(straight);
      pairKeys.push(pairKey);
      exitKeys.push(source);
      entryKeys.push(entryKey);
      pairTotals.set(pairKey, (pairTotals.get(pairKey) ?? 0) + 1);
      // A straight edge renders with a single y and uses no exit/entry fan, so it does not
      // consume a slot in those groups: the edges that do bend keep their fan centred on the
      // ports actually in play instead of on a slot reserved for an edge that never lands there.
      if (!straight) {
        exitTotals.set(source, (exitTotals.get(source) ?? 0) + 1);
        entryTotals.set(entryKey, (entryTotals.get(entryKey) ?? 0) + 1);
      }
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
      const straight = straightFlags[index];
      const pairIndex = pairSeen.get(pairKey) ?? 0;
      const exitIndex = straight ? 0 : (exitSeen.get(exitKey) ?? 0);
      const entryIndex = straight ? 0 : (entrySeen.get(entryKey) ?? 0);
      pairSeen.set(pairKey, pairIndex + 1);
      if (!straight) {
        exitSeen.set(exitKey, exitIndex + 1);
        entrySeen.set(entryKey, entryIndex + 1);
      }
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
        straight,
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
    nameEl.textContent = 'Any State';
    el.appendChild(nameEl);
    return el;
  }

  /** One state node. `label` is what the node shows, which is the state name everywhere except
   *  the parallel-entry graph: there every region reuses one state name, so the region name is
   *  what tells the rows apart. The tooltip always reports the real state name. */
  private buildStateElement(region: SMRegionData, state: SMStateData, label: string): HTMLElement {
    const el = document.createElement('div');
    el.className = 'sm-state';
    if (!state.previewSupported) {
      el.classList.add('sm-state-disabled');
    }
    const row = document.createElement('div');
    row.className = 'sm-state-row';
    // A state with no bound animation plays nothing, so it gets no repeat badge either.
    if (state.animationId) {
      const iconEl = document.createElement('span');
      iconEl.className = 'sm-state-icon';
      iconEl.innerHTML = state.loop === 'once' ? LOOP_SINGLE_ICON_SVG : LOOP_SEQUENCE_ICON_SVG;
      row.appendChild(iconEl);
    }
    const nameEl = document.createElement('span');
    nameEl.className = 'sm-state-name';
    nameEl.textContent = label;
    row.appendChild(nameEl);
    el.appendChild(row);
    el.addEventListener('mouseenter', () => {
      this.showTooltip(el, state, region.name);
    });
    el.addEventListener('mouseleave', () => {
      this.tooltip.style.display = 'none';
    });
    let clickTimer: number | null = null;
    el.addEventListener('click', () => {
      if (clickTimer != null) {
        return;
      }
      clickTimer = window.setTimeout(() => {
        clickTimer = null;
        this.focusState(region, state.name);
      }, SINGLE_CLICK_DELAY_MS);
    });
    el.addEventListener('dblclick', (event) => {
      event.stopPropagation();
      if (clickTimer != null) {
        window.clearTimeout(clickTimer);
        clickTimer = null;
      }
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

  /** Details of one state. The node itself is a single 36px row now, so everything beyond the
   *  name - the bound animation, its duration and how it repeats - lives here instead. */
  private showTooltip(stateEl: HTMLElement, state: SMStateData, regionName: string): void {
    const durationText = state.durationUs > 0 ? `${(state.durationUs / 1000000).toFixed(2)}s`
      : state.durationUs === 0 ? '0s' : 'unknown';
    const lines = [
      `Region: ${regionName}`,
      `State: ${state.name}`,
      state.animationId ? `Animation: ${state.animationId}` : 'Animation: (none)',
      `Duration: ${durationText}`,
    ];
    if (state.loop) {
      lines.push(`Loop: ${state.loop}`);
    }
    this.showTooltipAt(stateEl, lines.join('\n'));
  }

  /** Shows `text` in the tooltip, anchored to the right of `anchor`. The tooltip is mounted as a
   *  sibling of the panel (see attach), so it is positioned against its offsetParent - the player
   *  root - rather than the panel, and can overhang onto the canvas. Falls back to the panel rect
   *  when there is no positioned ancestor yet. */
  private showTooltipAt(anchor: Element, text: string): void {
    this.tooltip.textContent = text;
    this.tooltip.style.display = 'block';
    const hostRect = (this.tooltip.offsetParent as HTMLElement | null)?.getBoundingClientRect() ??
        this.root.getBoundingClientRect();
    const anchorRect = anchor.getBoundingClientRect();
    const tooltipLeft = Math.max(0, anchorRect.right - hostRect.left + 6);
    const tooltipTop = Math.max(0, anchorRect.top - hostRect.top - 6);
    this.tooltip.style.left = `${tooltipLeft}px`;
    this.tooltip.style.top = `${tooltipTop}px`;
  }

  private drawEdge(svg: SVGElement,
                  positions: Map<string, { x: number; y: number }>,
                  transition: SMTransitionData,
                  markerId: string,
                  slots: EdgeSlots,
                  grid: GridMetrics,
                  tracks: EdgeTracks): DrawnEdge | null {
    const fromPos = positions.get(transition.fromAny ? ANY_NODE_KEY : transition.from);
    const toPos = positions.get(transition.to);
    if (fromPos == null || toPos == null) {
      return null;
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
      // Forward edge. When source and target share a row the edge is a single straight horizontal
      // run (BFS bounds a forward edge to one column, so it crosses only the node-free gutter) and
      // its whole line carries the pair offset alone: a straight edge has ONE y, while the exit and
      // entry fans — stepped differently (10 vs 14) and computed against different groups — put its
      // two ends a couple of pixels apart, which the old 1px endpoint comparison read as "needs a
      // turn" and bent every fanned same-row edge at its midpoint. AssignVerticalTracks never
      // reserved a turn track for these edges, so that bend was generation-logic drift, not a
      // routed corner. Different rows: run along the source row, turn down/up inside the gutter on
      // a track no other edge occupies, then run along the target row into its left edge.
      const x1 = fromPos.x + NODE_W;
      const x2 = toPos.x;
      const y1 = fromPos.y + NODE_H / 2 + offset + (slots.straight ? 0 : sourceOffset);
      if (slots.straight) {
        path.setAttribute('d', `M ${x1} ${y1} L ${x2} ${y1}`);
        // Label rides the middle OF THE LINE, not above it: the backdrop is opaque, so the stroke
        // it masks shows up as two stubs of equal length leaving the label on either side — the
        // reading that makes "halfway along this edge" obvious.
        midX = (x1 + x2) / 2;
        midY = y1;
      } else {
        const y2 = toPos.y + NODE_H / 2 + offset + targetOffset;
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
        // Label sits at the middle of the WHOLE path, found by walking its accumulated length, so
        // it lands on whichever run holds the halfway point and the edge sticks out of the backdrop
        // by the same length on both ends. Pinning it to the source-side run (the previous shape)
        // put every label at the first quarter of its edge, next to the source node.
        const mid = OrthogonalMidpoint(x1, y1, turnX, y2, x2, sweep);
        midX = mid.x;
        midY = mid.y;
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
    let labelGroup: SVGGElement | null = null;
    if (transition.conditions && transition.conditions !== 'always') {
      // Label sits on top of the edge path; without a solid backdrop the line would visually
      // slice through the glyphs. Measure the label with a canvas 2D context (not getBBox,
      // which returns zeros when the panel is still display:none during refresh()) so the
      // backdrop is exactly the width the rendered text will occupy.
      let caption = transition.conditions;
      if (caption.length > 18) {
        caption = `${caption.slice(0, 17)}\u2026`;
      }
      const textWidth = this.measureLabelWidth(caption);
      const textHeight = 12;
      const paddingX = 6;
      const paddingY = 3;
      // The pill gets its own group so it can accept pointer events while the rest of the edge
      // layer stays click-through, and so the caller can bind the "drive this condition" click to
      // exactly the pill rather than to the whole edge.
      labelGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      labelGroup.setAttribute('class', 'sm-edge-label-group');
      const backdrop = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      backdrop.setAttribute('class', 'sm-edge-label-bg');
      backdrop.setAttribute('x', String(midX - textWidth / 2 - paddingX));
      backdrop.setAttribute('y', String(midY - textHeight / 2 - paddingY));
      backdrop.setAttribute('width', String(textWidth + paddingX * 2));
      backdrop.setAttribute('height', String(textHeight + paddingY * 2));
      backdrop.setAttribute('rx', '3');
      labelGroup.appendChild(backdrop);
      const captionEl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      captionEl.setAttribute('class', 'sm-edge-label');
      captionEl.setAttribute('x', String(midX));
      captionEl.setAttribute('y', String(midY));
      captionEl.textContent = caption;
      labelGroup.appendChild(captionEl);
      group.appendChild(labelGroup);
    }
    svg.appendChild(group);
    return { path, label: labelGroup };
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
      const previousName = this.currentStates[region.name] ?? '';
      const nextName = next[region.name] ?? '';
      if (previousName !== nextName) {
        changed = true;
        // The region just walked a transition, so flag that edge. The first observation of a
        // region reads as a move from "" - that is the initial state being reported, not a
        // transition, and pulseTransition skips it.
        this.pulseTransition(region, previousName, nextName);
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

  /** Builds the Inputs section from the declarations on the state machine. A document with no
   *  <Inputs> says so plainly instead of showing an empty list: nothing outside the document can
   *  move such a machine, and the graph alone cannot either. */
  private buildInputsList(inputs: { name: string; type: string }[]): void {
    for (const input of inputs) {
      this.inputTypes.set(input.name, input.type);
    }
    if (inputs.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'sm-input-empty';
      empty.textContent = 'This document declares no <Inputs>:\n' +
        'the state machine cannot be driven from outside.';
      this.inputList.appendChild(empty);
      return;
    }
    for (const input of inputs) {
      const row = this.buildInputRow(input);
      this.inputRowEls.set(input.name, row);
      this.inputList.appendChild(row);
    }
  }

  /** One input row: the name (with its declared type) plus the control matching that type. */
  private buildInputRow(input: { name: string; type: string }): HTMLElement {
    const row = document.createElement('div');
    row.className = 'sm-input-row';
    const nameEl = document.createElement('span');
    nameEl.className = 'sm-input-name';
    nameEl.textContent = input.name;
    const typeEl = document.createElement('span');
    typeEl.className = 'sm-input-type';
    typeEl.textContent = input.type;
    nameEl.appendChild(typeEl);
    row.appendChild(nameEl);
    if (input.type === 'bool') {
      row.appendChild(this.buildBoolSwitch(input.name));
    } else if (input.type === 'number') {
      row.appendChild(this.buildNumberField(input.name));
    } else {
      row.appendChild(this.buildFireButton(input.name));
    }
    return row;
  }

  /** T/F segmented switch for a bool input. Both glyphs stay visible so the value reads as one of
   *  two known states rather than as an on/off toggle's absence. */
  private buildBoolSwitch(name: string): HTMLElement {
    const sw = document.createElement('div');
    sw.className = 'sm-switch';
    const trueBtn = document.createElement('button');
    trueBtn.type = 'button';
    trueBtn.textContent = 'T';
    const falseBtn = document.createElement('button');
    falseBtn.type = 'button';
    falseBtn.textContent = 'F';
    const sync = (): void => {
      const on = this.inputValues.get(name) === true;
      trueBtn.classList.toggle('on', on);
      falseBtn.classList.toggle('on', !on);
    };
    trueBtn.addEventListener('click', () => {
      this.writeBoolInput(name, true);
      sync();
    });
    falseBtn.addEventListener('click', () => {
      this.writeBoolInput(name, false);
      sync();
    });
    sync();
    this.inputSyncFns.set(name, sync);
    sw.appendChild(trueBtn);
    sw.appendChild(falseBtn);
    return sw;
  }

  /** Plain number field. <Input> carries no min/max (only a ViewModel <Property> does), so a
   *  slider would have to invent a range - and threshold conditions are verified by typing exact
   *  boundaries (19 / 20 / 79 / 80), which a slider cannot express. */
  private buildNumberField(name: string): HTMLInputElement {
    const field = document.createElement('input');
    field.className = 'sm-number';
    field.type = 'number';
    field.step = '1';
    const sync = (): void => {
      field.value = String(this.inputValues.get(name) ?? 0);
    };
    field.addEventListener('input', () => {
      if (field.value === '') {
        return;
      }
      const value = parseFloat(field.value);
      if (Number.isFinite(value)) {
        this.writeNumberInput(name, value);
      }
    });
    sync();
    this.inputSyncFns.set(name, sync);
    return field;
  }

  private buildFireButton(name: string): HTMLElement {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'sm-fire';
    btn.textContent = 'Fire';
    btn.addEventListener('click', () => {
      this.fireInput(name);
    });
    return btn;
  }

  /** Writes one bool input, mirrors the value for the controls and lights the row up. */
  private writeBoolInput(name: string, value: boolean): void {
    this.host.ensureSMDriving();
    if (!this.host.setSMInputBool(name, value)) {
      return;
    }
    this.inputValues.set(name, value);
    this.highlightInputRow(name);
  }

  /** Number counterpart of writeBoolInput. */
  private writeNumberInput(name: string, value: number): void {
    this.host.ensureSMDriving();
    if (!this.host.setSMInputNumber(name, value)) {
      return;
    }
    this.inputValues.set(name, value);
    this.highlightInputRow(name);
  }

  /** Fires one trigger input. A trigger is consumed by the next advance, so the write itself is
   *  the whole interaction - there is no value to mirror. */
  private fireInput(name: string): void {
    this.host.ensureSMDriving();
    if (!this.host.fireSMInputTrigger(name)) {
      return;
    }
    this.highlightInputRow(name);
  }

  /** Marks the row an input was just written through, so "which parameter did that" stays obvious
   *  when the graph is what drove it. */
  private highlightInputRow(name: string): void {
    const row = this.inputRowEls.get(name);
    if (row == null) {
      return;
    }
    row.classList.remove('sm-row-flash');
    // Reading a layout property flushes the class removal so the flash restarts when the same row
    // is driven twice in a row.
    void row.offsetWidth;
    row.classList.add('sm-row-linked');
    row.classList.add('sm-row-flash');
    window.setTimeout(() => row.classList.remove('sm-row-linked'), HIGHLIGHT_DURATION_MS);
  }

  /** Drives every input named by a condition summary. Numeric groups are solved before anything
   *  is written, so an unsatisfiable condition writes nothing at all rather than leaving the
   *  machine in a state the user did not ask for. Returns whether anything was driven. */
  private driveClauses(clauses: ConditionClause[]): boolean {
    if (clauses.length === 0) {
      return false;
    }
    const byInput = new Map<string, ConditionClause[]>();
    for (const clause of clauses) {
      const list = byInput.get(clause.input) ?? [];
      list.push(clause);
      byInput.set(clause.input, list);
    }
    const numbers = new Map<string, number>();
    for (const [name, list] of byInput) {
      if (list[0].kind !== 'number') {
        continue;
      }
      const value = SampleNumberValue(list);
      if (value == null) {
        return false;
      }
      numbers.set(name, value);
    }
    // Resume the machine BEFORE writing: resuming restarts it from its initial states, which also
    // restores the declared input defaults, so a write that came first would be discarded.
    this.host.ensureSMDriving();
    for (const [name, list] of byInput) {
      const value = numbers.get(name);
      if (value != null) {
        if (!this.host.setSMInputNumber(name, value)) {
          return false;
        }
        this.inputValues.set(name, value);
      } else if (list[0].kind === 'bool') {
        if (!this.host.setSMInputBool(name, list[0].boolValue)) {
          return false;
        }
        this.inputValues.set(name, list[0].boolValue);
      } else if (!this.host.fireSMInputTrigger(name)) {
        return false;
      }
      const sync = this.inputSyncFns.get(name);
      if (sync != null) {
        sync();
      }
      this.highlightInputRow(name);
    }
    return true;
  }

  /** An edge's condition pill is a control, not a caption: clicking it writes the inputs that
   *  condition needs, so the machine walks that transition and its target animation plays. */
  private onConditionClick(transition: SMTransitionData): void {
    const clauses = ParseConditions(transition.conditions, this.inputTypes);
    if (clauses.length === 0) {
      this.showNote('always: this transition fires on its own, nothing to drive');
      return;
    }
    if (!this.driveClauses(clauses)) {
      this.showNote('no input value can satisfy this condition');
    }
  }

  /** Clicking a state drives its region into that state by writing the inputs its incoming
   *  transition needs. The graph and the Inputs section are two views of one machine, and this is
   *  what makes the graph itself a control. */
  private focusState(region: SMRegionData, stateName: string): void {
    if ((this.currentStates[region.name] ?? '') === stateName) {
      return;
    }
    for (const transition of region.transitions) {
      if (transition.to !== stateName) {
        continue;
      }
      if (this.driveClauses(ParseConditions(transition.conditions, this.inputTypes))) {
        return;
      }
    }
    this.showNote(`${stateName}: no input can drive into this state`);
  }

  /** Flags the edge a region just walked. The 250ms poll coalesces two fast hops into a single
   *  observation, in which case only the last edge is flagged - the flash is a hint, not an audit
   *  log. */
  private pulseTransition(region: SMRegionData, previous: string, next: string): void {
    if (previous === '' || next === '') {
      return;
    }
    const index = region.transitions.findIndex((transition) =>
      (transition.from === previous || transition.fromAny) && transition.to === next);
    if (index < 0) {
      return;
    }
    const path = this.edgePaths.get(region.name)?.get(EdgeKey(region.name, index));
    if (path == null) {
      return;
    }
    path.classList.remove('sm-edge-fire');
    void path.getBoundingClientRect();
    path.classList.add('sm-edge-fire');
    window.setTimeout(() => path.classList.remove('sm-edge-fire'), EDGE_FIRE_DURATION_MS);
  }

  /** Transient message in the panel header, which is always visible and otherwise only shows the
   *  machine's name. Used for the "nothing can drive this" cases, where a silent no-op would look
   *  like a bug. */
  private showNote(text: string): void {
    this.titleEl.textContent = text;
    if (this.noteTimer != null) {
      window.clearTimeout(this.noteTimer);
    }
    this.noteTimer = window.setTimeout(() => {
      this.noteTimer = null;
      this.titleEl.textContent = this.smTitle;
    }, NOTE_DURATION_MS);
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
