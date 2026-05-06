# P4 ExpFilter migration review

## Side-by-side comparison

| LedFx.ExpFilter | AudioDSP.Filter | Match |
| --- | --- | --- |
| `LedFx.ExpFilter = function(alpha_decay, alpha_rise)` stores `this.alpha_decay`, `this.alpha_rise`, and `this.value = null`. | `AudioDSP.Filter = function(alphaDecay, alphaRise)` stores `this.alpha_decay`, `this.alpha_rise`, and `this.value = null`. | Same instance shape and initial state; only local parameter names differ to match the requested `AudioDSP.Filter` signature. |
| `update(newValue)` initializes `this.value` on first call, then chooses `alpha_rise` when input rises and `alpha_decay` otherwise. | `update(value)` initializes `this.value` on first call, then chooses `alpha_rise` when input rises and `alpha_decay` otherwise. | Same scalar smoothing algorithm and return value. |
| `updateArray(newArray)` initializes or resizes `this.value` to a copy of the incoming array. | `updateArray(arr)` initializes or resizes `this.value` to a copy of the incoming array. | Same array initial state behavior and return value. |
| `updateArray(newArray)` smooths each element independently using per-element rise/decay comparison. | `updateArray(arr)` smooths each element independently using per-element rise/decay comparison. | Same per-element smoothing algorithm and in-place state update. |

## Verification notes

- `AudioDSP.Filter` preserves the same public properties as `LedFx.ExpFilter`: `alpha_decay`, `alpha_rise`, and `value`.
- Constructor state starts at `null`, matching `LedFx.ExpFilter`.
- Scalar and array formulas are unchanged: `alpha * input + (1.0 - alpha) * previous`.
- `audiodsp.js` is included in `resources/rgbscripts/CMakeLists.txt` for installation.
