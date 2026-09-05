# Prebuilt releases

Ready-to-import packages of the Indexed Parallel Navigation plugin. Each `.tar.gz` in this folder is a complete OpenCPN plugin package (compiled `.dll` plus `metadata.xml`) — no build tools required to use one.

Currently these are **Windows (MSVC, x86, wxWidgets 3.2) builds only**. To use the plugin on Linux or macOS, build it from source instead — see [`../source/README.md`](../source/README.md).

## Installing a release

1. Download the `.tar.gz` for the version you want (the highest version number is the latest).
2. In OpenCPN: **Options → Plugins → Import Plugin...** and select the downloaded file.
3. Restart OpenCPN if prompted.
4. If you're upgrading from an older version of this plugin, uninstall/remove the old one first (Options → Plugins → select it → Remove), since OpenCPN can otherwise keep a stale copy loaded.

## Changelog

See [`versions.md`](versions.md) for what changed in each version.

## File naming

`indexed_parallel_lines_pi-<version>-msvc-x86-wx32-<windows-build>-MSVC.tar.gz` — `<version>` is `major.minor.patch.tweak`, `<windows-build>` is the Windows SDK/build version the plugin was compiled against.
