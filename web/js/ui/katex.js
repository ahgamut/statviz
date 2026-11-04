// KaTeX, loaded on first use from cdnjs.
const BASE = 'https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.18.11/';
const JS_SRI = 'sha512-hqULNlSWGNtoWUsYyRSpdOdww05s2hqNof5e1P4WxIVfLpL8ThFH/QqNNtax5fElgRkNZGkFyeKAmVvdZ/kpSA==';
const CSS_SRI = 'sha512-P0yrJ9fZYAz71mx0p68JgoPBpvjXPW2H0m+AXYSzzL9kT6AyDQ9eDQNLfqdjSYgphq4Hi6llKmesa6mdQgZOHw==';

let loading = null;

function load() {
  loading ??= new Promise((resolve, reject) => {
    const css = Object.assign(document.createElement('link'), {
      rel: 'stylesheet', href: BASE + 'katex.min.css', integrity: CSS_SRI, crossOrigin: 'anonymous',
    });
    const js = Object.assign(document.createElement('script'), {
      src: BASE + 'katex.min.js', integrity: JS_SRI, crossOrigin: 'anonymous',
      onload: () => resolve(window.katex), onerror: reject,
    });
    document.head.append(css, js);
  });
  return loading;
}

// Renders TeX into el; shows the source if KaTeX can't load.
export function renderTex(el, tex) {
  el.textContent = tex;
  el.dataset.tex = tex;
  load().then(
    (katex) => { if (el.dataset.tex === tex) katex.render(tex, el, { throwOnError: false }); },
    () => {},
  );
}
