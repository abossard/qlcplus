import { create } from 'zustand';
import type { VCData, PageData, WidgetData } from '../lib/types';
import { createWSClient, type WSClient } from '../lib/ws-client';

// Flat widget map for O(1) lookup by ID
type WidgetMap = Map<number, WidgetData>;

interface VCStore {
  // Connection state
  connected: boolean;
  projectLoaded: boolean;

  // VC data
  pages: PageData[];
  selectedPage: number;
  pixelDensity: number;
  uiColors: Record<string, string>;
  widgets: WidgetMap;
  grandMasterValue: number;

  // Actions
  wsClient: WSClient | null;
  init: () => void;
  cleanup: () => void;
  setSelectedPage: (index: number) => void;
  sendWidgetValue: (id: number, value: number | string) => void;
  sendWidgetCommand: (id: number, cmd: string, ...args: (string | number)[]) => void;
}

// Build flat widget map from nested page tree
function buildWidgetMap(pages: PageData[]): WidgetMap {
  const map: WidgetMap = new Map();
  function walk(widgets: WidgetData[]) {
    for (const w of widgets) {
      map.set(w.id, w);
      if (w.children) walk(w.children);
    }
  }
  for (const page of pages) {
    walk(page.children ?? []);
  }
  return map;
}

// Deep-clone widget data to enable immutable updates
function cloneWidget(w: WidgetData): WidgetData {
  return JSON.parse(JSON.stringify(w));
}

function applyVCData(data: VCData, set: (partial: Partial<VCStore>) => void) {
  const pages = data.pages ?? [];
  const rawSelected = data.selectedPage ?? 0;
  const selectedPage = (rawSelected >= 0 && rawSelected < pages.length) ? rawSelected : 0;
  set({
    pages,
    selectedPage,
    pixelDensity: data.pixelDensity ?? 1,
    uiColors: data.uiStyle?.colors ?? {},
    widgets: buildWidgetMap(pages),
    projectLoaded: true,
  });
}

export const useVCStore = create<VCStore>((set, get) => {
  let wsClient: WSClient | null = null;

  function handleMessage(parts: string[]) {
    const state = get();
    const tag = parts[0];

    // Project loaded response — fetch vc.json on first confirmation
    if (tag === 'QLC+API') {
      if (parts[1] === 'isProjectLoaded' && parts[2] === 'true') {
        if (!state.projectLoaded) {
          fetchVC();
        }
      }
      return;
    }

    // Grand master
    if (tag === 'GM_VALUE') {
      set({ grandMasterValue: parseInt(parts[1], 10) || 0 });
      return;
    }

    // Page switch from server
    if (tag === 'VC_PAGE') {
      set({ selectedPage: parseInt(parts[1], 10) || 0 });
      return;
    }

    // Function state / Alert (ignore for now)
    if (tag === 'FUNCTION' || tag === 'ALERT') return;

    // Widget-targeted messages: <id>|<CMD>|<args...>
    const id = parseInt(tag, 10);
    if (isNaN(id)) return;
    const cmd = parts[1];

    set((prev) => {
      const widget = prev.widgets.get(id);
      if (!widget) return prev;

      const updated = cloneWidget(widget);
      let changed = false;

      switch (cmd) {
        case 'BUTTON':
          updated.state = parts[2];
          changed = true;
          break;

        case 'SLIDER':
          updated.value = parseInt(parts[2], 10);
          changed = true;
          break;

        case 'SLIDER_OVERRIDE':
          updated.isOverriding = parts[2] === '1' || parts[2] === 'true';
          changed = true;
          break;

        case 'CNG_COLORS':
          updated.cngPrimaryColor = parts[2] || '';
          updated.cngSecondaryColor = parts[3] || '';
          changed = true;
          break;

        case 'BUTTON_DISABLE':
        case 'SLIDER_DISABLE':
        case 'LABEL_DISABLE':
        case 'FRAME_DISABLE':
        case 'CUE_DISABLE':
        case 'WIDGET_DISABLE':
          updated.disabled = parts[2] === '1' || parts[2] === 'true';
          changed = true;
          break;

        case 'WIDGET_VISIBLE':
          updated.visible = parts[2] === '1' || parts[2] === 'true';
          changed = true;
          break;

        case 'CUE':
          updated.currentStep = parseInt(parts[2], 10);
          changed = true;
          break;

        case 'CUE_STATE':
          updated.playbackStatus = parseInt(parts[2], 10);
          updated.currentStep = parseInt(parts[3], 10);
          changed = true;
          break;

        case 'CUE_SIDE':
          updated.sideFaderLevel = parseInt(parts[2], 10);
          updated.primaryTop = parts[3] === '1' || parts[3] === 'true';
          changed = true;
          break;

        case 'FRAME': {
          const pageIndex = parseInt(parts[2], 10);
          if (!isNaN(pageIndex)) {
            updated.currentPage = pageIndex;
            changed = true;
          }
          break;
        }

        case 'XYPAD': {
          updated.xPos = parseFloat(parts[2]);
          updated.yPos = parseFloat(parts[3]);
          changed = true;
          break;
        }

        case 'XYPAD_PRESET':
          changed = true;
          break;

        case 'SPEED_STATE':
          updated.speedValue = parseInt(parts[2], 10);
          updated.speedFactor = parseInt(parts[3], 10);
          changed = true;
          break;

        case 'CLOCK':
          updated.timeDisplay = parts[2];
          updated.isRunning = parts[3] === '1' || parts[3] === 'true';
          changed = true;
          break;

        case 'CLOCK_DISABLE':
          updated.disabled = parts[2] === '1' || parts[2] === 'true';
          changed = true;
          break;

        case 'MATRIX_STATE':
          updated.matrixState = parts[2];
          updated.matrixSliderValue = parseInt(parts[3], 10);
          changed = true;
          break;

        case 'MATRIX_SLIDER':
          updated.matrixSliderValue = parseInt(parts[2], 10);
          changed = true;
          break;

        case 'MATRIX_COLOR_1':
        case 'MATRIX_COLOR_2':
        case 'MATRIX_COLOR_3':
        case 'MATRIX_COLOR_4':
        case 'MATRIX_COLOR_5': {
          const colorIdx = parseInt(cmd.slice(-1), 10) - 1;
          if (!updated.matrixColors) updated.matrixColors = [];
          updated.matrixColors[colorIdx] = parts[2];
          changed = true;
          break;
        }

        case 'MATRIX_COMBO':
          updated.matrixComboValue = parts[2];
          changed = true;
          break;

        case 'MATRIX_KNOB':
          updated.matrixKnob = parseInt(parts[2], 10);
          changed = true;
          break;

        case 'AUDIOTRIGGERS':
          updated.audioEnabled = parts[2] === '1' || parts[2] === 'true';
          changed = true;
          break;

        case 'AUDIO_VOLUME':
          updated.audioVolume = parseInt(parts[2], 10);
          changed = true;
          break;

        default:
          break;
      }

      if (!changed) return prev;

      const newMap = new Map(prev.widgets);
      newMap.set(id, updated);

      // Also update in nested page tree
      const newPages = prev.pages.map(p => ({
        ...p,
        children: updateWidgetInTree(p.children, id, updated),
      }));

      return { widgets: newMap, pages: newPages };
    });
  }

  function fetchVC() {
    // Determine vc.json URL — works both in dev (proxy) and production (served from /vc/)
    const vcUrl = window.location.pathname.startsWith('/vc')
      ? `/vc.json?ts=${Date.now()}`   // served from QLC+, vc.json is at root
      : `/vc.json?ts=${Date.now()}`;  // dev proxy
    
    fetch(vcUrl)
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then((data: VCData) => {
        applyVCData(data, set);
      })
      .catch(() => {
        // Fallback: load static sample for offline development
        console.warn('Live vc.json unavailable, trying static sample...');
        const sampleUrl = `${import.meta.env.BASE_URL}vc-sample.json`;
        fetch(sampleUrl)
          .then(res => res.ok ? res.json() : Promise.reject('no sample'))
          .then((data: VCData) => {
            applyVCData(data, set);
          })
          .catch(err => console.error('No VC data available', err));
      });
  }

  return {
    connected: false,
    projectLoaded: false,
    pages: [],
    selectedPage: 0,
    pixelDensity: 1,
    uiColors: {},
    widgets: new Map(),
    grandMasterValue: 255,
    wsClient: null,

    init() {
      // Step 1: Try to fetch vc.json immediately via HTTP (most reliable)
      fetchVC();

      // Step 2: Connect WebSocket for live updates (may fail if server is unstable)
      wsClient = createWSClient();
      wsClient.onStatusChange = (connected) => {
        set({ connected });
      };
      wsClient.onMessage = handleMessage;
      wsClient.connect();
      set({ wsClient });
    },

    cleanup() {
      wsClient?.disconnect();
      wsClient = null;
      set({ wsClient: null, connected: false, projectLoaded: false });
    },

    setSelectedPage(index: number) {
      set({ selectedPage: index });
      wsClient?.sendPageSwitch(index);
    },

    sendWidgetValue(id: number, value: number | string) {
      wsClient?.sendWidgetValue(id, value);
    },

    sendWidgetCommand(id: number, cmd: string, ...args: (string | number)[]) {
      wsClient?.sendWidgetCommand(id, cmd, ...args);
    },
  };
});

// Helper: update a widget deep in the nested tree
function updateWidgetInTree(children: WidgetData[], id: number, updated: WidgetData): WidgetData[] {
  return children.map(w => {
    if (w.id === id) return updated;
    if (w.children) {
      return { ...w, children: updateWidgetInTree(w.children, id, updated) };
    }
    return w;
  });
}
