# Indexed Parallel Navigation (indexed_parallel_lines_pi)

An [OpenCPN](https://opencpn.org/) plugin for planning **indexed parallel navigation** — lines offset from a route leg (or crossing it perpendicularly) by a chosen distance, drawn live on the chart and tracked automatically as the underlying route is edited.

This repository is split into two parts:

- [`source/`](source/) — the plugin's source code. See [`source/README.md`](source/README.md) for how to build it on Windows, Linux, and macOS.
- [`releases/`](releases/) — prebuilt, ready-to-import plugin packages for Windows. See [`releases/README.md`](releases/README.md) for how to install one, and [`releases/versions.md`](releases/versions.md) for the changelog.

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

## License

GPLv3 or later — see [`LICENSE`](LICENSE).
