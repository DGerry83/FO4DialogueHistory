/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text, questId, questName,
 *                                     questType} (schema v3; C7 added the raw
 *                                      QUEST_TYPE integer — v2 DLLs omit it,
 *                                      v1 DLLs omit the quest fields too;
 *                                      defaults are applied below)
 *               setFontSize(px)
 *               setGeometry(json)   {"x","y","width","height"} — persisted
 *                                     panel rect; absent/invalid = keep the
 *                                     default centered layout
 * JS -> plugin: window.requestHistory() on DOM ready
 *               window.closeRequested() on Esc
 *               window.geometryChanged(json) once per drag end (M5)
 *               window.clearAllRequested("") / window.clearQuestRequested(
 *                 "<decimal questId>") on confirmed clear (C5)
 * (RegisterJSListener binds each name as a global window function;
 * window.prisma has no sendEvent — its emit() routes to Papyrus only.)
 * Two-pane model (M4): dhAllLines is the client-side source of truth (the
 * plugin only pushes; it never edits or removes). The left pane is a
 * projection grouped on questId — "All", then one entry per quest sorted by
 * most recent line, with "Unattributed" for questId-0 lines (only when such
 * lines exist). Old-DLL payloads without quest fields land in Unattributed.
 * Scroll contract: snap to bottom on open and on selection switch;
 * auto-scroll on append only when the user is already at the bottom.
 * M8: the pane separator (#split) drag-resizes the index pane (session-
 * only, no persistence); quest names in the index word-wrap.
 * C16: the panel starts CSS-hidden and is revealed by the first geometry
 * application (kills the first-open flash at the default layout while the
 * plugin's setGeometry InteropCall is still in flight); hover tooltips are
 * rendered in-page from title attributes (native title never renders under
 * Ultralight); the clear-confirm message names its scope.
 * C17: the log is block-windowed (virtualized). dhViewLines is the filtered
 * view; only the blocks intersecting the viewport (+1 block of overscan)
 * are mounted as .vblock divs between two spacer divs, so the mounted DOM
 * stays ~200 rows at any model size and panel resize reflows only those.
 * Unmounted blocks cost a running-average height estimate; scrolling,
 * panel resize / split drag, and font-size changes re-window or re-estimate
 * via dhUpdateWindow / dhInvalidateHeights (C17 section below). C18: while
 * a drag is active, dhDragActive gates both the scroll handler and the
 * width check — all re-windowing is deferred to one settle pass on mouseup
 * (the per-mousemove invalidation cascade made resize lag at any model
 * size). C19: at drag start the panes themselves are frozen — pinned to
 * their rendered px size, parents clipping the slack — because even one
 * engine-natural re-wrap of visible text per mousemove built a multi-
 * second event backlog; the mouseup settle re-anchors against true
 * geometry. */
"use strict";

function dhParse(value, fallback) {
  if (typeof value === "string") {
    try {
      return JSON.parse(value);
    } catch (e) {
      return fallback;
    }
  }
  return value;
}

function dhSetEmptyVisible(visible) {
  const empty = document.getElementById("empty");
  if (empty) {
    empty.style.display = visible ? "" : "none";
  }
}

/* Client-side model (M4): the plugin pushes lines and full snapshots; the
 * view owns grouping, selection, and search. dhSelection is "all",
 * "0" (unattributed), or the decimal questId string. */
const dhAllLines = [];
let dhSelection = "all";

/* Normalize one raw payload line into the model. Tolerant of old-DLL
 * payloads: missing quest fields default to 0/"", missing/non-finite
 * questType to 0 (Unattributed), missing speaker/text to "", unknown kind
 * to "unknown". Returns null for non-object input. */
function dhNormalizeLine(line) {
  line = dhParse(line, null);
  if (!line || typeof line !== "object") {
    return null;
  }
  return {
    speaker: typeof line.speaker === "string" ? line.speaker : "",
    kind: line.kind === "player" || line.kind === "npc" ? line.kind : "unknown",
    text: typeof line.text === "string" ? line.text : "",
    questId: typeof line.questId === "number" ? line.questId : 0,
    questName: typeof line.questName === "string" ? line.questName : "",
    questType: typeof line.questType === "number" && isFinite(line.questType) ? line.questType : 0
  };
}

/* C7 — bucket mapping (frozen in GATES.md), implemented ONLY here in the
 * view: raw 1-5 -> "main", 6 -> "misc", 7 -> "side", >=8 -> "side"
 * (DLC01-06), 0/missing/invalid -> "unattributed". */
function dhBucketFor(questType) {
  if (typeof questType !== "number" || !Number.isInteger(questType)) {
    return "unattributed";
  }
  if (questType >= 1 && questType <= 5) {
    return "main";
  }
  if (questType === 6) {
    return "misc";
  }
  if (questType >= 7) {
    return "side";
  }
  return "unattributed";
}

/* C7 — friendly per-type display names (never raw enum/DLC names). Values
 * above the known range get the generic "DLC"; 0/invalid -> Unattributed. */
const dhTypeNames = {
  0: "Unattributed",
  1: "Main Quest",
  2: "Brotherhood of Steel",
  3: "Institute",
  4: "Minutemen",
  5: "Railroad",
  6: "Miscellaneous",
  7: "Side Quests",
  8: "Automatron",
  9: "Wasteland Workshop",
  10: "Far Harbor",
  11: "Nuka-World",
  12: "Contraptions Workshop",
  13: "Vault-Tec Workshop"
};

function dhTypeName(questType) {
  if (typeof questType !== "number" || !Number.isInteger(questType) || questType < 0) {
    return dhTypeNames[0];
  }
  if (questType > 13) {
    return "DLC";
  }
  return dhTypeNames[questType];
}

/* C7 — bucket labels as shown on the index tags and filter checkboxes. */
const dhBucketLabels = {
  main: "Main",
  side: "Side",
  misc: "Misc",
  unattributed: "Unattributed"
};

/* C7 — checkbox ids per bucket. State lives in the DOM (session-only, no
 * persistence) and defaults to all-checked via the markup; a missing box
 * (old html) means the bucket stays visible. */
const dhBuckets = ["main", "side", "misc", "unattributed"];

function dhBucketVisible(bucket) {
  const box = document.getElementById("ftype-" + bucket);
  return !box || box.checked;
}

/* Checkbox change: re-filter the index; the log only when the "All" view is
 * up (a selected quest's view is unaffected by the checkboxes). */
function dhFilterChanged() {
  dhRenderIndex();
  if (dhSelection === "all") {
    dhRenderLog();
  }
}

/* Display label for a quest bucket: first non-empty questName seen, else the
 * formID as hex (a quest can lack both fullName and editorID). */
function dhQuestLabel(entry) {
  if (entry.name) {
    return entry.name;
  }
  return "Quest " + entry.questId.toString(16).toUpperCase().padStart(8, "0");
}

/* Group the model on questId. Returns one bucket per questId (including 0),
 * each {questId, key, name, count, last, type}, sorted by most recent line.
 * type = the entry's newest line with a numeric questType (0 when none) —
 * it drives both the entry's C7 bucket and its tooltip name. */
function dhBuildIndex() {
  const quests = new Map();
  for (let i = 0; i < dhAllLines.length; i++) {
    const line = dhAllLines[i];
    const key = String(line.questId);
    let entry = quests.get(key);
    if (!entry) {
      entry = { questId: line.questId, key: key, name: "", count: 0, last: i, type: 0 };
      quests.set(key, entry);
    }
    entry.count += 1;
    entry.last = i;
    if (!entry.name && line.questName) {
      entry.name = line.questName;
    }
    if (typeof line.questType === "number" && isFinite(line.questType)) {
      entry.type = line.questType;
    }
  }
  const entries = Array.from(quests.values());
  entries.sort(function (a, b) { return b.last - a.last; });
  return entries;
}

function dhMakeEntry(key, label, count, type) {
  const row = document.createElement("div");
  row.className = "qentry" + (key === dhSelection ? " active" : "");
  row.dataset.key = key;
  const name = document.createElement("span");
  name.className = "qname";
  name.textContent = label;
  row.append(name);
  if (type !== undefined) {  // real quest entries only — "All" gets no tag
    row.title = dhTypeName(type);
    const tag = document.createElement("span");
    tag.className = "qtype";
    tag.textContent = dhBucketLabels[dhBucketFor(type)];
    row.append(tag);
  }
  const num = document.createElement("span");
  num.className = "qcount";
  num.textContent = String(count);
  row.append(num);
  row.addEventListener("click", function () {
    dhSelection = key;
    dhRenderIndex();
    dhRenderLog();
  });
  return row;
}

/* Left pane: "All" plus the quest buckets, filtered by the search box on
 * label substring (case-insensitive) and by the C7 bucket checkboxes — an
 * entry hides while its bucket (its newest line's bucket) is unchecked; the
 * "All" entry is never hidden by a checkbox. Selection is not reset by a
 * search or a filter — the right pane keeps showing the selected bucket
 * while its entry is filtered out, and restoring the filter brings it back. */
function dhRenderIndex() {
  const list = document.getElementById("quests");
  if (!list) {
    return;
  }
  const search = document.getElementById("search");
  const term = search ? search.value.toLowerCase() : "";
  const matches = function (label) {
    return term === "" || label.toLowerCase().indexOf(term) !== -1;
  };

  list.textContent = "";
  if (matches("All")) {
    list.append(dhMakeEntry("all", "All", dhAllLines.length));
  }
  for (const entry of dhBuildIndex()) {
    if (!dhBucketVisible(dhBucketFor(entry.type))) {
      continue;
    }
    const label = entry.questId === 0 ? "Unattributed" : dhQuestLabel(entry);
    if (matches(label)) {
      list.append(dhMakeEntry(entry.key, label, entry.count, entry.type));
    }
  }
}

/* Build one log row for a model line: speaker span (per-NPC color on npc
 * kinds), text span, dataset fields — the exact markup the CSS expects.
 * Pure construction, no DOM insertion or scrolling; callers own both. */
function dhMakeRow(line) {
  const row = document.createElement("div");
  row.className = "line " + line.kind;
  row.dataset.questId = String(line.questId);
  row.dataset.questName = line.questName;
  const speaker = document.createElement("span");
  speaker.className = "speaker";
  speaker.textContent = line.speaker + ":";
  if (line.kind === "npc") {
    speaker.style.color = dhNameColor(String(line.speaker));
  }
  const text = document.createElement("span");
  text.className = "text";
  text.textContent = " " + line.text;
  row.append(speaker, text);
  return row;
}

/* C17 — block-windowed log rendering (view virtualization). The full
 * filtered view lives in dhViewLines, but the DOM only ever holds the
 * blocks intersecting the viewport plus one block of overscan on each
 * side, so render work and resize reflow stay O(screenful) no matter how
 * large the history grows. Blocks are fixed-count (kBlockRows) because
 * rows wrap to variable heights; unmounted blocks cost the running
 * average of measured block heights so the scrollbar geometry stays
 * truthful, and a block measured above the viewport silently adjusts
 * scrollTop by the estimate->measured delta so the visible content
 * never jumps. */
const kBlockRows = 50;
const kOverscanBlocks = 1;
const kInitialRowEst = 23;  // px/row guess: 16px font x 1.4 line-height (+pad)

/* C17 — windowing state. dhViewLines mirrors dhAllLines through the
 * current selection/bucket predicate; dhBlockHeights is parallel to the
 * block count (0 = not measured in this view); dhMounted maps block
 * index -> element. The spacers are reused empty divs whose heights are
 * the summed heights of the unmounted blocks on each side of the
 * mounted window. */
let dhViewLines = [];
let dhBlockHeights = [];
let dhBlockSum = 0;
let dhBlockMeasured = 0;
let dhEstSeed = 0;              // C18: last layout average, kept across invalidation
const dhMounted = new Map();
let dhTopSpacer = null;
let dhBottomSpacer = null;
let dhLogWidth = -1;            // last seen #log clientWidth (-1 = unknown)
let dhScrollSilencing = false;  // reentrancy guard for silent scrollTop fixes
let dhDragActive = false;       // C18: a panel or split drag is in progress
let dhPaneFreeze = null;        // C19: saved pane inline styles while frozen (null = not frozen)

/* C17 — the selection predicate, verbatim from the pre-virtualization
 * dhRenderLog/appendLine: "All" applies the C7 bucket checkboxes; a
 * directly selected quest is unaffected by them. Centralized so the view
 * rebuild and the append path can never disagree about membership. */
function dhLineInSelection(line) {
  return dhSelection === "all"
    ? dhBucketVisible(dhBucketFor(line.questType))
    : String(line.questId) === dhSelection;
}

/* C17 — rebuild the filtered view and reset the block state. Runs on
 * snapshot replay, selection click, and bucket-filter change (all funnel
 * through dhRenderLog); the running average resets here because the view
 * content changed — width/font invalidation keeps it (dhInvalidateHeights)
 * so re-anchoring never falls back to the crude initial guess. */
function dhRebuildView() {
  dhViewLines = [];
  for (const line of dhAllLines) {
    if (dhLineInSelection(line)) {
      dhViewLines.push(line);
    }
  }
  dhBlockHeights = new Array(Math.ceil(dhViewLines.length / kBlockRows));
  for (let b = 0; b < dhBlockHeights.length; b++) {
    dhBlockHeights[b] = 0;
  }
  dhBlockSum = 0;
  dhBlockMeasured = 0;
  dhEstSeed = 0;  // C18: the content changed — the saved average is meaningless
}

/* C17 — current block geometry: measured heights where known, the
 * running average elsewhere (before anything is measured: one block of
 * kInitialRowEst-high rows). Returns heights plus prefix offsets and the
 * total, i.e. the virtual content height the spacers simulate. */
function dhBlockLayout() {
  const n = dhBlockHeights.length;
  // C18 estimate fallback chain: fresh measured average -> the average saved
  // across invalidation (dhEstSeed) -> the crude initial per-row guess.
  let est = kBlockRows * kInitialRowEst;
  if (dhBlockMeasured > 0) {
    est = dhBlockSum / dhBlockMeasured;
  } else if (dhEstSeed > 0) {
    est = dhEstSeed;
  }
  const heights = new Array(n);
  for (let b = 0; b < n; b++) {
    heights[b] = dhBlockHeights[b] || est;
  }
  const offsets = new Array(n + 1);
  offsets[0] = 0;
  for (let b = 0; b < n; b++) {
    offsets[b + 1] = offsets[b] + heights[b];
  }
  return { est: est, heights: heights, offsets: offsets, total: offsets[n] };
}

/* C17 — the spacers are created once and re-appended around the mounted
 * blocks by dhRenderLog on every rebuild. */
function dhEnsureSpacers() {
  if (!dhTopSpacer) {
    dhTopSpacer = document.createElement("div");
    dhBottomSpacer = document.createElement("div");
  }
}

/* C17 — build one block's rows (the unchanged dhMakeRow markup) into a
 * plain wrapper and mount it; the caller owns ordering and measurement. */
function dhMountBlock(log, b) {
  const block = document.createElement("div");
  block.className = "vblock";
  const start = b * kBlockRows;
  const end = Math.min(start + kBlockRows, dhViewLines.length);
  const fragment = document.createDocumentFragment();
  for (let i = start; i < end; i++) {
    fragment.append(dhMakeRow(dhViewLines[i]));
  }
  block.append(fragment);
  log.append(block);
  dhMounted.set(b, block);
}

/* C17 — spacer heights = summed heights of the unmounted blocks on each
 * side of the mounted window. Writes are skipped when unchanged so
 * steady-state scroll handling touches neither style nor layout. */
function dhSetSpacerHeights(top, bottom) {
  if (!dhTopSpacer) {
    return;
  }
  const t = top + "px";
  const b = bottom + "px";
  if (dhTopSpacer.style.height !== t) {
    dhTopSpacer.style.height = t;
  }
  if (dhBottomSpacer.style.height !== b) {
    dhBottomSpacer.style.height = b;
  }
}

/* C17 — recompute the spacers from the current layout + mounted set
 * (used by the append paths, which mutate a mounted block or extend the
 * block list without going through dhUpdateWindow). */
function dhRefreshSpacers() {
  if (dhMounted.size === 0 || dhBlockHeights.length === 0) {
    dhSetSpacerHeights(0, 0);
    return;
  }
  let first = dhBlockHeights.length;
  let last = -1;
  for (const b of dhMounted.keys()) {
    if (b < first) {
      first = b;
    }
    if (b > last) {
      last = b;
    }
  }
  const layout = dhBlockLayout();
  dhSetSpacerHeights(layout.offsets[first],
    layout.total - layout.offsets[last] - layout.heights[last]);
}

/* C17 — measure mounted blocks whose height is still unknown, in
 * ascending order. A block that sits fully above the entry viewport
 * (judged with the pre-measurement layout, which is what the DOM was
 * built against) contributes its estimate->measured delta; the caller
 * applies the sum to scrollTop so the visible content stays put. */
function dhMeasureMountedBlocks(first, last, entryScrollTop, layout) {
  let delta = 0;
  for (let b = first; b <= last; b++) {
    if (dhBlockHeights[b] !== 0) {
      continue;
    }
    const el = dhMounted.get(b);
    if (!el) {
      continue;
    }
    const h = el.offsetHeight;
    dhBlockHeights[b] = h;
    dhBlockSum += h;
    dhBlockMeasured += 1;
    if (layout.offsets[b] + layout.heights[b] <= entryScrollTop) {
      delta += h - layout.heights[b];
    }
  }
  return delta;
}

/* C17 — scrollTop write under the reentrancy guard: if the engine
 * dispatches the scroll event synchronously from the setter, the
 * in-flight window pass finishes before another one starts. */
function dhSetScrollSilently(log, value) {
  dhScrollSilencing = true;
  log.scrollTop = value;
  dhScrollSilencing = false;
}

/* C17 — mount the blocks intersecting the viewport +/- one overscan
 * block and drop the rest. Runs on every #log scroll; in steady state
 * the window barely moves, so a pass measures at most the couple of
 * blocks that just entered. Newly measured blocks above the viewport
 * silently shift scrollTop (reentrancy-guarded) so nothing visible
 * moves; the spacers are then refreshed against the new measurements. */
function dhUpdateWindow() {
  if (dhScrollSilencing) {
    return;
  }
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const n = dhBlockHeights.length;
  if (n === 0) {
    for (const el of dhMounted.values()) {
      el.remove();
    }
    dhMounted.clear();
    dhSetSpacerHeights(0, 0);
    return;
  }
  const layout = dhBlockLayout();
  const entryScroll = log.scrollTop;
  const overscanPx = kOverscanBlocks * layout.est;
  const winTop = entryScroll - overscanPx;
  const winBottom = entryScroll + log.clientHeight + overscanPx;
  let first = 0;
  while (first < n - 1 && layout.offsets[first] + layout.heights[first] <= winTop) {
    first++;
  }
  let last = first;
  while (last < n - 1 && layout.offsets[last + 1] < winBottom) {
    last++;
  }
  let dirty = false;
  for (let b = first; b <= last; b++) {
    if (!dhMounted.has(b)) {
      dhMountBlock(log, b);
      dirty = true;
    }
  }
  const strays = [];
  for (const pair of dhMounted) {
    if (pair[0] < first || pair[0] > last) {
      strays.push(pair[0]);
    }
  }
  for (const b of strays) {
    dhMounted.get(b).remove();
    dhMounted.delete(b);
    dirty = true;
  }
  if (dirty) {
    // Re-append ascending: append() on a live child moves it, so one pass
    // fixes the order and parks the bottom spacer last (topSpacer is
    // never moved and stays first).
    for (let b = first; b <= last; b++) {
      log.append(dhMounted.get(b));
    }
    log.append(dhBottomSpacer);
  }
  const delta = dhMeasureMountedBlocks(first, last, entryScroll, layout);
  const now = dhBlockLayout();
  dhSetSpacerHeights(now.offsets[first],
    now.total - now.offsets[last] - now.heights[last]);
  if (delta !== 0) {
    dhSetScrollSilently(log, entryScroll + delta);
  }
}

/* C18 — scroll-event gate: the engine fires #log scroll events during a
 * drag too (clientHeight changes as the panel resizes), and each one
 * would otherwise run a full window pass mid-drag. Forward only when no
 * drag is active; the mouseup settle re-windows once. */
function dhOnScroll() {
  if (!dhDragActive) {
    dhUpdateWindow();
  }
}

/* C17 — width or font size changed, so every measured height is stale.
 * Re-anchor on the block visible at the top edge: remember it, drop the
 * per-block heights (the current average is stashed in dhEstSeed first —
 * re-anchoring on the raw initial guess would teleport deep scroll
 * positions by anchorBlocks x (avgReal - fallback)), re-window (which
 * re-measures the mounted blocks and silently corrects), then re-anchor
 * once more against the refreshed estimates so the same line really sits
 * at the top. C18: the sum/count reset with the heights so re-measurement
 * starts a fresh average (the old survival of the running sum
 * double-counted), and this runs ONCE per drag at mouseup — the old
 * per-mousemove invalidation forced a ~6-10-reflow layout cascade every
 * frame and made panel resize lag at any model size. */
function dhInvalidateHeights() {
  const log = document.getElementById("log");
  if (!log || dhBlockHeights.length === 0) {
    return;
  }
  const before = dhBlockLayout();
  const entryScroll = log.scrollTop;
  let anchor = 0;
  while (anchor < before.heights.length - 1
      && before.offsets[anchor] + before.heights[anchor] <= entryScroll) {
    anchor++;
  }
  dhEstSeed = before.est;
  for (let b = 0; b < dhBlockHeights.length; b++) {
    dhBlockHeights[b] = 0;
  }
  dhBlockSum = 0;
  dhBlockMeasured = 0;
  dhSetScrollSilently(log, anchor * before.est);
  dhUpdateWindow();
  const now = dhBlockLayout();
  const target = now.offsets[anchor];
  if (Math.abs(target - log.scrollTop) >= 1) {
    dhSetScrollSilently(log, target);
    dhUpdateWindow();
  }
}

/* C17 — width-driven invalidation hook: dhApplyGeometry calls here on
 * setGeometry (C18: it skips the read while a drag is active — the drags
 * re-check once on mouseup instead), and the M8 split drag calls here on
 * mouseup. Only an actual #log clientWidth change re-anchors, and the
 * first sighting just seeds the baseline. */
function dhCheckLogWidth() {
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const w = log.clientWidth;
  if (dhLogWidth === -1) {
    dhLogWidth = w;
    return;
  }
  if (w !== dhLogWidth) {
    dhLogWidth = w;
    dhInvalidateHeights();
  }
}

/* M5 — movable/resizable panel (geometry persistence is the plugin's job:
 * JS reports the final rect ONCE per drag end via geometryChanged; the plugin
 * pushes setGeometry back when the panel opens). Clamping is duplicated on
 * both sides — each side clamps what it owns at the time. */
const dhMinPanelWidth = 320;
const dhMinPanelHeight = 200;

function dhViewportSize() {
  return {
    width: window.innerWidth || document.documentElement.clientWidth,
    height: window.innerHeight || document.documentElement.clientHeight
  };
}

function dhClampGeometry(geo) {
  const view = dhViewportSize();
  const width = Math.max(dhMinPanelWidth, Math.round(geo.width));
  const height = Math.max(dhMinPanelHeight, Math.round(geo.height));
  const x = Math.min(Math.max(0, Math.round(geo.x)), Math.max(0, view.width - width));
  const y = Math.min(Math.max(0, Math.round(geo.y)), Math.max(0, view.height - height));
  return { x: x, y: y, width: width, height: height };
}

/* C16 — first-open flash fix: the plugin's setGeometry arrives via
 * InteropCall a frame or two after the first paint, so the panel is
 * CSS-hidden (visibility) until geometry is applied. dhRevealPanel is also
 * the fallback for first-ever runs with no saved geometry (default centered
 * layout), fired on a timer from DOMContentLoaded. */
function dhRevealPanel() {
  const panel = document.getElementById("panel");
  if (panel) {
    panel.style.visibility = "visible";
  }
}

/* Switch from margin-centering to absolute positioning at the given rect. */
function dhApplyGeometry(geo) {
  const panel = document.getElementById("panel");
  geo = dhClampGeometry(geo);
  panel.style.position = "absolute";
  panel.style.margin = "0";
  panel.style.left = geo.x + "px";
  panel.style.top = geo.y + "px";
  panel.style.width = geo.width + "px";
  panel.style.height = geo.height + "px";
  dhRevealPanel();
  if (!dhDragActive) {
    // C18: while a drag is active the width is re-checked once on mouseup
    // (the drags' onUp) — a clientWidth read here ran per mousemove and
    // forced a reflow every frame.
    dhCheckLogWidth();
  }
}

/* Plugin -> JS: apply persisted geometry on open. Absent/invalid payloads
 * leave the default centered layout untouched. */
function setGeometry(json) {
  const geo = dhParse(json, null);
  if (!geo || typeof geo !== "object") {
    return;
  }
  const x = Number(geo.x);
  const y = Number(geo.y);
  const width = Number(geo.width);
  const height = Number(geo.height);
  if (!isFinite(x) || !isFinite(y) || !isFinite(width) || !isFinite(height) || width <= 0 || height <= 0) {
    return;
  }
  dhApplyGeometry({ x: x, y: y, width: width, height: height });
}

function dhPanelRect() {
  const panel = document.getElementById("panel");
  return { x: panel.offsetLeft, y: panel.offsetTop, width: panel.offsetWidth, height: panel.offsetHeight };
}

/* C19 — freeze pane geometry during drags. Ultralight re-layouts every
 * visible wrapping text line whenever a pane's width changes — mandatory
 * engine work, not gateable JS — and even that one natural reflow per
 * mousemove piled up a multi-second event backlog (INVESTIGATION_LOG.md,
 * C19 addendum). So at drag start each requested pane is pinned to its
 * exact rendered size (one clientWidth/clientHeight read per pane, at
 * freeze time only) and the parent's overflow clips the slack: the text
 * stays visible and perfectly static while the panel chrome resizes, and
 * the one settle pass on mouseup re-anchors against true geometry. User
 * ruling: NO hiding of any kind — the panes stay rendered at every moment.
 * Idempotent: the saved-state record doubles as the frozen flag, so a
 * second freeze is a no-op. */
function dhFreezePanes(pinLog, pinIndex) {
  if (dhPaneFreeze) {
    return;  // already frozen — never clobber the saved values
  }
  const jobs = [];
  if (pinLog) {
    jobs.push([document.getElementById("log"), document.getElementById("dialogue")]);
  }
  if (pinIndex) {
    jobs.push([document.getElementById("index"), document.getElementById("body")]);
  }
  const saved = [];
  for (const job of jobs) {
    const pane = job[0];
    const parent = job[1];
    if (!pane) {
      continue;
    }
    saved.push({
      pane: pane,
      parent: parent,
      width: pane.style.width,
      height: pane.style.height,
      flex: pane.style.flex,
      boxSizing: pane.style.boxSizing,
      overflow: parent ? parent.style.overflow : ""
    });
    /* Read the rendered size BEFORE touching anything, then pin under
     * border-box so the pinned width/height equal the pre-freeze
     * clientWidth/clientHeight exactly — history.css sets no box-sizing,
     * so under the content-box default a clientWidth-sized write would
     * grow the pane by its padding (#log carries 8px/10px). */
    const pinW = pane.clientWidth + "px";
    const pinH = pane.clientHeight + "px";
    pane.style.boxSizing = "border-box";
    pane.style.width = pinW;
    pane.style.height = pinH;
    pane.style.flex = "none";
    if (parent) {
      parent.style.overflow = "hidden";
    }
  }
  if (saved.length > 0) {
    dhPaneFreeze = saved;
  }
}

/* C19 — restore every inline value dhFreezePanes saved, verbatim (an empty
 * string hands the property back to the CSS defaults), and drop the record.
 * Runs FIRST in every drag onUp — before the flag clears and before the C18
 * settle — so the settle measures true post-drag geometry. Calling it with
 * nothing frozen (every move drag) is a deliberate no-op. */
function dhUnfreezePanes() {
  if (!dhPaneFreeze) {
    return;
  }
  for (const rec of dhPaneFreeze) {
    rec.pane.style.width = rec.width;
    rec.pane.style.height = rec.height;
    rec.pane.style.flex = rec.flex;
    rec.pane.style.boxSizing = rec.boxSizing;
    if (rec.parent) {
      rec.parent.style.overflow = rec.overflow;
    }
  }
  dhPaneFreeze = null;
}

/* Header drag-move / grip drag-resize. PrismaUI delivers the full mouse
 * stream to the focused view and holds native capture during drags, so
 * document-level mousemove/mouseup keep firing outside the panel. The final
 * geometry is reported exactly once, on mouseup (never per mousemove). */
function dhStartDrag(mode) {
  return function (event) {
    if (event.button !== 0) {
      return;
    }
    dhDragActive = true;  // C18: gate the width check + window passes
    if (mode === "resize") {
      dhFreezePanes(true, true);  // C19: freeze both panes (move: nothing to pin)
    }
    const start = dhPanelRect();
    const origin = { x: event.clientX, y: event.clientY };
    dhApplyGeometry(start);  // absolute at the current resolved spot, no jump
    event.preventDefault();

    function onMove(moveEvent) {
      const dx = moveEvent.clientX - origin.x;
      const dy = moveEvent.clientY - origin.y;
      if (mode === "move") {
        dhApplyGeometry({ x: start.x + dx, y: start.y + dy, width: start.width, height: start.height });
      } else {
        dhApplyGeometry({ x: start.x, y: start.y, width: start.width + dx, height: start.height + dy });
      }
    }
    function onUp() {
      document.removeEventListener("mousemove", onMove);
      document.removeEventListener("mouseup", onUp);
      dhUnfreezePanes();  // C19: restore the frozen panes FIRST (move: no-op)
      dhDragActive = false;
      if (mode === "resize") {
        // C18 settle: re-check the width (invalidates iff it changed) and
        // re-window once — the unconditional pass also covers clientHeight-
        // only resizes.
        dhCheckLogWidth();
        dhUpdateWindow();
      }
      const rect = dhPanelRect();
      const payload = JSON.stringify({ x: rect.x, y: rect.y, width: rect.width, height: rect.height });
      const fn = window.geometryChanged;
      if (typeof fn === "function") {  // registered by the plugin (new DLLs)
        fn(payload);
      }
    }
    document.addEventListener("mousemove", onMove);
    document.addEventListener("mouseup", onUp);
  };
}

/* M8 — draggable pane separator. Session-only (not persisted). Same
 * no-jump pattern as the panel drags: the index pane's CSS defaults
 * (32% width, 320px max) are pinned to the current rendered px width at
 * mousedown, then the mouse is followed with per-move clamps so the
 * dialogue pane always keeps room. */
const dhMinIndexWidth = 140;
const dhMinDialogueWidth = 160;

function dhStartSplitDrag(event) {
  if (event.button !== 0) {
    return;
  }
  const index = document.getElementById("index");
  const body = document.getElementById("body");
  if (!index || !body) {
    return;
  }
  dhDragActive = true;  // C18: gate the width check + window passes
  dhFreezePanes(true, false);  // C19: freeze the log; the index is the pane being sized
  const startWidth = index.offsetWidth;
  const originX = event.clientX;
  index.style.maxWidth = "none";  // explicit px width takes over from CSS
  index.style.width = startWidth + "px";
  event.preventDefault();

  function onMove(moveEvent) {
    const maxWidth = Math.max(dhMinIndexWidth, body.clientWidth - dhMinDialogueWidth);
    const width = Math.min(Math.max(dhMinIndexWidth, startWidth + moveEvent.clientX - originX), maxWidth);
    index.style.width = Math.round(width) + "px";
  }
  function onUp() {
    document.removeEventListener("mousemove", onMove);
    document.removeEventListener("mouseup", onUp);
    dhUnfreezePanes();  // C19: restore the frozen log FIRST
    dhDragActive = false;
    dhCheckLogWidth();  // C17: the split drag moved the log's width edge
    dhUpdateWindow();   // C18 settle, same as the grip resize
  }
  document.addEventListener("mousemove", onMove);
  document.addEventListener("mouseup", onUp);
}

/* Right pane: re-render the selected bucket and snap to its newest line.
 * C17: rebuilt as a block window — dhRebuildView re-filters the model,
 * the final blocks are mounted and measured, and the spacers stand in
 * for the unmounted rest; scrollTop = scrollHeight then snaps to the
 * true bottom exactly like the old full render. Batched (C6a): each
 * block builds its rows into a DocumentFragment, so a rebuild forces
 * one layout pass per measurement round, not one reflow per row.
 * C7: in the "All" view, lines whose bucket is unchecked are skipped; a
 * directly selected quest's view is unaffected by the checkboxes (same
 * precedent as search). */
function dhRenderLog() {
  const log = document.getElementById("log");
  const t0 = performance.now();
  dhRebuildView();
  log.textContent = "";
  dhMounted.clear();
  dhEnsureSpacers();
  log.append(dhTopSpacer);
  const n = dhBlockHeights.length;
  const startBlock = Math.max(0, n - (2 * kOverscanBlocks + 1));
  let mountedRows = 0;
  for (let b = startBlock; b < n; b++) {
    dhMountBlock(log, b);
    mountedRows += Math.min(kBlockRows, dhViewLines.length - b * kBlockRows);
  }
  log.append(dhBottomSpacer);
  if (n > 0) {
    // Measure before the snap: scrollTop is 0 on the cleared container,
    // so no above-viewport correction can fire and the snap lands on
    // truthful geometry (the last block is always measured here).
    dhMeasureMountedBlocks(startBlock, n - 1, 0, dhBlockLayout());
    const layout = dhBlockLayout();
    dhSetSpacerHeights(layout.offsets[startBlock],
      layout.total - layout.offsets[n - 1] - layout.heights[n - 1]);
  } else {
    dhSetSpacerHeights(0, 0);
  }
  log.scrollTop = log.scrollHeight;
  dhSetEmptyVisible(dhAllLines.length === 0);
  const t1 = performance.now();
  console.info("[DialogueHistory] render: view " + dhViewLines.length
    + " lines, mounted " + mountedRows + " rows in " + (t1 - t0).toFixed(1) + " ms");
}

function setHistory(lines) {
  lines = dhParse(lines, []);
  if (!Array.isArray(lines)) {
    lines = [];
  }
  // Full snapshot replay (sent on panel open): reset to the defaults —
  // "All" selected, search cleared, all C7 bucket filters re-checked
  // (filter state is session-only — accepted post-replay UX) — then rebuild
  // model and panes.
  dhAllLines.length = 0;
  dhSelection = "all";
  const search = document.getElementById("search");
  if (search) {
    search.value = "";
  }
  for (const bucket of dhBuckets) {
    const box = document.getElementById("ftype-" + bucket);
    if (box) {
      box.checked = true;
    }
  }
  for (const raw of lines) {
    const line = dhNormalizeLine(raw);
    if (line) {
      dhAllLines.push(line);
    }
  }
  dhRenderIndex();
  dhRenderLog();
}

/* Per-NPC speaker colors: golden-angle assignment. Each distinct NPC name
 * gets the next hue ~137.5° from the previous one — maximally separated,
 * so two NPCs in the same conversation can't collide the way raw name
 * hashes could. The name→hue map is stable for the session and rebuilt in
 * line order on snapshot replay, so a name keeps its color while the
 * history lives. Player lines keep the CSS gold; other kinds keep green. */
const dhSpeakerHues = new Map();
let dhNextHue = 90;  // start clear of the player gold (~45)

function dhNameColor(name) {
  let hue = dhSpeakerHues.get(name);
  if (hue === undefined) {
    hue = dhNextHue % 360;
    dhNextHue += 137.508;  // golden angle — consecutive picks stay far apart
    if (hue >= 15 && hue <= 75) {
      hue = (hue + 120) % 360;  // keep clear of the player gold (~45)
    }
    dhSpeakerHues.set(name, hue);
  }
  return "hsl(" + hue + ", 70%, 65%)";
}

function appendLine(line) {
  line = dhNormalizeLine(line);
  if (!line) {
    return;
  }
  dhAllLines.push(line);
  dhRenderIndex();  // cheap (bounded by the buffer size): recount + resort
  // C7: mirror the view predicate — never append a hidden-bucket line
  // into the "All" view; a selected quest's view stays unaffected.
  if (dhLineInSelection(line)) {
    const log = document.getElementById("log");
    // Scroll contract: the at-bottom test runs BEFORE the DOM grows.
    const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
    dhViewLines.push(line);
    dhSetEmptyVisible(false);
    const prevBlocks = dhBlockHeights.length;
    const newBlocks = Math.ceil(dhViewLines.length / kBlockRows);
    if (newBlocks > prevBlocks) {
      dhBlockHeights.push(0);  // the boundary row starts a new block
    }
    const last = newBlocks - 1;
    if (newBlocks > prevBlocks) {
      // Boundary crossed: mount the fresh block only when the previous
      // tail block is mounted (i.e. the user is at the bottom); while
      // scrolled up there is no row DOM work — the taller bottom spacer
      // keeps the scrollbar truthful.
      if (prevBlocks === 0 || dhMounted.has(prevBlocks - 1)) {
        dhMountBlock(log, last);
        log.append(dhBottomSpacer);  // keep the spacer the last child
        const el = dhMounted.get(last);
        const h = el.offsetHeight;
        if (dhBlockHeights[last] !== 0) {
          dhBlockSum += h - dhBlockHeights[last];
        } else {
          dhBlockSum += h;
          dhBlockMeasured += 1;
        }
        dhBlockHeights[last] = h;
      }
      dhRefreshSpacers();
    } else if (dhMounted.has(last)) {
      const el = dhMounted.get(last);
      el.append(dhMakeRow(line));
      const h = el.offsetHeight;  // the block grew by one row
      if (dhBlockHeights[last] !== 0) {
        dhBlockSum += h - dhBlockHeights[last];
      } else {
        dhBlockSum += h;
        dhBlockMeasured += 1;
      }
      dhBlockHeights[last] = h;
      dhRefreshSpacers();
    }
    // else: last block not mounted — state and spacers only.
    if (atBottom) {
      log.scrollTop = log.scrollHeight;
    }
  }
}

function setFontSize(size) {
  document.getElementById("log").style.fontSize = Number(size) + "px";
  dhInvalidateHeights();  // C17: row heights changed — re-estimate + re-anchor
}

function dhSend(eventName) {
  const fn = window[eventName];
  if (typeof fn === "function") {
    fn("");
  } else {
    console.warn("[DialogueHistory] listener not registered: " + eventName);
  }
}

/* Clear history (C5): one selection-scoped path. The Clear button opens a
 * confirmation overlay naming the scope; Confirm dispatches to the plugin,
 * Cancel/Esc hides the overlay without touching the model (Esc must NOT
 * fall through to closeRequested while the overlay is up). After the
 * plugin mutates the buffer it re-pushes a snapshot; setHistory's reset
 * (selection -> "All", search cleared) is the accepted post-clear UX. */
let dhClearTarget = "all";  // "all" or the decimal questId string

/* Display name for a quest-scope clear target (only called when
 * dhClearTarget is a questId, never "all"). */
function dhClearScopeText() {
  for (const entry of dhBuildIndex()) {
    if (entry.key === dhClearTarget) {
      return entry.questId === 0 ? "Unattributed" : dhQuestLabel(entry);
    }
  }
  return "the selected quest";
}

function dhOverlayVisible() {
  const overlay = document.getElementById("overlay");
  return !!overlay && !overlay.hidden;
}

function dhShowClearOverlay() {
  dhClearTarget = dhSelection;
  const msg = document.getElementById("overlaymsg");
  if (msg) {
    // C16: the message names its scope. Built from text nodes (never
    // innerHTML) so a quest name can't inject markup.
    msg.textContent = "";
    if (dhClearTarget === "all") {
      const all = document.createElement("span");
      all.className = "clearall";
      all.textContent = "ALL";
      msg.append(document.createTextNode("This will clear "), all,
        document.createTextNode(" dialogue history. Are you sure?"));
    } else {
      const name = document.createElement("span");
      name.className = "clearscope";
      name.textContent = dhClearScopeText();
      msg.append(document.createTextNode("This will clear all dialogue history for "), name,
        document.createTextNode(". Are you sure?"));
    }
  }
  const overlay = document.getElementById("overlay");
  if (overlay) {
    overlay.hidden = false;
  }
}

function dhHideClearOverlay() {
  const overlay = document.getElementById("overlay");
  if (overlay) {
    overlay.hidden = true;
  }
}

function dhConfirmClear() {
  dhHideClearOverlay();
  if (dhClearTarget === "all") {
    const fn = window.clearAllRequested;
    if (typeof fn === "function") {  // registered by the plugin (new DLLs)
      fn("");
    }
  } else {
    const fn = window.clearQuestRequested;
    if (typeof fn === "function") {
      fn(String(dhClearTarget));
    }
  }
}

/* C16 — in-page tooltips. Ultralight/PrismaUI has no OS tooltip surface, so
 * native `title` never renders; on first hover the text is moved to
 * data-tip (attribute stripped, so a future native implementation can't
 * double-show) and mirrored into #dhtip at the cursor. Delegated at the
 * document level, so re-rendered quest rows need no wiring. parentNode walk
 * instead of closest(): guaranteed to exist under Ultralight's JSC. */
let dhTipEl = null;

function dhTipFor(node) {
  while (node && node !== document) {
    if (node.getAttribute) {
      const tip = node.getAttribute("title") || node.getAttribute("data-tip");
      if (tip) {
        return { el: node, tip: tip };
      }
    }
    node = node.parentNode;
  }
  return null;
}

function dhTipPlace(x, y) {
  if (!dhTipEl) {
    return;
  }
  const view = dhViewportSize();
  dhTipEl.style.left = Math.min(x + 14, Math.max(0, view.width - dhTipEl.offsetWidth - 4)) + "px";
  dhTipEl.style.top = Math.min(y + 18, Math.max(0, view.height - dhTipEl.offsetHeight - 4)) + "px";
}

function dhTipHide() {
  if (dhTipEl) {
    dhTipEl.hidden = true;
  }
}

function dhTipHover(event) {
  if (!dhTipEl) {
    return;
  }
  const found = dhTipFor(event.target);
  if (!found) {
    dhTipHide();
    return;
  }
  if (found.el.hasAttribute("title")) {
    found.el.setAttribute("data-tip", found.tip);
    found.el.removeAttribute("title");
  }
  dhTipEl.textContent = found.tip;
  dhTipEl.hidden = false;
  dhTipPlace(event.clientX, event.clientY);
}

document.addEventListener("DOMContentLoaded", () => {
  dhSend("requestHistory");
  setTimeout(dhRevealPanel, 250);  // C16 fallback: no saved geometry
  dhTipEl = document.getElementById("dhtip");
  document.addEventListener("mouseover", dhTipHover);
  document.addEventListener("mousemove", function (event) {
    if (dhTipEl && !dhTipEl.hidden) {
      dhTipPlace(event.clientX, event.clientY);
    }
  });
  document.addEventListener("mouseleave", dhTipHide);
  const search = document.getElementById("search");
  if (search) {
    search.addEventListener("input", dhRenderIndex);
  }
  for (const bucket of dhBuckets) {
    const box = document.getElementById("ftype-" + bucket);
    if (box) {
      box.addEventListener("change", dhFilterChanged);
    }
  }
  const header = document.getElementById("header");
  if (header) {
    header.addEventListener("mousedown", dhStartDrag("move"));
  }
  const grip = document.getElementById("grip");
  if (grip) {
    grip.addEventListener("mousedown", dhStartDrag("resize"));
  }
  const split = document.getElementById("split");
  if (split) {
    split.addEventListener("mousedown", dhStartSplitDrag);
  }
  const log = document.getElementById("log");
  if (log) {
    log.addEventListener("scroll", dhOnScroll);  // C17 windowing, C18 drag-gated
  }
  const clearBtn = document.getElementById("clear");
  if (clearBtn) {
    // The header is the panel drag handle: keep this mousedown from
    // bubbling into dhStartDrag ("move"), which would also fire a
    // spurious geometryChanged on mouseup.
    clearBtn.addEventListener("mousedown", function (event) {
      event.stopPropagation();
    });
    clearBtn.addEventListener("click", dhShowClearOverlay);
  }
  const overlayConfirm = document.getElementById("overlayconfirm");
  if (overlayConfirm) {
    overlayConfirm.addEventListener("click", dhConfirmClear);
  }
  const overlayCancel = document.getElementById("overlaycancel");
  if (overlayCancel) {
    overlayCancel.addEventListener("click", dhHideClearOverlay);
  }
  document.addEventListener("keydown", (event) => {
    // Ultralight reports Escape as "Unidentified" — match by keyCode.
    if (event.key === "Escape" || event.keyCode === 27) {
      if (dhOverlayVisible()) {
        dhHideClearOverlay();  // dismiss only; the panel stays open
        return;
      }
      dhSend("closeRequested");
    }
  });
});
