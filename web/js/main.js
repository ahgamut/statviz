import { loadEngine } from './engine.js';
import { createColormaps } from './gl/colormap.js';
import { createEvaluator } from './gl/evaluate.js';
import { createLines } from './gl/lines.js';
import { createOverlay } from './gl/overlay.js';
import { createParams } from './gl/params.js';

const SCENE = {
  family: 'normal',
  params: ['1', '0.02 + 0.1*u'],
  u: { lo: 0, hi: 1, n: 12 },
  t0: 0, t1: 60, samples: 4000,
};
const STYLE = { width: 1.5, opacity: 1, colorBy: 'u', colormap: 'magma', reverse: false };
const SCALE = 0.9;  // unit disk radius in clip space at zoom 1

function showError(err) {
  const el = document.getElementById('error');
  el.textContent = String(err?.message ?? err);
  el.hidden = false;
  console.error(err);
}

function buildScene(engine) {
  const family = engine.meta.families.find((f) => f.key === SCENE.family);
  engine.setFamily(family.id);
  SCENE.params.forEach((src, i) => {
    const err = engine.setParam(i, src);
    if (err) throw new Error(`${family.params[i].name} = ${src}: ${err.message} (at ${err.pos})`);
  });
  engine.setSweep('u', SCENE.u.lo, SCENE.u.hi, SCENE.u.n);
  engine.setSweep('v', 0, 0, 1);
  return { family: family.id, ...engine.buildGrid() };
}

async function main() {
  const canvas = document.getElementById('view');
  const gl = canvas.getContext('webgl2', { antialias: false, premultipliedAlpha: true });
  if (!gl) throw new Error('WebGL2 is not available in this browser.');

  const colormaps = createColormaps(gl);
  const [engine, lines, overlay, evaluator] = await Promise.all([
    loadEngine(),
    createLines(gl, colormaps),
    createOverlay(gl),
    createEvaluator(gl),
  ]);

  const { family, data, stats } = buildScene(engine);
  const params = createParams(gl);
  params.set(data, stats.curves);
  evaluator.evaluate(params, family, 0, stats.curves, SCENE);

  const draw = () => {
    const w = canvas.width, h = canvas.height, s = Math.min(w, h);
    const view = [0, 0, (SCALE * s) / w, (SCALE * s) / h];
    gl.viewport(0, 0, w, h);
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    overlay.draw({ view });
    lines.draw({
      params, points: evaluator.texture, count: stats.curves, samples: SCENE.samples,
      view, viewport: [w, h], style: { ...STYLE, width: (STYLE.width * s) / 1000 },
    });
  };

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
}

main().catch(showError);
