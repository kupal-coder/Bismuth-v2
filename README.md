# Bismuth
Rendering optimization mod with the goal of fully
rewriting the rendering side of Geometry Dash
for maximum performance.

Currently experimental

## Platforms

* **Windows** – the original target, OpenGL 4.3 required.
* **Android** (32/64) – supported on devices with OpenGL ES 3.1; the mod falls
  back to the game's own renderer everywhere else. See [MOBILE.md](MOBILE.md)
  for requirements, what degrades and how it was ported.
* **macOS / iOS** – build, but neither platform exposes the GL 4.3 / GLES 3.1
  features the renderer needs, so Bismuth stays disabled there.

<img src="logo.png" alt="Bismuth logo" width="100"/>

## License

Bismuth - Optimization mod for Geometry Dash

Copyright (C) 2026 IliasHDZ

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

THIS PROJECT CANNOT BE USED UNDER ANY CIRCUMSTANCES TO FACILITATE OR
ADVANCE GEOMETRY DASH LEVEL GENERATION OR MODIFICATION USING
GENERATIVE AI.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.