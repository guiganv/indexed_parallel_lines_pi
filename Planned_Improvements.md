# indexed_parallel_lines_pi — Planned Improvements

A living backlog of ideas discussed for this plugin. The plugin is a **planning**
tool first: it exists to let a user lay out indexed lines against routes before
a voyage, not to run live on-watch alarms — keep that framing in mind when
weighing new ideas here.

Items are appended as they come up in conversation; each is marked with its
status so this file stays a useful reference across sessions rather than a
one-time dump.

## Done

- **Cancel a pending pick.** Escape now aborts "New Indexed Line" / "New
  Perpendicular Line" / "Repick Distance on Chart" mid-flow, with a status
  label ("Picking... (Esc to cancel)") shown in the manager window while a
  pick is in progress. (v0.0.13.0)
- **Settings dialog.** Parallel-line length factor, chart-click hit
  tolerance, and line/selected-line colors are now user-configurable
  (previously hardcoded constants) and persist across restarts. (v0.0.13.0)
- **Confirm before delete.** "Delete Selected" now asks for confirmation
  before removing an indexed line. (v0.0.13.0)
- **GPX Export/Import.** A route and its indexed lines can be exported to a
  standard GPX 1.1 file (indexed lines ride in a custom `<ipl:...>` extension
  block) and imported back in, including on a different OpenCPN install that
  doesn't already have the route. Duplicate lines on re-import are skipped
  and reported. "Export" respects the current route filter; "Export All"
  exports every route that has indexed lines regardless of filter. (v0.0.13.0)

## Backlog

### Navigation usefulness
- Proximity/crossing alarm: alert (visual flash + optional sound) when the
  vessel's own-ship position crosses or comes within a set distance of an
  indexed line. Lower priority — the plugin is plan-time, not watch-time, so
  this is a "maybe" rather than a near-term item.
- Live readout of current distance from own-ship to the nearest/selected
  indexed line, shown in the manager window.

### Chart rendering
- Label background halo/box behind indexed-line names so labels stay
  legible over chart clutter.

### List/manager UX
- Multi-select support in the list for batch operations (hide/delete
  several lines at once) instead of one at a time.
- Undo for "Delete Selected" (a "last deleted" restore) — softened for now
  by the delete-confirmation prompt, but still not reversible after
  confirming.

### Robustness/performance
- `OnListRefreshTimer` currently polls all routes on a fixed ~500ms interval
  to re-resolve live geometry. If OpenCPN's plugin API exposes route-change
  notifications, switching to event-driven refresh would cut needless work,
  especially with many routes/lines.
