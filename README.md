# Indexed Parallel Navigation (indexed_parallel_lines_pi)

An [OpenCPN](https://opencpn.org/) plugin for planning **indexed parallel navigation** — lines offset from a route leg (or crossing it perpendicularly) by a chosen distance, drawn live on the chart and tracked automatically as the underlying route is edited.

## Features

- **Manager window** listing every indexed line, with the route it belongs to, its name, live course, offset distance, and side (Port/Starboard, or Ahead/Astern for perpendicular lines).
- **New Indexed Line** — pick a leg of any route on the chart, then click again to set the offset distance and side. The line is drawn parallel to the leg, extended for readability, with a live preview while picking.
- **New Perpendicular Line** — pick a reference leg, then a single click sets both where along the leg it crosses and how far it extends to each side. The crossing point can fall beyond either end of the leg (labeled Ahead/Astern).
- **Live geometry** — every indexed line is stored relative to its route's waypoints (not a frozen position), so dragging, editing, or reversing the route updates the drawn line and its displayed course/side automatically.
- Select, rename, edit the offset distance, re-pick the offset on the chart, show/hide individually, and delete (with confirmation) any indexed line — from either the manager list or by clicking it on the chart.
- **Route filter dropdown** in the manager window to show only one route's indexed lines, or all of them.
- **Hover info** — hovering the mouse over a drawn indexed line shows its name, route, course, offset, and side, on both the standard and OpenGL chart renderers.
- **Escape to cancel** any in-progress pick, with a status label showing what's being picked.
- **Settings dialog** for the parallel-line length factor, chart-click hit tolerance, and line colors.
- **GPX export/import** — export a route and its indexed lines to a standard GPX 1.1 file (indexed lines ride along as a custom extension block) to share a navigation plan with another OpenCPN user, or back it up separately from the full OpenCPN config. Importing matches an existing route by name/waypoints when possible, otherwise recreates it; duplicate lines are skipped and reported.
- Indexed lines persist across OpenCPN restarts.

## Installing

Download the tarball for your platform from a [release](https://github.com/guiganv/indexed_parallel_lines_pi/releases) (or build it yourself, see below), then in OpenCPN: **Options → Plugins → Import Plugin...** and select the `.tar.gz`. Restart OpenCPN if prompted.

## Building from source

Requires a working OpenCPN plugin build environment (CMake, a matching wxWidgets build, and a C++ toolchain for your platform).

```
git clone https://github.com/guiganv/indexed_parallel_lines_pi.git
cd indexed_parallel_lines_pi
mkdir build && cd build
cmake -T v143 -A Win32 ..
cmake --build . --target package --config Release
```

On Windows, `run_build.bat` wraps the above with the environment setup (VsDevCmd, wxWidgets/NSIS paths) used for this repo's own builds. The `package` target produces a `.tar.gz` plus a sibling `.xml` metadata file; OpenCPN's plugin manager expects the metadata copied into the tarball root as `metadata.xml` before it's importable.

## License

GPLv3 or later — see [`cmake/gpl.txt`](cmake/gpl.txt).
