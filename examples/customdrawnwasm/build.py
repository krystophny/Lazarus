#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the WASI reactor and browser host from already built FPC/Pas2JS sources."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--fpc-source', type=Path, required=True)
parser.add_argument('--pas2js-source', type=Path, required=True)
parser.add_argument('--fpcres', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parent
repo = root.parents[1]
fpc = args.fpc_source.resolve()
pas2js = args.pas2js_source.resolve()
build = root/'build'
web = root/'web'
build.mkdir(exist_ok=True)
web.mkdir(exist_ok=True)
compiler = fpc/'compiler/ppcrosswasm32'
transpiler = pas2js/'bin/x86_64-linux/pas2js'
for tool in [compiler, transpiler, args.fpcres]:
    if not tool.is_file():
        parser.error(f'Missing built tool: {tool}')
flags = ['-n', '-Twasip1', '-Pwasm32', '-O1', '-dLCL', '-dLCLcustomdrawn',
         f'-Fi{repo}/lcl/include', f'-Fi{repo}/lcl/interfaces/customdrawn',
         f'-FU{build}', f'-FE{web}', f'-Fu{fpc}/rtl/units/wasm32-wasip1']
flags += [f'-Fu{path}' for path in sorted((fpc/'packages').glob('*/units/wasm32-wasip1'))]
flags += [f'-Fu{repo}/{path}' for path in ['lcl', 'lcl/widgetset',
          'lcl/interfaces/customdrawn', 'components/lazutils', 'lcl/nonwin32']]
config = build/'compiler.cfg'
config.write_text('\n'.join(flags)+'\n')
env = os.environ.copy()
env['PATH'] = str(args.fpcres.resolve().parent)+os.pathsep+env['PATH']
with (build/'compile.log').open('w') as log:
    subprocess.run([str(compiler), '@'+str(config), str(root/'demo.lpr')],
                   cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
shutil.copyfile(web/'demo', web/'demo.wasm')
# Explicit paths avoid relying on a developer's global pas2js configuration.
pasflags = ['-n', '-Jc', '-Jirtl.js', f'-Fu{fpc}/utils/pas2js/dist']
pasflags += [f'-Fu{path}' for path in sorted((pas2js/'packages').glob('*/src'))]
subprocess.run([str(transpiler), *pasflags, str(web/'jobhost.lpr'),
                '-o'+str(web/'jobhost.js')], cwd=root, check=True)
subprocess.run([str(root/'node_modules/.bin/esbuild'), str(web/'jobhost.js'),
                '--minify', '--outfile='+str(web/'jobhost.js'), '--allow-overwrite'],
               cwd=root, check=True)
subprocess.run([str(root/'node_modules/.bin/esbuild'), str(web/'host.js'),
                '--bundle', '--format=esm', '--outfile='+str(web/'host.bundle.js')],
               cwd=root, check=True)
shutil.copyfile(pas2js/'packages/job/src/job_browser.pp', web/'source/job_browser.pp')
print(f'Built {web}/demo.wasm, jobhost.js and host.bundle.js')
