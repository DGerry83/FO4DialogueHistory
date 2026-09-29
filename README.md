# FO4 Dialogue History

A Fallout 4 mod that records and stores dialogue history so you can read it later.  By pulling the subtitles directly, FO4 Dialogue History ensures the history contains what was actually said, all without needing subtitles to be enabled in the options menu.

## How to use/Features

The default hotkey is 'H', just press it and the window will open with a fairly straightforward interface.  Filter the dialogue by clicking on a quest on the left pane, or choose "all" to see all past dialogue at once. The "clear" button respects your selection on the left pane, so you can erase individual quest logs or the entire dialogue history.  The top of the window has checkboxes to allow you to filter quest categories:  Main, Side, Misc, and "Unattributed".  The panel is fully resizable/draggable and remembers its size/position between sessions.

Not *all* dialogue is captured.  Random NPC lines that get spoken as you're walking around for ambience are intentionally not recorded.  Some player opening lines for dialogue are also not recorded, and where this happens it's because the dialogue lines are assigned to a generic "player lines" quest.

The INI file (FO4 Dialogue History\F4SE\Plugins\FO4DialogueHistory.ini) contains a few settings you can customize:

- Hotkey lets you customize the hotkey to open the window.
- BufferSize lets you set a maximum number of lines to store(the buffer works on a "first in, first out" principle, so the oldest line gets bumped out for the newest one when you hit the limit)
- FontSize lets you...well, set the font size obviously.
- VerboseCapture will write extra data about every line being detected including ones that are rejected from capture(you probably don't need to turn this on, but if you're seeing dialogue ending up in the wrong place, not being captured when it should or vice versa, this option will generate the diagnostic logging necessary to figure out what's going on)
- StressTestKey sets the hotkey for firing off the "stress test", which was really just for me to verify that the mod could handle having thousands of lines in the history.  StressTestLines and StressTestQuests do what they say.


## Technical info

The mod will save the dialogue history into your f4se "cosave" file - this is the .f4se file that rides along side your .fos save files.  In my testing I've found that it's about 1MB of cosave per 5000 lines of dialogue history, give or take.  I tested up to 35,000 lines and a 6MB cosave file and did not notice any issues, save loading was not delayed and the dialogue history window was still responsive with quest selections and scrolling still feeling snappy.  If you use the clear feature to erase the stored dialogue and save your game, you'll see the cosave file deflate back to a more normal size of a few tens of KB.

## Install/Uninstall info

Installation is the same as any other F4SE plugin for the most part - just install the zip with your modloader of choice.  I use MO2, so for that you just drag and drop the release zip into your downloads panel and then install the mod.  It includes no ESP/ESL, just the f4se dll, ini file for settings, and the PrismaUI files for the UI.

You can uninstall simply by disabling and removing the mod, however the cosave data will remain.  It shouldn't "do" anything other than take up some space, and you shouldn't notice any differences with save loading times etc. I would recommend using the "clear" function to clear your entire dialogue history before removing the mod though just to get rid of that data.  I don't have any reason to think it would be a problem, but it's just the cleaner way to uninstall.

## Requirements

- Fallout 4 (1.10.163 or next-gen 1.10.984+) - I've tested on 1.10.163, but the project was built using the "All Versions" fork of CommonLibF4 - https://github.com/LucaDotGit/CommonLibF4 - and so it *should* work on later versions as well.
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