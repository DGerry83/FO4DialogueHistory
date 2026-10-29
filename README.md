# FO4 Dialogue History

A Fallout 4 mod that records and stores dialogue history so you can read it later.  By pulling the subtitles directly, FO4 Dialogue History ensures the history contains what was actually said, all without needing subtitles to be enabled in the options menu.

## How to use & Features

The default hotkey is 'H', just press it and the window will open with a fairly straightforward interface. The panel has three tabs: **Dialogue History**, **Message History**, and **Note History**. Filter the dialogue by clicking on a quest on the left pane, or choose "all" to see all past dialogue at once. The "clear" button respects your selection on the left pane, so you can erase individual quest logs or the entire dialogue history.  The top of the window has checkboxes to allow you to filter quest categories:  Main, Side, Misc, and "Unattributed".  The panel is fully resizable/draggable and remembers its size/position between sessions.

The **Message History** tab shows the recent top-left HUD messages (item pickups, quest updates, warnings, radio messages, and similar notifications) as a simple chronological log. Its search box filters the list to case-insensitive substring matches without highlighting; clearing the box restores every message. The most recent messages are kept up to the configured limit (see `MessageBufferSize` below), and its Clear button only ever affects the message history, never the dialogue history. Message capture runs in the background regardless of which tab you're looking at.

The **Note History** tab records notes and documents actually read after installation. Pickup alone only remembers first-encounter metadata and never makes a note visible. Select a title to read its body, or search titles and resolved body text. The detail shows when and where the note was first picked up or read, whichever happened first. Magazines and texture-only records are excluded. Static note bodies resolve from their source records; if a source plugin is missing, the title and metadata remain and the body is marked unavailable. Dynamic notes retain their resolved text. Note History has no capacity limit, and its Clear button removes only note history. OG 1.10.163 is supported by the current prepared-text capture hook; NG/AE requires separate hook verification and is not yet enabled for note capture.

Some HUD mods use the notification channel for invisible widget-update traffic. `FO4DialogueHistory.MessageFilters.ini`, beside the main INI, excludes these messages by exact, case-sensitive text prefix. To add a rule, put `ExcludedPrefix = the exact prefix` under `[MessageFilters]`, one rule per line, then restart the game. The file ships with rules for HUDFramework and Companion Status Enhanced, plus examples and instructions. Keep prefixes specific: a broad rule can hide legitimate notifications from history. With `VerboseCapture` enabled, the log names the prefix responsible for each rejection. Advanced users with the HUDFramework-patched HUD menu can add `ExcludedPrefix = |` as a catch-all for its command/widget channel, but this **may inadvertently filter legitimate messages**, especially with other HUD replacements or without that patch.

Not *all* dialogue is captured.  Random NPC lines that get spoken as you're walking around for ambience are intentionally not recorded.  Some player opening lines for dialogue are also not recorded, and where this happens it's because the dialogue lines are assigned to a generic "player lines" quest.

The search above the right panel allows you to search the entire Dialogue History body of text.  Results will both filter the quest list so that only quests containing the searched term are shown, and the dialogue lines containing the term will be highlighted.  A total count will also be shown with arrow buttons for moving forward/back through the list of found results.

The header includes panel-wide font-size controls and a **Pause on open** checkbox. Drag the slider for sizes 10–48 px, or enter a size in that range in the adjacent box and press Enter. During a slider drag, the quest list, dialogue, overlays, and tooltips update live while the header stays still; releasing the slider applies the new header size. Numeric entry and keyboard/controller changes update all text immediately. Pause on open is off by default; enabling it pauses the game while the panel is open, and closing the panel resumes play. Both choices are saved in `FO4DialogueHistory.preferences.ini` beside the plugin and restored on restart. Keyboard/controller users can reach these controls with Up from the Clear button.

The INI file (FO4 Dialogue History\F4SE\Plugins\FO4DialogueHistory.ini) contains a few settings you can customize:

- Hotkey lets you customize the hotkey to open the window.
- BufferSize lets you set a maximum number of lines to store(the buffer works on a "first in, first out" principle, so the oldest line gets bumped out for the newest one when you hit the limit)
- MessageBufferSize does the same for the Message History tab: the maximum number of HUD messages kept (1-1000, default 200). This buffer is always bounded — unlike dialogue there is no unlimited mode — and the oldest message is dropped first.
- NoteGenericTitles optionally overrides the case-insensitive exact titles treated as generic when generating Note History labels (default `Note|Letter|Ransom Note`). Generic and later-duplicate titles use a short resolved opening, with numeric suffixes for remaining collisions.
- FontSize sets the initial panel font size (10–48 px). In-game changes take precedence.
- PauseOnOpen sets the initial pause preference (0 = off, 1 = on). In-game changes take precedence.
- VerboseCapture will write extra data about every line being detected including ones that are rejected from capture(you probably don't need to turn this on, but if you're seeing dialogue ending up in the wrong place, not being captured when it should or vice versa, this option will generate the diagnostic logging necessary to figure out what's going on)
- StressTestKey sets the hotkey for firing off the "stress test", which was really just for me to verify that the mod could handle having thousands of lines in the history.  StressTestLines and StressTestQuests do what they say.

Controller support has been added.  Please note that I do not play with a controller so I did my best at making it feel "right" to navigate.  I'm open to feedback on any changes or improvements I could make.  The ini file provides space for setting the default controller hotkey/button combo.

## Technical info

The mod will save the dialogue history into your f4se "cosave" file - this is the .f4se file that rides along side your .fos save files.  In my testing I've found that it's about 1MB of cosave per 5000 lines of dialogue history, give or take.  I tested up to 35,000 lines and a 6MB cosave file and did not notice any issues, save loading was not delayed and the dialogue history window was still responsive with quest selections and scrolling still feeling snappy.  Message History and Note History use independent `MSGS` and `NOTE` records in the same cosave, so all three histories remain isolated and a malformed record only affects its own history. Pending note pickups persist but never appear in the UI. Saved form IDs are remapped on load; missing source plugins preserve note titles and metadata with an unavailable-body message. If you use the clear feature to erase the stored dialogue and save your game, you'll see the cosave file deflate back to a more normal size of a few tens of KB.

## Install/Uninstall info

Installation is the same as any other F4SE plugin for the most part - just install the zip with your modloader of choice.  I use MO2, so for that you just drag and drop the release zip into your downloads panel and then install the mod.  It includes no ESP/ESL, just the f4se dll, ini file for settings, and the PrismaUI files for the UI.

You can uninstall simply by disabling and removing the mod, however the cosave data will remain.  It shouldn't "do" anything other than take up some space, and you shouldn't notice any differences with save loading times etc. I would recommend using the "clear" function on each tab to clear your entire dialogue and message history before removing the mod though just to get rid of that data.  I don't have any reason to think it would be a problem, but it's just the cleaner way to uninstall.

## Requirements

- Fallout 4 (OG 1.10.163, NG, or AE 1.11.240) - Tested on 1.10.163 and 1.11.240, NG should work but please report if it does not.
- F4SE (the correct version for YOUR game install)
- Address Library for F4SE
- PrismaUI F4 ≥ 2.2.1.1

## Repository layout

- `src/` - F4SE C++ plugin source
- `PrismaUI_F4/views/DialogueHistory/` - HTML/JS/CSS view assets (deploy under `Data/`)
- `F4SE/Plugins/` - INI settings (deploy under `Data/`)
- `tests/Core.Tests/` - Catch2 unit tests for the engine-independent Core layer (dialogue buffer, conversation filter, cosave codec, panel geometry, payload builder)

## Credits and Acknolwedgements

- CommonLibF4 (All Versions fork) - https://github.com/LucaDotGit/CommonLibF4
- {fmt} - https://github.com/fmtlib/fmt 
- spdlog - https://github.com/gabime/spdlog
- Catch2 - https://github.com/catchorg/Catch2
- FloatingSubtitlesF4 (powerof3) - https://github.com/powerof3/FloatingSubtitlesF4
- PrismaUI F4 (NomadsReach / StarkMP) - https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework
- F4SE and Address Library - https://f4se.silverlock.org/ and https://www.nexusmods.com/fallout4/mods/47327