# kw_plugin_fps_patch

Brokered FPS/interpolation and adaptive scheduling.

## Layout and build

Production implementation is under `src/`; supported/public or compatibility
headers are under `include/`. Generated files go only to ignored `build/`:
`build/bin` for DLLs/executables, `build/lib` for libraries, `build/obj` for
objects/dependency files, `build/config` for copied configuration templates.
Use GNU Make and x86 MinGW (`i686-w64-mingw32-gcc/g++/ar`).

```sh
make -j4
make sdk-paths
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
installed game. See `kw_modloader/docs/BUNDLES.md` for composition and AI caveats.

Build and staging targets do not install into a game or overwrite user settings.
Use matching loader/SDK artifacts and preserve customized INIs when installing.
Runtime loading uses `.kwplugin` manifests; configuration uses `.ini` files.

Optional pacing-protocol integration defaults off for standalone `make`/`all`.
`bundle`/`stage` automatically compiles it when pacing's public header is
available; a workspace composition also requires pacing to be selected in its
plugin list. Explicit `WITH_PACING_INTEGRATION=0|1` overrides the default.
`PACING_SDK_ROOT` selects the provider's public policy header, never its sources
or libraries. Runtime provider discovery is automatic; absence is normal.
Neutral broker checks always accommodate a separately reserved native wait,
including standalone builds without that policy header. There is no AI plugin
header, library, service lookup, ordering rule or build switch.


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
