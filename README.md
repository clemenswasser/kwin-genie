# Genie — macOS genie minimize effect for KWin

Native KWin effect plugin reproducing the macOS Tahoe genie minimize/restore animation. It appears
under **System Settings → Animations → Window minimize**.

![Genie minimize effect](docs/genie-demo.gif)

The mesh generator is a bit-exact port of the Dock's genie routine, and rendering matches
WindowServer's per-cell projective texture mapping. By default the funnel is sampled densely enough
to avoid visible facets; set `GENIE_QUALITY=exact` for the original Dock mesh. Timing is 0.5 s;
hold **Shift** for 10× slow motion.

## Build

Needs `kwin-devel` (6.8, including `KWinDBusInterface`), `extra-cmake-modules`, Qt 6 and KF6
CoreAddons/Config/ConfigWidgets/KCMUtils development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Select **Genie** in System Settings → Animations → Window minimize.

After a rebuild, use `tools/reload_effect.sh`; KWin may keep the old library mapped under the same
file name.

## Mesh quality

By default the funnel is subdivided to roughly 12 device pixels. This is the measured
performance/quality knee for large and HiDPI windows: 8 px is a little smoother but costs about
0.9 ms more per frame, while 16 px is slightly cheaper and 24 px is nearly free but closer to the
exact mesh. These settings are available in **System Settings → Animations → Window minimize →
Genie**. For testing, `GENIE_MESH_PX` and `GENIE_QUALITY=exact` can override the configured values
in KWin's environment; the exact mesh is bit-identical to the recovered Dock output, but it can show
facets when a very large window is compressed into a small target.

## Provenance

The mesh and shadow algorithm are a reverse-engineered reimplementation; see
[PROVENANCE.md](PROVENANCE.md).

## Tests

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

- `genie_mesh` checks the mesh generator against 40 golden frames covering all four
  directions, normal and overlapping targets, and the overlap lift/slide phase.
  It also checks the target-rect and tile-inset formulas.
- `genie_shadow` checks the shadow mesh against 70 vectors recorded from the live
  macOS 26.7.2 SkyLight path.
- `genie_timeline` checks the KWin `TimeLine` forward/backward and interruption
  contract used by minimize and restore.
- `genie_target_matrix` checks the target position, size and direction for bottom,
  top, left, right and wide-label panels.
- `genie_direction_matrix` checks direction selection for normal windows and for
  windows that extend far below/above a panel.
- `genie_multioutput_matrix` checks that targets land on the output containing the
  panel/icon.
- `genie_scale_matrix` checks logical target coordinates at fractional scale.
- `genie_autohide_matrix` checks that auto-hidden panels keep the target on the
  visible screen edge. These run an isolated headless KWin; they do not affect the
  running session.

## Performance benchmark

`tools/benchmark_gpu.sh` runs an isolated headless KWin at 3840×2160 with a
Dolphin-sized window and collects KWin's render-time CSV. `tools/benchmark_parse.py`
prints the per-density medians:

```sh
tools/benchmark_gpu.sh --reps 5 --p 0.5
tools/benchmark_parse.py /tmp/genie-benchmark
```

Use `--p 0.1,0.5,0.9` for the full animation and `--sizes 8,12,16,24,32` to compare
mesh densities. This is a manual, GPU-specific benchmark, not part of CTest.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE). This is a third-party KWin effect and is not
affiliated with or endorsed by Apple.
