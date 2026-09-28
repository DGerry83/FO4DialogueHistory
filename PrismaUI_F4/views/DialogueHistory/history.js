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
 * Ultralight); the clear-confirm message names its scope. */
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

/* Header drag-move / grip drag-resize. PrismaUI delivers the full mouse
 * stream to the focused view and holds native capture during drags, so
 * document-level mousemove/mouseup keep firing outside the panel. The final
 * geometry is reported exactly once, on mouseup (never per mousemove). */
function dhStartDrag(mode) {
  return function (event) {
    if (event.button !== 0) {
      return;
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
  }
  document.addEventListener("mousemove", onMove);
  document.addEventListener("mouseup", onUp);
}

/* Right pane: re-render the selected bucket and snap to its newest line.
 * Batched (C6a): all matching rows are built into a DocumentFragment, then
 * one append + one scrollTop write force a single layout pass per render
 * instead of one synchronous reflow per row. C7: in the "All" view, lines
 * whose bucket is unchecked are skipped; a directly selected quest's view
 * is unaffected by the checkboxes (same precedent as search). */
function dhRenderLog() {
  const log = document.getElementById("log");
  const t0 = performance.now();
  let rendered = 0;
  const fragment = document.createDocumentFragment();
  for (const line of dhAllLines) {
    const inSelection = dhSelection === "all"
      ? dhBucketVisible(dhBucketFor(line.questType))
      : String(line.questId) === dhSelection;
    if (inSelection) {
      fragment.append(dhMakeRow(line));
      rendered += 1;
    }
  }
  log.textContent = "";
  log.append(fragment);
  log.scrollTop = log.scrollHeight;
  dhSetEmptyVisible(dhAllLines.length === 0);
  const t1 = performance.now();
  console.info("[DialogueHistory] render: " + rendered + " rows in " + (t1 - t0).toFixed(1) + " ms");
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
  // C7: mirror dhRenderLog's visibility — never append a hidden-bucket line
  // into the "All" view; a selected quest's view stays unaffected.
  const inSelection = dhSelection === "all"
    ? dhBucketVisible(dhBucketFor(line.questType))
    : String(line.questId) === dhSelection;
  if (inSelection) {
    const log = document.getElementById("log");
    const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
    dhSetEmptyVisible(false);
    log.append(dhMakeRow(line));
    if (atBottom) {
      log.scrollTop = log.scrollHeight;
    }
  }
}

function setFontSize(size) {
  document.getElementById("log").style.fontSize = Number(size) + "px";
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
