// Frame composition: background, overlay, then the curves of the current mode.
// Render mode draws the grid into an offscreen target a slice per frame;
// while a view change is redrawn, the previous image is shown warped.
import { createColormaps } from './gl/colormap.js';
import { createEvaluator } from './gl/evaluate.js';
import { createLines, repeats } from './gl/lines.js';
import { createOverlay } from './gl/overlay.js';
import { createParams } from './gl/params.js';
import { createTrailTools } from './gl/trail.js';
import { createScrub } from './scrub.js';

export const SCALE = 0.9;        // unit disk radius in clip space at zoom 1
export const MAX_EXPORT = 2048;  // px per side
const DRAW_CHUNK = 1 << 20;      // points per lines.draw call, counting repeats
const BUDGET = [1 << 18, 1 << 22, 1 << 26];  // points per frame: min, start, max

export const hexColor = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);

// clip = (z - c) * k; the unit disk fits the shorter side.
export function viewTransform({ cx, cy, zoom }, w, h) {
  const s = SCALE * zoom * Math.min(w, h);
  return [cx, cy, s / w, s / h];
}

export async function createRenderer(gl, engine) {
  const colormaps = createColormaps(gl);
  const [lines, overlay, evaluator, trails] = await Promise.all([
    createLines(gl, colormaps), createOverlay(gl), createEvaluator(gl), createTrailTools(gl),
  ]);
  const params = createParams(gl);
  const grid = { family: 0, total: 0, slab: -1, version: 0 };
  const targets = { front: trails.createTarget(), back: trails.createTarget() };
  const budget = createBudget();
  let job = null;  // Render mode: {key, content, curves, progress, paused}

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

  function loadSlab(slab) {
    if (grid.slab === slab) return;
    const { data, stats } = engine.buildGrid(slab, Math.min(engine.maxCurves, grid.total - slab));
    params.set(data, stats.curves);
    grid.slab = slab;
  }

  // Draws every grid curve into target (under target.view), yielding the
  // points drawn per call. Evaluated points are reused while still valid.
  function* gridCurves(s, target, progress = {}) {
    const T = { t0: s.t.lo, t1: s.t.hi, samples: s.t.n };
    const perEval = Math.floor(evaluator.capacity / T.samples);
    const reps = repeats(s.style);
    const chunk = Math.max(1, Math.floor(DRAW_CHUNK / (T.samples * reps)));
    const style = lineStyle(s.style, target.w, target.h);
    const total = grid.total, version = grid.version;
    progress.done = 0;
    progress.total = total;
    for (let slab = 0; slab < total; slab += engine.maxCurves) {
      const n = Math.min(engine.maxCurves, total - slab);
      for (let c = 0; c < n; c += perEval) {
        const m = Math.min(perEval, n - c), tag = JSON.stringify(['grid', version, slab, c, T]);
        for (let d = 0; d < m; d += chunk) {
          if (grid.version !== version) throw new Error('the scene changed while drawing');
          loadSlab(slab);
          if (evaluator.tag !== tag) evaluator.evaluate(params, grid.family, c, m, T, tag);
          const k = Math.min(chunk, m - d);
          target.bind();
          blend(s.style.blend);
          lines.draw({ params, points: evaluator.texture, first: c + d, offset: d, count: k,
            samples: T.samples, view: target.view, viewport: [target.w, target.h], style });
          progress.done = slab + c + d + k;
          yield k * T.samples * reps;
        }
      }
    }
  }

  function drawRender(s, w, h) {
    const { background, overlay: _, ...look } = s.style;  // not part of the target
    const content = JSON.stringify([grid.version, look, s.t]);
    const key = JSON.stringify([content, s.view, w, h]);
    if (targets.front.key !== key && job?.key !== key) {
      const { back } = targets;
      back.resize(w, h);
      back.clear();
      back.view = viewTransform(s.view, w, h);
      const progress = { done: 0, total: grid.total };
      job = { key, content, progress, paused: false, curves: gridCurves(s, back, progress) };
    }
    if (job && !job.paused) {
      budget.frame();
      if (run(job.curves, budget.points)) {
        [targets.front, targets.back] = [targets.back, targets.front];
        Object.assign(targets.front, { key: job.key, content: job.content });
        job = null;
        budget.idle();
      }
    }
    // A redraw of the same content keeps showing the old image until done.
    const { front, back } = targets;
    const shown = !job ? front : front.key && front.content === job.content ? front : back;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, w, h);
    if (shown === back || shown.key) {
      blend(s.style.blend);
      trails.composite(shown, viewTransform(s.view, w, h));
    }
    return !!job && !job.paused;
  }

  const scrub = createScrub(gl, engine, {
    lines, evaluator, trails, targets, lineStyle, blend, family: () => grid.family,
  });

  return {
    colormaps,
    scrub,

    setFamily(family) {
      grid.family = family;
      grid.version++;
    },

    // After the engine's scene changed: rebuilds the grid's first slab and
    // returns its stats.
    setGrid(family) {
      grid.family = family;
      grid.total = engine.gridSize();
      grid.version++;
      const { data, stats } = engine.buildGrid(0, Math.min(grid.total, engine.maxCurves));
      params.set(data, stats.curves);
      grid.slab = 0;
      return stats;
    },

    // Points must be re-evaluated (t range changed) but params are still valid.
    invalidatePoints() { grid.version++; },

    // Render mode progress: null when idle, else {done, total, paused}.
    get progress() { return job && { ...job.progress, paused: job.paused }; },
    pause(paused) { if (job) job.paused = paused; },

    // Largest export side this GPU allows.
    maxExport: Math.min(MAX_EXPORT, gl.getParameter(gl.MAX_TEXTURE_SIZE), ...gl.getParameter(gl.MAX_VIEWPORT_DIMS)),

    // Draws the current mode's image at w x h px offscreen, a slice per
    // animation frame. Resolves to {read(y, n, out), dispose()}: read gets
    // RGBA rows [y, y + n), top first, straight alpha. preview: the canvas
    // size, so a Scrub fade covers the same distance. onProgress gets
    // {done, total} after each frame; an aborted signal rejects.
    async renderImage(s, w, h, { transparent, circle, preview, onProgress, signal }) {
      const progress = { done: 0, total: 0 };
      const image = trails.createTarget();
      const out = gl.createTexture(), fbo = gl.createFramebuffer();
      const release = () => {
        gl.deleteFramebuffer(fbo);
        gl.deleteTexture(out);
      };
      try {
        image.resize(w, h);
        image.clear();
        image.view = viewTransform(s.view, w, h);
        const scale = Math.min(w, h) / Math.min(...preview);
        const curves = s.mode === 'render' ? gridCurves(s, image, progress)
          : s.mode === 'scrub' ? scrub.replayCurves(s, image, { pending: 1 }, { scale, progress })
          : [].values();
        const pace = createBudget();
        for (;;) {
          await new Promise(requestAnimationFrame);
          signal?.throwIfAborted();
          pace.frame();
          const done = run(curves, pace.points);
          onProgress?.(progress);
          if (done) break;
        }
        // The circle goes in last, under the curves, so a Scrub fade doesn't dim it.
        if (circle) {
          image.bind();
          gl.enable(gl.BLEND);
          if (s.style.blend === 'add') gl.blendFunc(gl.ONE, gl.ONE);
          else gl.blendFunc(gl.ONE_MINUS_DST_ALPHA, gl.ONE);
          overlay.draw({ view: image.view, axes: false });
        }

        gl.bindTexture(gl.TEXTURE_2D, out);
        gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, w, h);
        gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, out, 0);
        gl.viewport(0, 0, w, h);
        gl.disable(gl.BLEND);
        const bg = transparent ? [0, 0, 0, 0] : [...hexColor(s.style.background), 1];
        trails.flatten(image, bg, s.style.blend === 'add');
        gl.enable(gl.BLEND);
      } catch (err) {
        release();
        throw err;
      } finally {
        image.dispose();
      }
      return {
        read(y, n, rgba) {
          gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
          gl.readPixels(0, y, w, n, gl.RGBA, gl.UNSIGNED_BYTE, rgba);
        },
        dispose: release,
      };
    },

    // Returns true while more frames are needed.
    draw(state, w, h) {
      clear(state, w, h);
      if (state.mode === 'scrub') return scrub.draw(state, w, h);
      if (state.mode === 'render') return drawRender(state, w, h);
      return false;
    },
  };
}

// Runs a curve generator until it has drawn `points`; true once finished.
function run(curves, points) {
  for (let spent = 0; spent < points;) {
    const r = curves.next();
    if (r.done) return true;
    spent += r.value;
  }
  return false;
}

// Points per frame, adapted to the frame interval (the GPU's pace shows up
// as late animation frames).
function createBudget() {
  let points = BUDGET[1], last = 0;
  return {
    get points() { return points; },
    frame() {
      const now = performance.now(), dt = last ? now - last : 0;
      last = now;
      if (dt && dt < 24) points = Math.min(points * 1.25, BUDGET[2]);
      else if (dt > 40) points = Math.max(points * 0.7, BUDGET[0]);
    },
    idle() { last = 0; },
  };
}
