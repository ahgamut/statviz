// PNG export of the current mode's image, with the scene in a tEXt chunk
// so a file dropped back on the page restores it.
import { parseScene, sceneJson } from './state.js';

const BAND = 128;  // rows per readback
const SCENE_KEY = 'statviz';
const SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

// s: a snapshot of the state. onProgress(phase, fraction) with phase
// 'drawing' or 'encoding'. Resolves to a Blob; an aborted signal rejects.
export async function exportPng({ renderer, engine }, s, { preview, signal, onProgress }) {
  const { w, h, transparent, circle } = s.export;
  const image = await renderer.renderImage(s, w, h, {
    transparent, circle, preview, signal,
    onProgress: (p) => onProgress?.('drawing', p.total ? p.done / p.total : 0),
  });
  try {
    const parts = engine.pngBegin(w, h, transparent, [[SCENE_KEY, sceneJson(s)]]);
    const rows = new Uint8Array(w * BAND * 4);
    for (let y = 0; y < h; y += BAND) {
      signal?.throwIfAborted();
      onProgress?.('encoding', y / h);
      const n = Math.min(BAND, h - y);
      image.read(y, n, rows);
      parts.push(engine.pngRows(rows, n));
      await new Promise((r) => setTimeout(r));
    }
    parts.push(engine.pngEnd());
    return new Blob(parts, { type: 'image/png' });
  } finally {
    image.dispose();
  }
}

export function exportName(s, date = new Date()) {
  const p = (x) => String(x).padStart(2, '0');
  const stamp = `${date.getFullYear()}${p(date.getMonth() + 1)}${p(date.getDate())}`
    + `-${p(date.getHours())}${p(date.getMinutes())}${p(date.getSeconds())}`;
  return `statviz-${s.family}-${stamp}.png`;
}

export function download(blob, name) {
  const url = URL.createObjectURL(blob);
  const a = Object.assign(document.createElement('a'), { href: url, download: name });
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

// The scene in a PNG exported here, or null.
export async function sceneFromPng(file) {
  const b = new Uint8Array(await file.arrayBuffer());
  if (b.length < 8 || SIGNATURE.some((x, i) => b[i] !== x)) return null;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const latin1 = new TextDecoder('latin1');
  for (let p = 8; p + 12 <= b.length;) {
    const len = dv.getUint32(p), type = latin1.decode(b.subarray(p + 4, p + 8));
    const data = b.subarray(p + 8, Math.min(b.length, p + 8 + len));
    if (type === 'tEXt') {
      const nul = data.indexOf(0);
      if (nul > 0 && latin1.decode(data.subarray(0, nul)) === SCENE_KEY) {
        return parseScene(latin1.decode(data.subarray(nul + 1)));
      }
    }
    if (type === 'IDAT' || type === 'IEND') break;  // ours come before the image data
    p += 12 + len;
  }
  return null;
}
