/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text, questId, questName,
 *                                     questType} (schema v3; C7 added the raw
 *                                      QUEST_TYPE integer — v2 DLLs omit it,
 *                                      v1 DLLs omit the quest fields too;
 *                                      defaults are applied below)
 *               setFontSize(px)
 *               setViewPreferences(json) {"fontSize","pauseOnOpen"} —
 *                                     persisted header preferences (font
 *                                     slider / numeric override / pause
 *                                     checkbox); pushed before every
 *                                     snapshot replay; absent = 16 px /
 *                                     unpaused defaults
 *               setGeometry(json)   {"x","y","width","height"} — persisted
 *                                     panel rect; absent/invalid = keep the
 *                                     default centered layout
 *               setFilters(json)    {"main","side","misc","unattributed"} —
 *                                     persisted bucket checkboxes (#001);
 *                                     absent = all-checked default
 * JS -> plugin: window.requestHistory() on DOM ready
 *               window.closeRequested() on Esc
 *               window.geometryChanged(json) once per drag end (M5)
 *               window.filtersChanged(json) once per checkbox change (#001)
 *               window.clearAllRequested("") / window.clearQuestRequested(
 *                 "<decimal questId>") / window.clearQuestsRequested(
 *                 "<decimal questId,decimal questId,...>") on confirmed
 *                 clear (C5, #003)
 *               window.typingStateChanged("1"/"0") when a text box
 *                 (#search/#bodysearch/#fontnum) gains/loses focus (#005: the plugin
 *                 consumes keyboard ButtonEvents while typing)
 *               window.viewPreferencesChanged(json) once per completed
 *                 preferences commit — slider change, numeric Enter/blur,
 *                 pause checkbox toggle — {"fontSize":int,"pauseOnOpen":
 *                 bool} (2026-10-24 header controls)
 * Keyboard surface (C6): one document-level keydown handler — Esc clears
 * a focused non-empty search box first, leaves a focused empty box
 * (#007: search -> filters, bodysearch -> log), else closes (overlay up:
 * dismiss only); arrows/Enter/Space/PageUp-Down drive a roving focus
 * model (zones
 * search -> filters -> quests -> bodysearch -> log -> clear; Left/Right
 * cycle with wrap — caret keys while a text box is non-empty, zone cycle
 * when it is empty (#007) — Up/Down move within a zone and across zones
 * at edges, Enter/Space activate; the clear-confirm overlay traps focus,
 * starting on Cancel). PrismaUI's legacy controller conversion
 * (D-pad -> arrows, B -> Escape) rides the same events, so
 * this is also the controller navigation layer. C7 controller actions
 * call dhFocusCycleZone/dhFocusMove/dhFocusActivate/dhFocusPageScroll/
 * dhFocusJumpLog — never raw DOM.
 * Controller actions (C7): the plugin binds A/X/Y/LB/RB/LT/RT/Start
 * through
 * the V12 BindControllerAction API (only when PrismaUI exposes V12);
 * PrismaUI re-dispatches them as the window event
 * 'prisma-controller-action' with detail {action, button, state:
 * "pressed"|"repeat"|"released"}. dhOnControllerAction handles "pressed"
 * for every action, "repeat" only for page_up/page_down, and ignores
 * "released". clear_quest/clear_all open the EXISTING confirm overlay —
 * scope = current selection / forced ALL — and are ignored while the
 * overlay is up; clear_quest does nothing when no quest is selected.
 * page_up/page_down and jump_oldest/jump_newest delegate to the C6
 * dhFocus* scroll surface (body search: while a term is active,
 * page_up/page_down WALK MATCHES via dhBodySearchPageOrMatch instead of
 * paging; a cleared term restores paging); close mirrors Esc exactly
 * (overlay up: dismiss only; else closeRequested). activate = A bound as
 * a V12 action (#007/#010 — VirtualPointer would otherwise hijack A into
 * a click at the cursor; bound actions are evaluated first): it calls
 * dhFocusActivate, except while a text box owns DOM focus (typing). B
 * and the D-pad never arrive here — they stay on the legacy
 * synthetic-key route the focus model consumes.
 * Device-aware hints (C8): the plugin pushes the active input device via
 * setInputDevice({"gamepad":bool}) from OnDomReady and every Focus/re-open
 * (V12 runtimes only; V11 fallback and old DLLs are never pushed). PrismaUI
 * itself fires 'prisma-input-device-change' at all views when the device
 * changes (ControllerGlyphs.cpp: detail {gamepad, style}) — the listener
 * covers switches mid-session. Both paths swap #hint textContent between
 * the keyboard and controller wordings — text only, never layout. With no
 * push and no event, #hint keeps its markup default ("Esc to close").
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
/* Body search (2026-10-17 feature): debounced (~200 ms) case-insensitive
 * substring scan over "speaker: text" across the whole dhAllLines model
 * (never the DOM). While a term is active: the quest index narrows to
 * quests with >=1 match (AND with the bucket checkboxes and the quest-name
 * box; "No matching quests." placeholder when nothing matches); matching
 * log rows carry .dh-hit-line (.dh-hit-line-current on the cursor line),
 * added at row build in dhMakeRow so C17 remounts re-apply them for free;
 * committing jumps to the most recent match (estimate + bounded
 * DOM-anchored correction in dhBodySearchJump); Enter=older /
 * Shift+Enter=newer
 * walk the matches with wrap and a "n of m" readout numbered FROM the
 * most recent match ("1 of m" on commit). appendLine grows the match set
 * incrementally without a re-jump; setHistory clears the term. Empty term
 * = every extension inert (idle paths byte-identical). Chunk B wires the
 * chrome around this core: #dialoguebar markup (index.html) with
 * #bodysearchcount / ‹ › buttons / #bodysearch, the DOMContentLoaded
 * input + button wiring, the bodysearch focus zone (dhFocusZoneOrder),
 * the Esc clear-first chain, and the LB/RB controller overload
 * (dhBodySearchPageOrMatch). No C++, no INI, no persistence. */
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
 * view owns grouping, selection, and search. Selection is a SET of quest
 * keys (#003): the Set IS the selection — "All" is the empty set, a plain
 * click selects exactly {key}, Ctrl/Cmd+click toggles membership. dhSelection
 * mirrors the set (the sole key while exactly one quest is selected, "all"
 * otherwise) so the single-select paths keep working unchanged. */
const dhAllLines = [];
let dhSelection = "all";
const dhSelectedQuests = new Set();

/* #003 — keep the dhSelection mirror in sync with the selection set: the
 * sole key while exactly one quest is selected, "all" for zero or many.
 * Call after EVERY dhSelectedQuests mutation. */
function dhSyncSelectionMirror() {
  dhSelection = dhSelectedQuests.size === 1
    ? dhSelectedQuests.values().next().value
    : "all";
}

/* #003 — index-row active test: with a non-empty set every selected quest
 * is active; otherwise only the dhSelection key is (the "All" row when
 * dhSelection is "all"). */
function dhIsSelectedKey(key) {
  return dhSelectedQuests.size > 0 ? dhSelectedQuests.has(key) : key === dhSelection;
}

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

/* C7 — checkbox ids per bucket. State lives in the DOM and is persisted by
 * the plugin (#001: setFilters pushes the saved state in, filtersChanged
 * reports changes out); the markup defaults to all-checked and a missing
 * box (old html) means the bucket stays visible. */
const dhBuckets = ["main", "side", "misc", "unattributed"];

function dhBucketVisible(bucket) {
  const box = document.getElementById("ftype-" + bucket);
  return !box || box.checked;
}

/* Checkbox change: re-filter the index; the log only when the "All" view is
 * up (#003: empty selection set and the mirror "all") — a selected quest's
 * view, single or multi, is unaffected by the checkboxes. */
function dhFilterChanged() {
  dhRenderIndex();
  if (dhSelectedQuests.size === 0 && dhSelection === "all") {
    dhRenderLog();
  }
}

/* #001 — persisted bucket filters. The plugin pushes the saved state via
 * setFilters (on view open and before every snapshot replay) and the view
 * reports every checkbox change back through filtersChanged. With an old
 * DLL no push ever arrives: dhFilterState stays null and the view keeps the
 * all-checked default (today's behavior). */
let dhFilterState = null;  // last pushed persisted state (null = never pushed)

/* Apply a filter state to the four checkbox elements, when present. Pure
 * checked-state application — no re-render side effects (matches how
 * setGeometry/dhApplyGeometry behaves); the snapshot replay that follows a
 * push re-renders against the new state anyway. */
function dhApplyFilterState(state) {
  for (const bucket of dhBuckets) {
    const box = document.getElementById("ftype-" + bucket);
    if (box) {
      box.checked = state[bucket];
    }
  }
}

/* Plugin -> JS: persisted filter state on open / before snapshot replay.
 * Tolerant per-field parse mirroring the C++ ViewFilterState semantics
 * (D4 ruling): each key is independently validated as a boolean, a missing
 * or invalid key keeps the true default; a payload that does not parse to a
 * JSON object is dropped entirely. Applies the checked state only. */
function setFilters(json) {
  const parsed = dhParse(json, null);
  if (!parsed || typeof parsed !== "object") {
    return;
  }
  const state = {};
  for (const bucket of dhBuckets) {
    state[bucket] = typeof parsed[bucket] === "boolean" ? parsed[bucket] : true;
  }
  dhFilterState = state;
  dhApplyFilterState(state);
}

/* Current checkbox state as a filter-state object, in the contract's exact
 * key order (dhBuckets). A missing box (old html) reads as visible. */
function dhFiltersFromBoxes() {
  const state = {};
  for (const bucket of dhBuckets) {
    const box = document.getElementById("ftype-" + bucket);
    state[bucket] = !box || box.checked;
  }
  return state;
}

/* JS -> plugin: report the checkbox state after a change, exactly once per
 * change event (wired as a second listener after dhFilterChanged).
 * Optional call — old DLLs never register the listener. */
function dhReportFilters() {
  const fn = window.filtersChanged;
  if (typeof fn === "function") {  // registered by the plugin (new DLLs)
    fn(JSON.stringify(dhFiltersFromBoxes()));
  }
}

/* C8 — device-aware hint chrome. Two wordings for #hint, swapped by
 * textContent only (no layout impact). The plugin pushes the active device
 * via setInputDevice (V12 runtimes: OnDomReady + every Focus/re-open);
 * PrismaUI fires 'prisma-input-device-change' at all views on a device
 * switch (ControllerGlyphs.cpp NotifyPromptStateChanged: detail
 * {gamepad, style}) — the window listener covers switches mid-session.
 * With no push and no event (V11 fallback, old DLL) #hint keeps its
 * markup default, which is also the keyboard wording's pre-push state. */
const dhHintText = {
  keyboard: "Esc close · ↑↓ navigate · Enter select",
  controller: "B close · A select · X clear quest · Y clear all · LB/RB page"
};
let dhHintMode = "keyboard";  // "keyboard" | "controller"

/* Swap #hint to the given mode's wording. Unknown modes are ignored; the
 * state updates even when #hint is absent (old html) so a later push still
 * lands on the right wording if the element appears. */
function dhApplyHintMode(mode) {
  if (mode !== "keyboard" && mode !== "controller") {
    return;
  }
  dhHintMode = mode;
  const hint = document.getElementById("hint");
  if (hint) {
    hint.textContent = dhHintText[mode];
  }
}

/* Plugin -> JS (C8): active input device on open / re-open. Tolerant parse
 * mirroring setFilters: a payload that does not parse to an object is
 * dropped entirely; a non-boolean gamepad keeps the keyboard default — a
 * bogus payload must never strand the hint in controller mode. */
function setInputDevice(json) {
  const parsed = dhParse(json, null);
  if (!parsed || typeof parsed !== "object") {
    return;
  }
  dhApplyHintMode(parsed.gamepad === true ? "controller" : "keyboard");
}

/* C8 — PrismaUI fires 'prisma-input-device-change' at every view when the
 * active input device changes. detail.gamepad is the shipped field; the
 * contract's defensive alternate detail.usingGamepad is accepted too.
 * Malformed events (no detail / non-boolean both-fields) are ignored, so a
 * stray event can never move the hint off the keyboard wording by accident. */
function dhOnInputDeviceChange(event) {
  const detail = event && event.detail;
  if (!detail) {
    return;
  }
  const gamepad = typeof detail.gamepad === "boolean"
    ? detail.gamepad
    : detail.usingGamepad === true;
  dhApplyHintMode(gamepad ? "controller" : "keyboard");
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
  row.className = "qentry" + (dhIsSelectedKey(key) ? " active" : "");
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
  row.addEventListener("click", function (event) {
    // #003: "All" is always an exclusive reset; a plain click selects
    // exactly the clicked quest; Ctrl/Cmd+click toggles membership in the
    // selection set. Emptying the set returns to "All" (mirror sync).
    if (key === "all") {
      dhSelectedQuests.clear();
    } else if (event.ctrlKey || event.metaKey) {
      if (dhSelectedQuests.has(key)) {
        dhSelectedQuests.delete(key);
      } else {
        dhSelectedQuests.add(key);
      }
    } else {
      dhSelectedQuests.clear();
      dhSelectedQuests.add(key);
    }
    dhSyncSelectionMirror();
    dhRenderIndex();
    dhRenderLog();
  });
  return row;
}

/* Left pane: "All" plus the quest buckets, filtered by the search box on
 * label substring (case-insensitive) and by the C7 bucket checkboxes — an
 * entry hides while its bucket (its newest line's bucket) is unchecked; the
 * "All" entry is never hidden by a checkbox. Body search: while a term is
 * active the list narrows further to quests with >=1 matching line (AND
 * with both filters above), and an empty result shows a "No matching
 * quests." placeholder. Selection is not reset by a search or a filter —
 * the right pane keeps showing the selected bucket while its entry is
 * filtered out, and restoring the filter brings it back. */
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

  /* Body search (AC1): inactive = the original behavior, byte for byte. */
  const bsActive = dhBodySearch.term !== "";
  const bsQuestHits = dhBodySearch.questMatchIds;

  list.textContent = "";
  let shown = 0;
  if (matches("All") && (!bsActive || bsQuestHits.size > 0)) {
    list.append(dhMakeEntry("all", "All", dhAllLines.length));
    shown++;
  }
  for (const entry of dhBuildIndex()) {
    if (!dhBucketVisible(dhBucketFor(entry.type))) {
      continue;
    }
    const label = entry.questId === 0 ? "Unattributed" : dhQuestLabel(entry);
    if (matches(label) && (!bsActive || bsQuestHits.has(entry.questId))) {
      list.append(dhMakeEntry(entry.key, label, entry.count, entry.type));
      shown++;
    }
  }
  if (shown === 0 && bsActive) {
    const row = document.createElement("div");
    row.className = "qentry qempty";
    row.textContent = "No matching quests.";
    list.append(row);
  }
}

/* Build one log row for a model line: speaker span (per-NPC color on npc
 * kinds), text span, dataset fields — the exact markup the CSS expects.
 * Pure construction, no DOM insertion or scrolling; callers own both.
 * Body search (AC2; whole-line ruling 2026-10-18): while a term is active
 * and the line matches, the whole row carries .dh-hit-line —
 * .dh-hit-line-current on the cursor line. Row text stays plain
 * textContent, byte-identical to the unsearched row. Derived at row
 * build, so C17 remounts re-apply the highlight for free. */
function dhMakeRow(line) {
  const row = document.createElement("div");
  row.className = "line " + line.kind;
  row.dataset.questId = String(line.questId);
  row.dataset.questName = line.questName;
  if (dhBodySearch.term !== ""
      && dhBodySearchLineMatches(line, dhBodySearch.term)) {
    row.classList.add(dhAllLines[dhBodySearch.matches[dhBodySearch.cursor]] === line
      ? "dh-hit-line-current"
      : "dh-hit-line");
  }
  const speaker = document.createElement("span");
  speaker.className = "speaker";
  speaker.textContent = line.speaker + ":";
  const text = document.createElement("span");
  text.className = "text";
  text.textContent = " " + line.text;
  if (line.kind === "npc") {
    speaker.style.color = dhNameColor(String(line.speaker));
  }
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

/* C17/#003 — the selection predicate, verbatim from the pre-virtualization
 * dhRenderLog/appendLine: "All" (empty set) applies the C7 bucket checkboxes;
 * a non-empty selection set matches membership only — single or multi, the
 * checkboxes don't apply (user ruling, G9). Centralized so the view rebuild
 * and the append path can never disagree about membership. */
function dhLineInSelection(line) {
  if (dhSelectedQuests.size > 0) {
    return dhSelectedQuests.has(String(line.questId));
  }
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
    if (dhViewLines.length >= (b + 1) * kBlockRows) {
      // Full blocks only: a partially filled tail drags the running
      // average toward zero and poisons every unmeasured block's price
      // (its height is still recorded above, so totals stay truthful).
      dhBlockSum += h;
      dhBlockMeasured += 1;
    }
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

/* ------ Body search (full-text search over the dialogue log) -------------
 * Design authority: .flybywire/active/2026-10-17_DesignSpec_BodySearch/
 * DESIGN_SPEC.md (D1-D11 settled). Debounced (~200 ms) case-insensitive
 * substring scan over "speaker: text" across the whole dhAllLines model —
 * never the DOM. While a term is active: the quest index narrows to quests
 * with >=1 match (AC1, AND with bucket checkboxes + quest-name box), all
 * matching rows carry the whole-line .dh-hit-line class with
 * .dh-hit-line-current on the cursor line (AC2, whole-line ruling
 * 2026-10-18), committing jumps to the most recent match (AC3), and
 * prev/next walk the matches with wrap (AC4). appendLine grows the match
 * set incrementally without a re-jump (AC8); setHistory clears the term.
 * Empty/whitespace term = Idle: every extension here is inert and the idle
 * paths stay byte-identical (AC9). Perf KB binding: jumps run on discrete
 * input only — dhBodySearchJump is a bounded (<=3 passes) estimate +
 * DOM-anchored correction loop, never on a scroll/resize path. */

const kBodySearchDebounceMs = 200;

/* Committed search state (singleton). matches holds indices into
 * dhAllLines in ascending order (line granularity — a line appears at most
 * once); cursor is the current position into matches. The count readout
 * numbers FROM the most recent match, so a fresh commit lands on "1 of m"
 * (AC3) and walking older counts up. */
const dhBodySearch = {
  term: "",
  matches: [],
  cursor: 0,
  questMatchIds: new Set(),
  timer: null
};
let dhBodySearchInput = null;  // element handed to dhBodySearchWireInput (Chunk B markup)

/* Case-insensitive substring test over the line's search haystack
 * ("speaker: text"); the term arrives lowercased (dhBodySearchCommit
 * normalizes). Whole-line highlight (2026-10-18 ruling): a boolean is all
 * dhMakeRow needs — where the match sits inside the line is irrelevant. */
function dhBodySearchLineMatches(line, term) {
  return term !== ""
    && (line.speaker + ": " + line.text).toLowerCase().indexOf(term) !== -1;
}

/* Rebuild every mounted block's rows through dhMakeRow — the C17 remount
 * path, driven manually. Used after commit/cursor moves/clear so
 * already-mounted rows pick up fresh .dh-hit-line / .dh-hit-line-current
 * state without waiting for a scroll. Content is identical, so block
 * heights and spacers are untouched; no layout reads anywhere (writes
 * only). */
function dhBodySearchRefreshMountedRows() {
  for (const pair of dhMounted) {
    const b = pair[0];
    const block = pair[1];
    block.textContent = "";
    const start = b * kBlockRows;
    const end = Math.min(start + kBlockRows, dhViewLines.length);
    const fragment = document.createDocumentFragment();
    for (let i = start; i < end; i++) {
      fragment.append(dhMakeRow(dhViewLines[i]));
    }
    block.append(fragment);
  }
}

/* The "[n of m]" readout: "" when idle, "0" when active with no matches,
 * otherwise "n of m" with n numbered FROM the most recent match (AC3: a
 * fresh commit lands on the most recent match and reads "1 of m"; walking
 * older counts up). */
function dhBodySearchCountText() {
  if (dhBodySearch.term === "") {
    return "";
  }
  const m = dhBodySearch.matches.length;
  if (m === 0) {
    return "0";
  }
  return (m - dhBodySearch.cursor) + " of " + m;
}

/* Refresh the "[n of m]" readout when the bar exists. The count span id
 * (#bodysearchcount) is Chunk B markup — an absent element is inert, so
 * old html never breaks. */
function dhBodySearchUpdateCount() {
  const el = document.getElementById("bodysearchcount");
  if (el) {
    el.textContent = dhBodySearchCountText();
  }
}

/* Jump the log so the given match (a dhAllLines index) sits at the top of
 * the viewport. Two phases, discrete-input only and bounded (perf KB):
 *  1. ESTIMATE — the target offset is dhBlockLayout() arithmetic: block
 *     prefix offset + row-in-block x the block's own per-row rate (its
 *     measured height when known — divided by its ACTUAL row count so a
 *     partial tail block doesn't under-rate — the running average
 *     otherwise), clamped against the simulated total. The scrollTop
 *     write is silent, then a synchronous dhUpdateWindow() mounts and
 *     measures the landing window.
 *  2. CORRECTION — once the target block is mounted, ONE rect-delta read
 *     (row top vs log top) re-anchors to the row's REAL position exactly,
 *     then a final window pass settles the mount set. Blocks far from the
 *     panel-open window are priced by estimate, so the first landing can
 *     miss by more than the overscan; every pass measures the blocks it
 *     lands on, so re-aiming converges. Capped at 3 passes — if the
 *     target never mounts, the last estimate stands.
 * When the target's line is filtered out of the current view, the cursor
 * instead adopts the nearest visible match (walking outward from the
 * cursor; older wins ties — spec §5.4: visibility is a render concern,
 * the count stays global) and the mounted rows + readout refresh to
 * match. */
function dhBodySearchJump(matchIdx, viewH) {
  const log = document.getElementById("log");
  if (!log || dhBodySearch.term === "" || dhBodySearch.matches.length === 0) {
    return;
  }
  const target = dhAllLines[matchIdx];
  let viewPos = -1;
  for (let i = 0; i < dhViewLines.length; i++) {
    if (dhViewLines[i] === target) {
      viewPos = i;
      break;
    }
  }
  if (viewPos < 0) {
    const inView = new Set(dhViewLines);
    const m = dhBodySearch.matches;
    let best = -1;
    for (let off = 0; off < m.length && best < 0; off++) {
      const lo = dhBodySearch.cursor - off;
      const hi = dhBodySearch.cursor + off;
      if (lo >= 0 && inView.has(dhAllLines[m[lo]])) {
        best = lo;
      } else if (hi < m.length && inView.has(dhAllLines[m[hi]])) {
        best = hi;
      }
    }
    if (best < 0) {
      return;  // no visible match at all — no scroll change
    }
    dhBodySearch.cursor = best;
    for (let i = 0; i < dhViewLines.length; i++) {
      if (dhViewLines[i] === dhAllLines[m[best]]) {
        viewPos = i;
        break;
      }
    }
    if (viewPos < 0) {
      return;  // defensive only — inView already proved membership
    }
    dhBodySearchRefreshMountedRows();
    dhBodySearchUpdateCount();
  }
  if (viewH === undefined) {
    viewH = log.clientHeight;  // standalone call: the read stays before the writes
  }
  const block = Math.floor(viewPos / kBlockRows);
  const rowInBlock = viewPos - block * kBlockRows;
  const rowsInBlock = Math.min(kBlockRows, dhViewLines.length - block * kBlockRows);
  for (let pass = 0; pass < 3; pass++) {
    const layout = dhBlockLayout();
    const targetPx = layout.offsets[block] + rowInBlock * (layout.heights[block] / rowsInBlock);
    const max = Math.max(0, layout.total - viewH);
    dhSetScrollSilently(log, Math.min(max, Math.max(0, targetPx)));
    dhUpdateWindow();
    const el = dhMounted.get(block);
    if (!el) {
      continue;  // missed by more than the overscan — re-aim from the fresh measurements
    }
    const rowEl = el.children[rowInBlock];
    if (!rowEl) {
      break;  // defensive: a mounted block always holds all its rows
    }
    const delta = rowEl.getBoundingClientRect().top - log.getBoundingClientRect().top;
    if (Math.abs(delta) >= 1) {
      dhSetScrollSilently(log, Math.max(0, log.scrollTop + delta));
      dhUpdateWindow();
    }
    break;
  }
}

/* Cursor move with wrap: dir < 0 = previous/older, dir > 0 = next/newer
 * (AC4). Inert when idle or when there are no matches (§5.2). Highlights
 * and the readout refresh, then the jump runs its bounded estimate +
 * correction loop (discrete input only — perf KB). */
function dhBodySearchStep(dir) {
  const m = dhBodySearch.matches;
  if (dhBodySearch.term === "" || m.length === 0) {
    return;
  }
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const viewH = log.clientHeight;  // read first
  dhBodySearch.cursor = (dhBodySearch.cursor + (dir < 0 ? -1 : 1) + m.length) % m.length;
  dhBodySearchRefreshMountedRows();
  dhBodySearchUpdateCount();
  dhBodySearchJump(m[dhBodySearch.cursor], viewH);
}

function dhBodySearchPrev() {  // Enter — previous (older) match
  dhBodySearchStep(-1);
}

function dhBodySearchNext() {  // Shift+Enter — next (newer) match
  dhBodySearchStep(1);
}

/* Controller context overload (AC6): while a body-search term is active,
 * the LB/RB actions (page_up/page_down) walk the matches — prev = older,
 * next = newer — instead of paging; a cleared term restores the page
 * scroll. Term active with zero matches: the walk is inert (§5.2). */
function dhBodySearchPageOrMatch(dir) {
  if (dhBodySearch.term !== "") {
    if (dir < 0) {
      dhBodySearchPrev();
    } else {
      dhBodySearchNext();
    }
  } else {
    dhFocusPageScroll(dir < 0 ? -1 : 1);
  }
}

/* Commit a search term (the debounced "input" endpoint). Rescans the
 * model, rebuilds the match state, targets the most recent match, and
 * re-renders: quest index, mounted-row highlights, count readout, then
 * the auto-jump. Unchanged term = no-op (never re-jumps); empty/whitespace
 * term = the full reset to Idle. The viewport height is read before the
 * DOM writes; the jump itself is dhBodySearchJump's bounded correction
 * loop. */
function dhBodySearchCommit(raw) {
  const log = document.getElementById("log");
  const viewH = log ? log.clientHeight : 0;  // the only layout read — first
  const term = (typeof raw === "string" ? raw : "").toLowerCase().trim();
  if (term === dhBodySearch.term) {
    return;
  }
  dhBodySearch.term = term;
  dhBodySearch.matches = [];
  dhBodySearch.cursor = 0;
  dhBodySearch.questMatchIds.clear();
  if (term !== "") {
    for (let i = 0; i < dhAllLines.length; i++) {
      const line = dhAllLines[i];
      if (dhBodySearchLineMatches(line, term)) {
        dhBodySearch.matches.push(i);
        dhBodySearch.questMatchIds.add(line.questId);
      }
    }
    if (dhBodySearch.matches.length > 0) {
      dhBodySearch.cursor = dhBodySearch.matches.length - 1;  // most recent
    }
  }
  // DOM writes only from here on — no layout reads below this line.
  dhRenderIndex();
  dhBodySearchRefreshMountedRows();
  dhBodySearchUpdateCount();
  if (dhBodySearch.matches.length > 0) {
    dhBodySearchJump(dhBodySearch.matches[dhBodySearch.cursor], viewH);
  }
}

/* Full reset to Idle: setHistory, the empty-term commit, and Esc-clear
 * (Chunk B) all land here. Cancels a pending debounce, clears the state
 * and the wired input, and refreshes the readout. Mounted-row highlights
 * clean up through the next row build — setHistory re-renders the log
 * right after; the empty-commit path refreshes them explicitly. */
function dhBodySearchClear() {
  if (typeof clearTimeout === "function" && dhBodySearch.timer !== null) {
    clearTimeout(dhBodySearch.timer);
  }
  dhBodySearch.timer = null;
  dhBodySearch.term = "";
  dhBodySearch.matches = [];
  dhBodySearch.cursor = 0;
  dhBodySearch.questMatchIds.clear();
  if (dhBodySearchInput) {
    dhBodySearchInput.value = "";
  }
  dhBodySearchUpdateCount();
}

/* Wire a body-search input element (Chunk B's #bodysearch): debounced
 * "input" commits and Enter/Shift+Enter match walking. Enter = previous
 * (older), Shift+Enter = next (newer); both inert when the term is empty
 * or there are no matches. Timer calls are guarded so timerless hosts
 * (the c6/c7/c8 harness sandboxes) commit synchronously instead of
 * throwing. */
function dhBodySearchWireInput(el) {
  if (!el || typeof el.addEventListener !== "function") {
    return;
  }
  dhBodySearchInput = el;
  el.addEventListener("input", function () {
    if (typeof clearTimeout === "function" && dhBodySearch.timer !== null) {
      clearTimeout(dhBodySearch.timer);
      dhBodySearch.timer = null;
    }
    if (typeof setTimeout !== "function") {
      dhBodySearchCommit(el.value);
      return;
    }
    dhBodySearch.timer = setTimeout(function () {
      dhBodySearch.timer = null;
      dhBodySearchCommit(el.value);
    }, kBodySearchDebounceMs);
  });
  el.addEventListener("keydown", function (event) {
    if (event.key !== "Enter" && event.keyCode !== 13) {
      return;
    }
    if (dhBodySearch.term === "" || dhBodySearch.matches.length === 0) {
      return;  // idle / no results — inert (§5.2)
    }
    if (typeof event.preventDefault === "function") {
      event.preventDefault();
    }
    if (event.shiftKey) {
      dhBodySearchNext();
    } else {
      dhBodySearchPrev();
    }
  });
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
  // "All" selected (selection set cleared), search cleared — then apply the
  // persisted filter state (#001; all-checked default when the plugin never
  // pushed one, e.g. an old DLL) and rebuild model and panes.
  dhAllLines.length = 0;
  dhSelection = "all";
  dhSelectedQuests.clear();
  const search = document.getElementById("search");
  if (search) {
    search.value = "";
  }
  dhBodySearchClear();  // body search resets with the snapshot (spec §4.4)
  dhApplyFilterState(dhFilterState
    || { main: true, side: true, misc: true, unattributed: true });
  for (const raw of lines) {
    const line = dhNormalizeLine(raw);
    if (line) {
      dhAllLines.push(line);
    }
  }
  dhRenderIndex();
  dhRenderLog();
  dhFocusReset();  // C6/D9: view-open focus — log, or quest index when empty
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
  // Body search (AC8): grow the match set incrementally while a term is
  // active — the new line is the newest, so appending keeps matches
  // ascending. Count refresh only, NEVER a re-jump (the user may be
  // reading). The dhRenderIndex below re-narrows the quest index.
  if (dhBodySearch.term !== "") {
    if (dhBodySearchLineMatches(line, dhBodySearch.term)) {
      dhBodySearch.matches.push(dhAllLines.length - 1);
      dhBodySearch.questMatchIds.add(line.questId);
    }
    dhBodySearchUpdateCount();
  }
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
        // A fresh tail block holds exactly one row: record the height so
        // totals stay truthful, but NEVER into the running average — a
        // partial block would poison every unmeasured block's price. The
        // block joins the average once it fills (the other arm below).
        dhBlockHeights[last] = el.offsetHeight;
      }
      dhRefreshSpacers();
    } else if (dhMounted.has(last)) {
      const el = dhMounted.get(last);
      el.append(dhMakeRow(line));
      const h = el.offsetHeight;  // the block grew by one row
      if (dhViewLines.length >= (last + 1) * kBlockRows) {
        // The tail just filled: count its full height exactly once. Any
        // earlier partial measurement was recorded but never summed, so
        // add h whole — not a delta.
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

/* 2026-10-25 panel-wide typography: one root custom property scales every
 * chrome font-size (all fixed declarations are calc(Npx * var(--dh-font-
 * scale)) — see history.css); #log keeps its inline px so the virtualized
 * row measurements stay exact, and the single C17 invalidation re-anchors
 * the dialogue. The setProperty guard is a DOM-stub tolerance (real
 * Ultralight always writes; legacy harness worlds lack the method).
 * C1 deferred header channel: the HEADER scale lives on the #header
 * element itself (--dh-header-font-scale) and is written ONLY by full
 * applications (setFontSize callers: native push, numeric commit,
 * keyboard/controller step, slider release) — slider input preview writes
 * the body channel via dhApplyBodyFont and leaves the header untouched,
 * so a live pointer drag never relayouts the header. */
function dhApplyBodyFont(size) {
  const root = document.documentElement;
  if (root && root.style && typeof root.style.setProperty === "function") {
    root.style.setProperty("--dh-font-scale", String(Number(size) / 16));
  }
  document.getElementById("log").style.fontSize = Number(size) + "px";
  dhInvalidateHeights();  // C17: row heights changed — re-estimate + re-anchor
}

function setFontSize(size) {
  dhApplyBodyFont(size);  // body channel + log inline + the one invalidation
  const header = document.getElementById("header");
  if (header && header.style && typeof header.style.setProperty === "function") {
    header.style.setProperty("--dh-header-font-scale", String(Number(size) / 16));
  }
}

function dhSend(eventName, payload) {
  const fn = window[eventName];
  if (typeof fn === "function") {
    fn(payload === undefined ? "" : payload);
  } else {
    console.warn("[DialogueHistory] listener not registered: " + eventName);
  }
}

/* #005 — input exclusivity while typing: the plugin needs to know when a
 * text box owns keystrokes so its input sink can consume keyboard
 * ButtonEvents before they bleed to the game (FO4 builds them from polled
 * key state, so PrismaUI cannot starve them itself). DOM-level focus/blur
 * listeners cover BOTH focus paths with one hook each — the C6 zone model
 * (dhFocusApply calls el.focus()/blur()) and mouse clicks — and browsers
 * don't refire focus on an already-focused box, so a zone re-apply while
 * the box stays focused emits nothing. Payload is exactly "1" (focused) /
 * "0" (blurred); old DLLs simply never register the listener (dhSend
 * guards the call). */
function dhWireTypingEmit(el) {
  el.addEventListener("focus", function () { dhSend("typingStateChanged", "1"); });
  el.addEventListener("blur", function () { dhSend("typingStateChanged", "0"); });
}

/* Header preferences (2026-10-24 feature): font slider, numeric override,
 * and Pause-on-open checkbox living in the header (#prefs). The plugin
 * pushes the persisted state via setViewPreferences before every snapshot
 * replay; the view reports every completed commit through
 * viewPreferencesChanged (one combined {"fontSize":int,"pauseOnOpen":
 * bool} payload per gesture — the exact mirror of the C++ ViewPreferences
 * codec). dhPrefs is the view-side cache of the last valid NATIVE PUSH or
 * LOCAL COMMIT ("last valid native/local value"): slider movement
 * PREVIEWS through dhApplyBodyFont — body channel + #log inline + numeric
 * mirror, leaving the header scale and dhPrefs untouched and sending
 * nothing (C1 defers the header channel to full applications only); a
 * completed slider gesture (change) runs the FULL setFontSize (body +
 * header) and then commits once; a valid numeric Enter/blur, a
 * keyboard/controller step, and every checkbox change apply fully and
 * send once. Invalid/empty/fractional/
 * out-of-range numeric input restores dhPrefs.fontSize into the box and
 * sends nothing. The slider and the overall accepted range are the same
 * 10–48 (C2), so every valid value is a slider value and no out-of-slider
 * state exists. Pause changes
 * commit immediately and never touch the font path; the native side
 * performs the ordered Unfocus->Focus itself, so the view only sends.
 * Old-DLL tolerance both ways: no push ever arrives -> the markup
 * defaults (16 / unchecked) stay live; no listener registered -> dhSend
 * warns and the local commit still lands in dhPrefs (the restore target
 * stays sane). Every applied font size runs the C17 virtualization
 * invalidation/re-anchoring exactly once (inside dhApplyBodyFont, the
 * shared body-apply both paths use).
 * Keyboard/controller surface (parent rulings V1/V3/V4, 2026-10-24): the
 * prefs cluster is a focus-model zone OUTSIDE dhFocusZoneOrder — the
 * legacy six-zone cycle stays byte-for-byte. ArrowUp from the clear zone
 * enters prefs at the slider; inside, Up/Down step slider -> numeric ->
 * pause without wrap, exiting back to clear past either end. Left/Right
 * on the slider adjusts +/-1 (preview + one commit; endpoint presses send
 * nothing); on the numeric they are caret keys while non-empty and rove
 * to the adjacent stop while empty; on the pause label they rove to the
 * adjacent stop (Right from pause is inert). Enter/Space on the pause
 * label toggles the checkbox; Enter on the slider is inert; Enter on the
 * numeric is owned by the input's own keydown (commits). Escape in the
 * numeric restores the last valid text and returns to the clear zone; a
 * one-shot flag suppresses the blur-commit the move causes. */
const kPrefsSliderMin = 10;
const kPrefsSliderMax = 48;
const kPrefsFontMin = 10;
const kPrefsFontMax = 48;
const dhPrefsStops = ["fontslider", "fontnum", "pauselabel"];
const dhPrefs = { fontSize: 16, pauseOnOpen: false };
let dhPrefsEscBlur = false;  // one-shot Escape blur-commit suppression (ruling V2)

/* The stop element at the current roving position (null on old html). */
function dhPrefsTarget() {
  return document.getElementById(dhPrefsStops[dhFocusPos.prefs]);
}

/* Push the cached preferences into all three controls. The range input
 * clamps an out-of-range font to its nearest endpoint by itself. */
function dhPrefsSyncControls() {
  const slider = document.getElementById("fontslider");
  if (slider) {
    slider.value = String(dhPrefs.fontSize);
  }
  const num = document.getElementById("fontnum");
  if (num) {
    num.value = String(dhPrefs.fontSize);
  }
  const box = document.getElementById("pausebox");
  if (box) {
    box.checked = dhPrefs.pauseOnOpen;
  }
}

/* One combined commit payload per completed gesture. */
function dhPrefsCommit() {
  dhSend("viewPreferencesChanged", JSON.stringify({
    fontSize: dhPrefs.fontSize,
    pauseOnOpen: dhPrefs.pauseOnOpen
  }));
}

/* Slider preview (input event): applies the BODY channel + #log inline
 * and mirrors the value into the numeric box — the header channel is
 * deliberately untouched (C1 deferral), and zero dhSend/native/disk work
 * leaves this path (contract preview cadence). The single invalidation
 * runs inside dhApplyBodyFont. */
function dhPrefsPreviewFont(size) {
  dhApplyBodyFont(size);
  const num = document.getElementById("fontnum");
  if (num) {
    num.value = String(size);
  }
}

/* Commit a font size known valid: cache, apply (invalidation), sync the
 * slider/numeric display, send once. */
function dhPrefsCommitFont(size) {
  dhPrefs.fontSize = size;
  setFontSize(size);
  dhPrefsSyncControls();
  dhPrefsCommit();
}

/* Strict integer 10–48 parse (the view-side mirror of the native codec):
 * surrounding whitespace tolerated, digits only, range-checked. */
function dhPrefsParseFont(text) {
  const t = String(text).trim();
  if (!/^\d+$/.test(t)) {
    return null;
  }
  const v = parseInt(t, 10);
  return v >= kPrefsFontMin && v <= kPrefsFontMax ? v : null;
}

/* Restore the numeric box to the last valid value. */
function dhPrefsRestoreNumeric() {
  const num = document.getElementById("fontnum");
  if (num) {
    num.value = String(dhPrefs.fontSize);
  }
}

/* Numeric commit (Enter or blur): a valid value commits; anything else
 * restores the last valid value and sends nothing. */
function dhPrefsCommitNumeric() {
  const num = document.getElementById("fontnum");
  if (!num) {
    return;
  }
  const v = dhPrefsParseFont(num.value);
  if (v === null) {
    dhPrefsRestoreNumeric();
    return;
  }
  dhPrefsCommitFont(v);
}

/* Slider keyboard step (focus model): +/-1 with full body/header
 * application + one combined commit; an endpoint press consumes the key
 * but sends nothing. */
function dhPrefsSliderStep(dir) {
  const slider = document.getElementById("fontslider");
  if (!slider) {
    return;
  }
  const cur = Number(slider.value);
  const next = Math.min(kPrefsSliderMax, Math.max(kPrefsSliderMin, cur + (dir < 0 ? -1 : 1)));
  if (next === cur) {
    return;  // endpoint — no send, no redundant invalidation
  }
  slider.value = String(next);
  dhPrefs.fontSize = next;
  setFontSize(next);  // C1: discrete path — full body+header application
  dhPrefsSyncControls();
  dhPrefsCommit();
}

/* Plugin -> JS: persisted preferences on open / before snapshot replay.
 * Tolerant per-field parse mirroring the C++ ViewPreferences semantics:
 * an integer fontSize in 10–48 and a boolean pauseOnOpen each apply
 * independently; a missing/invalid field keeps the cached value; a
 * payload with NO valid field is dropped entirely (cached state and
 * controls untouched). A valid push always re-syncs the controls, and the
 * font applies through the existing setFontSize — but only when the
 * pushed value or the rendered style actually disagrees with the cache,
 * so an unchanged push costs no invalidation while an abandoned local
 * preview (style drifted from cache with no commit) still re-anchors. */
function setViewPreferences(json) {
  const parsed = dhParse(json, null);
  if (!parsed || typeof parsed !== "object") {
    return;
  }
  let any = false;
  let fontSize = dhPrefs.fontSize;
  if (typeof parsed.fontSize === "number" && Number.isInteger(parsed.fontSize)
      && parsed.fontSize >= kPrefsFontMin && parsed.fontSize <= kPrefsFontMax) {
    fontSize = parsed.fontSize;
    any = true;
  }
  let pauseOnOpen = dhPrefs.pauseOnOpen;
  if (typeof parsed.pauseOnOpen === "boolean") {
    pauseOnOpen = parsed.pauseOnOpen;
    any = true;
  }
  if (!any) {
    return;
  }
  const changed = fontSize !== dhPrefs.fontSize;
  dhPrefs.fontSize = fontSize;
  dhPrefs.pauseOnOpen = pauseOnOpen;
  const log = document.getElementById("log");
  if (changed || !log || log.style.fontSize !== fontSize + "px") {
    setFontSize(fontSize);
  }
  dhPrefsSyncControls();
}

/* Vertical movement inside the prefs cluster (parent ruling V4): no
 * wrap — slider Up exits back to clear, pause Down exits to clear, the
 * rest step between adjacent stops. */
function dhPrefsMove(dir) {
  if (dhFocusPos.prefs === 0) {
    if (dir < 0) {
      dhFocusSetZone("clear");
      return;
    }
    dhFocusPos.prefs = 1;
  } else if (dhFocusPos.prefs === 1) {
    dhFocusPos.prefs = dir < 0 ? 0 : 2;
  } else {
    if (dir > 0) {
      dhFocusSetZone("clear");
      return;
    }
    dhFocusPos.prefs = 1;
  }
  dhFocusApply();
}

/* Horizontal movement inside the prefs cluster (parent ruling V4): the
 * slider adjusts its value (never zone-cycles); the numeric keeps caret
 * keys while non-empty (D3 convention — NOT prevented) and roves to the
 * adjacent stop while empty; the pause label roves to the adjacent stop,
 * with Right from pause inert. preventDefault lands per case. */
function dhPrefsHorizontal(dir, event) {
  if (dhFocusPos.prefs === 0) {
    if (typeof event.preventDefault === "function") {
      event.preventDefault();
    }
    dhPrefsSliderStep(dir);
    return;
  }
  if (dhFocusPos.prefs === 1) {
    const el = dhPrefsTarget();
    if (el && el.value) {
      return;  // caret keys — no preventDefault, default caret move
    }
    if (typeof event.preventDefault === "function") {
      event.preventDefault();
    }
    dhFocusPos.prefs = dir < 0 ? 0 : 2;
    dhFocusApply();
    return;
  }
  if (typeof event.preventDefault === "function") {
    event.preventDefault();
  }
  if (dir < 0) {
    dhFocusPos.prefs = 1;
    dhFocusApply();
  }  // Right from pause: inert (consumed)
}

/* Clear history (C5/#003): one selection-scoped path. The Clear button opens
 * a confirmation overlay naming the scope — "all", a single quest (also a
 * one-quest selection set), or "multiple quests" for a larger set; Confirm
 * dispatches to the plugin, Cancel/Esc hides the overlay without touching
 * the model (Esc must NOT fall through to closeRequested while the overlay
 * is up). After the plugin mutates the buffer it re-pushes a snapshot;
 * setHistory's reset (selection -> "All", search cleared) is the accepted
 * post-clear UX. */
let dhClearTarget = "all";  // "all", "multi", or the decimal questId string
let dhClearIds = [];        // #003: numerically sorted ids captured for "multi"

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

function dhShowClearOverlay(scope) {
  // #003: the clear scope is the current selection — "all", a single quest
  // (incl. a one-quest selection set), or "multi" with the numerically
  // sorted ids for a larger set (deterministic payload order). C7: the
  // controller's clear_all action passes scope "all" to force ALL scope
  // regardless of the selection; the button and the focus model pass
  // nothing and keep the selection-derived behavior.
  if (scope === "all") {
    dhClearTarget = "all";
    dhClearIds = [];
  } else if (dhSelectedQuests.size > 1) {
    dhClearTarget = "multi";
    dhClearIds = Array.from(dhSelectedQuests, Number).sort(function (a, b) { return a - b; });
  } else {
    dhClearTarget = dhSelection;
    dhClearIds = [];
  }
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
      name.className = "clearscope";  // "multiple quests" keeps the quest-scope styling
      name.textContent = dhClearTarget === "multi" ? "multiple quests" : dhClearScopeText();
      msg.append(document.createTextNode("This will clear all dialogue history for "), name,
        document.createTextNode(". Are you sure?"));
    }
  }
  const overlay = document.getElementById("overlay");
  if (overlay) {
    overlay.hidden = false;
  }
  dhFocusEnterOverlay();  // C6: trap focus on the overlay (starts on Cancel)
}

function dhHideClearOverlay() {
  const overlay = document.getElementById("overlay");
  if (overlay) {
    overlay.hidden = true;
  }
  dhFocusExitOverlay();  // C6/D6: restore the pre-overlay zone position
}

function dhConfirmClear() {
  dhHideClearOverlay();
  if (dhClearTarget === "all") {
    const fn = window.clearAllRequested;
    if (typeof fn === "function") {  // registered by the plugin (new DLLs)
      fn("");
    }
  } else if (dhClearTarget === "multi") {
    const fn = window.clearQuestsRequested;
    if (typeof fn === "function") {  // registered by the plugin (new DLLs)
      fn(dhClearIds.join(","));
    }
  } else {
    const fn = window.clearQuestRequested;
    if (typeof fn === "function") {
      fn(String(dhClearTarget));
    }
  }
}

/* C7 — V12 controller actions. PrismaUI delivers bound buttons as the
 * window event 'prisma-controller-action' with detail {action, button,
 * state}. "pressed" drives every action; "repeat" only page_up/page_down
 * (held-button paging); "released" is ignored. clear_quest/clear_all reuse
 * the existing confirm overlay — dhShowClearOverlay IS the button's code
 * path, so nothing here duplicates overlay logic; both are ignored while
 * the overlay is already up, and clear_quest does nothing when no quest
 * is selected (the dedicated clear_all/Y action owns ALL scope). close
 * mirrors the Esc branch exactly: overlay up -> dismiss only, else
 * closeRequested. Scroll/jump actions delegate to the C6 dhFocus* surface
 * (O(virtualized-window) arithmetic per event — no DOM measurement). */
function dhOnControllerAction(event) {
  const detail = event && event.detail;
  if (!detail) {
    return;
  }
  const action = detail.action;
  if (detail.state === "repeat") {
    if (action === "page_up") {
      dhBodySearchPageOrMatch(-1);  // AC6: match walk while a term is active
    } else if (action === "page_down") {
      dhBodySearchPageOrMatch(1);
    }
    return;
  }
  if (detail.state !== "pressed") {
    return;  // "released" (and anything unknown) is ignored
  }
  if (action === "clear_quest") {
    if (dhOverlayVisible() || dhSelectedQuests.size === 0) {
      return;  // already confirming, or no quest scope to clear
    }
    dhShowClearOverlay();
  } else if (action === "clear_all") {
    if (dhOverlayVisible()) {
      return;
    }
    dhShowClearOverlay("all");
  } else if (action === "page_up") {
    dhBodySearchPageOrMatch(-1);  // AC6: match walk while a term is active
  } else if (action === "page_down") {
    dhBodySearchPageOrMatch(1);
  } else if (action === "jump_oldest") {
    dhFocusJumpLog(false);
  } else if (action === "jump_newest") {
    dhFocusJumpLog(true);
  } else if (action === "activate") {
    // #007/#010: A is a V12 action (VirtualPointer would hijack it into a
    // click at the cursor). Typing owns a focused text box — activating
    // there would blur it (search Enter semantics), so A stays inert.
    const active = document.activeElement;
    if (active && (active.id === "search" || active.id === "bodysearch"
        || active.id === "fontnum")) {
      return;
    }
    dhFocusActivate();
  } else if (action === "close") {
    if (dhOverlayVisible()) {
      dhHideClearOverlay();  // dismiss only; the panel stays open
    } else {
      dhSend("closeRequested");
    }
  }
}

/* C6 — keyboard/controller focus model (roving zones). PrismaUI's legacy
 * controller conversion (D-pad -> arrows, B -> Escape — A is a bound V12
 * action, #007/#010) rides
 * the same keydown events, so this model is the whole in-menu navigation
 * layer for keyboard and controller alike. Zones cycle in the contract
 * order (D2, body search added per spec §6.1): search -> filters -> quests
 * -> bodysearch -> log -> clear, Left/Right =
 * prev/next with cyclic wrap; Up/Down move within a zone and, at zone
 * edges (the log consumes Up/Down for scrolling — D4), move between
 * zones. Focus is tracked in JS state — the real DOM focus only ever
 * lands on the two text boxes (#search, #bodysearch — so typing works);
 * every other zone resolves its
 * target element from state on demand, so index re-renders can rebuild
 * rows without stale-element bookkeeping. The visible indicator is the
 * .dh-focus class (history.css) — deliberately NOT .qentry.active, which
 * means selection (contract). Perf (KB rule 1): each handler reads
 * first (scrollTop / one clientHeight / row offsets) and writes last
 * (class + scrollTop); per-keyrepeat work is dhBlockLayout() arithmetic
 * or O(1), and the scrollTop write rides the existing dhOnScroll ->
 * dhUpdateWindow windowing pass — no new measurement anywhere.
 * C7 surface (INTEGRATION_CONTRACT): dhFocusCycleZone, dhFocusMove,
 * dhFocusActivate, dhFocusPageScroll, dhFocusJumpLog. */

/* Zone order (D2, extended by body search spec §6.1): the bodysearch zone
 * sits between quests and log — the box lives in #dialoguebar above the
 * log pane. Frozen by the c6 harness expectations (2026-10-24 ruling V1):
 * the prefs header cluster is deliberately NOT part of this array — it is
 * a separate zone entered by ArrowUp from clear and exited back to clear,
 * so the six-zone cycle and its wrap stay byte-for-byte. */
const dhFocusZoneOrder = ["search", "filters", "quests", "bodysearch", "log", "clear"];

/* Per-zone position: filters = checkbox index (dhBuckets order), quests =
 * visible row index; roving — a zone remembers its position across
 * visits, clamped to what is currently rendered. bodysearch is a
 * single-position zone (like search/clear) and needs no roving slot.
 * prefs (2026-10-24) = stop index into dhPrefsStops (slider/numeric/
 * pause) for the header cluster zone, which lives OUTSIDE
 * dhFocusZoneOrder (parent ruling V1). */
const dhFocusPos = { search: 0, filters: 0, quests: 0, log: 0, clear: 0, prefs: 0 };
let dhFocusZone = "log";
let dhFocusPrev = null;    // D6: zone saved while the overlay traps focus
let dhFocusOverlay = -1;   // D6: trap position (0 = confirm, 1 = cancel)
let dhFocusMarked = [];    // elements currently carrying .dh-focus

/* Quest rows eligible for roving focus: the body-search "No matching
 * quests." placeholder (.qentry.qempty — no data.key) is skipped (I6), so
 * the placeholder can never hold a roving slot and positions always mean
 * real entries. Shared by dhFocusTarget, dhFocusMove and the click sync
 * so they can never disagree about the indexing. */
function dhQuestRowElements() {
  const list = document.getElementById("quests");
  if (!list) {
    return [];
  }
  const rows = [];
  for (let i = 0; i < list.children.length; i++) {
    if (list.children[i].dataset.key) {
      rows.push(list.children[i]);
    }
  }
  return rows;
}

/* Resolve the current zone's target element from state (never cached
 * across renders): dhRenderIndex rebuilds the rows, so the quests target
 * is re-queried by position each time. Returns null when nothing
 * renderable is there. */
function dhFocusTarget() {
  if (dhFocusOverlay >= 0) {
    return document.getElementById(dhFocusOverlay === 0 ? "overlayconfirm" : "overlaycancel");
  }
  if (dhFocusZone === "search") {
    return document.getElementById("search");
  }
  if (dhFocusZone === "filters") {
    return document.getElementById("ftype-" + dhBuckets[dhFocusPos.filters]);
  }
  if (dhFocusZone === "quests") {
    const rows = dhQuestRowElements();
    if (rows.length === 0) {
      return null;
    }
    const i = Math.min(dhFocusPos.quests, rows.length - 1);
    dhFocusPos.quests = i;  // adopt the clamp so later moves start here
    return rows[i];
  }
  if (dhFocusZone === "bodysearch") {
    return document.getElementById("bodysearch");
  }
  if (dhFocusZone === "prefs") {
    return dhPrefsTarget();  // 2026-10-24 header cluster (ruling V1)
  }
  if (dhFocusZone === "log") {
    return document.getElementById("log");
  }
  return document.getElementById("clear");
}

/* Move the visible indicator to whatever dhFocusTarget resolves: strip
 * the class from the previous marks (detached rows are harmless no-ops)
 * and add it to the new target. Real DOM focus only accompanies the text
 * zones (#search, #bodysearch, #fontnum — so typing works); leaving one
 * blurs, so typed text can't land in a box while another zone owns the
 * model. */
function dhFocusApply() {
  for (const el of dhFocusMarked) {
    el.classList.remove("dh-focus");
  }
  dhFocusMarked = [];
  const el = dhFocusTarget();
  if (el) {
    el.classList.add("dh-focus");
    dhFocusMarked.push(el);
  }
  const active = document.activeElement;
  const textBox = active && (active.id === "search" || active.id === "bodysearch"
    || active.id === "fontnum");
  if (textBox && (!el || el !== active)) {
    active.blur();
  }
  if (el && (el.id === "search" || el.id === "bodysearch" || el.id === "fontnum")
      && dhFocusOverlay < 0 && typeof el.focus === "function") {
    el.focus();
  }
}

/* Enter a zone at its roving position and show the indicator. */
function dhFocusSetZone(zone) {
  dhFocusZone = zone;
  dhFocusApply();
}

/* C7 surface — Left/Right zone cycle (D2): prev/next in dhFocusZoneOrder,
 * cyclic wrap. dir < 0 = prev, > 0 = next. */
function dhFocusCycleZone(dir) {
  let i = dhFocusZoneOrder.indexOf(dhFocusZone);
  if (i < 0) {
    i = 0;
  }
  i = (i + (dir < 0 ? -1 : 1) + dhFocusZoneOrder.length) % dhFocusZoneOrder.length;
  dhFocusSetZone(dhFocusZoneOrder[i]);
}

/* D6 — the confirm overlay owns the model while it is up: arrows toggle
 * confirm/cancel, Enter activates, Esc stays on the existing dismiss
 * branch. Entry saves the pre-overlay zone (once — a re-show while
 * trapped must not clobber the saved state) and starts on Cancel, the
 * safe default against a double-press clearing history. */
function dhFocusEnterOverlay() {
  if (dhFocusOverlay < 0) {
    dhFocusPrev = dhFocusZone;
  }
  dhFocusOverlay = 1;  // Cancel
  dhFocusApply();
}

function dhFocusExitOverlay() {
  dhFocusOverlay = -1;
  if (dhFocusPrev) {
    dhFocusZone = dhFocusPrev;
    dhFocusPrev = null;
  }
  dhFocusApply();
}

/* Quest-row reveal: geometry reads FIRST (KB rule 1), then the single
 * container scrollTop write. The index list is small and unvirtualized,
 * so one offsetTop pair per keypress is bounded and cheap. */
function dhFocusRevealRow(row) {
  const list = document.getElementById("quests");
  if (!list) {
    return;
  }
  const top = row.offsetTop;       // reads...
  const h = row.offsetHeight;
  const view = list.clientHeight;
  const cur = list.scrollTop;
  let target = cur;
  if (top < cur) {
    target = top;
  } else if (top + h > cur + view) {
    target = top + h - view;
  }
  if (target !== cur) {
    list.scrollTop = target;       // ...then the only write
  }
}

/* After a quest-row activation rebuilds the index, land the focus back
 * on the same quest (D8): find the fresh row by data.key and adopt its
 * position; a row that filtered out lands on the clamped position. */
function dhFocusReacquireQuest(key) {
  const list = document.getElementById("quests");
  if (list) {
    const rows = list.children;
    for (let i = 0; i < rows.length; i++) {
      if (rows[i].dataset.key === key) {
        dhFocusPos.quests = i;
        dhFocusRevealRow(rows[i]);
        dhFocusApply();
        return;
      }
    }
  }
  dhFocusApply();
}

/* C6/D1 log scroll helpers: line step comes from the virtualization
 * accounting — dhBlockLayout().est is the current per-block average
 * (measured -> dhEstSeed -> kInitialRowEst fallback chain), so a key
 * repeat is O(blocks) arithmetic, zero DOM measurement. Page steps read
 * one clean clientHeight BEFORE any write (KB rule 1) and clamp against
 * the simulated total (dhBlockLayout().total — the same height the
 * spacers present to the scrollbar). The scrollTop write rides the
 * existing dhOnScroll -> dhUpdateWindow pass for windowing. */

function dhLogLineStepPx() {
  return dhBlockLayout().est / kBlockRows;
}

function dhLogScrollLines(delta) {
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const cur = log.scrollTop;  // read first — nothing written yet this key
  log.scrollTop = Math.max(0, cur + delta * dhLogLineStepPx());  // write last
}

function dhLogScrollPages(delta) {
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const cur = log.scrollTop;   // reads first...
  const view = log.clientHeight;
  const max = Math.max(0, dhBlockLayout().total - view);
  log.scrollTop = Math.min(max, Math.max(0, cur + delta * view));  // ...write last
}

/* C7 surface — page the log by whole viewports (LB/RB actions and the
 * PageUp/PageDown keys share this). */
function dhFocusPageScroll(dir) {
  dhLogScrollPages(dir < 0 ? -1 : 1);
}

/* C7 surface — oldest/newest jump (LT/RT actions): clamp against the
 * simulated total, then let the scroll event re-window. */
function dhFocusJumpLog(toNewest) {
  const log = document.getElementById("log");
  if (!log) {
    return;
  }
  const max = Math.max(0, dhBlockLayout().total - log.clientHeight);
  log.scrollTop = toNewest ? max : 0;
}

/* D5 — within the filters zone the horizontal axis moves the roving
 * checkbox (cyclic among the four); the vertical axis exits the zone.
 * Kept separate from dhFocusMove because that is the Up/Down axis. */
function dhFocusStepFilter(dir) {
  dhFocusPos.filters = (dhFocusPos.filters + (dir < 0 ? -1 : 1)
    + dhBuckets.length) % dhBuckets.length;
  dhFocusApply();
}

/* C7 surface — Up/Down within the current zone. Returns true when the
 * key was consumed; false means a zone edge (or a single-position zone)
 * and the caller falls through to zone cycling. */
function dhFocusMove(dir) {
  if (dhFocusOverlay >= 0) {  // trap: toggle between the two buttons
    dhFocusOverlay = 1 - dhFocusOverlay;
    dhFocusApply();
    return true;
  }
  if (dhFocusZone === "quests") {
    const rows = dhQuestRowElements();
    const count = rows.length;
    if (count === 0) {
      return false;
    }
    const next = dhFocusPos.quests + (dir < 0 ? -1 : 1);
    if (next < 0 || next >= count) {
      return false;  // zone edge — Up/Down move between zones
    }
    dhFocusPos.quests = next;
    dhFocusRevealRow(rows[next]);  // reads+write BEFORE the class write
    dhFocusApply();
    return true;
  }
  if (dhFocusZone === "log") {
    dhLogScrollLines(dir < 0 ? -1 : 1);  // D4: always scroll, never cycle
    return true;
  }
  return false;  // search / bodysearch / filters / clear: no vertical within — edges fall through
}

/* C7 surface — Enter/Space activation. Every path delegates to the
 * element's own wired handler: row.click() for quest rows (D8 — the
 * dhMakeEntry click closure is the single selection code path; a plain
 * synthetic click carries no Ctrl modifier), checkbox.click() for
 * filters (fires the existing change listeners), and the overlay/clear
 * functions directly (they ARE the click handlers). */
function dhFocusActivate() {
  if (dhFocusOverlay >= 0) {
    if (dhFocusOverlay === 0) {
      dhConfirmClear();
    } else {
      dhHideClearOverlay();
    }
    return true;
  }
  if (dhFocusZone === "search") {
    dhFocusSetZone("quests");  // contract §3: Enter moves to the quest index
    return true;
  }
  if (dhFocusZone === "filters") {
    const box = document.getElementById("ftype-" + dhBuckets[dhFocusPos.filters]);
    if (box) {
      box.click();
      return true;
    }
    return false;
  }
  if (dhFocusZone === "quests") {
    const row = dhFocusTarget();
    if (row) {
      const key = row.dataset.key;  // rows rebuild on click — refocus by key
      row.click();
      dhFocusReacquireQuest(key);
      return true;
    }
    return false;
  }
  if (dhFocusZone === "bodysearch") {
    return true;  // Enter/Space are owned by the input's own handlers (I7)
  }
  if (dhFocusZone === "prefs") {
    if (dhFocusPos.prefs === 2) {
      const box = document.getElementById("pausebox");
      if (box) {
        box.click();  // ruling V4: Enter/Space toggle via the change path
        return true;
      }
    }
    return true;  // slider: inert; numeric: its own keydown owns Enter
  }
  if (dhFocusZone === "log") {
    return false;  // nothing to activate
  }
  dhShowClearOverlay();  // clear button — the existing confirm flow
  return true;
}

/* Route a keypress through the focus model; the shared keydown handler
 * keeps the Esc branch and delegates everything else here. Returns true
 * when the key belongs to the model. Key names are matched with keyCode
 * fallbacks, same convention as the Esc branch — Ultralight's synthetic
 * controller KeyEvents are not guaranteed to carry full key names. */
function dhFocusOnKey(event) {
  const key = event.key;
  const code = event.keyCode;
  const up = key === "ArrowUp" || code === 38;
  const down = key === "ArrowDown" || code === 40;
  const left = key === "ArrowLeft" || code === 37;
  const right = key === "ArrowRight" || code === 39;
  const enter = key === "Enter" || code === 13;
  const space = key === " " || key === "Spacebar" || code === 32;
  const pageUp = key === "PageUp" || code === 33;
  const pageDown = key === "PageDown" || code === 34;
  if (!(up || down || left || right || enter || space || pageUp || pageDown)) {
    return false;
  }
  if (space && event.target
      && (event.target.id === "search" || event.target.id === "bodysearch"
        || event.target.id === "fontnum")) {
    return false;  // a typed character while a text box owns the real focus
  }
  if (dhFocusOverlay >= 0) {
    if (up || down || left || right) {
      event.preventDefault();
      dhFocusOverlay = 1 - dhFocusOverlay;
      dhFocusApply();
    } else if (enter) {
      event.preventDefault();
      dhFocusActivate();
    } else {
      event.preventDefault();  // Space/PageUp-Down must not act natively
    }
    return true;
  }
  if (left || right) {
    if (dhFocusZone === "prefs") {
      dhPrefsHorizontal(right ? 1 : -1, event);  // ruling V4 — never zone-cycles
      return true;
    }
    if (dhFocusZone === "search" || dhFocusZone === "bodysearch") {
      const box = document.getElementById(dhFocusZone);
      if (box && !box.value) {
        // #007: empty box — nothing to caret over, so Left/Right leave the
        // zone; a non-empty box keeps them as caret keys (typing UX)
        event.preventDefault();
        dhFocusCycleZone(right ? 1 : -1);
      }
      return true;  // D3: caret keys — no preventDefault, default caret move
    }
    event.preventDefault();
    if (dhFocusZone === "filters") {
      dhFocusStepFilter(right ? 1 : -1);  // D5: wrap among the 4 checkboxes
      return true;
    }
    dhFocusCycleZone(right ? 1 : -1);
    return true;
  }
  if (up || down) {
    event.preventDefault();
    const dir = up ? -1 : 1;
    if (dhFocusZone === "prefs") {
      dhPrefsMove(dir);  // ruling V4 — steps within the cluster, exits to clear
      return true;
    }
    if (dhFocusZone === "search") {
      dhFocusSetZone("filters");  // D3: the adjacent zone, both directions
      return true;
    }
    if (dhFocusZone === "bodysearch") {
      dhFocusSetZone("log");  // spec §6.1: Up/Down leave the box for the log
      return true;
    }
    if (up && dhFocusZone === "clear") {
      dhFocusPos.prefs = 0;  // ruling V1: ArrowUp from clear enters prefs at
      dhFocusSetZone("prefs");  // the slider — outside the legacy cycle
      return true;
    }
    if (!dhFocusMove(dir)) {
      dhFocusCycleZone(dir);  // zone edge / single-position zone
    }
    return true;
  }
  if (enter) {
    if (dhFocusZone === "bodysearch" && event.target
        && event.target.id === "bodysearch") {
      return true;  // I7: the input's own keydown handler already navigates
    }                // — activating here too would move TWICE per Enter
    if (dhFocusZone === "prefs" && event.target
        && event.target.id === "fontnum") {
      return true;  // ruling V4: the input's own keydown commits (I7 pattern)
    }
    event.preventDefault();
    dhFocusActivate();
    return true;
  }
  if (space) {
    event.preventDefault();
    if (dhFocusZone === "filters") {
      dhFocusActivate();  // contract §4: Space toggles the focused checkbox
    } else if (dhFocusZone === "prefs" && dhFocusPos.prefs === 2) {
      dhFocusActivate();  // ruling V4: Space toggles the pause checkbox
    }
    return true;  // swallowed elsewhere — no native button/checkbox surprise
  }
  if (pageUp || pageDown) {
    event.preventDefault();
    if (dhFocusZone === "log") {
      dhLogScrollPages(pageDown ? 1 : -1);
    }
    return true;
  }
  return false;
}

/* D9 — view-open focus: the snapshot replay that opens the view (and the
 * post-clear re-push) resets the model to its contract start — the log
 * pane, or the quest index when there is nothing to read yet. */
function dhFocusReset() {
  dhFocusOverlay = -1;
  dhFocusPrev = null;
  dhFocusZone = dhAllLines.length === 0 ? "quests" : "log";
  dhFocusApply();
}

/* Keep the model honest about mouse use (contract: optional, cheap): map
 * a click back to its zone so the next arrow key continues from where
 * the user clicked. parentNode walks — Element.closest is not guaranteed
 * under Ultralight's JSC (dhTipFor convention). Bubbling lands this AFTER
 * the target's own handler, so a rebuilt quest list is re-queried live. */
function dhFocusSyncFromClick(event) {
  if (dhFocusOverlay >= 0) {
    return;  // the trap owns the model until dismissed
  }
  let node = event.target;
  while (node && node !== document) {
    const id = node.id;
    if (id === "search") {
      dhFocusZone = "search";
      dhFocusApply();
      return;
    }
    if (id === "bodysearch") {
      dhFocusZone = "bodysearch";
      dhFocusApply();
      return;
    }
    if (id && id.indexOf("ftype-") === 0) {
      dhFocusZone = "filters";
      const b = dhBuckets.indexOf(id.slice(6));
      if (b >= 0) {
        dhFocusPos.filters = b;
      }
      dhFocusApply();
      return;
    }
    if (id === "filters") {
      dhFocusZone = "filters";  // label-text click: keep the roving box
      dhFocusApply();
      return;
    }
    if (node.classList && node.classList.contains("qentry")) {
      const rows = dhQuestRowElements();  // placeholder rows never sync (I6)
      for (let i = 0; i < rows.length; i++) {
        if (rows[i] === node) {
          dhFocusZone = "quests";
          dhFocusPos.quests = i;
          dhFocusApply();
          return;
        }
      }
      return;
    }
    if (id === "log" || (node.classList && node.classList.contains("line"))) {
      dhFocusZone = "log";
      dhFocusApply();
      return;
    }
    if (id === "fontslider" || id === "fontnum" || id === "pauselabel"
        || id === "pausebox") {
      dhFocusZone = "prefs";  // 2026-10-24 header cluster (ruling V1)
      dhFocusPos.prefs = id === "fontslider" ? 0 : id === "fontnum" ? 1 : 2;
      dhFocusApply();
      return;
    }
    if (id === "clear") {
      dhFocusZone = "clear";
      dhFocusApply();
      return;
    }
    if (id === "overlayconfirm" || id === "overlaycancel") {
      return;  // overlay clicks flow through show/hide — trap stays put
    }
    node = node.parentNode;
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
    dhWireTypingEmit(search);  // #005: report focus/blur to the plugin
  }
  /* Body search bar (Chunk B): the input carries the debounced commit +
   * Enter/Shift+Enter navigation via dhBodySearchWireInput; the ‹ ›
   * buttons walk matches for the mouse. Every lookup is guarded — old
   * html without the bar stays inert. */
  const bodySearch = document.getElementById("bodysearch");
  if (bodySearch) {
    dhBodySearchWireInput(bodySearch);
    dhWireTypingEmit(bodySearch);  // #005: report focus/blur to the plugin
  }
  const bodySearchPrev = document.getElementById("bodysearchprev");
  if (bodySearchPrev) {
    bodySearchPrev.addEventListener("click", dhBodySearchPrev);  // ‹ = older
  }
  const bodySearchNext = document.getElementById("bodysearchnext");
  if (bodySearchNext) {
    bodySearchNext.addEventListener("click", dhBodySearchNext);  // › = newer
  }
  for (const bucket of dhBuckets) {
    const box = document.getElementById("ftype-" + bucket);
    if (box) {
      box.addEventListener("change", dhFilterChanged);
      box.addEventListener("change", dhReportFilters);  // #001: after dhFilterChanged
    }
  }
  /* Header preferences (2026-10-24): slider previews locally (input) and
   * commits once per completed gesture (change); the numeric commits on
   * Enter/blur and restores the last valid value on anything invalid;
   * the pause checkbox commits immediately. Every lookup is guarded — old
   * html without the cluster stays inert. */
  const prefs = document.getElementById("prefs");
  if (prefs) {
    // The header is the panel drag handle: keep cluster mousedowns from
    // bubbling into dhStartDrag("move") (same pattern as #clear).
    prefs.addEventListener("mousedown", function (event) {
      event.stopPropagation();
    });
  }
  const fontSlider = document.getElementById("fontslider");
  if (fontSlider) {
    fontSlider.addEventListener("input", function () {
      dhPrefsPreviewFont(Number(fontSlider.value));  // local only — zero sends
    });
    fontSlider.addEventListener("change", function () {
      dhPrefs.fontSize = Number(fontSlider.value);
      setFontSize(dhPrefs.fontSize);  // C1: release applies body+header once
      dhPrefsSyncControls();
      dhPrefsCommit();  // one combined payload per completed gesture
    });
  }
  const fontNum = document.getElementById("fontnum");
  if (fontNum) {
    dhWireTypingEmit(fontNum);  // #005: report focus/blur to the plugin
    fontNum.addEventListener("keydown", function (event) {
      if (event.key === "Enter" || event.keyCode === 13) {
        dhPrefsCommitNumeric();  // valid commits; invalid restores, no send
      }
    });
    fontNum.addEventListener("blur", function () {
      if (dhPrefsEscBlur) {
        dhPrefsEscBlur = false;  // one-shot (ruling V2): Esc already restored
        dhPrefsRestoreNumeric();
        return;
      }
      dhPrefsCommitNumeric();  // every non-Esc blur commits
    });
  }
  const pauseBox = document.getElementById("pausebox");
  if (pauseBox) {
    pauseBox.addEventListener("change", function () {
      dhPrefs.pauseOnOpen = pauseBox.checked;
      dhPrefsCommit();  // immediate; the native side refocuses itself
    });
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
  window.addEventListener("prisma-controller-action", dhOnControllerAction);  // C7: V12 controller actions
  window.addEventListener("prisma-input-device-change", dhOnInputDeviceChange);  // C8: device switch mid-session
  document.addEventListener("click", dhFocusSyncFromClick);  // C6: model follows mouse
  document.addEventListener("keydown", (event) => {
    // Ultralight reports Escape as "Unidentified" — match by keyCode.
    if (event.key === "Escape" || event.keyCode === 27) {
      if (dhOverlayVisible()) {
        dhHideClearOverlay();  // dismiss only; the panel stays open
        return;
      }
      // Esc chain (AC5, invariant 6): a focused NON-EMPTY search box
      // clears FIRST and the panel stays open; a focused EMPTY box leaves
      // the text zone (#007 — where Down would land: search -> filters,
      // bodysearch -> log); only then does Esc close. Overlay dismiss
      // stays above all three. A focused font numeric (2026-10-24)
      // restores its uncommitted text and returns to the clear zone —
      // the blur the move causes must NOT commit, so the one-shot
      // dhPrefsEscBlur flag suppresses the blur-commit (ruling V2/V3).
      const target = event.target;
      if (target && target.id === "bodysearch" && target.value) {
        dhBodySearchClear();  // clears term + box + count; LB/RB revert
        return;
      }
      if (target && target.id === "search" && target.value) {
        target.value = "";  // symmetric clear for the quest-name box
        dhRenderIndex();
        return;
      }
      if (target && target.id === "fontnum") {
        dhPrefsEscBlur = true;  // suppress the blur-commit this move causes
        dhPrefsRestoreNumeric();  // discard uncommitted text, send nothing
        dhFocusSetZone("clear");  // leave prefs back to the entry zone
        return;
      }
      if (target && (target.id === "search" || target.id === "bodysearch")) {
        dhFocusSetZone(target.id === "search" ? "filters" : "log");  // blurs the box
        return;
      }
      dhSend("closeRequested");
      return;
    }
    dhFocusOnKey(event);  // C6: arrows/Enter/Space/PageUp-Down (controller-ready)
  });
});
