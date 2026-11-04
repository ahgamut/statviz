import { loadEngine } from './engine.js';
import { download, exportName, exportPng, sceneFromPng } from './export.js';
import { createRenderer } from './render.js';
import { SLIDER_VARS, createStore, decodeState, defaultState, encodeState } from './state.js';
import { createPanel } from './ui/panel.js';
import { attachViewControls } from './ui/viewctl.js';

function showError(err) {
  const el = document.getElementById('error');
  el.textContent = String(err?.message ?? err);
  el.hidden = false;
  console.error(err);
}

async function main() {
  const canvas = document.getElementById('view');
  const gl = canvas.getContext('webgl2', { antialias: false, premultipliedAlpha: true });
  if (!gl) throw new Error('WebGL2 is not available in this browser.');

  const engine = await loadEngine();
  const renderer = await createRenderer(gl, engine);
  const meta = engine.meta;
  const initial = decodeState(location.hash) ?? defaultState();
  if (!meta.families.some((f) => f.key === initial.family)) initial.family = defaultState().family;
  const store = createStore(initial);
  const info = { errors: [], stats: null, varsUsed: new Set(), status: '' };
  const familyExprs = new Map();  // per-family expressions, this session only

  const familyOf = (key) => meta.families.find((f) => f.key === key) ?? meta.families[0];

  // Pushes the scene into the engine and rebuilds the grid. A field with an
  // error keeps its last valid expression in the engine.
  let engineFamily = null;
  function applyScene() {
    actions.cancelExport('the scene changed');
    const s = store.state;
    const family = familyOf(s.family);
    if (family !== engineFamily) {
      engine.setFamily(family.id);
      engineFamily = family;
    }
    info.errors = family.params.map((_, i) => engine.setParam(i, s.exprs[i] ?? ''));
    engine.setPolicy(s.policy);
    info.varsUsed = new Set(engine.varsUsed());
    for (const name of SLIDER_VARS) engine.setVar(name, s.vars[name].value);
    for (const axis of ['u', 'v']) {
      const r = s[axis];
      engine.setSweep(axis, r.lo, r.hi, info.varsUsed.has(axis) ? r.n : 1);
    }
    if (s.mode === 'render') {
      info.stats = renderer.setGrid(family.id);
    } else {
      renderer.setFamily(family.id);
      renderer.scrub.reset();
      if (s.mode === 'scrub') pushScrub();
    }
  }

  function pushScrub() {
    renderer.scrub.push(store.state, canvas.width, canvas.height);
    info.stats = renderer.scrub.stats;
  }

  let exporting = null;  // {ctl: AbortController, reason}

  const actions = {
    setFamily(key) {
      const s = store.state;
      familyExprs.set(s.family, [...s.exprs]);
      const family = familyOf(key);
      store.set('family', key);
      store.set('exprs', familyExprs.get(key) ?? family.params.map((p) => String(p.default)));
    },
    toggleRender() {
      const p = renderer.progress;
      if (p) renderer.pause(!p.paused);
      requestDraw();
    },
    clearTrail() {
      if (store.state.mode !== 'scrub') return;
      actions.cancelExport('the trail was cleared');
      renderer.scrub.reset();
      pushScrub();
      requestDraw();
    },

    exportSize(kind) {
      const max = renderer.maxExport, aspect = canvas.width / Math.max(1, canvas.height);
      const [w, h] = kind === 'square' ? [max, max]
        : aspect >= 1 ? [max, Math.round(max / aspect)] : [Math.round(max * aspect), max];
      store.set('export.w', Math.max(16, w));
      store.set('export.h', Math.max(16, h));
    },

    async exportPng() {
      if (exporting || store.state.mode === 'animate') return;
      const s = structuredClone(store.state);
      const max = renderer.maxExport;
      s.export.w = Math.min(max, s.export.w);
      s.export.h = Math.min(max, s.export.h);
      const job = { ctl: new AbortController(), reason: '' }, ctl = job.ctl;
      exporting = job;
      info.export = { running: true, phase: 'drawing', fraction: 0 };
      panel.update(store.state, info);
      try {
        const blob = await exportPng({ renderer, engine }, s, {
          preview: [canvas.width, canvas.height],
          signal: ctl.signal,
          onProgress(phase, fraction) {
            Object.assign(info.export, { phase, fraction });
            panel.update(store.state, info);
          },
        });
        const name = exportName(s);
        download(blob, name);
        const size = blob.size < 2 ** 20 ? `${Math.ceil(blob.size / 1024)} KB` : `${(blob.size / 2 ** 20).toFixed(1)} MB`;
        info.export = { note: `saved ${name} · ${size}` };
      } catch (err) {
        if (!ctl.signal.aborted) console.error(err);
        info.export = { note: ctl.signal.aborted ? 'cancelled' + (job.reason ? `: ${job.reason}` : '')
          : `export failed: ${err.message ?? err}` };
      } finally {
        exporting = null;
        panel.update(store.state, info);
        requestDraw();
      }
    },

    // A scene from a PNG exported here (file picker or drop).
    async loadScene(file) {
      const next = await sceneFromPng(file).catch(() => null);
      if (next && meta.families.some((f) => f.key === next.family)) {
        store.replace(next);
        info.export = { note: `loaded the scene from ${file.name}` };
      } else {
        info.export = { note: `${file.name} holds no statviz scene` };
      }
      panel.update(store.state, info);
    },

    cancelExport(reason = '') {
      if (!exporting) return;
      exporting.reason = reason;
      exporting.ctl.abort();
    },
  };

  const panel = createPanel(document.getElementById('panel'), { meta, store, actions, maxExport: renderer.maxExport });
  attachViewControls(canvas, store);
  window.statviz = { store, engine, renderer };  // for the console

  let frame = 0;
  const requestDraw = () => { frame ||= requestAnimationFrame(draw); };
  function draw() {
    frame = 0;
    const s = store.state;
    const busy = renderer.draw(s, canvas.width, canvas.height);
    if (busy) requestDraw();
    info.progress = s.mode === 'render' ? renderer.progress : null;
    const total = engine.gridSize();
    if (s.mode === 'scrub') {
      const keys = engine.scrubSize();
      info.status = s.trail.mode === 'refresh' ? 'one curve'
        : `${keys.toLocaleString()} slider state${keys === 1 ? '' : 's'} in the trail` + (busy ? ' · redrawing…' : '');
    } else if (s.mode === 'render') {
      info.status = `${total.toLocaleString()} curves × ${s.t.n.toLocaleString()} samples`
        + (info.stats && info.stats.valid < Math.min(total, engine.maxCurves) ? ` · ${(Math.min(total, engine.maxCurves) - info.stats.valid).toLocaleString()} skipped` : '');
    }
    panel.update(s, info);
  }

  // The scene lives in the URL hash, so a link reproduces the image.
  let hashTimer = 0, written = location.hash.slice(1);
  store.subscribe(() => {
    clearTimeout(hashTimer);
    hashTimer = setTimeout(() => {
      written = encodeState(store.state);
      history.replaceState(null, '', '#' + written);
    }, 300);
  });
  window.addEventListener('hashchange', () => {
    if (location.hash.slice(1) === written) return;
    const next = decodeState(location.hash);
    if (next && meta.families.some((f) => f.key === next.family)) store.replace(next);
  });

  store.subscribe((paths) => {
    const has = (prefix) => [...paths].some((p) => p === '*' || p.startsWith(prefix));
    if (has('style.custom')) renderer.colormaps.setCustom(store.state.style.custom.map((c, i) => [i / 2, c]));
    const scrub = store.state.mode === 'scrub';
    const all = (re) => [...paths].every((p) => re.test(p));
    const scene = ['family', 'exprs', 'policy', 'u.', 'v.', 'vars.', 'mode'].some(has);
    if (scrub && all(/^(u|v)\.value$|^vars\.\w+\.value$/)) pushScrub();
    else if (scrub && all(/^vars\.\w+\.(lo|hi)$/)) { /* slider ranges only */ }
    else if (scene || (scrub && (has('t.') || has('trail.mode')))) applyScene();
    else if (has('t.')) renderer.invalidatePoints();
    panel.update(store.state, info);
    requestDraw();
  });

  renderer.colormaps.setCustom(store.state.style.custom.map((c, i) => [i / 2, c]));
  applyScene();
  panel.update(store.state, info);

  const resize = new ResizeObserver(([entry]) => {
    const box = entry.devicePixelContentBoxSize?.[0];
    const dpr = window.devicePixelRatio || 1;
    canvas.width = box ? box.inlineSize : Math.round(entry.contentRect.width * dpr);
    canvas.height = box ? box.blockSize : Math.round(entry.contentRect.height * dpr);
    draw();
  });
  try {
    resize.observe(canvas, { box: 'device-pixel-content-box' });
  } catch {
    resize.observe(canvas);  // Safari
  }

  // Dropping an exported PNG anywhere loads its scene.
  const files = (e) => [...(e.dataTransfer?.items ?? [])].some((i) => i.kind === 'file');
  window.addEventListener('dragover', (e) => {
    if (!files(e)) return;
    e.preventDefault();
    document.body.classList.add('dropping');
  });
  window.addEventListener('dragleave', (e) => {
    if (!e.relatedTarget) document.body.classList.remove('dropping');
  });
  window.addEventListener('drop', (e) => {
    document.body.classList.remove('dropping');
    const file = e.dataTransfer?.files[0];
    if (!file) return;
    e.preventDefault();
    actions.loadScene(file);
  });

  window.addEventListener('keydown', (e) => {
    if (e.target.closest('input, select, textarea') || e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.key === 'h') document.body.classList.toggle('panel-hidden');
    else if (e.key === 'c') actions.clearTrail();
    else if (e.key === 'e') actions.exportPng();
  });
}

main().catch(showError);
