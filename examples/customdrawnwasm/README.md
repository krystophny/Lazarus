# CustomDrawn browser experiment

This first branch runs an ordinary LCL form from `demomain.lfm` in a browser.
The Pascal application uses `TButton`, `TCheckBox`, `TLabel`, `TPaintBox` and
normal event handlers. The experimental widgetset supplies DOM buttons and
checkboxes through Free Pascal's JS Object Bridge (JOB), while existing
CustomDrawn/LazCanvas code paints the label and drawing area.

The application has no browser bindings. A small browser host supplies the
outer event loop, pointer events, text rasterization, timers and WASI services.
The initial executable is a WASI reactor, initialized once and called through
exported widgetset callbacks. It requires current FPC development sources.

## Build

The tested source versions are:

- Lazarus base: `ee000f4ee91ada21146bf79d5890f43c79f2f77e`.
- FPC: `86125b587067db5bc22f5fa12426aacd7fd9d4e0`.
- Pas2JS: `b69f26b05baeacb0e2da29df3e33d21b28fbe277`, from
  [the JOB fix branch](https://gitlab.com/krystophny/pas2js/-/tree/fix/job-release-object-id).
  [Upstream MR !103](https://gitlab.com/freepascal.org/fpc/pas2js/-/merge_requests/103)
  repairs a confirmed object-release bug; use this commit until the fix is merged.

Build FPC's compiler, RTL and packages for `wasm32-wasip1`, using an installed
FPC 3.2.2 as bootstrap compiler:

```sh
make -j8 crossall CPU_TARGET=wasm32 OS_TARGET=wasip1 BINUTILSPREFIX= PP=/usr/bin/fpc OPT='-O-'
```

Build a native `fpcres` from the same FPC source tree. The 3.2.2 resource compiler
cannot handle this WASM target. In the FPC tree, use:

```sh
mkdir -p /absolute/path/to/toolchain/bin /absolute/path/to/toolchain/units
fpc -MObjFPC -Fu./packages/fcl-res/src -Fu./packages/fcl-hash/src \
  -Fu./packages/paszlib/src -FU/absolute/path/to/toolchain/units \
  -FE/absolute/path/to/toolchain/bin ./utils/fpcres/fpcres.pas
```

Build Pas2JS with the FPC source path in the process environment:

```sh
FPCDIR=/absolute/path/to/fpc make -j4 all PP=/usr/bin/fpc
```

From this example directory, install the pinned browser dependencies and build:

```sh
npm ci
python3 build.py --fpc-source /absolute/path/to/fpc \
  --pas2js-source /absolute/path/to/pas2js --fpcres /absolute/path/to/bin/fpcres
python3 -m http.server 8765 --directory web
```

Open `http://localhost:8765/`. `build.py` writes its compiler output to
`build/compile.log`, compiles the small Pascal JOB host, and bundles the WASI
shim. Its generated files are excluded from Git. The script currently expects
a Linux x86-64 build host, with the Pas2JS executable in its standard build path.

## Browser checks

Install Python Playwright and provide a Chromium executable:

```sh
python3 tests/browser.py --url http://127.0.0.1:8765/ --chromium /usr/bin/chromium
```

The checks use observable behavior: mouse and Enter activate the Pascal counter;
Space and pointer clicks change checkbox state and the painted ellipse; a canvas
click places the Pascal marker; Reset clears it. Exact RGB pixels check the raw
image byte order. A platform resize exercises the form's existing Anchors, and
100 activation callbacks must keep JOB's live-object count constant. These
checks passed with real headless Chromium on the first branch. The same
application also compiled with the native X11 CustomDrawn backend and FPC 3.2.2;
native runtime behavior was not exercised in this browser experiment.

## Supported slice

One top-level form, DOM buttons and checkbox, a painted label and paintbox,
caption/state changes, basic positioning, anchored resize and pointer painting
are exercised. DOM controls retain browser keyboard activation and focus.
The example uses a fixed initial 720 × 480 surface with scrolling on small screens;
automatic viewport adaptation is future work.

This branch does not establish TpX compatibility. Text editors and IME,
LCL-wide focus/key routing, menus, dialogs, file transfer, clipboard, multiple
windows and drawing accessibility still need implementation and tests. Font
metrics are approximate and currently use a fixed browser sans-serif font.
Modal forms and blocking message waits raise explicit unsupported exceptions.
Other inherited CustomDrawn components are unqualified. Full-frame image copying
and repeated DOM property synchronization have not been optimized or benchmarked.

The next slice should add an edit control with Unicode/IME and lifecycle tests,
then a minimal TpX drawing view with file upload/download. Keep further browser
work below the application and measure changes needed to TpX itself.

## Licenses

The new backend files follow LCL's modified LGPL with linking exception;
the example application, browser host and test scripts are MIT. The generated
JOB host includes Pas2JS code under its modified LGPL/linking exception. This
LCL experiment is separate from FortUI's permissive dependency stack.

The example's `web/licenses.txt` provides source links and license texts.
