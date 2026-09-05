# indexed_parallel_lines_pi — Version History

Each entry corresponds to a tarball backed up in this directory:
`indexed_parallel_lines_pi-<version>-msvc-x86-wx32-10.0.26200-MSVC.tar.gz`

## 0.0.17.0

- Fixed "Repick Distance on Chart" losing the line entirely if the pick was cancelled (Escape) after it had already been started: the line was deleted immediately when the repick began, and cancelling never put it back. It's now kept in a backup and only actually removed once a new distance is confirmed by clicking the chart; cancelling (via Escape, or the reference leg vanishing mid-pick because its route/waypoint was deleted) now restores it exactly as it was.
- "Repick Distance on Chart" no longer requires pre-selecting a line via the manager list first: if nothing is selected when the button is pressed, it now arms a click-to-pick mode - click any drawn indexed line on the chart to choose which one to repick. If a line is already selected, pressing the button still repicks that one directly.
- Starting a new pick ("New Indexed Line", "New Perpendicular Line", or "Repick Distance on Chart") while another pick was already in progress used to silently abandon it (and could orphan a repick's already-removed line); it now cleanly cancels the previous pick first, restoring anything that needed restoring.
- First build produced from the restructured repo layout (`source/` + `releases/`).

## 0.0.16.0

- Updated plugin metadata to replace leftover values from the `testplugin_pi` template this plugin was originally cloned from: author is now "Guilherme Vieira" (was "Jon Gough"), and info-url now points to the plugin's own repo instead of an unrelated OpenCPN wiki page for a different plugin.
- Created a new public GitHub repo for the plugin at https://github.com/guiganv/indexed_parallel_lines_pi and repointed this working copy's `origin` remote to it (was still pointing at `jongough/testplugin_pi.git`, which is also why `<source>` in the metadata was wrong - that field is auto-derived from the git remote at build time, so it now correctly reads the new repo URL too). Pushed the current code as a single clean initial commit, since the old remote's inherited git history (a shallow clone of the unrelated template, complete with its own tags) was blocking a normal push and had no value for this plugin.
- Added `build/`, `build_log.txt`, and `backup/` to `.gitignore` so build artifacts and backup tarballs aren't tracked in the repo.

## 0.0.15.0

- Fixed the hover info box from v0.0.14.0 not appearing at all. It was only implemented in `RenderOverlay` (the non-GL `wxDC` overlay path); if OpenCPN's chart is rendering in OpenGL mode, `RenderOverlay` is never called at all (only `RenderGLOverlay` is) - the same reason this plugin's on-chart line-name labels have never shown in GL mode either, since they were also DC-only. Added an equivalent hover box to `RenderGLOverlay` via a new `DrawHoverBoxGL()` helper: since OpenGL has no built-in text drawing, the box (background + text) is rendered into an offscreen bitmap with a regular `wxDC` (identical font handling to the non-GL box) and then blitted onto the chart with `glDrawPixels`, vertically flipped via `glPixelZoom(1, -1)` to match `wxImage`'s top-down row order. Hover info should now appear regardless of whether the chart canvas is using GL or DC rendering.

## 0.0.14.0

- Fixed the manager dialog's layout: it was still created at a hardcoded 560×340 size from before v0.0.13.0 added a pick-status label and a second button row (Settings/Export/Export All/Import) on top of the existing filter row, list, and first button row (6 buttons) - the fixed size no longer fit, clipping/squeezing everything including the route filter dropdown (which could appear empty as a result, even though it was actually populated). The dialog is now built with `wxDefaultSize` and `SetSizerAndFit()`, so it's sized to exactly fit its content; both button rows now use `wxWrapSizer` so they reflow onto extra rows instead of clipping if the window is later resized narrower; the list gets an explicit minimum size so `Fit()` doesn't shrink it to near-zero.
- Optimized the ~500ms list-refresh poll (`OnListRefreshTimer`/`RefreshList()`). Checked OpenCPN's plugin API and core source directly: there is genuinely no event-driven route-change notification available (no message is ever broadcast for route edit/drag/delete, and no virtual callback exists), so polling remains the only option - but each tick previously fetched every line's route twice (once in `RefreshList()` for the route name, once again inside `GetLiveLeg()` for geometry). `GetLiveLeg()` is now split into a thin GUID-based wrapper plus `GetLiveLegFromRoute()`, and `RefreshList()` fetches each distinct route at most once per tick via a small cache, reusing it for both the name and the geometry lookup. List columns are now also only rewritten via `SetItem` when their text actually changed, instead of unconditionally every tick, cutting needless control redraws.
- Added hover info: hovering the mouse over a drawn indexed line (when no pick is in progress) now shows a small info box near the cursor with the line's name, route, course, offset, and side/ahead-astern - the same information shown in the manager list - similar to how OpenCPN shows leg info when hovering a route. Shown only on the non-GL overlay (matching the existing line-name labels, which were already non-GL-only).

## 0.0.13.0

- Added GPX Export/Import: a route and its indexed lines can now be exported to a standard GPX 1.1 file, so two users can exchange a navigation plan (previously indexed lines only lived in OpenCPN's own config). Indexed lines ride along as a custom `<ipl:indexedLines>` extension block inside each `<rte>`, referencing legs by waypoint position within that route rather than by GUID (GUIDs aren't portable across OpenCPN installs). New "Export" (exports the currently-filtered route, or all routes with lines if the filter is "All Routes"), "Export All" (always exports every route with lines, regardless of the current filter), and "Import..." buttons. On import, a route already present (matched by name and waypoint positions) is reused rather than duplicated; a new route is created via `AddPlugInRoute`/`GetNewGUID()` otherwise. Lines that already exist on a route (same leg, type, offset, side/along-track) are skipped on import and reported as duplicates rather than re-added. Uses the already-vendored-but-previously-unused tinyxml library, no new dependency.
- Added Escape-to-cancel for any in-progress pick ("New Indexed Line", "New Perpendicular Line", "Repick Distance on Chart"): pressing Escape while a pick is armed now resets cleanly instead of forcing the user through to a result they'd have to delete. A status label in the manager window ("Picking... (Esc to cancel)") shows whenever a pick is in progress.
- Added a "Settings..." dialog: the parallel-line length factor (previously a fixed 1.25x), the chart-click hit tolerance for selecting an existing line, and the line/selected-line colors are now user-configurable and persist across restarts, instead of being hardcoded constants.
- "Delete Selected" now asks for a Yes/No confirmation before removing the line, since deletes are otherwise silent and irreversible.
- Added `Planned_Improvements.md` in this directory's parent (`D:\Dev\indexed_parallel_lines_pi\Planned_Improvements.md`) as a living backlog of ideas not yet implemented (proximity/crossing alarm, per-line label halo, multi-select, undo-delete, event-driven refresh).

## 0.0.12.0

- Removed the clamp that restricted a perpendicular line's crossing point to within the leg (`[0, legDist]`); it can now be placed beyond either end.
- Fixed `TrackOffsets()`'s along-track distance, which came from `acos()` and was therefore always non-negative — it could not actually represent a point behind wp0 at all. It now derives a sign from the click's bearing relative to the leg's course, so a point astern of wp0 correctly gets a negative along-track value instead of being folded onto the positive (ahead) side.
- The manager list's Side column now shows "Ahead" or "Astern" for a perpendicular line whose crossing point falls beyond wp1 or before wp0 respectively (still "Perpendicular" when it crosses within the leg itself), flipping consistently with "Port"/"Starboard" when the route has been reversed.

## 0.0.11.0

- Added a "Route" dropdown at the top of the manager window, listing all routes currently on the chart (plus "All Routes"). Selecting a route filters the list to only that route's indexed lines. The dropdown only rebuilds when the actual set of routes changes, and list row indices are now mapped through the active filter (`m_visibleRowToLineIndex`) so selection, delete, rename, checkbox, and right-click all operate on the correct underlying line even while filtered. The filter only affects the manager list — chart rendering is unaffected.
- Added a "New Perpendicular Line" button: pick a reference leg (same leg-picking flow as a parallel line), then a single click sets both where along the leg the line crosses (its along-track position) and how far it extends to each side (from the click's distance off the leg) - so one click fully defines the perpendicular line's position and length. Implemented via a new `TrackOffsets()` helper (cross-track + along-track together) and new `IndexedLine` fields `isPerpendicular`/`alongTrackNM`, persisted alongside the existing fields. The Side list column shows "Perpendicular" instead of Port/Starboard for these lines.

## 0.0.10.0

- Fixed indexed lines disappearing when their reference route was reversed. `GetLiveLeg()` matched the two reference waypoints only in the exact order they were originally picked (wp0 immediately followed by wp1); reversing the whole route keeps the waypoints adjacent but swaps that order, so the leg (and any line built on it) could no longer be found. `GetLiveLeg()` now matches either order by identity (GUID) and reports the swap via a new `reversedInRoute` output.
- The drawn line's geometry is intentionally unaffected by this (still computed from the fixed wp0GUID->wp1GUID definition), so reversing a route no longer moves or hides the indexed line. The manager list's Course and Side columns, however, now flip to match the leg as currently sailed: when `reversedInRoute` is true, displayed course is offset +180° and Port/Starboard is inverted, since the same absolute geographic side is now on the opposite hand relative to the new travel direction.
- `SortIndexLines()`'s leg-position lookup (used to order lines within a route) was made order-agnostic the same way, so line ordering stays sensible after a route reversal too.

## 0.0.9.0

- Indexed lines are no longer drawn on the chart when their source route is currently hidden (route display toggled off in OpenCPN's Route Manager). Checked via `GetRouteEx_Plugin()->m_isVisible`, since `GetRoute_Plugin`/`GetRouteGUIDArray()` return all routes regardless of visibility and carry no visibility field themselves.
- Added a per-line visibility checkbox (via `wxListCtrl::EnableCheckBoxes()`), shown before the Route column, letting the user show/hide each indexed line independently of its route's visibility. Persisted as a new `Visible` config key (defaults to true for lines saved before this version). Hidden lines are skipped both when rendering and when hit-testing chart clicks for selection.
- Added a "Rename..." button and right-click context menu item, prompting for a new name for the selected indexed line via a text dialog.

## 0.0.8.0

- Renamed "Change Distance..." to "Edit Distance..." (button and right-click menu item).
- Added a "Repick Distance on Chart" button: for the selected line, it deletes the entry and re-enters the offset-pick flow using the same reference leg (route/waypoints unchanged), so the next chart click sets a new distance for that same leg. The line's name is preserved across the redo instead of being renumbered.
- Course column is now zero-padded to 3 digits (e.g. "007°", "045°") instead of one decimal place.
- Fixed the degree symbol rendering as "Â°" (mojibake) in the Course column: MSVC was not interpreting the source file's UTF-8-encoded "°" as intended, so each byte of the 2-byte UTF-8 sequence was read as a separate character. Replaced the literal character with the `°` universal-character-name escape in the format string, which is encoding-independent.

## 0.0.7.0

- Fixed the manager window not appearing after clicking the toolbar icon (the icon still toggled to its pressed state, meaning the dialog was constructed and `Show()` ran, but nothing was visible on screen) by switching `CentreOnParent()` to `CentreOnScreen()` and explicitly calling `Raise()`/`SetFocus()` after `Show()`, to guard against a degenerate parent rect or the new window opening behind OpenCPN's main window.

## 0.0.6.0

- Moved the "Route" column to be first in the manager list.
- Indexed lines are now classified/sorted first by route name, then by leg order within that route (the leg's position among the route's consecutive waypoint pairs). Sorting happens on every list refresh via `SortIndexLines()`, which remaps the tracked selection to the same physical line by identity so the highlighted/selected line doesn't jump when the order is recomputed.
- Added a "Change Distance..." button and a list right-click context menu item, both prompting for a new offset distance (nautical miles) for the currently selected indexed line via a text-entry dialog, then saving and refreshing.

## 0.0.5.0

- Removed the drag-and-drop editing feature added in 0.0.4 (per user request).
- Added a "Route" column to the manager list, showing the live name of each indexed line's source route (looked up via `GetRoute_Plugin`).
- Indexed lines are now selectable both from the chart and from the list, kept in sync through a single `m_selectedLineIndex`: clicking within ~8 pixels of a drawn line on the chart selects it (and updates the list's selection), clicking a row in the list selects it (via `wxEVT_LIST_ITEM_SELECTED`/`_DESELECTED`), and the selected line is drawn highlighted (thicker, yellow) in both the non-GL and OpenGL overlays. Clicking empty chart space clears the selection without swallowing the click, so normal chart panning/interaction still works.

## 0.0.4.0

- Reverted the drawn parallel line's length back to 1.25x the reference leg (had been temporarily bumped to 2x while diagnosing the extension bug, see 0.0.3).
- Fixed the manager window's list selection being lost every ~500ms: the periodic Course-column refresh was calling `DeleteAllItems()` + re-inserting every row on every tick, which resets wxListCtrl's selection. Now only rebuilds rows when the count changes (add/delete); a live tick updates cell text in place via `SetItem`, leaving selection untouched.
- Added drag-and-drop: clicking within ~8 screen pixels of an already-drawn indexed line (not the reference route leg) grabs it, dragging updates its offset distance/side live using the same cross-track math as creating a new line, and releasing the mouse saves it immediately. Requires caching the last `PlugIn_ViewPort` passed to the renderer so mouse clicks can be hit-tested in pixel space.

## 0.0.3.0

- Fixed the parallel line rendering shorter than the reference leg instead of longer. Root cause: OpenCPN's `DistanceBearingMercator_Plugin(A, B, ...)` returns the bearing from B back to A — the reciprocal of what the parameter order suggests, because the core implementation's own parameter names are bound backwards. Difference-based uses of that bearing (the port/starboard cross-track math) canceled the reversal automatically, which is why the side was already correct; but the line-extension code used the raw bearing directly, so both ends were extending toward each other instead of away, shrinking the line. `GetLiveLeg()` now corrects the raw bearing once (+180°) so callers get the true wp0->wp1 course.
- Fixed the drawn parallel line rendering on the opposite side from what the manager list labeled ("Port"/"Starboard"). Same root cause as above — the perpendicular offset bearing formula needed the sign flip that this bearing reversal implies.

## 0.0.2.0

Multiple rebuilds under this version number, culminating in:

- Replaced the original route-info viewer (route name / leg heading / leg distance dialog) with the full **Indexed Parallel Navigation** feature: a manager window listing all indexed lines (name, course, offset, side), a "New Indexed Line" flow (click a route leg on the chart to use as reference course, click again to set offset distance/side via cross-track distance), "Delete Selected", and both non-GL (`RenderOverlay`) and OpenGL (`RenderGLOverlay`) rendering of the resulting parallel lines on the chart.
- Live drag preview while picking the offset (dashed blue line follows the cursor before committing).
- Indexed lines track their source route/waypoints live via GUID lookup (`GetLiveLeg`) instead of a frozen lat/lon snapshot, so dragging/editing the route on the chart updates the drawn parallel line automatically.
- Persistence via `wxFileConfig` (`GetOCPNConfigObject()`), so indexed lines survive an OpenCPN restart.

## 0.0.1.0

- Initial working plugin (scaffolded from `testplugin_pi`, stripped down to a minimal skeleton and renamed to `indexed_parallel_lines_pi`). Toolbar button toggled a dialog showing the first route's name, first-leg heading, and first-leg distance, with "Select Route on Chart" (click-to-select nearest route) and "Next Leg" buttons.
