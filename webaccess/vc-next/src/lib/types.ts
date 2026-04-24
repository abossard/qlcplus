// Widget type enum matching QLC+ VC_TYPE constants
export const WidgetType = {
  Unknown: 0,
  Button: 1,
  Slider: 2,
  XYPad: 3,
  Frame: 4,
  SoloFrame: 5,
  Speed: 6,
  CueList: 7,
  Label: 8,
  AudioTriggers: 9,
  Animation: 10, // RGB Matrix
  Clock: 11,
} as const;
export type WidgetTypeValue = (typeof WidgetType)[keyof typeof WidgetType];

// Button action types
export const ButtonAction = {
  Toggle: 0,
  Flash: 1,
  Blackout: 2,
  StopAll: 3,
} as const;

// Slider modes
export const SliderMode = {
  Level: 0,
  Playback: 1,
  Submaster: 2,
} as const;

// Clock types
export const ClockType = {
  Clock: 0,
  Stopwatch: 1,
  Countdown: 2,
} as const;

// CueList side fader modes
export const SideFaderMode = {
  None: 0,
  Crossfade: 1,
  Steps: 2,
} as const;

// Geometry from vc.json
export interface WidgetGeometry {
  x: number;
  y: number;
  w: number;
  h: number;
}

// Font from vc.json
export interface WidgetFont {
  family?: string;
  size?: number;
  bold?: boolean;
  italic?: boolean;
}

// Cue list step
export interface CueStep {
  index: number;
  label: string;
  fadeIn?: string;
  fadeOut?: string;
  hold?: string;
  duration?: string;
  note?: string;
}

// XY Pad preset
export interface XYPadPreset {
  type: string;
  name: string;
  id?: number;
  x?: number;
  y?: number;
}

// Speed dial function info
export interface SpeedFunction {
  id: number;
  name?: string;
}

// Matrix combo option
export interface MatrixComboOption {
  label: string;
  value: string;
}

// Base widget data from vc.json
export interface WidgetData {
  id: number;
  type: WidgetTypeValue | string; // numeric or string ("Button", "Slider", etc.)
  typeId?: WidgetTypeValue; // numeric type from vc.json
  caption?: string;
  geometry: WidgetGeometry;
  bgColor?: string;
  fgColor?: string;
  bgImage?: string;
  font?: WidgetFont;
  disabled?: boolean;
  visible?: boolean;
  page?: number;

  // Button-specific
  actionType?: number;
  state?: string;
  functionType?: string;

  // Slider-specific
  value?: number;
  min?: number;
  max?: number;
  inverted?: boolean;
  widgetStyle?: string; // "Knob" | "Slider"
  valueDisplay?: string; // "DMX" | "Percentage"
  mode?: number;
  monitorEnabled?: boolean;
  isOverriding?: boolean;
  clickAndGoType?: string;
  cngPrimaryColor?: string;
  cngSecondaryColor?: string;

  // Frame-specific (vc.json uses different names)
  children?: WidgetData[];
  collapsed?: boolean;
  isCollapsed?: boolean; // vc.json name
  headerVisible?: boolean;
  showHeader?: boolean; // vc.json name
  enableButtonVisible?: boolean;
  showEnable?: boolean; // vc.json name
  multipage?: boolean;
  multiPageMode?: boolean; // vc.json name
  totalPages?: number;
  currentPage?: number;
  pageLabels?: string[];
  pagesLoop?: boolean;

  // CueList-specific
  steps?: CueStep[];
  playbackStatus?: number;
  currentStep?: number;
  sideFaderMode?: number;
  sideFaderLevel?: number;
  primaryTop?: boolean;
  primaryLabel?: string;
  secondaryLabel?: string;
  crossfadeEnabled?: boolean;

  // XY Pad-specific
  xPos?: number;
  yPos?: number;
  presets?: XYPadPreset[];
  displayMode?: string;
  rangeX?: [number, number];
  rangeY?: [number, number];

  // Speed-specific
  speedValue?: number;
  speedType?: number;
  visibilityMask?: number;
  functions?: SpeedFunction[];
  speedFactor?: number;

  // Clock-specific
  clockType?: number;
  timeDisplay?: string;
  isRunning?: boolean;

  // Matrix-specific
  matrixColors?: string[];
  matrixComboOptions?: MatrixComboOption[];
  matrixComboValue?: string;
  matrixSliderValue?: number;
  matrixKnob?: number;
  matrixState?: string;

  // Audio-specific
  audioEnabled?: boolean;
  audioVolume?: number;
  bars?: number[];
}

// Page from vc.json — pages are actually Frame widgets
export interface PageData {
  name?: string;
  caption?: string;
  id: number;
  index?: number;
  type?: string;
  typeId?: number;
  children: WidgetData[];
  geometry?: WidgetGeometry;
  bgColor?: string;
  fgColor?: string;
  font?: WidgetFont;
  isCollapsed?: boolean;
  showHeader?: boolean;
  showEnable?: boolean;
  multiPageMode?: boolean;
  totalPages?: number;
  currentPage?: number;
  pageLabels?: string[];
  disabled?: boolean;
  visible?: boolean;
}

// Full VC state from vc.json
export interface VCData {
  version?: string;
  app?: string;
  pixelDensity?: number;
  selectedPage?: number;
  uiStyle?: {
    colors?: Record<string, string>;
  };
  pages: PageData[];
}
