// Combine multiple PNG screenshots into an animated GIF.
// Uses gifenc (pure JS, no native deps) + pngjs for decoding.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';
import { PNG } from 'pngjs';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { GIFEncoder, quantize, applyPalette } = require('gifenc');

export interface GifOptions {
  /** Milliseconds per frame (default 800). */
  delay?: number;
  /** Max colors in palette (default 128 — smaller file). */
  maxColors?: number;
  /** Scale factor 0–1 to shrink frames (default 0.5). */
  scale?: number;
}

/** Downscale RGBA pixel data by integer factor. */
function downscale(
  rgba: Uint8Array, srcW: number, srcH: number, factor: number,
): { data: Uint8Array; width: number; height: number } {
  if (factor >= 1) return { data: rgba, width: srcW, height: srcH };
  const dstW = Math.round(srcW * factor);
  const dstH = Math.round(srcH * factor);
  const out = new Uint8Array(dstW * dstH * 4);
  for (let y = 0; y < dstH; y++) {
    const sy = Math.min(Math.round(y / factor), srcH - 1);
    for (let x = 0; x < dstW; x++) {
      const sx = Math.min(Math.round(x / factor), srcW - 1);
      const si = (sy * srcW + sx) * 4;
      const di = (y * dstW + x) * 4;
      out[di] = rgba[si];
      out[di + 1] = rgba[si + 1];
      out[di + 2] = rgba[si + 2];
      out[di + 3] = rgba[si + 3];
    }
  }
  return { data: out, width: dstW, height: dstH };
}

/**
 * Combine PNG files into a single animated GIF.
 * @param pngPaths Array of PNG file paths (in frame order).
 * @param outPath  Output GIF file path.
 * @param opts     Animation options.
 */
export function pngsToGif(pngPaths: string[], outPath: string, opts: GifOptions = {}): void {
  const delay = opts.delay ?? 800;
  const maxColors = opts.maxColors ?? 128;
  const scale = opts.scale ?? 0.5;

  const gif = GIFEncoder();
  let firstWidth = 0;
  let firstHeight = 0;

  for (let i = 0; i < pngPaths.length; i++) {
    const png = PNG.sync.read(readFileSync(pngPaths[i]));
    const raw = new Uint8Array(png.data.buffer, png.data.byteOffset, png.data.byteLength);
    const { data, width, height } = downscale(raw, png.width, png.height, scale);

    if (i === 0) { firstWidth = width; firstHeight = height; }

    const palette = quantize(data, maxColors, { format: 'rgba4444' });
    const index = applyPalette(data, palette, 'rgba4444');
    gif.writeFrame(index, width, height, { palette, delay });
  }

  gif.finish();
  mkdirSync(dirname(outPath), { recursive: true });
  writeFileSync(outPath, gif.bytes());
}
