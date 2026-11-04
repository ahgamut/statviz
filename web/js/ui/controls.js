// Small form controls bound to store paths.
import { h } from './dom.js';

const focused = (el) => document.activeElement === el;

export function numberInput(store, path, { step = 'any', min, max, integer = false, title, wide = false } = {}) {
  const el = h(wide ? 'input.num.wide' : 'input.num', { type: 'number', step, min, max, title });
  el.addEventListener('input', () => {
    let x = Number(el.value);
    if (el.value === '' || !Number.isFinite(x)) return;
    if (integer) x = Math.round(x);
    if (min != null) x = Math.max(min, x);
    if (max != null) x = Math.min(max, x);
    store.set(path, x);
  });
  return {
    el,
    update(value) { if (!focused(el)) el.value = String(value); },
  };
}

// Range slider over [lo, hi] plus an editable number.
export function slider(store, path, { steps = 1000 } = {}) {
  const range = h('input.range', { type: 'range', min: 0, max: steps, step: 1 });
  const num = h('input.num', { type: 'number', step: 'any' });
  let lo = 0, hi = 1;
  range.addEventListener('input', () => store.set(path, lo + ((hi - lo) * Number(range.value)) / steps));
  num.addEventListener('input', () => {
    const x = Number(num.value);
    if (num.value !== '' && Number.isFinite(x)) store.set(path, x);
  });
  return {
    el: h('span.slider', {}, range, num),
    update(value, l, r) {
      lo = l; hi = r;
      range.value = String(hi > lo ? Math.round(((value - lo) / (hi - lo)) * steps) : 0);
      if (!focused(num)) num.value = String(+value.toPrecision(6));
    },
  };
}

// Mutually exclusive buttons.
export function segmented(store, path, options, { disabled = [] } = {}) {
  const buttons = options.map(([value, label, title]) =>
    h('button.seg', { type: 'button', title, disabled: disabled.includes(value), on: { click: () => store.set(path, value) } }, label));
  return {
    el: h('span.segmented', {}, buttons),
    update(value) {
      buttons.forEach((b, i) => b.classList.toggle('on', options[i][0] === value));
    },
  };
}

export function select(store, path, options, { parse = (x) => x } = {}) {
  const el = h('select', { on: { change: () => store.set(path, parse(el.value)) } },
    options.map(([value, label]) => h('option', { value }, label)));
  return { el, update(value) { el.value = String(value); } };
}

export function checkbox(store, path, label) {
  const input = h('input', { type: 'checkbox', on: { change: () => store.set(path, input.checked) } });
  return { el: h('label.check', {}, input, label), update(value) { input.checked = !!value; } };
}

export function colorInput(store, path) {
  const el = h('input.color', { type: 'color', on: { input: () => store.set(path, el.value) } });
  return { el, update(value) { el.value = value; } };
}

// Monospace expression input with the error range underlined in place.
export function exprInput(onInput) {
  const mirror = h('div.expr-mirror', { 'aria-hidden': 'true' });
  const input = h('input.expr', { type: 'text', spellcheck: false, autocomplete: 'off' });
  const sync = () => { mirror.scrollLeft = input.scrollLeft; };
  input.addEventListener('input', () => { onInput(input.value); sync(); });
  input.addEventListener('scroll', sync);
  input.addEventListener('keyup', sync);
  return {
    el: h('div.expr-wrap', {}, mirror, input),
    update(value, err) {
      if (!focused(input)) input.value = value;
      const src = input.value;
      mirror.replaceChildren();
      if (err) {
        const a = Math.min(err.pos, src.length), b = Math.min(src.length, a + Math.max(err.len, 1));
        mirror.append(src.slice(0, a), h('mark', {}, src.slice(a, b) || ' '), src.slice(b));
      }
      input.classList.toggle('bad', !!err);
      sync();
    },
  };
}
