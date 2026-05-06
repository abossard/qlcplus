/*
  Q Light Controller Plus
  audiodsp.js

  Audio DSP helpers for audio-reactive RGB scripts.
*/

var AudioDSP = AudioDSP || {};

AudioDSP.Filter = function(alphaDecay, alphaRise) {
    this.alpha_decay = alphaDecay;
    this.alpha_rise = alphaRise;
    this.value = null;
};

AudioDSP.Filter.prototype.update = function(value) {
    if (this.value === null) {
        this.value = value;
        return this.value;
    }
    var alpha = (value > this.value) ? this.alpha_rise : this.alpha_decay;
    this.value = alpha * value + (1.0 - alpha) * this.value;
    return this.value;
};

AudioDSP.Filter.prototype.updateArray = function(arr) {
    if (this.value === null || this.value.length !== arr.length) {
        this.value = new Array(arr.length);
        for (var i = 0; i < arr.length; i++)
            this.value[i] = arr[i];
        return this.value;
    }
    for (var i = 0; i < arr.length; i++) {
        var alpha = (arr[i] > this.value[i]) ? this.alpha_rise : this.alpha_decay;
        this.value[i] = alpha * arr[i] + (1.0 - alpha) * this.value[i];
    }
    return this.value;
};
