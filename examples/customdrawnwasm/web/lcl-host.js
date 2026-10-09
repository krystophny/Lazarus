// SPDX-License-Identifier: MIT
import { WASI, File, OpenFile, ConsoleStdout, PreopenDirectory } from '@bjorn3/browser_wasi_shim';

export async function startLCL({moduleURL = './demo.wasm', argv = ['lcl-demo']} = {}) {
const canvas = document.querySelector('#lcl');
const context = canvas.getContext('2d', {alpha: false, willReadFrequently: true});
const status = document.querySelector('#status');
if (!WebAssembly.Suspending || !WebAssembly.promising) {
  status.textContent = 'This browser needs WebAssembly JSPI support. Please update your browser.';
  status.dataset.state = 'error';
  return;
}
const textCanvas = document.createElement('canvas');
const textContext = textCanvas.getContext('2d', {willReadFrequently: true});
const decoder = new TextDecoder();
const api = {};
let image, currentFont = "13px sans-serif", fontStyle = 0, fontAngle = 0;
const messageWaiters = [];
let activeForm = 0;
let pointerIdleTimer, lastHoverMove = 0;
let fileSystem;
const downloadPaths = new Set();
let instance, ready = false, scheduled = false, pendingMove = null;
const text = (pointer, length) => decoder.decode(new Uint8Array(instance.exports.memory.buffer, pointer, length));
function fail(error) {
  ready = false;
  wakeMessage();
  // Name the innermost Pascal routines so a report locates the failure.
  const frames = String(error.stack || '').match(/at [A-Z0-9_$]+\$\$?_?[A-Z0-9_$]*/g) || [];
  const where = frames.slice(0, 6).map(frame => frame.slice(3)).join(' ← ');
  status.textContent = `Application error: ${error.message || error}${where ? ` in ${where}` : ''}`;
  status.dataset.state = 'error';
  console.error(error);
}
function wakeMessage() {
  for (let i = messageWaiters.length-1; i >= 0; i--) {
    if (messageWaiters[i].form !== activeForm) continue;
    const [{resolve}] = messageWaiters.splice(i, 1);
    resolve();
  }
}
function guarded(callback) {
  try { const result = callback(); if (result?.catch) result.catch(fail); } catch (error) { fail(error); }
}
function invalidate() {
  if (scheduled) return;
  scheduled = true;
  requestAnimationFrame(() => {
    scheduled = false;
    flushMove();
    if (ready) guarded(() => api.lcl_render());
  });
}
const imports = {
  invalidate,
  active_form: form => { activeForm = form; },
  wait_message: new WebAssembly.Suspending(form => new Promise(resolve => {
    messageWaiters.push({form, resolve}); invalidate();
  })),
  control_bind: (pointer, idPtr, idLen) => {
    const element = document.getElementById(text(idPtr, idLen));
    const dispatch = () => guarded(async () => {
      await api.lcl_control(pointer);
      wakeMessage();
    });
    const editable = element.matches('textarea, input:not([type=checkbox])');
    const routesKey = event => event.key === 'Escape' || event.key === 'Tab'
      || (editable && event.key === 'Enter' && element.tagName !== 'TEXTAREA');
    element.onfocus = () => guarded(() => api.lcl_focus(pointer));
    element.onkeydown = event => {
      if (!event.isComposing && routesKey(event)) {
        event.preventDefault(); key(0, event, true);
      }
    };
    element.onkeyup = event => {
      if (!event.isComposing && routesKey(event)) key(1, event, true);
    };
    if (editable) {
      element.oninput = event => { if (!event.isComposing) dispatch(); };
      element.oncompositionend = dispatch;
      element.onselect = dispatch;
    } else element.onclick = event => { event.stopPropagation(); dispatch(); };
  },
  title: (pointer, length) => { document.title = text(pointer, length); },
  timer: (handle, interval) => setInterval(() => {
    if (ready) guarded(async () => {
      await api.lcl_timer(handle);
      wakeMessage();
    });
  }, interval),
  clear_timer: handle => clearInterval(handle),
  font: (pointer, length, size, style, angle) => {
    const name = text(pointer, length).replace(/["\\]/g, '') || 'sans-serif';
    currentFont = `${style & 2 ? 'italic ' : ''}${style & 1 ? 'bold ' : ''}${size}px "${name}", sans-serif`;
    fontStyle = style; fontAngle = -angle * Math.PI / 1800;
  },
  measure: (pointer, length, size) => {
    textContext.font = currentFont;
    return Math.ceil(textContext.measureText(text(pointer, length)).width);
  },
  text: (pointer, width, height, x, y, string, length, size, color, left, top, right, bottom) => {
    if (textCanvas.width !== width) textCanvas.width = width;
    if (textCanvas.height !== height) textCanvas.height = height;
    textContext.font = currentFont;
    textContext.textBaseline = 'top';
    const stringValue = text(string, length);
    const metrics = textContext.measureText(stringValue);
    const c = Math.cos(fontAngle), s = Math.sin(fontAngle);
    const glyphLeft = -metrics.actualBoundingBoxLeft;
    const glyphTop = -metrics.actualBoundingBoxAscent;
    const glyphRight = metrics.actualBoundingBoxRight;
    const glyphBottom = Math.max(metrics.actualBoundingBoxDescent, fontStyle & 4 ? size : 0);
    const corners = [[glyphLeft,glyphTop],[glyphRight,glyphTop],[glyphLeft,glyphBottom],[glyphRight,glyphBottom]]
      .map(([gx,gy]) => [x+gx*c-gy*s, y+gx*s+gy*c]);
    const x0 = Math.max(0, left, Math.floor(Math.min(...corners.map(p=>p[0])))-1);
    const y0 = Math.max(0, top, Math.floor(Math.min(...corners.map(p=>p[1])))-1);
    const x1 = Math.min(width, right, Math.ceil(Math.max(...corners.map(p=>p[0])))+1);
    const y1 = Math.min(height, bottom, Math.ceil(Math.max(...corners.map(p=>p[1])))+1);
    if (x1 <= x0 || y1 <= y0) return;
    textContext.clearRect(x0, y0, x1-x0, y1-y0);
    textContext.save();
    textContext.fillStyle = `#${(color >>> 0).toString(16).padStart(6, '0')}`;
    textContext.beginPath();
    textContext.rect(x0, y0, x1-x0, y1-y0);
    textContext.clip();
    textContext.translate(x,y); textContext.rotate(fontAngle);
    textContext.fillText(stringValue,0,0);
    if (fontStyle & 4) textContext.fillRect(0,size-1,metrics.width,Math.max(1,size/16));
    if (fontStyle & 8) textContext.fillRect(0,size*.5,metrics.width,Math.max(1,size/16));
    textContext.restore();
    const layer = textContext.getImageData(x0, y0, x1-x0, y1-y0).data;
    const pixels = new Uint8Array(instance.exports.memory.buffer, pointer, width*height*4);
    for (let row=0; row<y1-y0; row++) {
      for (let column=0; column<x1-x0; column++) {
        const i = (row*(x1-x0)+column)*4;
        const alpha = layer[i+3] / 255;
        if (!alpha) continue;
        const dest = ((row+y0)*width+column+x0)*4;
        pixels[dest] = 255;
        pixels[dest+1] = Math.round(layer[i]*alpha + pixels[dest+1]*(1-alpha));
        pixels[dest+2] = Math.round(layer[i+1]*alpha + pixels[dest+2]*(1-alpha));
        pixels[dest+3] = Math.round(layer[i+2]*alpha + pixels[dest+3]*(1-alpha));
      }
    }
  },
  common_dialog: new WebAssembly.Suspending(async (pointer, length, output, capacity) => {
    const result = await commonDialog(JSON.parse(text(pointer,length)));
    if (result === null) return 0;
    const bytes = new TextEncoder().encode(JSON.stringify(result));
    if (bytes.length >= capacity) throw new Error('Dialog response exceeds its buffer');
    new Uint8Array(instance.exports.memory.buffer, output, bytes.length).set(bytes);
    return bytes.length;
  }),
  dialog: new WebAssembly.Suspending((pointer, length) => showDialog(JSON.parse(text(pointer, length)))),
  message: new WebAssembly.Suspending((textPtr, captionPtr, textLen, captionLen, flags) => {
    const sets = [[1], [1,2], [3,4,5], [6,7,2], [6,7], [4,2]];
    return showDialog({kind:'message', caption:text(captionPtr,captionLen),
      message:text(textPtr,textLen), buttons:sets[flags & 15] || [1], escape:(flags & 15) === 0 ? 1 : 2});
  }),
  menus: (pointer, length) => updateMenus(text(pointer, length)),
  present: (pointer, width, height, left = 0, top = 0, right = width, bottom = height, last = 1) => {
    if (canvas.width !== width) canvas.width = width;
    if (canvas.height !== height) canvas.height = height;
    // The surface is sized by CSS; the canvas keeps the form's pixel size and
    // scales down to fit, so the UI never overflows the browser window.
    // LazCanvas clfARGB32 bytes A, R, G, B become canvas bytes R, G, B, A:
    // one shift per little-endian word into a reused buffer.
    if (!image || image.width !== width || image.height !== height) image = context.createImageData(width,height);
    const source = new Uint32Array(instance.exports.memory.buffer, pointer, width*height);
    const target = new Uint32Array(image.data.buffer);
    for (let y=top; y<bottom; y++) {
      const end = y*width+right;
      for (let i=y*width+left; i<end; i++) target[i] = (source[i] >>> 8) | 0xFF000000;
    }
    context.putImageData(image,0,0,left,top,right-left,bottom-top);
    if (last) canvas.dataset.frames = String(Number(canvas.dataset.frames || 0)+1);
  }
};
const buttonNames = {1:'OK', 2:'Cancel', 3:'Abort', 4:'Retry', 5:'Ignore', 6:'Yes', 7:'No',
  8:'All', 9:'No to all', 10:'Yes to all', 11:'Close', 12:'Continue', 13:'Try again'};
function showDialog(options) {
  if (options.kind === 'select') return selectItemDialog(options);
  const dialog = document.createElement('dialog');
  dialog.setAttribute('aria-label', options.caption || 'TpX');
  dialog.style.cssText = 'max-width:min(600px,90vw);border:1px solid #a5b8ce;padding:20px;background:white;color:#203e69;font:15px system-ui';
  const heading = document.createElement('h2');
  heading.textContent = options.caption || 'TpX';
  const body = document.createElement('p');
  body.textContent = options.message || '';
  body.style.whiteSpace = 'pre-wrap';
  const buttons = document.createElement('div');
  buttons.style.cssText = 'display:flex;justify-content:flex-end;gap:8px;margin-top:20px';
  dialog.append(heading, body, buttons);
  document.body.append(dialog);
  return new Promise(resolve => {
    function finish(value) {
      dialog.close(); dialog.remove(); resolve(value); canvas.focus({preventScroll:true});
    }
    const choices = options.buttons || [1,2];
    choices.forEach((choice, index) => {
      const id = typeof choice === 'object' ? choice.id : choice;
      const button = document.createElement('button');
      button.textContent = (choice.caption || buttonNames[id] || String(id))
        .replace(/&&/g, '\u0000').replace(/&/g, '').replace(/\u0000/g, '&');
      button.type = 'button';
      button.addEventListener('click', () => finish(id));
      if (index === (options.default || 0)) button.autofocus = true;
      buttons.append(button);
    });
    dialog.addEventListener('cancel', event => {
      event.preventDefault();
      if (options.escape === -1) return;
      finish(options.escape ?? (choices.includes(2) ? 2 : typeof choices[0] === 'object' ? choices[0].id : choices[0]));
    });
    dialog.showModal();
  });
}
function selectItemDialog(options) {
  const dialog = document.createElement('dialog');
  dialog.setAttribute('aria-label', options.caption);
  dialog.style.cssText = 'max-width:90vw;padding:16px;border:1px solid #a5b8ce;font:15px system-ui';
  const form = document.createElement('form');
  const label = document.createElement('label');
  label.textContent = options.caption;
  const select = document.createElement('select');
  select.size = Math.min(10, options.items.length);
  select.style.cssText = 'display:block;min-width:240px;max-width:80vw;max-height:60vh;margin:12px 0;font:inherit';
  options.items.forEach((caption, index) => {
    const option = document.createElement('option');
    option.textContent = caption; option.value = index; select.append(option);
  });
  select.selectedIndex = 0;
  label.append(select);
  const ok = document.createElement('button'); ok.type = 'submit'; ok.textContent = 'OK';
  const cancel = document.createElement('button'); cancel.type = 'button'; cancel.textContent = 'Cancel';
  form.append(label, ok, cancel); dialog.append(form); document.body.append(dialog);
  return new Promise(resolve => {
    const finish = value => { dialog.close(); dialog.remove(); canvas.focus({preventScroll:true}); resolve(value); };
    form.onsubmit = event => { event.preventDefault(); finish(select.selectedIndex); };
    select.ondblclick = () => finish(select.selectedIndex);
    select.onkeydown = event => { if (event.key === 'Enter') { event.preventDefault(); finish(select.selectedIndex); } };
    cancel.onclick = () => finish(-1);
    dialog.addEventListener('cancel', event => { event.preventDefault(); finish(-1); });
    dialog.showModal(); select.focus();
  });
}
function commonDialog(options) {
  const dialog = document.createElement('dialog');
  dialog.style.cssText = 'min-width:320px;max-width:90vw;padding:20px;border:1px solid #a5b8ce;font:15px system-ui';
  dialog.setAttribute('aria-label', options.caption || options.kind);
  const heading = document.createElement('h2'); heading.textContent = options.caption || options.kind;
  const form = document.createElement('form'); form.method = 'dialog';
  dialog.append(heading, form);
  const fields = {};
  function field(name, label, type, value) {
    const row = document.createElement('label'); row.style.cssText = 'display:block;margin:12px 0';
    row.append(document.createTextNode(label + ' '));
    const input = document.createElement('input'); input.type=type; input.name=name;
    if (type==='checkbox') input.checked=Boolean(value); else input.value=value ?? '';
    row.append(input); form.append(row); fields[name]=input; return input;
  }
  if (options.kind==='open') {
    field('file', 'Choose file', 'file').multiple = Boolean(options.multiple);
    field('related', 'Related files (optional)', 'file').multiple = true;
  }
  if (options.kind==='save') field('filename', 'File name', 'text', options.filename || 'drawing.tpx');
  if (options.kind==='open' || options.kind==='save') {
    const label=document.createElement('label'); label.textContent='File type ';
    const select=document.createElement('select'); select.name='filterIndex';
    const filters=(options.filter || 'All files|*.*').split('|');
    for (let i=0;i<filters.length-1;i+=2) {
      const option=document.createElement('option'); option.value=i/2+1; option.textContent=filters[i];
      option.dataset.pattern=filters[i+1]; select.append(option);
    }
    select.value=options.filterIndex || 1; label.append(select); form.append(label); fields.filterIndex=select;
  }
  if (options.kind==='color') {
    const c=options.color>>>0;
    field('color','Color','color','#'+((c&255)<<16|(c&65280)|((c>>>16)&255)).toString(16).padStart(6,'0'));
  }
  if (options.kind==='font') {
    field('name','Font family','text',options.name || 'sans-serif');
    const size=field('size','Size (pt)','number',options.size || 12); size.min=1; size.max=1000;
    field('bold','Bold','checkbox',options.bold); field('italic','Italic','checkbox',options.italic);
    field('underline','Underline','checkbox',options.underline); field('strikeout','Strikeout','checkbox',options.strikeout);
  }
  const footer=document.createElement('div'); footer.style.cssText='display:flex;gap:12px;justify-content:flex-end;margin-top:20px';
  const ok=document.createElement('button'); ok.type='submit'; ok.textContent='OK';
  const cancel=document.createElement('button'); cancel.type='button'; cancel.textContent='Cancel';
  footer.append(ok,cancel); form.append(footer); document.body.append(dialog);
  return new Promise(resolve => {
    const finish=result => {dialog.close();dialog.remove();canvas.focus({preventScroll:true});resolve(result);};
    cancel.onclick=()=>finish(null);
    dialog.addEventListener('cancel',event=>{event.preventDefault();finish(null);});
    form.onsubmit=async event=>{
      event.preventDefault();
      if (!form.reportValidity()) return;
      let result;
      if (options.kind==='open') {
        const files=Array.from(fields.file.files); if (!files.length) return;
        for (const file of [...Array.from(fields.related.files), ...files]) {
          fileSystem.dir.contents.set(file.name, new File(new Uint8Array(await file.arrayBuffer())));
          downloadPaths.add(file.name);
        }
        result={filename:'/'+files[0].name,files:files.map(file=>'/'+file.name),filterIndex:Number(fields.filterIndex.value)};
      } else if (options.kind==='save') {
        let name=fields.filename.value.trim().replace(/[/\\]/g,'_'); if (!name) return;
        const pattern=fields.filterIndex.selectedOptions[0]?.dataset.pattern || '';
        const ext=(pattern.match(/\*\.(\w+)/)||[])[1] || (options.defaultExt || '').replace(/^\./,'');
        if (ext && !name.toLowerCase().endsWith('.'+ext.toLowerCase())) name+='.'+ext;
        downloadPaths.add(name); result={filename:'/'+name,filterIndex:Number(fields.filterIndex.value)};
      } else if (options.kind==='color') {
        const c=Number.parseInt(fields.color.value.slice(1),16);
        result={color:((c&255)<<16)|(c&65280)|((c>>>16)&255)};
      } else result={name:fields.name.value,size:Number(fields.size.value),bold:fields.bold.checked,italic:fields.italic.checked,underline:fields.underline.checked,strikeout:fields.strikeout.checked};
      finish(result);
    };
    dialog.showModal();
  });
}
function download(name, bytes) {
  const url=URL.createObjectURL(new Blob([bytes])); const link=document.createElement('a');
  link.href=url;link.download=name;link.click();setTimeout(()=>URL.revokeObjectURL(url),30000);
}
let previousMenus = '';
function updateMenus(json) {
  if (json === previousMenus) return;
  previousMenus = json;
  let nav = document.querySelector('#lcl-menus');
  if (!nav) {
    nav = document.createElement('nav'); nav.id = 'lcl-menus'; nav.setAttribute('aria-label', 'Application menu');
    nav.style.cssText = 'display:flex;flex-wrap:wrap;gap:14px;padding:6px 0';
    document.querySelector('#lcl-surface').parentElement.before(nav);
  }
  nav.replaceChildren();
  function append(items, parent) {
    for (const item of items) {
      if (!item.visible) continue;
      const caption = item.caption.replace(/&&/g, '\u0000').replace(/&/g, '').replace(/\u0000/g, '&');
      if (caption === '-') { parent.append(document.createElement('hr')); continue; }
      if (item.children.length) {
        const group = document.createElement('details');
        const summary = document.createElement('summary'); summary.textContent = caption;
        const content = document.createElement('div');
        group.style.position = 'relative';
        content.style.cssText = 'position:absolute;z-index:30;min-width:220px;padding:6px;background:white;border:1px solid #a5b8ce;box-shadow:0 3px 10px #0002';
        group.append(summary, content); append(item.children, content); parent.append(group);
      } else {
        const button = document.createElement('button');
        button.textContent = (item.checked ? '✓ ' : '') + caption;
        button.dataset.menuId = item.id; button.disabled = !item.enabled;
        button.style.cssText = 'display:block;width:100%;text-align:left;border:0;background:white;padding:5px;cursor:pointer';
        button.onclick = () => {
          nav.querySelectorAll('details[open]').forEach(d => {d.open=false;});
          canvas.focus({preventScroll:true});
          guarded(() => api.lcl_menu(item.id));
        };
        parent.append(button);
      }
    }
  }
  append(JSON.parse(json).children, nav);
}
function pointer(kind, x, y, button, modifiers) {
  if (ready) guarded(async () => {
    await api.lcl_pointer(kind, x, y, button, modifiers);
    clearTimeout(pointerIdleTimer);
    if (kind === 2) pointerIdleTimer = setTimeout(() => guarded(() => api.lcl_idle()), 120);
    wakeMessage();
  });
}
// Drag previews follow the display cadence. Hover coordinates and crosshairs
// update at 30 Hz; down/up always flush the latest position immediately.
function flushMove(force = false) {
  if (!pendingMove) return;
  const now = performance.now();
  if (!force && !(pendingMove[4] & 8) && now-lastHoverMove < 32) {
    invalidate();
    return;
  }
  lastHoverMove = now;
  const move = pendingMove;
  pendingMove = null;
  pointer(2, ...move);
}
for (const [name, kind] of [['pointerdown', 0], ['pointerup', 1], ['pointermove', 2]]) {
  canvas.addEventListener(name, event => {
    if (!ready) return;
    event.preventDefault();
    if (kind === 0) { canvas.focus({preventScroll: true}); canvas.setPointerCapture(event.pointerId); }
    const bounds = canvas.getBoundingClientRect();
    const x = Math.round((event.clientX-bounds.left)*canvas.width/bounds.width);
    const y = Math.round((event.clientY-bounds.top)*canvas.height/bounds.height);
    const modifiers = Number(event.shiftKey) | Number(event.ctrlKey || event.metaKey)<<1 | Number(event.altKey)<<2 | Number(event.buttons&1)<<3;
    if (kind === 2) {
      pendingMove = [x, y, event.button, modifiers];
      invalidate();
      return;
    }
    flushMove(true);
    pointer(kind, x, y, event.button, modifiers);
    if (kind === 1 && canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
  });
}
canvas.addEventListener('contextmenu', event => event.preventDefault());

// Keys: Windows virtual key codes for the keys LCL routes, Unicode scalar for
// text input. Modifiers follow the pointer encoding: shift 1, ctrl 2, alt 4.
const KEYS = {Backspace: 8, Tab: 9, Enter: 13, Shift: 16, Control: 17, Alt: 18,
  Escape: 27, PageUp: 33, PageDown: 34, End: 35, Home: 36, Insert: 45, Delete: 46,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Meta: 91};
const PREVENT = new Set(['Tab', 'Escape', 'Enter', 'Backspace', 'Delete', 'ArrowLeft',
  'ArrowUp', 'ArrowRight', 'ArrowDown', 'PageUp', 'PageDown', 'F1', 'F2', 'F3', 'F4',
  'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12']);
function vk(key) {
  if (key in KEYS) return KEYS[key];
  if (key.length === 1) return key.toUpperCase().charCodeAt(0);
  if (/^F([1-9]|1[0-2])$/.test(key)) return 111 + Number(key.slice(1));
  return 0;
}
const modifiers = event => Number(event.shiftKey) | Number(event.ctrlKey || event.metaKey) << 1 |
  Number(event.altKey) << 2 | Number(event.buttons & 1) << 3;
function key(kind, event, fromControl = false) {
  if (!ready || (!fromControl && event.target !== canvas && event.target.closest?.('dialog, #lcl-dom, #lcl-menus'))) return;
  if (PREVENT.has(event.key) || ((event.ctrlKey || event.metaKey) && event.key.length === 1)) event.preventDefault();
  const code = vk(event.key);
  guarded(async () => {
    await (kind === 2 ? api.lcl_key(2, code, event.key.codePointAt(0), modifiers(event))
      : api.lcl_key(kind, code, 0, modifiers(event)));
    wakeMessage();
  });
}
document.addEventListener('keydown', event => { key(0, event); if (event.key.length === 1 && !event.ctrlKey && !event.metaKey && !event.altKey) key(2, event); });
document.addEventListener('keyup', event => key(1, event));
canvas.addEventListener('wheel', event => {
  if (!ready) return;
  event.preventDefault();
  const b = canvas.getBoundingClientRect();
  const x = Math.round((event.clientX-b.left)*canvas.width/b.width);
  const y = Math.round((event.clientY-b.top)*canvas.height/b.height);
  // LCL wants Windows wheel ticks (120 per notch, positive scrolls up).
  const delta = Math.sign(-event.deltaY) * 120;
  guarded(() => api.lcl_wheel(x, y, delta, modifiers(event)));
}, {passive: false});

// The form follows the window instead of the desktop size it was saved with.
function surfaceBox() {
  const surface = document.querySelector('#lcl-surface');
  const left = surface.getBoundingClientRect().left;
  return [Math.max(320, Math.floor(document.documentElement.clientWidth - left*2)),
          Math.max(240, Math.floor(window.innerHeight - surface.getBoundingClientRect().top - 16))];
}
function resize() {
  if (!ready) return;
  const [width, height] = surfaceBox();
  guarded(() => api.lcl_resize(width, height));
}
// Native widgetsets receive WM_MOUSELEAVE; the browser must say so as well, or
// a crosshair painted on the paper is never erased.
canvas.addEventListener('pointerleave', () => {
  pendingMove = null;
  if (ready) guarded(() => api.lcl_leave());
});
canvas.addEventListener('pointercancel', () => {
  if (ready) guarded(() => api.lcl_pointer(1, 0, 0, 0, 0));
});

window.addEventListener('resize', () => requestAnimationFrame(resize));
async function start() {
  fileSystem = new PreopenDirectory('/', []);
  const wasi = new WASI(argv, [], [
    new OpenFile(new File([])),
    ConsoleStdout.lineBuffered(line => console.log(`[Pascal] ${line}`)),
    ConsoleStdout.lineBuffered(line => console.error(`[Pascal] ${line}`)),
    fileSystem
  ]);
  // FPC's generic WASI RTL imports these services even though this demo does not use them.
  const wasiImports = {...wasi.wasiImport};
  const paths=new Map(), written=new Set();
  const open=wasiImports.path_open, write=wasiImports.fd_write, close=wasiImports.fd_close;
  wasiImports.path_open=(...args)=>{
    const name=text(args[2],args[3]).replace(/^\/+/, '');
    const result=open(...args);
    if (!result) paths.set(new DataView(instance.exports.memory.buffer).getUint32(args[8],true),name);
    return result;
  };
  wasiImports.fd_write=(fd,...args)=>{const result=write(fd,...args);if (!result) written.add(fd);return result;};
  wasiImports.fd_close=fd=>{
    const name=paths.get(fd), file=wasi.fds[fd]?.file;
    const result=close(fd);
    if (!result && written.has(fd) && downloadPaths.has(name) && file) download(name,file.data.slice());
    written.delete(fd);paths.delete(fd);return result;
  };
  wasiImports.random_get = (pointer, length) => {
    const bytes = new Uint8Array(instance.exports.memory.buffer, pointer, length);
    for (let offset=0; offset<length; offset+=65536) crypto.getRandomValues(bytes.subarray(offset, offset+65536));
    return 0;
  };
  wasiImports.clock_time_get = (id, precision, pointer) => {
    const nanoseconds = BigInt(Math.floor((id === 0 ? Date.now() : performance.now())*1e6));
    new DataView(instance.exports.memory.buffer).setBigUint64(pointer, nanoseconds, true);
    return 0;
  };
  const response = await fetch(moduleURL);
  if (!response.ok) throw new Error(`WASM download failed (${response.status})`);
  const module = await WebAssembly.compileStreaming(response);
  const missing = WebAssembly.Module.imports(module).filter(i => i.module === 'wasi_snapshot_preview1' && !(i.name in wasiImports));
  if (missing.length) throw new Error(`Missing WASI services: ${missing.map(i=>i.name).join(', ')}`);
  instance = await WebAssembly.instantiate(module, {wasi_snapshot_preview1: wasiImports, job: jobHost.imports, lcl: imports});
  jobHost.connect(instance);
  for (const [name, value] of Object.entries(instance.exports)) {
    if (typeof value === 'function') api[name] = WebAssembly.promising(value);
  }
  wasi.inst = instance;
  if (api._initialize) await api._initialize();
  ready = true;
  window.lclDemo = api;
  status.textContent = 'Ready';
  status.dataset.state = 'ready';
  resize();
  invalidate();
}
await start().catch(fail);
}
