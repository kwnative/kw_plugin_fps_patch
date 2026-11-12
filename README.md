# kw_plugin_fps_patch

Licensed under **GPLv3**, with a game interoperability permission. See
[LICENSING.md](LICENSING.md) for scope, source-distribution obligations and
[third-party notices](THIRD_PARTY_NOTICES.md).

Brokered FPS/interpolation and adaptive scheduling.

## Layout and build

Production implementation is under `src/`; supported/public or compatibility
headers are under `include/`. Generated files go only to ignored `build/`:
`build/bin` for DLLs/executables, `build/lib` for libraries, `build/obj` for
objects/dependency files, `build/config` for copied configuration templates.

### Linux prerequisites

Use Bash, GNU Make 4.3+, native GCC/G++ (C++17 for bundle staging), and the
32-bit Windows MinGW-w64 toolchain (`i686-w64-mingw32-gcc`,
`i686-w64-mingw32-g++`, `i686-w64-mingw32-ar`).
For Debian/Ubuntu, install `build-essential mingw-w64`. Wine is only needed
to run the game or Windows test executables. No game files are needed to compile.

A checkout of **kw_modloader** is the required SDK dependency. Build it first:

```sh
export KWMOD_SDK_ROOT=/path/to/kw_modloader
make -C "$KWMOD_SDK_ROOT" -j2
```

Then, from this plugin's directory:

```sh
make -j2
make sdk-paths
make bundle
```

For relocated checkouts set `KWMOD_SDK_ROOT=/path/to/kw_modloader`.
It is the only mandatory SDK dependency: generic loader contracts and KW engine support
are both in its `include/` tree. Environment variables and Make arguments work.
`KWMOD_BIN_DIR` and `KWMOD_LIB_DIR` select matching SDK build artifacts;
`sdk-build` builds missing/stale SDK artifacts before linking. Compiler overrides
are `WINDOWS_CC`, `WINDOWS_CXX`, `WINDOWS_AR`; `BUILD_DIR` selects plugin output.

Run `make bundle` (or `make stage`) to assemble `build/bundle/` with this
plugin's DLLs, configuration and manifest in their engine-root-relative layout.
Override `BUNDLE_DIR=/path/to/staging` if needed. This is a plugin overlay,
not a standalone loader installation. Developer variants are not included.
Bundle generation replaces only previously staged, unmodified files and
refuses an unowned or locally edited output directory. Do not point it at an
installed game. See the loader's `docs/BUNDLES.md` for composition details.

Build and staging targets do not install into a game or overwrite user settings.
Use matching loader/SDK artifacts and preserve customized INIs when installing.
Runtime loading uses `.kwplugin` manifests; configuration uses `.ini` files.

### Manual installation

**kw_modloader must be installed for this plugin to load.** With the game closed,
copy the loader's `build/bundle/` contents into the engine directory containing
`cnc3ep1.dat` (normally `RetailExe/1.2`). Then copy this plugin's
`build/bundle/` contents into that same directory, retaining the `plugins/`,
`docs/` and `licenses/` layout. The runtime files are:

```text
RetailExe/1.2/
  dinput8.dll
  kw_runtime.dll
  kw_engine.dll
  plugins/
    kw_fps_patch.dll
    kw_fps_patch.kwplugin
    kw_fps_patch.ini
```

Preserve your customized INI when upgrading; merge new settings from the template.
Check `enabled` in the manifest and restart the game after changing settings.
There is no `make install` target or install-directory variable.
`BUNDLE_DIR` selects a dedicated staging directory, never a live game directory.

The current documented build host is Linux. Compiler overrides are retained for
a future documented native Windows build; that workflow is not yet qualified.

Optional pacing-protocol integration defaults off for standalone `make`/`all`.
`bundle`/`stage` automatically compiles it when pacing's public header is
available; a workspace composition also requires pacing to be selected in its
plugin list. Explicit `WITH_PACING_INTEGRATION=0|1` overrides the default.
`PACING_SDK_ROOT` selects the provider's public policy header, never its sources
or libraries. Runtime provider discovery is automatic; absence is normal.
Neutral broker checks always accommodate a separately reserved native wait,
including standalone builds without that policy header.


## Optional rendering clock integration

The versioned C interface in `include/kwfps/integration/render_clock.h` permits
an independent renderer to retain private FPS history and visual-time state.
It is discovered at runtime through `kwfps_get_render_clock_api_v1`; consumers
use the public header without linking provider sources. Standalone FPS behavior
requires no consumer or integration setting.

Snapshots belong to their creating thread, require canonical writers to be
quiescent during capture, and remain alive until actual consumer acknowledgement.
Rendering scopes use strict LIFO order on one consumer thread. Canonical rate
writes and session resets drain through the registered owner callback; ordinary
phases without a rate change do not join rendering. This covers FPS helpers only,
not native inline clock globals, complete scenes or simulation concurrency.
The threading integration remains experimental and disabled by default.

`make test-render-clock-build` builds `build/tests/test_render_clock.exe`, an
offline Windows fixture using private synthetic client memory. Run it directly
on Windows or explicitly through Wine; it installs no hooks and needs no game
content. It checks ordinary timing without a consumer, snapshot isolation,
creator/consumer thread fences, nested scopes and actual drain acknowledgement.
It is not included in release bundles or a full renderer concurrency test.
