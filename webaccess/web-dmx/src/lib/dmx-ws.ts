// DMX-specific WebSocket helpers layered on top of the existing pipe-protocol client.
// The base ws-client now delivers JSON frames as a single-element string array,
// so we just look at parts[0] and parse if it looks like JSON.

import type { WSClient } from './ws-client';

export interface DmxWSHandler {
  onDmxState?: (universe: number, fixtureID: number, address: number, values: number[]) => void;
  onDmxDelta?: (universe: number, changes: [number, number][]) => void;
}

// Wrap the client's onMessage so JSON DMX frames are routed to the handler
// and everything else is forwarded to the previously installed callback.
export function setupDmxWS(client: WSClient, handler: DmxWSHandler): () => void {
  const previous = client.onMessage;

  const wrapped = (parts: string[]) => {
    const first = parts[0];
    if (typeof first === 'string' && first.startsWith('{')) {
      try {
        const msg = JSON.parse(first);
        if (msg && typeof msg === 'object') {
          if (msg.cmd === 'DMX_STATE') {
            handler.onDmxState?.(msg.universe, msg.fixtureID, msg.address, msg.values);
            return;
          }
          if (msg.cmd === 'DMX_DELTA') {
            handler.onDmxDelta?.(msg.universe, msg.changes);
            return;
          }
        }
      } catch {
        // Fall through to legacy handler if parsing fails.
      }
    }
    previous?.(parts);
  };

  client.onMessage = wrapped;

  // Returned cleanup restores the previous handler (so multiple views can
  // attach and detach without permanently shadowing each other).
  return () => {
    if (client.onMessage === wrapped) {
      client.onMessage = previous;
    }
  };
}

export function sendDmxSubscribe(client: WSClient, fixtureIDs: number[]): void {
  client.send(JSON.stringify({ cmd: 'DMX_SUB', fixtureIDs }));
}

export function sendDmxUnsubscribe(client: WSClient, fixtureIDs: number[]): void {
  client.send(JSON.stringify({ cmd: 'DMX_UNSUB', fixtureIDs }));
}

export function sendDmxHeartbeat(client: WSClient): void {
  client.send(JSON.stringify({ cmd: 'DMX_HEARTBEAT' }));
}
