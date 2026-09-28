/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text, questId, questName}
 *                                     (schema v2; older DLLs omit the quest
 *                                      fields — defaults are applied below)
 *               setFontSize(px)
 *               setGeometry(json)   {"x","y","width","height"} — persisted
 *                                     panel rect; absent/invalid = keep the
 *                                     default centered layout
 * JS -> plugin: window.requestHistory() on DOM ready
 *               window.closeRequested() on Esc
 *               window.geometryChanged(json) once per drag end (M5)
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
 * only, no persistence); quest names in the index word-wrap. */
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
 * payloads: missing quest fields default to 0/"", missing speaker/text to
 * "", unknown kind to "unknown". Returns null for non-object input. */
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
    questName: typeof line.questName === "string" ? line.questName : ""
  };
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
 * each {questId, key, name, count, last}, sorted by most recent line. */
function dhBuildIndex() {
  const quests = new Map();
  for (let i = 0; i < dhAllLines.length; i++) {
    const line = dhAllLines[i];
    const key = String(line.questId);
    let entry = quests.get(key);
    if (!entry) {
      entry = { questId: line.questId, key: key, name: "", count: 0, last: i };
      quests.set(key, entry);
    }
    entry.count += 1;
    entry.last = i;
    if (!entry.name && line.questName) {
      entry.name = line.questName;
    }
  }
  const entries = Array.from(quests.values());
  entries.sort(function (a, b) { return b.last - a.last; });
  return entries;
}

function dhMakeEntry(key, label, count) {
  const row = document.createElement("div");
  row.className = "qentry" + (key === dhSelection ? " active" : "");
  row.dataset.key = key;
  const name = document.createElement("span");
  name.className = "qname";
  name.textContent = label;
  const num = document.createElement("span");
  num.className = "qcount";
  num.textContent = String(count);
  row.append(name, num);
  row.addEventListener("click", function () {
    dhSelection = key;
    dhRenderIndex();
    dhRenderLog();
  });
  return row;
}

/* Left pane: "All" plus the quest buckets, filtered by the search box on
 * label substring (case-insensitive). Selection is not reset by a search —
 * the right pane keeps showing the selected bucket while its entry is
 * filtered out, and clearing the search restores it. */
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
    const label = entry.questId === 0 ? "Unattributed" : dhQuestLabel(entry);
    if (matches(label)) {
      list.append(dhMakeEntry(entry.key, label, entry.count));
    }
  }
}

/* Append one model line to the log. autoScroll=true applies the live-append
 * contract (scroll only when already at the bottom); false always snaps. */
function dhAppendRow(log, line, autoScroll) {
  const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
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
  log.append(row);
  if (!autoScroll || atBottom) {
    log.scrollTop = log.scrollHeight;
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

/* Right pane: re-render the selected bucket and snap to its newest line. */
function dhRenderLog() {
  const log = document.getElementById("log");
  log.textContent = "";
  for (const line of dhAllLines) {
    if (dhSelection === "all" || String(line.questId) === dhSelection) {
      dhAppendRow(log, line, false);
    }
  }
  dhSetEmptyVisible(dhAllLines.length === 0);
  log.scrollTop = log.scrollHeight;
}

function setHistory(lines) {
  lines = dhParse(lines, []);
  if (!Array.isArray(lines)) {
    lines = [];
  }
  // Full snapshot replay (sent on panel open): reset to the defaults —
  // "All" selected, search cleared — then rebuild model and panes.
  dhAllLines.length = 0;
  dhSelection = "all";
  const search = document.getElementById("search");
  if (search) {
    search.value = "";
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
  if (dhSelection === "all" || String(line.questId) === dhSelection) {
    dhSetEmptyVisible(false);
    dhAppendRow(document.getElementById("log"), line, true);
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

document.addEventListener("DOMContentLoaded", () => {
  dhSend("requestHistory");
  const search = document.getElementById("search");
  if (search) {
    search.addEventListener("input", dhRenderIndex);
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
  document.addEventListener("keydown", (event) => {
    // Ultralight reports Escape as "Unidentified" — match by keyCode.
    if (event.key === "Escape" || event.keyCode === 27) {
      dhSend("closeRequested");
    }
  });
});
