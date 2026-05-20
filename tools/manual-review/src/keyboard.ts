/** Keyboard shortcut manager. */

export interface ShortcutMap {
  [key: string]: () => void;
}

export function initKeyboard(shortcuts: ShortcutMap): () => void {
  function handler(e: KeyboardEvent) {
    // Don't intercept when typing in inputs
    const tag = (e.target as HTMLElement)?.tagName;
    if (tag === 'INPUT' || tag === 'TEXTAREA') return;

    const key = buildKey(e);
    const action = shortcuts[key];
    if (action) {
      // On macOS, Meta+key is the system modifier — only intercept if
      // our shortcut map explicitly uses Ctrl (which we map Meta to).
      // But never block Meta+R (reload) or Meta+W (close tab) etc.
      if (e.metaKey && !e.ctrlKey) {
        const systemKeys = ['r', 'w', 't', 'l', 'n', 'q', 'a', 'c', 'v', 'x', 'z', 'f'];
        if (systemKeys.includes(e.key.toLowerCase())) return;
      }
      e.preventDefault();
      action();
    }
  }

  document.addEventListener('keydown', handler);
  return () => document.removeEventListener('keydown', handler);
}

function buildKey(e: KeyboardEvent): string {
  const parts: string[] = [];
  if (e.ctrlKey || e.metaKey) parts.push('Ctrl');
  if (e.shiftKey) parts.push('Shift');
  if (e.altKey) parts.push('Alt');

  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  parts.push(key);
  return parts.join('+');
}
