# Provenance

`src/genie.c` and `src/genie.h` are a C reimplementation of the genie mesh and shadow
algorithm recovered by reverse-engineering Apple's macOS 26.7.2 Dock and SkyLight binaries.
They are not a clean-room implementation and are not affiliated with or endorsed by Apple.
No Apple binaries or decompiler output are included. The code intentionally mirrors the
original constants and rounding so the mesh is bit-exact. The tests include recorded mesh and
shadow vectors captured from the live macOS Dock and SkyLight; these are behavioral test data,
not Apple binaries or decompiler output.

The KWin effect integration, rendering, timing and tests are original work.
