# CustomDrawn WASI browser backend

This example runs an ordinary LCL form from `demomain.lfm` in a browser. The
Pascal code uses `TButton`, `TCheckBox`, `TLabel`, `TPaintBox`, `TMemo` and
`TSaveDialog` with normal event handlers and has no browser bindings.

The CustomDrawn widgetset creates buttons, checkboxes and edits as DOM elements
through Free Pascal's JS Object Bridge (JOB) and paints all other controls with
LazCanvas. A small JavaScript host (`web/lcl-host.js`) supplies the event loop,
pointer and keyboard input, text rasterization, timers, dialogs and WASI file
services. The executable is a WASI reactor, initialized once and called through
exported widgetset callbacks. Blocking LCL dialogs and modal forms suspend the
Pascal call with WebAssembly JSPI, so a browser with `WebAssembly.Suspending`
and `WebAssembly.promising` is required.

## Build

The tested source versions are:

- FPC: `86125b587067db5bc22f5fa12426aacd7fd9d4e0`.
- Pas2JS: [`dad0f277fdeaca1b1e8c6f2866de5f54ec12e703`](https://github.com/krystophny/pas2js/commit/dad0f277fdeaca1b1e8c6f2866de5f54ec12e703),
  which adds the JOB object-release repair from
  [pas2js!103](https://gitlab.com/freepascal.org/fpc/pas2js/-/merge_requests/103).

Build FPC's compiler, RTL and packages for `wasm32-wasip1`, using an installed
FPC 3.2.2 as bootstrap compiler:

```sh
make -j8 crossall CPU_TARGET=wasm32 OS_TARGET=wasip1 BINUTILSPREFIX= PP=/usr/bin/fpc OPT='-O-'
```

The 3.2.2 resource compiler cannot handle this target. Build a native `fpcres`
from the same FPC tree:

```sh
fpc -MObjFPC -Fu./packages/fcl-res/src -Fu./packages/fcl-hash/src \
  -Fu./packages/paszlib/src -FU/tmp/fpcres-units -FE/path/to/bin ./utils/fpcres/fpcres.pas
```

Build Pas2JS with the FPC source path in the environment:

```sh
FPCDIR=/path/to/fpc make -j4 all PP=/usr/bin/fpc
```

Then build the example, fetch the pinned WASI shim and serve `web/`:

```sh
./build.sh /path/to/fpc /path/to/pas2js /path/to/bin/fpcres
(cd web && npm ci)
python3 -m http.server 8765 --directory web
```

Open `http://localhost:8765/`. `build.sh` writes `web/demo.wasm` and
`web/jobhost.js`; compiler output goes to `build/`. The page loads
`@bjorn3/browser_wasi_shim` through an import map, so no bundler is needed.
The script expects a Linux x86-64 build host with Pas2JS in its standard build
path.

## Scope

Files live in the browser's temporary in-memory file system. An open dialog
uploads into it, and a file written after a save dialog is offered as a
download. Native process launching, printer dialogs, installed-font
enumeration and file watching are unavailable. Clipboard operations stay
within the application.

[TpX](https://github.com/krystophny/tpx) uses this backend for an existing
drawing application, and its continuous integration runs a headless Chromium
suite against this widgetset. That suite covers drawing, text controls, menus,
modal and common dialogs, file upload and download, clipboard and viewport
changes, plus a focused regression probe for memo text fidelity, IME
composition, focus traversal, scrolling and JOB object lifetime. The backend
does not yet support every LCL control or API.

## Licenses

The backend units follow LCL's modified LGPL with linking exception. The
example application, browser host and build script are MIT licensed
(`LICENSE-MIT`). A deployed page also contains the Free Pascal and Pas2JS
runtimes (modified LGPL with linking exception) and `@bjorn3/browser_wasi_shim`
(MIT or Apache-2.0); include their notices when publishing the built files.
