// Utility functions — pure calculations, no side effects

export function lightenColor(hex: string, factor: number): string {
  if (!hex?.startsWith('#') || hex.length !== 7) return hex;
  const r = Math.min(255, Math.round(parseInt(hex.slice(1, 3), 16) * factor));
  const g = Math.min(255, Math.round(parseInt(hex.slice(3, 5), 16) * factor));
  const b = Math.min(255, Math.round(parseInt(hex.slice(5, 7), 16) * factor));
  return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
}

// Perceived luminance per WCAG: 0 (black) → 1 (white). Returns null for invalid input.
export function relativeLuminance(hex?: string): number | null {
  if (!hex?.startsWith('#') || hex.length !== 7) return null;
  const channel = (n: number) => {
    const c = n / 255;
    return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  };
  const r = channel(parseInt(hex.slice(1, 3), 16));
  const g = channel(parseInt(hex.slice(3, 5), 16));
  const b = channel(parseInt(hex.slice(5, 7), 16));
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

// Pick black or white text for best contrast against the given background.
export function contrastTextColor(bgHex?: string, fallback = '#e7e9ef'): string {
  const lum = relativeLuminance(bgHex);
  if (lum == null) return fallback;
  return lum > 0.55 ? '#0b0d14' : '#f4f6fb';
}

export function sliderDisplayValue(value: number, displayMode?: string, max = 255): string {
  if (displayMode === 'Percentage') {
    return `${Math.round((value / max) * 100)}%`;
  }
  return `${value}`;
}

export function formatTime(totalMs: number): string {
  if (totalMs <= 0) return '00:00:00';
  const totalSec = Math.floor(totalMs / 1000);
  const h = String(Math.floor(totalSec / 3600)).padStart(2, '0');
  const m = String(Math.floor((totalSec % 3600) / 60)).padStart(2, '0');
  const s = String(totalSec % 60).padStart(2, '0');
  return `${h}:${m}:${s}`;
}

export function formatTimeTenths(ms: number): string {
  const totalMs = Math.max(0, ms);
  const totalTenths = Math.floor(totalMs / 100);
  const tenths = totalTenths % 10;
  const totalSeconds = Math.floor(totalTenths / 10);
  const h = String(Math.floor(totalSeconds / 3600)).padStart(2, '0');
  const m = String(Math.floor((totalSeconds % 3600) / 60)).padStart(2, '0');
  const sec = String(totalSeconds % 60).padStart(2, '0');
  return `${h}:${m}:${sec}.${tenths}`;
}

export function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

/** Haptic pulse for touch interactions — no-op when Vibration API is absent. */
export function haptic(ms = 10): void {
  try { navigator?.vibrate?.(ms); } catch { /* unsupported */ }
}

export function cssFont(font?: { family?: string; size?: number; bold?: boolean; italic?: boolean }): React.CSSProperties {
  if (!font) return {};
  const style: React.CSSProperties = {};
  if (font.family) style.fontFamily = font.family;
  if (font.size) style.fontSize = `${font.size}px`;
  if (font.bold) style.fontWeight = 'bold';
  if (font.italic) style.fontStyle = 'italic';
  return style;
}
