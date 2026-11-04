import { loadEngine } from './engine.js';
import { createRenderer } from './render.js';
import { SLIDER_VARS, createStore, defaultState } from './state.js';
import { createPanel } from './ui/panel.js';

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
  const store = createStore(defaultState());
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
    info.stats = s.mode === 'render' ? renderer.setGrid(family.id) : null;
  }

  const actions = {
    setFamily(key) {
      const s = store.state;
      familyExprs.set(s.family, [...s.exprs]);
      const family = familyOf(key);
      store.set('family', key);
      store.set('exprs', familyExprs.get(key) ?? family.params.map((p) => String(p.default)));
    },
    clearTrail() {},
  };

  const panel = createPanel(document.getElementById('panel'), { meta, store, actions });

  let frame = 0;
  const requestDraw = () => { frame ||= requestAnimationFrame(draw); };
  function draw() {
    frame = 0;
    const s = store.state;
    const { drawn, limited } = renderer.draw(s, canvas.width, canvas.height);
    const total = engine.gridSize();
    if (s.mode === 'render') {
      info.status = `${total.toLocaleString()} curves × ${s.t.n.toLocaleString()} samples`
        + (info.stats && info.stats.valid < Math.min(total, engine.maxCurves) ? ` · ${(Math.min(total, engine.maxCurves) - info.stats.valid).toLocaleString()} skipped` : '')
        + (limited ? ` · drawing the first ${drawn.toLocaleString()}` : '');
    }
    panel.update(s, info);
  }

  store.subscribe((paths) => {
    const has = (prefix) => [...paths].some((p) => p === '*' || p.startsWith(prefix));
    if (has('style.custom')) renderer.colormaps.setCustom(store.state.style.custom.map((c, i) => [i / 2, c]));
    if (['family', 'exprs', 'policy', 'u.', 'v.', 'vars.', 'mode'].some(has)) applyScene();
    else if (has('t.')) renderer.invalidatePoints();
    panel.update(store.state, info);
    requestDraw();
  });

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
