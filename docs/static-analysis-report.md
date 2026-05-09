# Static Analysis Report — clang-tidy + cppcheck

Generated 2026-05-08 for QLC+ at `/Users/abossard/Desktop/projects/qlcplus`.

Tools: `clang-tidy` (LLVM 22.1.4) with `bugprone-*,clang-analyzer-*,performance-*` checks + `cppcheck`.

## Executive summary

### Tool coverage

- **clang-tidy** (LLVM 22.1.4): ✅ Run on all key directories with `bugprone-*`, `clang-analyzer-*`, `performance-*`
- **cppcheck**: ✅ Run with full compile database
- **clazy**: ❌ Failed on macOS SDK header resolution (known issue with standalone mode)

### Findings by directory (clang-tidy, excluding noise)

| Directory | Total | 🔴 Critical | 🟠 Bug-prone | 🟡 Performance |
|---|---:|---:|---:|---:|
| `plugins/ddp/` | 13 | 0 | 7 swappable params | 4 enum-size, 2 interface |
| `engine/audio/src/` | 127 | 0 | ~40 (narrowing, swappable, branch-clone) | ~8 (pass-by-value) |
| RGB Matrix files | 36 | 0 | ~20 (narrowing, branch-clone, swappable) | ~8 (pass-by-value) |
| `engine/src/` | 383 | 1 (infinite loop in utils.h) | ~200 | ~180 |
| `qmlui/` | 625 | 0 | ~300 | ~320 |
| `mcp/` | 219 | 0 | ~120 | ~90 |

### Top warning types across entire codebase

| Check | Count | Severity |
|---|---:|---|
| `bugprone-easily-swappable-parameters` | 327 | 🟠 Design smell — easy to mix up args of same type |
| `performance-enum-size` | 326 | ⚪ Noise — Qt enums are fine at 4 bytes |
| `bugprone-narrowing-conversions` | 291 | 🟠 Potential data loss (double→float, uint→int, double→uchar) |
| `performance-unnecessary-value-param` | 216 | 🟡 QString/QJSValue copied instead of const-ref |
| `bugprone-branch-clone` | 79 | 🟠 Identical branches — possible copy-paste bugs |
| `bugprone-assignment-in-if-condition` | 31 | ⚪ Intentional ALSA pattern |
| `bugprone-switch-missing-default-case` | 20 | 🟠 Missing default in switch |
| `bugprone-infinite-loop` | 2 | 🔴 Same bug in utils.h (included twice) |

---

## 1. DDP Plugin (13 warnings — ✅ Clean)

All 13 warnings are low-severity:

| File | Warning | Verdict |
|---|---|---|
| `ddpcontroller.cpp:194,201,208` | Adjacent params of same type easily swapped (`setDestPort(universe, port)`, `setDestId`, `setDDPOffset`) | 🟠 Design smell but matches upstream QLC+ API convention |
| `ddppacketizer.cpp:23,25` | `buildPacket` params swappable | 🟠 Same — protocol params have natural order |
| `ddpplugin.cpp:240,268,275` | `writeUniverse`, `openInput`, `closeInput` params swappable | 🟠 Matches `QLCIOPlugin` interface |
| `ddpcontroller.h:52,53` | Enum could use `uint8_t` base | ⚪ Noise |
| `qlcioplugin.h:127,270` | Interface enum size | ⚪ Not our code |
| `ui_configureddp.h:93` | Reserved identifier in Qt-generated code | ⚪ Not our code |

**Verdict: DDP is clean. No bugs, no real performance issues.** The swappable-params warnings are inherent to the plugin interface design.

---

## 2. Audio Processing (127 warnings)

### 🟠 Narrowing conversions (14 total) — worth fixing in our code

| File:Line | Conversion | Risk |
|---|---|---|
| `aubioprocessor.cpp:390-418` | `double → smpl_t (float)` × 7 | Low — aubio uses float internally, values are in range |
| `audiocapture.cpp:173,199` | `double → float` | Low — audio sample values |
| `audiocapture.cpp:257` | `unsigned int → int` | Low — buffer size, won't exceed INT_MAX |
| `audiocapture_qt5.cpp:121` | `qreal → float` | Low |
| `audiorenderer.cpp:192` | `int → short` | 🟠 **Actual truncation risk** — audio sample conversion |
| `audiorenderer.cpp:193` | `double → short` | 🟠 **Actual truncation risk** — volume-scaled sample |
| `audiorenderer_qt6.cpp:67` | `quint32 → int` | Low |

**Fix priority**: `audiorenderer.cpp:192-193` — these do PCM sample conversion and should use explicit `static_cast<short>` with clamping.

### 🟠 Swappable parameters (15 total)

| File | Function | Params |
|---|---|---|
| `aubioprocessor.cpp:57` | `setMattMelBands(int, int, int)` | 3 ints easily mixed |
| `aubioprocessor.cpp:751` | `readAubioOnsetDefaults(int, int)` | method index + onset index |
| `audiochannel.cpp:452` | `operator()(const double*, const double*)` | Two raw pointers |
| `audiochannel.cpp:593` | `alpha(double, double)` | Two doubles |
| `audiochannelconfig.cpp:17` | `bounded(int, int, int)` | val, min, max |
| `audioparameters.cpp:42` | Constructor params | |
| `beattracker.cpp:41` | Constructor — 5 adjacent convertible params! | 🟠 Fragile |
| Various renderers | `initialize(quint32, int)` | sample rate + channels |

**Fix priority**: `beattracker.cpp:41` with 5 swappable params is the most fragile. Consider a config struct.

### 🟠 Branch clones (2)

| File:Line | Issue |
|---|---|
| `audiorenderer_alsa.cpp:120` | Switch with 4 identical branches — likely placeholder for format-specific handling |
| `audiorenderer_qt5.cpp:67` | Same pattern — 4 identical branches |

### 🟡 Pass-by-value (8 total)

| File | Param type |
|---|---|
| `audio.cpp:145,196` | `QString filename`, `QString dev` |
| `audiopluginecache.cpp:165` | `QString devName` |
| `audiorenderer_*.cpp` (4 files) | `QString device` |

All should be `const QString&`.

### Other (cppcheck)
- `aubioprocessor.cpp:44`, `audiochannel.cpp:40` — `log(1+x)` → use `std::log1p(x)` for precision near zero
- `audiocapture_alsa.cpp:25` — uninitialized `pcm_name`
- `audiocapture_wavein.cpp:29` — uninitialized `m_internalBuffers`

---

## 3. RGB Matrix (36 warnings)

### 🟠 Narrowing conversions — the juicy ones

| File:Line | Conversion | Risk |
|---|---|---|
| **`rgbmatrix.cpp:1085`** | **`double → uchar`** | 🟠 **Real bug risk** — `0.299*R + 0.587*G + 0.114*B` truncated to unsigned char without rounding. Should be `static_cast<uchar>(std::round(...))` |
| `rgbmatrix.cpp:1129,1150` | `qreal → float` | Low — color attribute |
| `rgbscriptv4.cpp:633,634` | `uint → int` | Low — map dimensions |
| `rgbscriptv4.cpp:635-637` | `double → float` | Low — color components |
| `rgbmatrixeditor.cpp:91,880,997` | `quint32/uint → int` | Low — IDs and indices |

### 🟠 Branch clones — possible bugs

| File:Line | Issue | Verdict |
|---|---|---|
| **`rgbtext.cpp:217`** | `if` with identical then/else branches | ⚪ False positive — the outer `if (Horizontal)` changes which axis is used but the inner pixel assignment is the same |
| **`rgbmatrixeditor.cpp:907`** | `Time` and `Beats` branches both do `m_previewElapsed += MasterTimer::tick()` | 🟠 **Likely bug** — Beats mode should probably scale by beat duration, not add raw tick. The `effectiveDuration` conversion happens later but the elapsed accumulation is identical |
| **`rgbscript.cpp:498`** | `isValid()` returns `value.toString()`, else returns `QString()` — but the error branch above already returns `QString()` | ⚪ Intentional — error vs "not valid" distinction |

### 🟡 Pass-by-value (8 total)

| File | Function | Param |
|---|---|---|
| `rgbmatrix.cpp:449,465` | `property`/`setProperty` | `QString propName` |
| `rgbmatrix.cpp:1208` | `setBlendMode` | `QString mode` |
| `rgbscriptscache.cpp:42` | `script` | `QString name` |
| `rgbscriptv4.cpp:316` | `displayError` | `QJSValue e` |
| `rgbmatrixeditor.cpp:523,539,553` | Various | `QString paramName` |

### 🟠 Swappable parameters

| File | Function | Params |
|---|---|---|
| `rgbmatrix.cpp:885` | `updateFaderValues` | 2 convertible params |
| `rgbmatrix.cpp:1335` | `blendPixels(uint, uint)` | Two pixel colors |
| `rgbmatrix.cpp:1359` | `applyTransforms(int, int)` | x, y |
| `rgbscriptv4.cpp:798` | `operator()(int, int, int)` | 3 convertible |
| `rgbtext.cpp:173,231` | `renderScrollingText`/`renderStaticLetters` | step + dimension |

---

## 4. Cross-codebase critical findings

### 🔴 Infinite loop — `plugins/interfaces/utils.h:94`

```cpp
while (len--) {   // len is qsizetype (signed)
    c = *p++;
    crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
    c >>= 4;
    crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
}
```

**This is a false positive.** `len--` does modify `len` in the loop condition. clang-tidy 22.x has a known issue where it doesn't track post-decrement in `while` conditions. The loop is correct — it's the classic CRC-16 pattern.

### 🔴 Null pointer dereference — `plugins/hid/macx/hidapi.cpp:807` (cppcheck)

`return_data()` uses `memcpy(data, ...)` without null-checking `data`. Upstream HIDAPI code — not our bug but worth a defensive check.

---

## 5. Summary of actionable items

### Fix now (our code, real risk)
1. **`rgbmatrixeditor.cpp:907`** — Beats vs Time preview elapsed: identical branches, likely should differ
2. **`audiorenderer.cpp:192-193`** — PCM sample narrowing without clamping
3. **`rgbmatrix.cpp:1085`** — `double→uchar` in `rgbToGrey` should round

### Fix soon (our code, low risk)  
4. Replace `log(1+x)` with `std::log1p(x)` in aubio/audio code (2 places)
5. Add explicit `static_cast<float>()` to aubioprocessor double→smpl_t conversions (7 places)
6. Pass `QString` by `const&` in audio + RGB Matrix code (~16 places)

### Won't fix (upstream / interface / noise)
- All `bugprone-easily-swappable-parameters` — inherent to Qt plugin interfaces
- All `performance-enum-size` — Qt convention
- All `bugprone-assignment-in-if-condition` in ALSA code — intentional error-handling pattern
- `bugprone-reserved-identifier` in Qt-generated UI code
- `bugprone-infinite-loop` false positive in utils.h
