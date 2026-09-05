# Building indexed_parallel_lines_pi from source

This is an [OpenCPN](https://opencpn.org/) plugin, built with CMake against a matching OpenCPN/wxWidgets development environment. See the [project README](../README.md) for what the plugin does.

## Get the source

```
git clone https://github.com/guiganv/indexed_parallel_lines_pi.git
cd indexed_parallel_lines_pi/source
git submodule update --init --recursive
```

The `git submodule` step is required — `opencpn-libs` (the vendored OpenCPN plugin API headers) is a submodule, not plain files, and the build will fail without it.

All commands below are run from this `source/` directory.

## Windows (MSVC)

Requires Visual Studio 2022 (or a compatible MSVC toolset), CMake, and a prebuilt wxWidgets 3.2 matching the one OpenCPN itself was built against.

```
rmdir /s /q build & mkdir build & cd build
cmake -T v143 -A Win32 ..
cmake --build . --target package --config Release
```

`run_build.bat` wraps the above with this repo's own environment setup (VsDevCmd, wxWidgets/NSIS paths) — copy and adjust the paths inside it for your own machine rather than running it as-is.

The `package` target produces a `.tar.gz` plus a sibling `.xml` metadata file. OpenCPN's plugin manager expects the metadata copied into the tarball root as `metadata.xml` before the package is importable — CMake/CPack only generates the sibling file, so this step has to be done by hand (or scripted) after every build:

```
tar -xzf indexed_parallel_lines_pi-<version>.tar.gz -C extracted/
cp indexed_parallel_lines_pi-<version>.xml extracted/metadata.xml
tar -czf indexed_parallel_lines_pi-<version>.tar.gz -C extracted/ .
```

## Linux (Debian/Ubuntu)

Install build dependencies (see [`ci/control`](ci/control) for the full list), then build:

```
sudo apt-get update
sudo apt-get install devscripts equivs
sudo mk-build-deps --install ci/control
sudo apt-get install libwxgtk3.2-dev

rm -rf build && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc)
make package
```

This mirrors [`ci/circleci-build-jammy.sh`](ci/circleci-build-jammy.sh), which is the exact script this repo's CI uses for Ubuntu/Debian builds — check it if something here doesn't match your distro's package names. Fedora has an equivalent path via [`ci/circleci-build-fedora.sh`](ci/circleci-build-fedora.sh) and [`ci/opencpn-fedora.spec`](ci/opencpn-fedora.spec).

## macOS

Requires [Homebrew](https://brew.sh/) and a prebuilt wxWidgets matching OpenCPN's own (downloaded automatically by the CI script below).

```
brew install cairo cmake gettext libarchive libexif python3 wget

# Download the prebuilt wxWidgets OpenCPN itself uses (adjust the URL/version
# to match your OpenCPN's wxWidgets version - see ci/circleci-build-macos.sh
# for the exact URLs this repo's CI uses).
WX_DOWNLOAD=/tmp/wx321_opencpn50_macos1010.tar.xz
wget -O "$WX_DOWNLOAD" https://download.opencpn.org/s/Djqm4SXzYjF8nBw/download
tar xJf "$WX_DOWNLOAD" -C /tmp

rm -rf build && mkdir build && cd build
cmake \
  -DwxWidgets_CONFIG_EXECUTABLE=/tmp/wx321_opencpn50_macos1010/bin/wx-config \
  -DwxWidgets_CONFIG_OPTIONS="--prefix=/tmp/wx321_opencpn50_macos1010" \
  -DCMAKE_INSTALL_PREFIX=app/files \
  -DBUILD_TYPE_PACKAGE:STRING=tarball \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  ..
make
make install
make package
```

See [`ci/circleci-build-macos.sh`](ci/circleci-build-macos.sh) for the full, exact script (including the current wxWidgets download URLs) this repo's CI runs.

## IDE setup & debugging

Point your IDE at both an OpenCPN build (built in Debug) and this plugin (also built in Debug). Run OpenCPN with the `-p` option so it looks for plugins/data alongside its own executable rather than a system install path, and set `CatalogExpert=1` in the `[Plugins]` section of `opencpn.conf`/`.ini` to get full access to Plugin Manager's local-install features. Once the plugin is installed and working via Plugin Manager once, subsequent iterations are just: stop OpenCPN, copy the newly built plugin library into `<opencpn build dir>/plugins/lib`, restart OpenCPN. If both projects are open in the same IDE, you should be able to step through both.

## Publishing a new build

After a successful build/repack, copy the resulting tarball into [`../releases/`](../releases/) and add an entry to [`../releases/versions.md`](../releases/versions.md) describing what changed.
