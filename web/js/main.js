import { loadEngine } from './engine.js';
import { createCurves } from './gl/curves.js';
import { createOverlay } from './gl/overlay.js';

const SCENE = { mu: 1, sigma0: 0.02, sigma1: 0.12, curves: 12, t0: 0, t1: 60, samples: 4000 };
const SCALE = 0.9;  // unit disk radius in clip space

function showError(err) {
  const el = document.getElementById('error');
  el.textContent = String(err?.message ?? err);
  el.hidden = false;
  console.error(err);
}

async function main() {
  const canvas = document.getElementById('view');
  const gl = canvas.getContext('webgl2', { antialias: true });
  if (!gl) throw new Error('WebGL2 is not available in this browser.');

  const [engine, curves, overlay] = await Promise.all([
    loadEngine(),
    createCurves(gl),
    createOverlay(gl),
  ]);

  curves.setParams(engine.normalSweep(SCENE.mu, SCENE.sigma0, SCENE.sigma1, SCENE.curves), SCENE.curves);

  const draw = () => {
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    overlay.draw({ scale: SCALE });
    curves.draw({ t0: SCENE.t0, t1: SCENE.t1, samples: SCENE.samples, scale: SCALE });
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
