// Scrub mode: one live curve following the sliders, with an optional trail.
// Every slider state is pushed to the engine's history, which fills gaps so
// neighbouring curves are at most 1 px apart. The trail accumulates in an
// offscreen target; when the view, size or style changes, the history is
// replayed into a second target over several frames while the old one is
// shown warped to the new view.
import { createParams } from './gl/params.js';
import { viewTransform } from './render.js';

const FADE_CHUNK = 0.97;  // apply accumulated fade once it reaches 3%
const REPLAY_KEYS = 16;   // keys per replay call
const BUDGET_MS = 12;     // replay time per frame

export const scrubVars = (s) => ({
  u: s.u.value, v: s.v.value,
  ...Object.fromEntries(Object.entries(s.vars).map(([k, x]) => [k, x.value])),
});

export function createScrub(gl, engine, { lines, evaluator, trails, family, lineStyle, blend }) {
  const batch = createParams(gl), current = createParams(gl);
  let front = trails.createTarget(), back = trails.createTarget();
  let fade = { pending: 1 };  // fade owed by the front target
  let replay = null;          // {key, next, fade}
  let hasCurrent = false;
  let lastStats = null;

  const T = (s) => ({ t0: s.t.lo, t1: s.t.hi, samples: s.t.n });
  const fadeK = (s) => (s.trail.mode === 'fade' ? 0.5 ** (1 / Math.max(1, s.trail.halfLife)) : 1);
  // 1 px in unit-disk units.
  const maxStep = (view, w) => 2 / (view[2] * w);
  const settingsKey = (s, w, h) => JSON.stringify([s.view, w, h, s.style, s.trail]);

  function drawCurves(target, owed, data, n, s, view) {
    if (!n) return;
    const k = fadeK(s);
    owed.pending *= k ** n;
    if (owed.pending < FADE_CHUNK) {
      target.scale(owed.pending);
      owed.pending = 1;
    }
    batch.set(data, n);
    const t = T(s), per = Math.floor(evaluator.capacity / t.samples);
    const style = lineStyle(s.style, target.w, target.h);
    for (let c = 0; c < n; c += per) {
      const m = Math.min(per, n - c);
      evaluator.evaluate(batch, family(), c, m, t);
      target.bind();
      blend(s.style.blend);
      lines.draw({ params: batch, points: evaluator.texture, first: c, count: m, samples: t.samples,
        view, viewport: [target.w, target.h], style, fade: [k, n - c] });
    }
  }

  function startReplay(s, w, h, key) {
    back.resize(w, h);
    back.clear();
    back.view = viewTransform(s.view, w, h);
    replay = { key, next: 0, fade: { pending: 1 } };
  }

  function stepReplay(s) {
    const t0 = performance.now(), t = T(s);
    const step = maxStep(back.view, back.w);
    while (replay.next < engine.scrubSize() && performance.now() - t0 < BUDGET_MS) {
      const k1 = Math.min(engine.scrubSize(), replay.next + REPLAY_KEYS);
      const r = engine.scrubReplay(replay.next, k1, t, step);
      drawCurves(back, replay.fade, r.data, r.stats.curves, s, back.view);
      replay.next = r.next;
    }
    if (replay.next >= engine.scrubSize()) {
      [front, back] = [back, front];
      front.key = replay.key;
      fade = replay.fade;
      replay = null;
    }
  }

  return {
    get stats() { return lastStats; },
    get replaying() { return !!replay; },

    // History and trail are discarded (the scene changed).
    reset() {
      engine.scrubClear();
      front.key = null;
      replay = null;
      hasCurrent = false;
    },

    // A slider moved: s holds the new values.
    push(s, w, h) {
      const view = viewTransform(s.view, w, h);
      const refresh = s.trail.mode === 'refresh';
      if (refresh) engine.scrubClear();
      const r = engine.scrubPush(scrubVars(s), T(s), refresh ? Infinity : maxStep(view, w));
      const n = r.stats.curves;
      lastStats = r.stats;
      current.set(r.data.subarray((n - 1) * engine.curveFloats), 1);
      hasCurrent = n > 0;
      if (!refresh && !replay && front.key === settingsKey(s, w, h)) {
        drawCurves(front, fade, r.data, n, s, view);
      }
    },

    // Draws into the bound default framebuffer; returns true while replaying.
    draw(s, w, h) {
      const view = viewTransform(s.view, w, h);
      const refresh = s.trail.mode === 'refresh';
      if (!refresh) {
        const key = settingsKey(s, w, h);
        if (front.key !== key && replay?.key !== key) startReplay(s, w, h, key);
        if (replay) stepReplay(s);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, w, h);
        if (front.key) {
          gl.enable(gl.BLEND);
          if (s.style.blend === 'add') gl.blendFunc(gl.ONE, gl.ONE);
          else gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
          trails.composite(front, view);
        }
      }
      if (hasCurrent && (refresh || replay || !front.key)) {
        const t = T(s);
        evaluator.evaluate(current, family(), 0, 1, t);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, w, h);
        blend(s.style.blend);
        lines.draw({ params: current, points: evaluator.texture, count: 1, samples: t.samples,
          view, viewport: [w, h], style: lineStyle(s.style, w, h) });
      }
      return !!replay;
    },
  };
}
