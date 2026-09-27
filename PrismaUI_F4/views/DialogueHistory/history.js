/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text}
 * JS -> plugin: window.requestHistory() on DOM ready
 *               window.closeRequested() on Esc
 * (RegisterJSListener binds each name as a global window function;
 * window.prisma has no sendEvent — its emit() routes to Papyrus only.)
 * Scroll contract: snap to bottom on open; auto-scroll on append only
 * when the user is already at the bottom. */
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

function setHistory(lines) {
  lines = dhParse(lines, []);
  if (!Array.isArray(lines)) {
    lines = [];
  }
  const log = document.getElementById("log");
  log.textContent = "";
  for (const line of lines) {
    appendLine(line);
  }
  dhSetEmptyVisible(lines.length === 0);
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
  line = dhParse(line, null);
  if (!line || typeof line !== "object") {
    return;
  }
  dhSetEmptyVisible(false);
  const log = document.getElementById("log");
  const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
  const row = document.createElement("div");
  row.className = "line " + line.kind;
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
  if (atBottom) {
    log.scrollTop = log.scrollHeight;
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
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      dhSend("closeRequested");
    }
  });
});
