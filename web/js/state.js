// The scene: everything needed to reproduce an image. Changes go through
// store.set(path, value), and listeners get the changed paths.

export const SLIDER_VARS = ['a', 'b', 'c', 'd', 'R'];

export function defaultState() {
  return {
    mode: 'render',          // scrub | animate | render
    family: 'normal',
    exprs: ['1', '0.02 + 0.1*u'],
    policy: 'skip',          // skip | clamp
    u: { lo: 0, hi: 1, n: 12, value: 0 },
    v: { lo: 0, hi: 1, n: 12, value: 0 },
    t: { lo: 0, hi: 60, n: 4000 },
    vars: {
      a: { value: 0, lo: -1, hi: 1 },
      b: { value: 0, lo: -1, hi: 1 },
      c: { value: 0, lo: -1, hi: 1 },
      d: { value: 0, lo: -1, hi: 1 },
      R: { value: 0, lo: 0, hi: 1 },
    },
    trail: { mode: 'keep', halfLife: 300 },  // refresh | keep | fade; halfLife in px of travel
    style: {
      width: 1.5,            // px per 1000 px of image
      opacity: 1,
      colorBy: 'u',          // u | v | t | abs | arg
      colormap: 'magma',
      reverse: false,
      custom: ['#1b1f3a', '#e85d75', '#ffd166'],
      blend: 'over',         // over | add
      background: '#0d0e10',
      overlay: true,
    },
    view: { cx: 0, cy: 0, zoom: 1 },
  };
}

const get = (obj, keys) => keys.reduce((o, k) => o?.[k], obj);

export function createStore(state) {
  const listeners = new Set();
  let pending = null;

  const flush = () => {
    const paths = pending;
    pending = null;
    for (const fn of listeners) fn(paths);
  };

  return {
    get state() { return state; },
    subscribe(fn) { listeners.add(fn); },

    // set('style.width', 2); listeners run once per microtask with all paths.
    set(path, value) {
      const keys = path.split('.');
      const parent = get(state, keys.slice(0, -1));
      if (parent == null || parent[keys.at(-1)] === value) return;
      parent[keys.at(-1)] = value;
      if (!pending) {
        pending = new Set();
        queueMicrotask(flush);
      }
      pending.add(path);
    },

    replace(next) {
      state = next;
      if (!pending) {
        pending = new Set();
        queueMicrotask(flush);
      }
      pending.add('*');
    },
  };
}
