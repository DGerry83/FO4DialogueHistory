/* Placeholder — real implementation at milestone 4.
 * See index.html for the plugin<->JS contract. */
"use strict";

function setHistory(lines) {
  const log = document.getElementById("log");
  log.textContent = "";
  for (const line of lines) appendLine(line);
}

function appendLine(line) {
  const log = document.getElementById("log");
  const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
  const row = document.createElement("div");
  row.className = "line " + line.kind;
  const speaker = document.createElement("span");
  speaker.className = "speaker";
  speaker.textContent = line.speaker + ":";
  const text = document.createElement("span");
  text.textContent = " " + line.text;
  row.append(speaker, text);
  log.append(row);
  if (atBottom) log.scrollTop = log.scrollHeight;
}
