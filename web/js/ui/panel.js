// Sidebar: built once from the engine metadata; update(state, info) refreshes it.
import { COLORMAP_NAMES } from '../gl/colormap.js';
import { SLIDER_VARS } from '../state.js';
import { checkbox, colorInput, exprInput, numberInput, segmented, select, slider } from './controls.js';
import { h } from './dom.js';
import { renderTex } from './katex.js';

const section = (title, ...children) => h('details.section', { open: true }, h('summary', {}, title), ...children);
const row = (label, ...children) => h('div.row', {}, h('span.label', {}, label), ...children);

export function createPanel(root, { meta, store, actions }) {
  const updates = [];
  const bind = (control, fn) => { updates.push((s, info) => fn(control, s, info)); return control.el; };

  // Mode
  const mode = segmented(store, 'mode', [
    ['scrub', 'Scrub', 'One curve following the sliders'],
    ['animate', 'Animate', 'Not available yet'],
    ['render', 'Render', 'Every curve of the u, v grid'],
  ], { disabled: ['animate'] });

  // Distribution
  const familySelect = h('select', { on: { change: () => actions.setFamily(familySelect.value) } },
    [['Continuous', false], ['Discrete', true]].map(([label, lattice]) =>
      h('optgroup', { label }, meta.families.filter((f) => f.lattice === lattice)
        .map((f) => h('option', { value: f.key }, f.name)))));
  const formula = h('div.formula');

  // Parameters (rebuilt per family)
  const paramBox = h('div.params');
  let paramRows = [], paramFamily = null;
  const buildParams = (family) => {
    paramRows = family.params.map((p, i) => {
      const input = exprInput((src) => store.set(`exprs.${i}`, src));
      const domain = h('div.domain');
      paramBox.append(h('div.param', {}, h('label.sym', { title: p.name }, p.symbol, ' ='), input.el, domain));
      return { p, input, domain };
    });
  };
  const policy = segmented(store, 'policy', [
    ['skip', 'Skip', 'Out-of-domain curves are not drawn'],
    ['clamp', 'Clamp', 'Out-of-domain values are clamped to the domain'],
  ]);

  // Variables: a main line, plus a compact range line for sliders.
  const sub = (...children) => h('div.row.sub', {}, h('span.label', {}, 'range'), ...children);
  const sweepRow = (axis) => {
    const lo = numberInput(store, `${axis}.lo`), hi = numberInput(store, `${axis}.hi`);
    const n = numberInput(store, `${axis}.n`, { integer: true, min: 1, max: 4096, title: 'curves' });
    const value = slider(store, `${axis}.value`);
    const grid = h('span.inline', {}, h('span.times', {}, '×'), n.el);
    const range = h('span.inline', {}, lo.el, h('span.sep', {}, '–'), hi.el);
    const main = row(axis, value.el, grid);
    const rangeRow = sub();
    const el = h('div.var', {}, main, rangeRow);
    updates.push((s, info) => {
      const scrub = s.mode === 'scrub';
      el.hidden = !info.varsUsed.has(axis);
      value.el.hidden = !scrub;
      grid.hidden = scrub;
      (scrub ? rangeRow : main).insertBefore(range, scrub ? null : grid);
      rangeRow.hidden = !scrub;
      lo.update(s[axis].lo); hi.update(s[axis].hi); n.update(s[axis].n);
      value.update(s[axis].value, s[axis].lo, s[axis].hi);
    });
    return el;
  };
  const varRow = (name) => {
    const value = slider(store, `vars.${name}.value`);
    const lo = numberInput(store, `vars.${name}.lo`), hi = numberInput(store, `vars.${name}.hi`);
    const el = h('div.var', {}, row(name, value.el), sub(lo.el, h('span.sep', {}, '–'), hi.el));
    updates.push((s, info) => {
      const v = s.vars[name];
      el.hidden = !info.varsUsed.has(name);
      value.update(v.value, v.lo, v.hi); lo.update(v.lo); hi.update(v.hi);
    });
    return el;
  };
  const t0 = numberInput(store, 't.lo'), t1 = numberInput(store, 't.hi');
  const tn = numberInput(store, 't.n', { integer: true, min: 2, max: 100000, title: 'samples per curve', wide: true });
  const twoPi = h('button.small', { type: 'button', title: 'One period of an integer-valued distribution',
    on: { click: () => { store.set('t.lo', 0); store.set('t.hi', +(2 * Math.PI).toFixed(6)); } } }, '0–2π');
  const noVars = h('p.hint', {}, 'The expressions use no variables: write u, v, a–d or R in them.');

  // Trail (scrub)
  const trailMode = segmented(store, 'trail.mode', [
    ['refresh', 'Refresh', 'Only the current curve'],
    ['keep', 'Keep', 'Keep every curve as a swept band'],
    ['fade', 'Fade', 'Older curves fade with distance dragged'],
  ]);
  const halfLife = numberInput(store, 'trail.halfLife', { min: 1, max: 100000, title: 'px of travel' });
  const halfLifeRow = row('half-life', halfLife.el, h('span.unit', {}, 'px'));
  const trailSection = section('Trail',
    row('', trailMode.el), halfLifeRow,
    row('', h('button.small', { type: 'button', on: { click: () => actions.clearTrail() } }, 'Clear (C)')));

  // Style
  const colorBy = select(store, 'style.colorBy', [['u', 'u'], ['v', 'v'], ['t', 't'], ['abs', '|φ|'], ['arg', 'arg φ']]);
  const cmap = select(store, 'style.colormap', COLORMAP_NAMES.map((n) => [n, n]));
  const reverse = checkbox(store, 'style.reverse', 'reverse');
  const stops = [0, 1, 2].map((i) => colorInput(store, `style.custom.${i}`));
  const stopsRow = row('stops', stops.map((c) => c.el));
  const width = numberInput(store, 'style.width', { min: 0.05, max: 100, step: 0.1, title: 'px per 1000 px of image' });
  const opacity = numberInput(store, 'style.opacity', { min: 0, max: 1, step: 0.05 });
  const blend = segmented(store, 'style.blend', [['over', 'Over'], ['add', 'Add', 'Additive: overlaps get brighter']]);
  const background = colorInput(store, 'style.background');
  const overlay = checkbox(store, 'style.overlay', 'unit circle and axes');

  const status = h('div.status');

  root.append(
    h('header', {}, h('h1', {}, 'statviz'), mode.el),
    section('Distribution', familySelect, formula),
    section('Parameters', paramBox, row('domain', policy.el)),
    section('Variables', sweepRow('u'), sweepRow('v'), SLIDER_VARS.map(varRow), noVars,
      row('t', t0.el, h('span.sep', {}, '–'), t1.el, twoPi), row('samples', tn.el)),
    trailSection,
    section('Style',
      row('colour by', colorBy.el), row('colormap', cmap.el, reverse.el), stopsRow,
      row('width', width.el, h('span.unit', {}, '‰')), row('opacity', opacity.el), row('blend', blend.el),
      row('background', background.el), row('', overlay.el)),
    status,
  );

  return {
    update(s, info) {
      mode.update(s.mode);
      const family = meta.families.find((f) => f.key === s.family);
      if (family !== paramFamily) {
        paramFamily = family;
        familySelect.value = family.key;
        renderTex(formula, family.latex);
        paramBox.replaceChildren();
        buildParams(family);
        twoPi.hidden = !family.lattice;
      }
      paramRows.forEach(({ p, input, domain }, i) => {
        input.update(s.exprs[i] ?? '', info.errors[i]);
        const err = info.errors[i];
        const bad = info.stats?.bad[i] ?? 0;
        domain.className = 'domain' + (err || bad ? ' warn' : '');
        domain.textContent = err ? err.message
          : `${p.domainText}` + (bad ? ` · ${bad} curve${bad > 1 ? 's' : ''} outside` : '');
      });
      policy.update(s.policy);
      for (const fn of updates) fn(s, info);
      noVars.hidden = info.varsUsed.size > 0;
      t0.update(s.t.lo); t1.update(s.t.hi); tn.update(s.t.n);
      trailSection.hidden = s.mode !== 'scrub';
      trailMode.update(s.trail.mode);
      halfLifeRow.hidden = s.trail.mode !== 'fade';
      halfLife.update(s.trail.halfLife);
      colorBy.update(s.style.colorBy); cmap.update(s.style.colormap); reverse.update(s.style.reverse);
      stopsRow.hidden = s.style.colormap !== 'custom';
      stops.forEach((c, i) => c.update(s.style.custom[i]));
      width.update(s.style.width); opacity.update(s.style.opacity); blend.update(s.style.blend);
      background.update(s.style.background); overlay.update(s.style.overlay);
      status.textContent = info.status ?? '';
    },
  };
}
