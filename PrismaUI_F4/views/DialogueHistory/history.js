/* FO4 Dialogue History — PrismaUI view.
 * Plugin -> JS: setHistory(array)   full snapshot, oldest first
 *               appendLine(object)  one {speaker, kind, text}
 * JS -> plugin: prisma.sendEvent("requestHistory") on DOM ready
 *               prisma.sendEvent("closeRequested") on Esc
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
  if (window.prisma && typeof window.prisma.sendEvent === "function") {
    window.prisma.sendEvent(eventName, "");
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
