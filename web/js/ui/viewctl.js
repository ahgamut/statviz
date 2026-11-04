// Pan and zoom on the canvas: wheel zooms about the cursor, drag pans,
// double-click resets.
import { viewTransform } from '../render.js';

const MIN_ZOOM = 0.1, MAX_ZOOM = 1e5;

export function attachViewControls(canvas, store) {
  // Cursor position in clip space and the current transform.
  const locate = (e) => {
    const r = canvas.getBoundingClientRect();
    const view = store.state.view;
    const [, , kx, ky] = viewTransform(view, r.width, r.height);
    return { view, kx, ky, x: (2 * (e.clientX - r.left)) / r.width - 1, y: 1 - (2 * (e.clientY - r.top)) / r.height };
  };

  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const { view, kx, ky, x, y } = locate(e);
    const dy = e.deltaMode === 1 ? e.deltaY * 33 : e.deltaMode === 2 ? e.deltaY * 400 : e.deltaY;
    const zoom = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, view.zoom * Math.exp(-dy * 0.0015)));
    const f = view.zoom / zoom;  // k scales with zoom
    store.set('view', {
      cx: view.cx + (x / kx) * (1 - f),
      cy: view.cy + (y / ky) * (1 - f),
      zoom,
    });
  }, { passive: false });

  let drag = null;
  canvas.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    drag = { id: e.pointerId, ...locate(e) };
    canvas.setPointerCapture(e.pointerId);
    canvas.classList.add('dragging');
  });
  canvas.addEventListener('pointermove', (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    const { x, y } = locate(e);
    store.set('view', {
      cx: drag.view.cx - (x - drag.x) / drag.kx,
      cy: drag.view.cy - (y - drag.y) / drag.ky,
      zoom: drag.view.zoom,
    });
  });
  const end = (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    drag = null;
    canvas.classList.remove('dragging');
  };
  canvas.addEventListener('pointerup', end);
  canvas.addEventListener('pointercancel', end);
  canvas.addEventListener('dblclick', () => store.set('view', { cx: 0, cy: 0, zoom: 1 }));
}
