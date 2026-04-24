// WebSocket client that speaks QLC+'s existing pipe-delimited protocol.
// Pure actions layer — no UI, no framework dependency.

type MessageHandler = (parts: string[]) => void;

export interface WSClient {
  connect(): void;
  disconnect(): void;
  send(msg: string): void;
  sendWidgetValue(id: number, value: number | string): void;
  sendWidgetCommand(id: number, cmd: string, ...args: (string | number)[]): void;
  sendPageSwitch(pageIndex: number): void;
  onMessage: MessageHandler | null;
  onStatusChange: ((connected: boolean) => void) | null;
}

export function createWSClient(getUrl?: () => string): WSClient {
  let socket: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let pollTimer: ReturnType<typeof setInterval> | null = null;
  let intentionalClose = false;
  let reconnectDelay = 1000; // exponential backoff

  const client: WSClient = {
    onMessage: null,
    onStatusChange: null,

    connect() {
      intentionalClose = false;
      const url = getUrl?.() ?? `ws://${window.location.host}/qlcplusWS`;
      try {
        socket = new WebSocket(url);
      } catch {
        // Browser may throw on invalid URL; schedule retry
        scheduleReconnect();
        return;
      }

      socket.onopen = () => {
        reconnectDelay = 1000; // reset backoff on success
        client.onStatusChange?.(true);
        // Start polling for project loaded
        pollTimer = setInterval(() => {
          client.send('QLC+API|isProjectLoaded');
        }, 1500);
      };

      socket.onclose = () => {
        client.onStatusChange?.(false);
        if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
        if (!intentionalClose) {
          scheduleReconnect();
        }
      };

      socket.onerror = () => {
        // Don't immediately close — onclose will fire after onerror
      };

      socket.onmessage = (ev: MessageEvent) => {
        const parts = String(ev.data).split('|');
        client.onMessage?.(parts);
      };
    },

    disconnect() {
      intentionalClose = true;
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
      if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
      socket?.close();
      socket = null;
    },

    send(msg: string) {
      if (socket?.readyState === WebSocket.OPEN) {
        socket.send(msg);
      }
    },

    sendWidgetValue(id: number, value: number | string) {
      client.send(`${id}|${value}`);
    },

    sendWidgetCommand(id: number, cmd: string, ...args: (string | number)[]) {
      client.send([id, cmd, ...args].join('|'));
    },

    sendPageSwitch(pageIndex: number) {
      client.send(`VC_PAGE|${pageIndex}`);
    },
  };

  function scheduleReconnect() {
    if (intentionalClose) return;
    if (reconnectTimer) clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(() => {
      client.connect();
    }, reconnectDelay);
    // Exponential backoff: 1s, 2s, 4s, max 10s
    reconnectDelay = Math.min(reconnectDelay * 2, 10000);
  }

  return client;
}
