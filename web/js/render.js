// Frame composition: background, overlay, then the curves of the current mode.
import { createColormaps } from './gl/colormap.js';
import { createEvaluator } from './gl/evaluate.js';
import { createLines } from './gl/lines.js';
import { createOverlay } from './gl/overlay.js';
import { createParams } from './gl/params.js';

export const SCALE = 0.9;              // unit disk radius in clip space at zoom 1
const MAX_DRAW_POINTS = 64 * 2 ** 20;  // per frame, across batches

export const hexColor = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);

// clip = (z - c) * k; the unit disk fits the shorter side.
export function viewTransform({ cx, cy, zoom }, w, h) {
  const s = SCALE * zoom * Math.min(w, h);
  return [cx, cy, s / w, s / h];
}

export async function createRenderer(gl, engine) {
  const colormaps = createColormaps(gl);
  const [lines, overlay, evaluator] = await Promise.all([
    createLines(gl, colormaps), createOverlay(gl), createEvaluator(gl),
  ]);
  const params = createParams(gl);
  const grid = { family: 0, total: 0, slab: -1, evaluated: false, T: null };

  const lineStyle = (style, w, h) => ({ ...style, width: (style.width * Math.min(w, h)) / 1000 });

  function blend(mode) {
    gl.enable(gl.BLEND);
    if (mode === 'add') gl.blendFunc(gl.ONE, gl.ONE);
    else gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  }

  function clear(state, w, h) {
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, w, h);
    const [r, g, b] = hexColor(state.style.background);
    gl.clearColor(r, g, b, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    if (state.style.overlay) overlay.draw({ view: viewTransform(state.view, w, h) });
  }

  function drawGrid(state, w, h) {
    const T = { t0: state.t.lo, t1: state.t.hi, samples: state.t.n };
    const perEval = Math.floor(evaluator.capacity / T.samples);
    const total = Math.min(grid.total, Math.floor(MAX_DRAW_POINTS / T.samples));
    const single = total <= engine.maxCurves && total <= perEval;
    const view = viewTransform(state.view, w, h);
    const style = lineStyle(state.style, w, h);
    blend(state.style.blend);
    for (let slab = 0; slab < total; slab += engine.maxCurves) {
      const n = Math.min(engine.maxCurves, total - slab);
      if (grid.slab !== slab) {
        const { data, stats } = engine.buildGrid(slab, n);
        params.set(data, stats.curves);
        grid.slab = slab;
        grid.evaluated = false;
      }
      for (let c = 0; c < n; c += perEval) {
        const m = Math.min(perEval, n - c);
        if (!(single && grid.evaluated)) evaluator.evaluate(params, grid.family, c, m, T);
        lines.draw({ params, points: evaluator.texture, first: c, count: m, samples: T.samples,
          view, viewport: [w, h], style });
      }
      grid.evaluated = true;
      if (!single) grid.slab = -1;
    }
    return total;
  }

  return {
    colormaps,

    // After the engine's scene changed: rebuilds the grid's first slab and
    // returns its stats.
    setGrid(family) {
      grid.family = family;
      grid.total = engine.gridSize();
      const { data, stats } = engine.buildGrid(0, Math.min(grid.total, engine.maxCurves));
      params.set(data, stats.curves);
      grid.slab = 0;
      grid.evaluated = false;
      return stats;
    },

    // Points must be re-evaluated (t range changed) but params are still valid.
    invalidatePoints() { grid.evaluated = false; },

    // Returns {drawn} curve count for the status line.
    draw(state, w, h) {
      clear(state, w, h);
      const drawn = state.mode === 'render' ? drawGrid(state, w, h) : 0;
      return { drawn, limited: drawn < grid.total };
    },
  };
}
