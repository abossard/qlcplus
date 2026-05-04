### Blocking issues

#### 1. `EFXFixture::nextStep()` cannot implement Beats mode correctly as-is
**Issue:** `EFXFixture::nextStep()` currently takes only `QList<Universe*>` and `GenericFader`; it does **not** receive `MasterTimer`. It advances with `MasterTimer::tick()` and compares against `m_parent->loopDuration()` directly.

Relevant current flow:

- `EFX::write(MasterTimer *timer, ...)` ignores `timer`.
- `EFXFixture::nextStep(...)` has no timer parameter.
- `EFXFixture::nextStep()` compares millisecond `m_elapsed` against `m_parent->loopDuration()`.
- In Beats mode, `loopDuration()` would return beat-encoded units, not milliseconds.

**Impact:** If `tempoType() == Beats`, the fixture would compare milliseconds to beat units, causing wrong speed, wrong wrapping, wrong PingPong reversal, and wrong propagation offsets.

**Recommended fix:** Pass timing context into fixture stepping. Either:

```cpp
void EFXFixture::nextStep(QList<Universe*> universes,
                          QSharedPointer<GenericFader> fader,
                          MasterTimer *timer);
```

or preferably pass already-converted runtime values:

```cpp
void EFXFixture::nextStep(QList<Universe*> universes,
                          QSharedPointer<GenericFader> fader,
                          quint32 loopDurationMs);
```

The second option keeps beat conversion centralized in `EFX`.

---

#### 2. BPM changes need explicit elapsed rescaling
**Issue:** `EFXFixture::durationChanged()` rescales `m_elapsed` from `m_currentAngle`, but it currently uses `m_parent->loopDuration()` directly. In Beats mode, a BPM change changes the effective loop duration in milliseconds even if the stored beat duration does not change.

RGBMatrix handles this by recalculating `m_stepBeatDuration = beatsToTime(duration(), timer->beatTimeDuration())`.

**Impact:** If BPM changes from 120 to 90 mid-cycle, the EFX can jump position, wrap early/late, or reverse PingPong at the wrong time.

**Recommended fix:** EFX needs a cached effective loop duration in milliseconds, e.g.:

```cpp
quint32 m_runtimeLoopDurationMs = 0;
quint32 m_runtimeFadeInMs = 0;
```

On each `write()` in Beats mode:

1. Compute `newLoopDurationMs = beatsToTime(loopDuration(), timer->beatTimeDuration())`.
2. If it differs from the cached value, rescale each fixture’s `m_elapsed` from `m_currentAngle` into the new millisecond loop.
3. Update the cache.
4. Use the cached millisecond loop duration for all stepping, offsets, wrapping, PingPong, and angle calculation.

---

#### 3. Fade-in is currently unit-incompatible in Beats mode
**Issue:** `EFX::rotateAndScale()` does:

```cpp
uint fadeIn = overrideFadeInSpeed() == defaultSpeed() ? fadeInSpeed() : overrideFadeInSpeed();

if (fadeIn > 0 && elapsed() <= fadeIn)
```

`elapsed()` is milliseconds. In Beats mode, `fadeInSpeed()` is beat-encoded units.

**Impact:** A one-beat fade-in at 120 BPM is stored as `1000`, but should last `500ms`. The EFX would fade in over the wrong duration.

**Recommended fix:** Convert fade-in to milliseconds before comparison. Since `rotateAndScale()` does not receive `MasterTimer`, EFX should cache the converted runtime fade-in duration before fixture stepping:

```cpp
m_runtimeFadeInMs = effectiveSpeedToMs(fadeIn, timer);
```

Then `rotateAndScale()` should compare against `m_runtimeFadeInMs`.

---

#### 4. `timeOffset()` must not return beat units when `m_elapsed` is milliseconds
**Issue:** `EFXFixture::timeOffset()` currently returns:

```cpp
m_parent->loopDuration() / (m_parent->fixtures().size() + 1) * serialNumber()
```

If `loopDuration()` is beat-encoded, `timeOffset()` returns beat units. But `m_elapsed` remains milliseconds.

**Impact:** Serial/asymmetric fixture propagation becomes wrong in Beats mode.

**Recommended fix:** Change `timeOffset()` to accept the already-converted loop duration:

```cpp
quint32 EFXFixture::timeOffset(quint32 loopDurationMs) const;
```

Use that value everywhere inside runtime stepping and duration rescaling.

---

#### 5. PingPong reversal must use effective milliseconds
**Issue:** PingPong reversal currently happens when:

```cpp
if (m_elapsed > m_parent->loopDuration())
```

In Beats mode, this compares milliseconds to beat units.

**Impact:** PingPong reverses at the wrong point.

**Recommended fix:** Use `loopDurationMs` everywhere in `nextStep()`:

```cpp
if (m_elapsed > loopDurationMs)
```

and calculate angle with:

```cpp
uint pos = (m_elapsed + timeOffset(loopDurationMs)) % loopDurationMs;
```

---

### Non-blocking issues

#### 6. `EFX::setDuration()` does not need its own snap-to-beat logic, but its rescale path must become beat-aware
**Issue:** `Function::setDuration()` already snaps values when `tempoType() == Beats`. `EFX::setDuration()` calls `Function::setDuration(ms)`, so duplicating snap logic in EFX would be unnecessary.

However, after setting duration, EFX calls:

```cpp
m_fixtures[i]->durationChanged();
```

and `durationChanged()` currently uses raw `loopDuration()`.

**Impact:** Beat snapping is probably already handled, but elapsed rescaling is not.

**Recommended fix:** Keep beat snapping in `Function::setDuration()`. Change EFX’s rescale path to pass effective milliseconds:

```cpp
m_fixtures[i]->durationChanged(effectiveLoopDurationMs);
```

When stopped, or when no timer is available, compute using `doc()->masterTimer()->beatTimeDuration()`.

---

#### 7. Chaser interaction needs a clear semantic decision
**Issue:** A chaser starts child functions with override fade/duration/tempo:

```cpp
func->start(timer, ..., newStep->m_fadeIn, newStep->m_fadeOut,
            newStep->m_duration, m_chaser->tempoType());
```

EFX currently uses `tempoType()` and partially uses override fade-in, but does not appear to use `overrideDuration()` for its own loop duration.

**Impact:** If a beat-mode chaser contains a time-mode EFX, or vice versa, there is risk of double-conversion or ignored override timing.

**Recommended fix:** Define behavior explicitly:

- The chaser controls when the EFX starts/stops.
- The EFX’s internal movement cycle uses its own `tempoType()` and duration.
- Chaser speed overrides should only affect EFX movement if that is already intended by existing override semantics.

For this feature, avoid expanding semantics unless tests require it. Do not accidentally make a beat-mode chaser convert an EFX’s own duration twice.

---

### Suggestions

#### 8. Add the QML tempo type dropdown by copying the RGBMatrix pattern
EFXEditor already uses:

```qml
tempoType: efxEditor.tempoType
TimeUtils.timeToQlcString(..., efxEditor.tempoType)
```

and `TimeEditTool` already supports beats. The missing UI piece is the tempo type selector.

The RGBMatrix editor already has the pattern:

```qml
CustomComboBox {
    model: [
        { mLabel: qsTr("Time"), mValue: QLCFunction.Time },
        { mLabel: qsTr("Beats"), mValue: QLCFunction.Beats }
    ]

    currValue: rgbMatrixEditor.tempoType
    onValueChanged: rgbMatrixEditor.tempoType = value
}
```

So EFX likely needs only the dropdown row, not new C++ exposure.

---

# Implementation plan

## Phase 1 — Add beat-aware runtime timing helpers in EFX

### Files

- `engine/src/efx.h`
- `engine/src/efx.cpp`

### Checklist

- [ ] Add private runtime timing members to `EFX`:

```cpp
quint32 m_runtimeLoopDurationMs;
quint32 m_runtimeFadeInMs;
```

Optional:

```cpp
quint32 m_lastEffectiveLoopDurationMs;
```

- [ ] Add helper methods:

```cpp
Function::TempoType runtimeTempoType() const;
quint32 speedToRuntimeMs(quint32 value, MasterTimer *timer) const;
quint32 effectiveDurationMs(MasterTimer *timer) const;
quint32 effectiveFadeInMs(MasterTimer *timer) const;
quint32 effectiveLoopDurationMs(MasterTimer *timer) const;
```

- [ ] `speedToRuntimeMs()` should return the value unchanged in Time mode and `beatsToTime(value, timer->beatTimeDuration())` in Beats mode.
- [ ] Preserve special values:
  - `Function::defaultSpeed()`
  - `Function::infiniteSpeed()`
  - `0`

### Exact behavior

`effectiveLoopDurationMs()` should mirror current `loopDuration()` semantics, but in milliseconds:

```cpp
fadeIn = overrideFadeInSpeed() == defaultSpeed()
    ? fadeInSpeed()
    : overrideFadeInSpeed();

duration = duration();

if (tempoType() == Beats) {
    fadeIn = beatsToTime(fadeIn, timer->beatTimeDuration());
    duration = beatsToTime(duration, timer->beatTimeDuration());
}

return Function::speedSubtract(duration, fadeIn);
```

If the implementation intentionally supports chaser `overrideDuration()`, use it deliberately and add tests. Otherwise, do not mix it into this feature.

### Verification

- [ ] Time-mode EFX produces the same `loopDuration()` behavior as before.
- [ ] Beats-mode EFX with duration `4000` beats and fade-in `0` at 120 BPM gives `2000ms` runtime loop duration.
- [ ] Beats-mode EFX with duration `4000` beats and fade-in `1000` at 120 BPM gives `1500ms` runtime loop duration.

---

## Phase 2 — Pass runtime loop duration into `EFXFixture::nextStep()`

### Files

- `engine/src/efxfixture.h`
- `engine/src/efxfixture.cpp`
- `engine/src/efx.cpp`

### Checklist

- [ ] Change the `nextStep()` signature from:

```cpp
void nextStep(QList<Universe *> universes, QSharedPointer<GenericFader> fader);
```

to:

```cpp
void nextStep(QList<Universe *> universes,
              QSharedPointer<GenericFader> fader,
              quint32 loopDurationMs);
```

- [ ] Update the call site in `EFX::write()`:

```cpp
ef->nextStep(universes, fader, m_runtimeLoopDurationMs);
```

- [ ] Replace all runtime uses of `m_parent->loopDuration()` inside `nextStep()` with `loopDurationMs`.

### Exact changes inside `nextStep()`

Replace:

```cpp
if (m_parent->loopDuration() == 0)
```

with:

```cpp
if (loopDurationMs == 0)
```

Replace:

```cpp
if (m_elapsed > m_parent->loopDuration())
```

with:

```cpp
if (m_elapsed > loopDurationMs)
```

Replace:

```cpp
uint pos = (m_elapsed + timeOffset()) % m_parent->loopDuration();
m_currentAngle = SCALE(float(pos),
                       float(0), float(m_parent->loopDuration()),
                       float(0), float(M_PI * 2));
```

with:

```cpp
uint offset = timeOffset(loopDurationMs);
uint pos = (m_elapsed + offset) % loopDurationMs;

m_currentAngle = SCALE(float(pos),
                       float(0), float(loopDurationMs),
                       float(0), float(M_PI * 2));
```

### Verification

- [ ] Time-mode EFX movement is unchanged.
- [ ] Beats-mode EFX no longer compares milliseconds to beat units.
- [ ] PingPong reverses after the converted loop duration.
- [ ] SingleShot completes after the converted loop duration.

---

## Phase 3 — Make fixture propagation offsets beat-safe

### Files

- `engine/src/efxfixture.h`
- `engine/src/efxfixture.cpp`

### Checklist

- [ ] Change `timeOffset()` from:

```cpp
uint timeOffset() const;
```

to:

```cpp
uint timeOffset(quint32 loopDurationMs) const;
```

- [ ] Implement it as:

```cpp
uint EFXFixture::timeOffset(quint32 loopDurationMs) const
{
    if (m_parent->propagationMode() == EFX::Asymmetric ||
        m_parent->propagationMode() == EFX::Serial)
    {
        return loopDurationMs / (m_parent->fixtures().size() + 1) * serialNumber();
    }

    return 0;
}
```

- [ ] Update all callers:
  - `nextStep()`
  - `durationChanged()`

### Verification

- [ ] Serial propagation in Time mode behaves exactly as before.
- [ ] Serial propagation in Beats mode spaces fixtures across the converted beat loop duration.
- [ ] Asymmetric propagation in Beats mode uses the same phase spacing as Time mode.

---

## Phase 4 — Make elapsed rescaling BPM-aware

### Files

- `engine/src/efx.h`
- `engine/src/efx.cpp`
- `engine/src/efxfixture.h`
- `engine/src/efxfixture.cpp`

### Checklist

- [ ] Change `EFXFixture::durationChanged()` from:

```cpp
void durationChanged();
```

to:

```cpp
void durationChanged(quint32 loopDurationMs);
```

- [ ] Replace all internal uses of `m_parent->loopDuration()` with the passed `loopDurationMs`.
- [ ] Replace `timeOffset()` call with `timeOffset(loopDurationMs)`.

Current logic should become conceptually:

```cpp
m_elapsed = SCALE(float(m_currentAngle),
                  float(0), float(M_PI * 2),
                  float(0), float(loopDurationMs));

uint offset = timeOffset(loopDurationMs);
if (offset)
{
    if (m_elapsed < offset)
        m_elapsed += loopDurationMs;

    m_elapsed -= offset;
}
```

- [ ] In `EFX::setDuration()`, compute the effective loop duration in milliseconds and pass it to fixtures.
- [ ] In `EFX::write()`, after computing `newLoopDurationMs`, if it differs from cached `m_runtimeLoopDurationMs`, call `durationChanged(newLoopDurationMs)` on each fixture before stepping.

### Verification

- [ ] Changing EFX duration while running preserves the current visual angle.
- [ ] Changing BPM while running preserves the current visual angle.
- [ ] Serial/asymmetric offsets remain correct after BPM change.
- [ ] PingPong does not immediately reverse incorrectly after BPM change.

---

## Phase 5 — Convert fade-in timing in Beats mode

### Files

- `engine/src/efx.h`
- `engine/src/efx.cpp`

### Checklist

- [ ] In `EFX::write()`, compute and cache:

```cpp
m_runtimeFadeInMs = effectiveFadeInMs(timer);
```

before fixtures calculate points.

- [ ] Change `EFX::rotateAndScale()` to use `m_runtimeFadeInMs` instead of raw `fadeInSpeed()` / `overrideFadeInSpeed()`.

Current code:

```cpp
uint fadeIn = overrideFadeInSpeed() == defaultSpeed() ? fadeInSpeed() : overrideFadeInSpeed();
if (fadeIn > 0 && elapsed() <= fadeIn)
```

New behavior:

```cpp
uint fadeIn = m_runtimeFadeInMs;
if (fadeIn > 0 && elapsed() <= fadeIn)
```

- [ ] Ensure `m_runtimeFadeInMs` is initialized in the constructor to `0`.
- [ ] Ensure Time mode still uses raw milliseconds.

### Verification

- [ ] Time-mode fade-in behavior is unchanged.
- [ ] Beats-mode one-beat fade-in at 120 BPM lasts about `500ms`.
- [ ] Beats-mode one-beat fade-in at 60 BPM lasts about `1000ms`.
- [ ] Changing BPM during fade-in changes the remaining fade scale smoothly enough and does not compare beat units to milliseconds.

---

## Phase 6 — Update `EFX::write()` to use `MasterTimer`

### Files

- `engine/src/efx.cpp`

### Checklist

- [ ] Remove:

```cpp
Q_UNUSED(timer);
```

- [ ] At the start of `write()` after pause checks, compute runtime timing:

```cpp
quint32 loopMs = effectiveLoopDurationMs(timer);
m_runtimeFadeInMs = effectiveFadeInMs(timer);
```

- [ ] If Beats mode and `loopMs` changed from cached value, rescale fixtures.
- [ ] Store `m_runtimeLoopDurationMs = loopMs`.
- [ ] Pass `m_runtimeLoopDurationMs` into every fixture’s `nextStep()`.

### Verification

- [ ] EFX builds without unused-variable warnings.
- [ ] Time mode remains behaviorally unchanged.
- [ ] Beats mode follows BPM live.

---

## Phase 7 — Optional but recommended: override `slotBPMChanged()`

### Files

- `engine/src/efx.h`
- `engine/src/efx.cpp`

### Checklist

- [ ] Add a private/protected flag:

```cpp
bool m_timingResyncNeeded;
```

- [ ] Add slot override:

```cpp
protected slots:
    void slotBPMChanged(int bpmNumber) override;
```

- [ ] Implement:

```cpp
void EFX::slotBPMChanged(int bpmNumber)
{
    Q_UNUSED(bpmNumber)
    m_timingResyncNeeded = true;
}
```

- [ ] In `write()`, if `m_timingResyncNeeded`, recompute effective loop duration and rescale fixtures.

### Verification

- [ ] BPM changes trigger rescaling even if the converted duration comparison would otherwise be skipped by rounding.
- [ ] No duplicate or broken signal connections occur when switching tempo type repeatedly.

---

## Phase 8 — Add tempo type dropdown to EFX editor

### Files

- `qmlui/qml/fixturesfunctions/EFXEditor.qml`

### Checklist

- [ ] In the Speed section, add a row equivalent to RGBMatrix’s tempo selector:

```qml
RobotoText {
    id: ttLabel
    height: UISettings.listItemHeight
    label: qsTr("Tempo type")
}

CustomComboBox {
    Layout.columnSpan: 2
    Layout.fillWidth: true
    height: UISettings.listItemHeight
    model: [
        { mLabel: qsTr("Time"), mValue: QLCFunction.Time },
        { mLabel: qsTr("Beats"), mValue: QLCFunction.Beats }
    ]

    currValue: efxEditor.tempoType
    onValueChanged: efxEditor.tempoType = value
}
```

- [ ] Keep `TimeEditTool` unchanged; it already has:

```qml
tempoType: efxEditor.tempoType
```

- [ ] Keep labels using `TimeUtils.timeToQlcString(..., efxEditor.tempoType)`.

### Verification

- [ ] EFX editor shows Time/Beats selector.
- [ ] Switching to Beats converts displayed fade/loop/duration values.
- [ ] Double-clicking speed values opens `TimeEditTool` in beat mode.
- [ ] Switching back to Time converts values using current BPM.

---

## Phase 9 — Tests

### Files

- `engine/test/efx/efx_test.h`
- `engine/test/efx/efx_test.cpp`
- `engine/test/efxfixture/efxfixture_test.h`
- `engine/test/efxfixture/efxfixture_test.cpp`

### Checklist

Add tests for:

- [ ] `EFX::effectiveLoopDurationMs()` in Time mode.
- [ ] `EFX::effectiveLoopDurationMs()` in Beats mode at different BPMs.
- [ ] `EFXFixture::timeOffset(loopDurationMs)` in Serial mode.
- [ ] `EFXFixture::timeOffset(loopDurationMs)` in Asymmetric mode.
- [ ] `EFXFixture::durationChanged(loopDurationMs)` preserves `m_currentAngle`.
- [ ] BPM change while running rescales `m_elapsed`.
- [ ] PingPong reverses at converted loop duration.
- [ ] Fade-in comparison uses converted milliseconds.
- [ ] XML save/load preserves `Tempo` and beat-encoded speed values.

### Verification command

```bash
cd /Users/abossard/Desktop/projects/qlcplus/build
cmake --build . --target efx_test efxfixture_test -j8
./engine/test/efx/efx_test
./engine/test/efxfixture/efxfixture_test
```

Also run RGBMatrix tests to ensure no shared Function timing behavior regressed:

```bash
cmake --build . --target rgbmatrix_test -j8
./engine/test/rgbmatrix/rgbmatrix_test
```

---

# Edge cases to test manually

- [ ] EFX in Time mode, 20s default duration, no behavior change.
- [ ] EFX in Beats mode, duration `4 beats`, BPM `120`, full circle takes about `2s`.
- [ ] Same EFX at BPM `60`, full circle takes about `4s`.
- [ ] Change BPM from `120` to `60` mid-cycle: position should not jump.
- [ ] Change BPM from `60` to `180` mid-cycle: position should not jump or wrap unexpectedly.
- [ ] Beats mode with fade-in `1 beat`.
- [ ] Beats mode with fade-in longer than loop duration.
- [ ] Beats mode with duration `0`.
- [ ] Beats mode with SingleShot.
- [ ] Beats mode with PingPong.
- [ ] Beats mode with Serial propagation.
- [ ] Beats mode with Asymmetric propagation.
- [ ] Beat-mode EFX inside Time-mode chaser.
- [ ] Time-mode EFX inside Beat-mode chaser.
- [ ] Beat-mode EFX inside Beat-mode chaser.
- [ ] Switching tempo type while EFX preview is running.
- [ ] Saving and reopening a project with beat-mode EFX.

---

# Effort estimate

### Engine timing changes

**Medium complexity — 1 to 2 days**

The risky part is not converting beats to milliseconds; it is keeping `m_elapsed`, propagation offset, fade-in, PingPong, and BPM-change rescaling all in the same unit.

### QML editor change

**Small — 1 to 2 hours**

The tempo type property is already exposed by `FunctionEditor`; EFXEditor.qml mainly needs the dropdown.

### Tests

**Medium — 0.5 to 1 day**

Existing `efx_test` and `efxfixture_test` are good places to add focused coverage.

### Total

**Rough estimate: 2 to 3 days**, including verification and manual playback checks.