# Shaders

## CRT Shader

By Anata on raylib Discord, tweaked accordingly.

## Scanlines

This is the standard scanline shader from the raylib example.

- https://github.com/raysan5/raylib/blob/master/examples/shaders/resources/shaders/glsl100/scanlines.fs
- https://github.com/raysan5/raylib/blob/master/examples/shaders/resources/shaders/glsl120/scanlines.fs
- https://github.com/raysan5/raylib/blob/master/examples/shaders/resources/shaders/glsl330/scanlines.fs

## xBRZ

- https://github.com/Mortalitas/GShade/blob/master/Shaders/4xBRZ.fx

```
////////////////////////////////////////////////////////////////////////////////
//
// 4xBRZ filter from (DirectX 9+ & OpenGL)
//
//    https://github.com/libretro/common-shaders/blob/master/xbrz/shaders/4xbrz.cg
//
// Ported by spiderh @2018
//
// NOTE:  Only work with pixelated games.
//
// ----------------------------------------------------------------------------
//
//     Strength = Height / Original Height
//
// Example in Mame, one game's original resolution is  "384 x 214"  then:
//
//     Strength = 1080 / 214 = 4.8214
//
// If you use  "1920 x 1080"  resolution, for most games use  "Strength = 6",
// If you use  "1280 x 720" then use  "Strength = 4".
//
//
////////////////////////////////////////////////////////////////////////////////
//
// 4xBRZ shader - Copyright (C) 2014-2016 DeSmuME team
//
// This file is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the this software.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////
/*
   Hyllian's xBR-vertex code and texel mapping

   Copyright (C) 2011/2016 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/
////////////////////////////////////////////////////////////////////////////////
```

## TXT Escaper

The .txt files is the shader code escaped for C code:

https://tomeko.net/online_tools/cpp_text_escape.php
