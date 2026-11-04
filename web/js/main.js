import { loadEngine } from './engine.js';
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
      renderer.scrub.reset();
      pushScrub();
      requestDraw();
    },
  };

  const panel = createPanel(document.getElementById('panel'), { meta, store, actions });
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

  window.addEventListener('keydown', (e) => {
    if (e.target.closest('input, select, textarea') || e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.key === 'h') document.body.classList.toggle('panel-hidden');
    else if (e.key === 'c') actions.clearTrail();
  });
}

main().catch(showError);
