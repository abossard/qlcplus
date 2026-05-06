/*
  Q Light Controller Plus
  audio_common.js

  Shared helpers for audio-reactive RGB scripts.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

// All DSP (gain, filter smoothing, AGC, hysteresis triggers, band power) is now
// performed in the C++ AudioChannel pipeline and exposed on the per-frame audio
// snapshot:
//   audio.bands.{sub,bass,low,lowMid,mid,high}      // 0-1 band powers
//   audio.triggers.{sub,bass,lowMid,mid,high,volume,beat}.{firedThisFrame,active,releasedThisFrame}
//   audio.volume.{normalized,smoothed,raw}
//   audio.audioDtMs                                 // frame delta for time-based math
//
// AudioParams now only carries the small per-script tuning values that drive
// AudioDSP.Filter rise time, brightness floor, and any remaining script-side
// trigger threshold. Defaults are seeded by installContinuous/installTrigger;
// the values are NOT exposed as UI sliders because the underlying DSP is
// configured globally via the AudioProfile.

var AudioParams = {
    installContinuous: function(algo, defaults) {
        var d = defaults || {};
        algo.presetReactivity = d.reactivity || 5;
        algo.presetFloor = d.floor || 0;
    },

    installTrigger: function(algo, defaults) {
        var d = defaults || {};
        algo.presetReactivity = d.reactivity || 5;
        algo.presetSensitivity = d.sensitivity || 5;
    },

    filterRise: function(algo) { return 0.1 + algo.presetReactivity * 0.09; },

    applyFloor: function(algo, brightness) {
        var f = algo.presetFloor / 100.0;
        return f + (1 - f) * brightness;
    },

    triggerThreshold: function(algo) { return 0.45 - algo.presetSensitivity * 0.04; }
};
