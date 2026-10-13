#!/bin/sh
# SPDX-License-Identifier: MIT
# Build the WASI reactor and the Pas2JS object bridge for the browser example.
#
#   ./build.sh FPC_SOURCE PAS2JS_SOURCE FPCRES
#
# FPC_SOURCE holds a wasm32-wasip1 cross build (compiler/ppcrosswasm32 and the
# RTL/package units), PAS2JS_SOURCE a built Pas2JS tree, FPCRES a native fpcres
# from the same FPC sources. Output goes to web/, intermediate files to build/.
set -eu
[ $# -eq 3 ] || { sed -n '4,9p' "$0" >&2; exit 2; }
ROOT=$(cd "$(dirname "$0")" && pwd)
LAZ=$(cd "$ROOT/../.." && pwd)
FPC=$(cd "$1" && pwd)
PAS2JS=$(cd "$2" && pwd)
FPCRES=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
PPC=$FPC/compiler/ppcrosswasm32
TRANSPILER=$PAS2JS/bin/x86_64-linux/pas2js
for tool in "$PPC" "$TRANSPILER" "$FPCRES"; do
  [ -x "$tool" ] || { echo "missing built tool: $tool" >&2; exit 2; }
done
mkdir -p "$ROOT/build"

CFG=$ROOT/build/compiler.cfg
{
  printf '%s\n' -n -B -Twasip1 -Pwasm32 -O1 -dLCL -dLCLcustomdrawn \
    "-Fi$LAZ/lcl/include" "-Fi$LAZ/lcl/interfaces/customdrawn" \
    "-FU$ROOT/build" "-FE$ROOT/web" "-Fu$FPC/rtl/units/wasm32-wasip1"
  for dir in "$FPC"/packages/*/units/wasm32-wasip1; do
    [ -d "$dir" ] && printf '%s\n' "-Fu$dir"
  done
  for dir in lcl lcl/widgetset lcl/interfaces/customdrawn components/lazutils lcl/nonwin32; do
    printf '%s\n' "-Fu$LAZ/$dir"
  done
} > "$CFG"
(cd "$ROOT" && PATH="$(dirname "$FPCRES"):$PATH" "$PPC" "@$CFG" demo.lpr) \
  > "$ROOT/build/compile.log" 2>&1 || { tail -20 "$ROOT/build/compile.log" >&2; exit 1; }
mv "$ROOT/web/demo" "$ROOT/web/demo.wasm"

# Explicit unit paths avoid relying on a global pas2js.cfg.
set -- -n -Jc -Jirtl.js "-Fu$FPC/utils/pas2js/dist"
for dir in "$PAS2JS"/packages/*/src; do
  [ -d "$dir" ] && set -- "$@" "-Fu$dir"
done
"$TRANSPILER" "$@" "$ROOT/web/jobhost.lpr" "-o$ROOT/web/jobhost.js" > "$ROOT/build/pas2js.log" 2>&1 ||
  { cat "$ROOT/build/pas2js.log" >&2; exit 1; }
echo "built $ROOT/web/demo.wasm and jobhost.js"
