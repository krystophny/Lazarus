# CustomDrawn WASI browser backend

This example runs an ordinary LCL form from `demomain.lfm` in a browser.
The Pascal application uses `TButton`, `TCheckBox`, `TLabel`, `TPaintBox` and
normal event handlers. The experimental widgetset supplies DOM buttons and
checkboxes through Free Pascal's JS Object Bridge (JOB), while existing
CustomDrawn/LazCanvas code paints the label and drawing area.

The application has no browser bindings. A small browser host supplies the
outer event loop, pointer events, text rasterization, timers and WASI services.
The executable is a WASI reactor, initialized once and called through exported
widgetset callbacks. It requires the FPC development sources below and a browser
with WebAssembly JSPI (`WebAssembly.Suspending` and `WebAssembly.promising`).

## Build

The tested source versions are:

- Lazarus base: `ee000f4ee91ada21146bf79d5890f43c79f2f77e`.
- FPC: `86125b587067db5bc22f5fa12426aacd7fd9d4e0`.
- Pas2JS: [`dad0f277fdeaca1b1e8c6f2866de5f54ec12e703`](https://github.com/krystophny/pas2js/commit/dad0f277fdeaca1b1e8c6f2866de5f54ec12e703),
  with JOB object-release, packed-string and HTTP startup repairs.

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
checks run against a real headless Chromium instance.

## Application showcase and scope

[TpX](https://github.com/krystophny/tpx) exercises the backend in an existing
Pascal drawing application: [try it in the browser](https://lcl-wasm-experiment.dusky-char-6178.chatgpt.site/tpx/).
Its browser suite covers drawing and editing, text controls, menus, modal dialogs,
file upload/download, bitmap export, clipboard operations and viewport changes.
Its separate performance checks measure startup, idle activity and pointer CPU;
normal successful test runs do not capture screenshots.

The backend combines DOM controls with CustomDrawn canvas rendering and tracks
damaged regions. Blocking LCL dialogs use JSPI to suspend and resume Pascal calls.
The example above remains a small standalone reproducer for control callbacks,
rendering, resize and JOB object lifetime. TpX's tests provide broader application
coverage, without establishing support for every LCL control or API.

Files are held in the browser's temporary filesystem. Native process launching,
system printer dialogs and installed-font enumeration are unavailable. Full
LaTeX and MetaPost compilation remain desktop features; TpX supplies its own
MathJax preview for TeX text labels.

## Licenses

The new backend files follow LCL's modified LGPL with linking exception;
the example application, browser host and test scripts are MIT. The generated
JOB host includes Pas2JS code under its modified LGPL/linking exception.

The example's `web/licenses.txt` provides source links and license texts.
