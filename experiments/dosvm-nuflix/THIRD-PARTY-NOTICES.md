# Upstream source used by the isolated experiment

NUFLIX Studio by Patai Gergely: https://github.com/cobbpg/nuflix-studio
Pinned commit: `b33f4d93875a3fdbabaf52216811962847fdfcf1`.

The runner reads the unmodified Constants.cs, CodeGeneration.cs,
NuflixFormat.cs and nufli-template.bin from a private build-directory checkout.
Its generated C64 programs contain that displayer. Keep this notice and the
upstream LICENSE with any copies. Headless.cs replaces Unity storage and GPU
color fitting with an explicitly constrained CPU experiment; it is not the
author's exhaustive GPU conversion algorithm.

The native-fit.h follow-up implements the same bounded search with exact
ties and incremental regions. native-display.h/cpp now port the pinned
upstream scheduling and packing algorithms to allocation-free C++.
native-export.h/native-data.mjs retain the upstream memory layout and
displayer template. The private live DOSVM F5 build uses this native path;
the managed exporter remains the independent picture reference. The
double-buffer.mjs experiment relocates that displayer into the other two
VIC banks and adapts its bank-select and NTSC indexed-write operands; the
generated mirrored programs retain the same upstream MIT notice/license.
The live double-client/double-video follow-up scatters the existing DOS receiver
around those two images. double-export/double-host implement the matching native
relocation, inactive-bank upload and swap protocol; they retain the same upstream
displayer and scheduler attribution and license.
The double-buffer-only bounded underlay deferral extends the author's existing
overflow-resolution policy for dynamic images; it can postpone a color write
that the upstream scheduling heuristics could not fit. The top-border fix
repositions the receiver's IRQ dispatcher and initializes the otherwise unused
sprite pointers without changing the upstream visible-line instruction stream.
The native-cache.h follow-up retains raw fitting choices and exact scheduled
colors in post-pack scratch space, selectively repacks rows, and tail-aligns
the generated routine. Its exporter patches both template calls to that entry;
it still uses the upstream scheduler and displayer, with the license below.
The
separate, earlier NFLXBEN module measures only native color fitting; it is
not a DOS interpreter or live NUFLIX display engine. Its text font is the existing
public-domain Daniel Hepper/Marcel Sondaar/IBM font, whose notice is retained
in engine/native-dos/mpe5_font8x8.h.

MIT License

Copyright (c) 2024 Patai Gergely

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
