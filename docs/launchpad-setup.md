# Launchpad Mini MK3 — QLC+ Setup Guide

## Overview
This guide explains how to set up the Novation Launchpad Mini MK3 with QLC+ v5 for controlling lighting shows with LED feedback.

## Prerequisites
- QLC+ v5 (QML UI) built with MCP server (`-Dmcp_server=ON`)
- Launchpad Mini MK3 connected via USB
- MIDI plugin loaded (check I/O Manager)

## Known Issue: Feedback Port Bug (Fixed)
QLC+ v5 had a bug where MIDI feedback was sent to the wrong port for multi-port devices like the Launchpad. The fix is in `qmlui/inputoutputmanager.cpp`:

**Before (broken):** `setFeedbackPatch()` searched outputs by name and picked the first match — always port 1.

**After (fixed):** Uses `patch->input()` to send feedback to the same port as the input — always matches correctly.

## Setup Steps

### 1. Identify the correct MIDI port
The Launchpad Mini MK3 has two USB MIDI ports:
- **Port 1 (MIDI)**: Does NOT receive pad input in Programmer Mode
- **Port 2 (DAW)**: Receives pad input + accepts LED feedback

In QLC+ I/O Manager, both show as "Launchpad Mini MK3". **Use the second one** (higher line number).

### 2. Configure Universe
- Set **Input** to: MIDI → Launchpad Mini MK3 (second device / line 2)
- **Do NOT set an Output** — it's not needed
- Apply **Input Profile**: "Novation Launchpad Mini MK3"
- Enable **Feedback** (checkbox in I/O Manager)

### 3. Set MIDI Init Message
In the MIDI output configuration (gear icon):
- Set **Init Message** to: "Novation Launchpad Mini MK3 Developer Mode"
- This sends SysEx to enter Programmer Mode and displays "QLC+" on the pads

### 4. Enter Programmer Mode
On the Launchpad:
1. Hold the **Session** button (top-left)
2. Tap the **orange pad** in the bottom row
3. Release Session

All pads should go dark.

### 5. Map Pads to VC Buttons
Currently must be done manually via QLC+ auto-detect:
1. Click a VC button → Properties → External Input → Auto Detect
2. Press the desired pad on the Launchpad
3. Repeat for each button

### 6. LED Feedback
Once mapped, pressing a pad:
- Triggers the linked function
- Lights up the pad LED (color from profile's UpperValue)

Custom feedback colors can be set via the MCP `configure_vc_feedback` tool.

## Pad Layout (Programmer Mode)
```
     [91][92][93][94][95][96][97][98]   Top row (CC)
[89] [81][82][83][84][85][86][87][88]   Row 8
[79] [71][72][73][74][75][76][77][78]   Row 7
[69] [61][62][63][64][65][66][67][68]   Row 6
[59] [51][52][53][54][55][56][57][58]   Row 5
[49] [41][42][43][44][45][46][47][48]   Row 4
[39] [31][32][33][34][35][36][37][38]   Row 3
[29] [21][22][23][24][25][26][27][28]   Row 2
[19] [11][12][13][14][15][16][17][18]   Row 1
```

QLC+ input channel = 128 + note number

## Color Table (Key Values)
| Value | Color | Value | Color |
|-------|-------|-------|-------|
| 0 | Off | 2 | White 30% |
| 4 | White 60% | 6 | White 100% |
| 8 | Bright Red | 10 | Red 100% |
| 12 | Red 60% | 14 | Red 30% |
| 16 | Bright Orange | 22 | Yellow |
| 26 | Green | 30 | Green 30% |
| 38 | Cyan | 42 | Cyan 30% |
| 50 | Blue | 54 | Blue 30% |
| 62 | Purple | 66 | Purple 30% |

## LED Modes
Set via MIDI channel in the profile's MidiChannelTable:
| MIDI Channel | Mode | Use Case |
|---|---|---|
| 0 | Static | Default — solid color |
| 1 | Flashing | Blinking — warnings, beat |
| 2 | Pulsing | Breathing — active effects |

## MCP Tools for Launchpad
| Tool | Purpose |
|------|---------|
| `query_midi_devices` | Find Launchpad ports |
| `configure_universes` | Set MIDI input (line 2, no output) |
| `set_input_profile` | Apply "Novation Launchpad Mini MK3" profile |
| `query_input_profiles` | List available profiles |
| `configure_vc_feedback` | Set LED idle/active colors + mode |
| `map_vc_inputs` | Map pads to VC widgets (has known limitation) |
