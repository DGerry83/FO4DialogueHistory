# Third-Party Notices

FO4 Dialogue History is built with the help of the following open-source
projects and community work. This file reproduces the license notices
required by those projects and credits the work that made this mod possible.

FO4 Dialogue History itself is licensed under the MIT License — see the
`LICENSE` file in this repository.

---

## CommonLibF4 (All Versions fork)

The plugin is statically linked against the "All Versions" fork of
CommonLibF4 maintained by LucaDotGit
(<https://github.com/LucaDotGit/CommonLibF4>), which is licensed under the
MIT License:

> MIT License
>
> Copyright (c) 2019 ryan-rsm-mckenzie
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## {fmt}

<https://github.com/fmtlib/fmt> — statically linked.

> Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors
>
> Permission is hereby granted, free of charge, to any person obtaining
> a copy of this software and associated documentation files (the
> "Software"), to deal in the Software without restriction, including
> without limitation the rights to use, copy, modify, merge, publish,
> distribute, sublicense, and/or sell copies of the Software, and to
> permit persons to whom the Software is furnished to do so, subject to
> the following conditions:
>
> The above copyright notice and this permission notice shall be
> included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
> EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
> MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
> NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
> LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
> OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
> WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## spdlog

<https://github.com/gabime/spdlog> — statically linked. Licensed under the
MIT License:

> The MIT License (MIT)
>
> Copyright (c) 2016 Gabi Melman.
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Catch2

<https://github.com/catchorg/Catch2> — licensed under the Boost Software
License 1.0. Used only to build and run the project's unit tests; it is not
linked into or distributed with the released plugin.

---

## Credits and runtime dependencies

The following projects are **not** distributed with FO4 Dialogue History and
remain the property of their respective authors. They are listed here in
thanks and attribution.

### FloatingSubtitlesF4 (powerof3)

<https://github.com/powerof3/FloatingSubtitlesF4> — MIT License,
Copyright (c) 2025 powerof3.

powerof3's FloatingSubtitlesF4 demonstrated the technique this mod's subtitle
capture is built on: a prologue detour on `SubtitleManager::ShowSubtitle`.
The OG (1.10.163) speaker-resolution ordering additionally draws on
northaxosky's 1.10.163 port of that mod. No code was copied — the hook here
is implemented against the CommonLibF4 AV fork's own hooking machinery — but
both projects are gratefully acknowledged as the proven reference.

### PrismaUI F4 (NomadsReach / StarkMP)

<https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework>

The dialogue history window is an HTML/CSS/JS view rendered by PrismaUI F4,
the Fallout 4 port of StarkMP's Prisma UI framework, maintained by
NomadsReach. PrismaUI F4 is a **required runtime dependency** and must be
installed separately; none of its files are distributed with this mod. The
plugin communicates with PrismaUI through its published developer API header
(`PrismaUI_F4_API.h`), which its authors explicitly invite modders to copy
into their own projects. PrismaUI F4 embeds the Ultralight rendering engine,
which is separately and commercially licensed by Ultralight, Inc. — those
terms ship with PrismaUI F4 itself.

### F4SE and Address Library

- **Fallout 4 Script Extender (F4SE)** — <https://f4se.silverlock.org/> —
  required runtime; the plugin loads through F4SE's plugin interface. F4SE is
  the work of the xSE team and is not distributed with this mod.
- **Address Library for F4SE Plugins** (meh321) —
  <https://www.nexusmods.com/fallout4/mods/47327> — required runtime;
  provides the version-independent address database the plugin resolves its
  engine hook through. Not distributed with this mod.
