/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text, questId, questName}
 *                                     (schema v2; older DLLs omit the quest
 *                                      fields — defaults are applied below)
 * JS -> plugin: window.requestHistory() on DOM ready
 *               window.closeRequested() on Esc
 * (RegisterJSListener binds each name as a global window function;
 * window.prisma has no sendEvent — its emit() routes to Papyrus only.)
 * Two-pane model (M4): dhAllLines is the client-side source of truth (the
 * plugin only pushes; it never edits or removes). The left pane is a
 * projection grouped on questId — "All", then one entry per quest sorted by
 * most recent line, with "Unattributed" for questId-0 lines (only when such
 * lines exist). Old-DLL payloads without quest fields land in Unattributed.
 * Scroll contract: snap to bottom on open and on selection switch;
 * auto-scroll on append only when the user is already at the bottom. */
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

/* Deterministic per-NPC speaker color: stable hue from the name hash, so a
 * given NPC keeps one color within and across conversations. Player lines
 * keep the CSS gold (.line.player); other kinds keep the Pip-Boy green. */
function dhNameColor(name) {
  let h = 0;
  for (let i = 0; i < name.length; i++) {
    h = (h * 31 + name.charCodeAt(i)) >>> 0;
  }
  let hue = h % 360;
  if (hue >= 15 && hue <= 75) {
    hue = (hue + 120) % 360;  // keep clear of the player gold (~45)
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
  document.addEventListener("keydown", (event) => {
    // Ultralight reports Escape as "Unidentified" — match by keyCode.
    if (event.key === "Escape" || event.keyCode === 27) {
      dhSend("closeRequested");
    }
  });
});
