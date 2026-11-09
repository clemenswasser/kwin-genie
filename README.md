# Genie — macOS genie minimize effect for KWin

Native KWin effect plugin reproducing the macOS Tahoe genie minimize/restore animation. It appears
under **System Settings → Animations → Window minimize**.

![Genie minimize effect](docs/genie-demo.gif)

The mesh is a bit-exact port of the Dock's genie routine, and rendering matches WindowServer's
per-cell projective texture mapping. Timing is 0.5 s; hold **Shift** for 10× slow motion.

## Build

Needs `kwin-devel` (6.8), `extra-cmake-modules`, Qt 6 and KF6 CoreAddons/Config development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Select **Genie** in System Settings → Animations → Window minimize.

After a rebuild, use `tools/reload_effect.sh`; KWin may keep the old library mapped under the same
file name.

## Provenance

The mesh and shadow algorithm are a reverse-engineered reimplementation; see
[PROVENANCE.md](PROVENANCE.md).

## Tests

```sh
tests/target_matrix.sh
```

Runs an isolated headless KWin; it does not affect the running session.
