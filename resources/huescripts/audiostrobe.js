/*
  Q Light Controller Plus
  audiostrobe.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(
  function () {
    var referenceState = null;
    var runtimeGeometryKey;
    var runtimeIdentityKey;
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Strobe";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 3;
    algo.usesAudio = true;
    algo.properties = new Array();

    algo.presetMode = "Artistic";
    algo.properties.push("name:mode|type:list|display:Response|values:Artistic,Reference,Percussive RGB|write:setMode|read:getMode");
    algo.setMode = function(v) {
        algo.presetMode = (v === "Reference" || v === "Percussive RGB") ? v : "Artistic";
        referenceState = null;
        resetDynamicState(0);
        runtimeIdentityKey = "";
        runtimeGeometryKey = "";
    };
    algo.getMode = function() { return algo.presetMode; };
    algo.referenceClock = "Detected";
    algo.properties.push("name:referenceClock|type:list|display:Reference Clock|values:Detected,Extrapolated|write:setReferenceClock|read:getReferenceClock");
    algo.setReferenceClock = function(v) { algo.referenceClock = v === "Extrapolated" ? v : "Detected"; };
    algo.getReferenceClock = function() { return algo.referenceClock; };
    algo.referenceBlur = 1.5;
    algo.referenceMirror = "On";
    algo.referenceBrightness = 0.7;
    algo.referencePalette = "#ff0000,#00ff00,#0000ff";
    algo.referencePositions = "";
    algo.referenceRoll = 0;
    algo.referencePulseCount = 2;
    algo.referenceStrobeDecay = 1.5;
    algo.referenceBeatDecay = 2;
    algo.referencePattern = "****";
    algo.referenceFlip = "Off";
    algo.referenceBackgroundMode = "Off";
    algo.referenceBackgroundColor = "#000000";
    algo.referenceBackgroundBrightness = 1;
    algo.properties.push("name:referenceBlur|type:float|display:Reference Blur|write:setReferenceBlur|read:getReferenceBlur");
    algo.properties.push("name:referenceMirror|type:list|display:Reference Mirror|values:Off,On|write:setReferenceMirror|read:getReferenceMirror");
    algo.properties.push("name:referenceBrightness|type:float|display:Reference Brightness|write:setReferenceBrightness|read:getReferenceBrightness");
    algo.properties.push("name:referencePalette|type:string|display:Reference RGB Palette|write:setReferencePalette|read:getReferencePalette");
    algo.properties.push("name:referencePositions|type:string|display:Reference Positions (0..1 CSV)|write:setReferencePositions|read:getReferencePositions");
    algo.properties.push("name:referenceRoll|type:float|values:0,1|display:Reference Palette Roll (0..1)|write:setReferenceRoll|read:getReferenceRoll");
    algo.properties.push("name:referencePulseCount|type:list|display:Reference Pulses|values:1,2,4,8,16,32|write:setReferencePulseCount|read:getReferencePulseCount");
    algo.properties.push("name:referenceStrobeDecay|type:float|values:1,10|display:Reference Strobe Decay (1..10)|write:setReferenceStrobeDecay|read:getReferenceStrobeDecay");
    algo.properties.push("name:referenceBeatDecay|type:float|values:0,10|display:Reference Beat Decay (0..10)|write:setReferenceBeatDecay|read:getReferenceBeatDecay");
    algo.properties.push("name:referencePattern|type:list|display:Reference Pattern|values:****,*.*.,.*.*,*...,...*|write:setReferencePattern|read:getReferencePattern");
    algo.properties.push("name:referenceFlip|type:list|display:Reference Flip|values:Off,On|write:setReferenceFlip|read:getReferenceFlip");
    algo.properties.push("name:referenceBackgroundMode|type:list|display:Reference Background Mode|values:Off,Additive|write:setReferenceBackgroundMode|read:getReferenceBackgroundMode");
    algo.properties.push("name:referenceBackgroundColor|type:string|display:Reference Background Color (#rrggbb)|write:setReferenceBackgroundColor|read:getReferenceBackgroundColor");
    algo.properties.push("name:referenceBackgroundBrightness|type:float|values:0,1|display:Reference Background Brightness (0..1)|write:setReferenceBackgroundBrightness|read:getReferenceBackgroundBrightness");
    algo.setReferenceBlur = function(v) { algo.referenceBlur = Math.max(0, Math.min(10, parseFloat(v) || 0)); };
    algo.getReferenceBlur = function() { return algo.referenceBlur; };
    algo.setReferenceMirror = function(v) { algo.referenceMirror = v === "On" ? "On" : "Off"; };
    algo.getReferenceMirror = function() { return algo.referenceMirror; };
    algo.setReferenceBrightness = function(v) { algo.referenceBrightness = HSVUtil.clamp01(parseFloat(v) || 0); };
    algo.getReferenceBrightness = function() { return algo.referenceBrightness; };
    algo.setReferencePalette = function(v) {
      if (typeof v !== "string") return;
      var parsed = parseReferenceGradient(v);
      if (parsed) {
        algo.referencePalette = parsed.palette;
        algo.referencePositions = parsed.positions;
      }
    };
    algo.getReferencePalette = function() { return algo.referencePalette; };
    algo.setReferencePositions = function(v) { if (typeof v === "string") algo.referencePositions = v; };
    algo.getReferencePositions = function() { return algo.referencePositions; };
    algo.setReferenceRoll = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0;
      algo.referenceRoll = HSVUtil.clamp01(value);
    };
    algo.getReferenceRoll = function() { return algo.referenceRoll; };
    algo.setReferencePulseCount = function(v) {
      var value = parseInt(v, 10);
      if ([1, 2, 4, 8, 16, 32].indexOf(value) < 0) value = 2;
      algo.referencePulseCount = value;
    };
    algo.getReferencePulseCount = function() { return algo.referencePulseCount; };
    algo.setReferenceStrobeDecay = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1.5;
      algo.referenceStrobeDecay = Math.max(1, Math.min(10, value));
    };
    algo.getReferenceStrobeDecay = function() { return algo.referenceStrobeDecay; };
    algo.setReferenceBeatDecay = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 2;
      algo.referenceBeatDecay = Math.max(0, Math.min(10, value));
    };
    algo.getReferenceBeatDecay = function() { return algo.referenceBeatDecay; };
    algo.setReferencePattern = function(v) {
      algo.referencePattern = ["****", "*.*.", ".*.*", "*...", "...*"].indexOf(v) >= 0 ? v : "****";
    };
    algo.getReferencePattern = function() { return algo.referencePattern; };
    algo.setReferenceFlip = function(v) { algo.referenceFlip = v === "On" ? "On" : "Off"; };
    algo.getReferenceFlip = function() { return algo.referenceFlip; };
    algo.setReferenceBackgroundMode = function(v) {
      algo.referenceBackgroundMode = v === "Additive" ? "Additive" : "Off";
    };
    algo.getReferenceBackgroundMode = function() { return algo.referenceBackgroundMode; };
    algo.setReferenceBackgroundColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
        algo.referenceBackgroundColor = v;
    };
    algo.getReferenceBackgroundColor = function() { return algo.referenceBackgroundColor; };
    algo.setReferenceBackgroundBrightness = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1;
      algo.referenceBackgroundBrightness = HSVUtil.clamp01(value);
    };
    algo.getReferenceBackgroundBrightness = function() { return algo.referenceBackgroundBrightness; };

    function parseReferenceGradient(value) {
      var raw = String(value).trim();
      if (!raw) return null;
      var parts = raw.split("|");
      var palettePart = parts[0];
      var positionPart = parts.length > 1 ? parts[1] : "";
      var tokens = palettePart.split(",");
      var stops = [];
      for (var i = 0; i < tokens.length; i++) {
        var token = String(tokens[i]).trim();
        if (!/^#[0-9a-f]{6}$/i.test(token)) return null;
        stops.push(token.toLowerCase());
      }
      if (!stops.length) return null;
      if (positionPart) {
        var parsed = HSVUtil.parsePositions(positionPart, stops.length);
        if (!parsed) return null;
        positionPart = parsed.join(",");
      }
      return { palette: stops.join(","), positions: positionPart };
    }

    function parseReferenceStops() {
      var colors = algo.referencePalette.split(",");
      var stops = [];
      for (var i = 0; i < colors.length; i++) {
        var rgb = HSVUtil.parseHexRgb(colors[i]);
        var hsv = HSVUtil.rgbToHsv(rgb[0], rgb[1], rgb[2]);
        stops.push({h: hsv.h, s: hsv.s, v: hsv.v});
      }
      if (!stops.length) stops = [{h: 0, s: 0, v: 0}];
      return stops;
    }

    function referenceOutput(pixels, width, height) {
        var n = pixels.length;
        var map = HSVUtil.createMap(width, height);
        for (var i = 0; i < n; i++) {
            var hsv = HSVUtil.rgbToHsvUnclipped(pixels[i][0], pixels[i][1], pixels[i][2]);
            map[i * 3] = hsv.h;
            map[i * 3 + 1] = hsv.s;
            map[i * 3 + 2] = hsv.v;
        }
        HSVUtil.applyStripTransforms(map, width, height, {
          flip: algo.referenceFlip,
          mirror: algo.referenceMirror,
          backgroundMode: algo.referenceBackgroundMode,
          backgroundColor: algo.referenceBackgroundColor,
          backgroundBrightness: algo.referenceBackgroundBrightness,
          brightness: algo.referenceBrightness,
          blur: algo.referenceBlur
        });
        if (algo.referenceDiagnostics) {
          var transformed = new Array(n);
          for (var j = 0; j < n; j++) {
            var o = j * 3;
            transformed[j] = HSVUtil.hsvToRgb(map[o], map[o + 1], map[o + 2]);
          }
          algo.referenceFrame = {pre: pixels, transformed: transformed};
        }
        return map;
    }

    function referenceMap(width, height, audio) {
        var n = width * height;
        var epoch = audio ? [
            audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision,
            algo.referencePalette, algo.referencePositions
        ].join(":") : "";
        if (!referenceState || referenceState.n !== n || referenceState.epoch !== epoch)
            referenceState = {n: n, epoch: epoch, rollPhase: 0};
        var phase = audio && audio.tempo ? HSVUtil.mod1(audio.tempo.beatPhase || 0) : 0;
        var bar = audio && audio.tempo ? HSVUtil.mod1(audio.tempo.barPhase || 0) : 0;
        var seconds = HSVUtil.audioSeconds(audio);
        if (seconds > 0)
            referenceState.rollPhase = HSVUtil.mod1(referenceState.rollPhase + seconds * algo.referenceRoll);
        var validClock = !!(audio && audio.tempo && audio.tempo.valid);
        if (algo.referenceClock === "Detected" && !validClock)
            return referenceOutput(new Array(n).fill([0, 0, 0]), width, height);

        var pulseCount = algo.referencePulseCount;
        var pulsePeriod = 1 / pulseCount;
        var pulse = ((-phase) % pulsePeriod + pulsePeriod) % pulsePeriod;
        pulse *= pulseCount;
        var base = Math.pow(pulse, algo.referenceStrobeDecay) * Math.pow(1 - phase, algo.referenceBeatDecay);
        var beatIndex = Math.floor(bar * 4) % 4;
        var maskOn = algo.referencePattern.charAt(beatIndex) === "*";
        var amplitude = maskOn ? base * base : 0;

        var stops = parseReferenceStops();
        var rgb = HSVUtil.gradientRgbAt(stops, HSVUtil.mod1(bar + referenceState.rollPhase), algo.referencePositions);
        var color = [rgb[0] * amplitude, rgb[1] * amplitude, rgb[2] * amplitude];
        return referenceOutput(new Array(n).fill(color), width, height);
    }

    var DOMINANT_TINT = 0.5;
    var DEFAULT_HSV_STOPS = [
        { h: 0.000, s: 1.0, v: 1.0 },
        { h: 0.884, s: 1.0, v: 1.0 },
        { h: 0.667, s: 1.0, v: 1.0 }
    ];
    var BASS_REFRACTORY_MS = 200;

    algo.color_step = 0.0625;
    algo.bass_strobe_decay_rate = 0.5;
    algo.strobe_color = "#ffffff";
    algo.strobe_width = 10;
    algo.strobe_decay_rate = 0.5;
    algo.color_shift_delay = 1.0;

    algo.properties.push("name:color_step|type:float|display:Color Step|write:setColorStep|read:getColorStep");
    algo.properties.push("name:bass_strobe_decay_rate|type:float|display:Bass Strobe Decay Rate|write:setBassStrobeDecayRate|read:getBassStrobeDecayRate");
    algo.properties.push("name:strobe_color|type:string|display:Percussion Color (#rrggbb)|write:setStrobeColor|read:getStrobeColor");
    algo.properties.push("name:strobe_width|type:range|display:Strobe Width|values:0,1000|write:setStrobeWidth|read:getStrobeWidth");
    algo.properties.push("name:strobe_decay_rate|type:float|display:Strobe Decay Rate|write:setStrobeDecayRate|read:getStrobeDecayRate");
    algo.properties.push("name:color_shift_delay|type:float|display:Color Shift Delay|write:setColorShiftDelay|read:getColorShiftDelay");

    algo.presetBassTrigger = "Volume Beat";
    algo.properties.push("name:bass_trigger|type:list|display:Bass Trigger|values:Volume Beat,Tempo Beat,Onset,Kick|write:setBassTrigger|read:getBassTrigger");
    algo.setBassTrigger = function(v) {
      var valid = ["Volume Beat", "Tempo Beat", "Onset", "Kick"];
      algo.presetBassTrigger = valid.indexOf(v) >= 0 ? v : "Volume Beat";
    };
    algo.getBassTrigger = function() { return algo.presetBassTrigger; };

    algo.setColorStep = function(v) { algo.color_step = parseFloat(v); };
    algo.getColorStep = function() { return algo.color_step; };
    algo.setBassStrobeDecayRate = function(v) { algo.bass_strobe_decay_rate = parseFloat(v); };
    algo.getBassStrobeDecayRate = function() { return algo.bass_strobe_decay_rate; };
    algo.setStrobeColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
        algo.strobe_color = v.toLowerCase();
    };
    algo.getStrobeColor = function() { return algo.strobe_color; };
    algo.setStrobeWidth = function(v) { algo.strobe_width = parseFloat(v); };
    algo.getStrobeWidth = function() { return algo.strobe_width; };
    algo.setStrobeDecayRate = function(v) { algo.strobe_decay_rate = parseFloat(v); };
    algo.getStrobeDecayRate = function() { return algo.strobe_decay_rate; };
    algo.setColorShiftDelay = function(v) { algo.color_shift_delay = parseFloat(v); };
    algo.getColorShiftDelay = function() { return algo.color_shift_delay; };

    var strobeOverlay = [];
    var bassStrobeOverlay = [];
    var percussiveStrobeOverlay = [];
    var percussiveBassOverlay = [];
    var onsetsQueued = 0;
    var elapsedMs = 0;
    var lastColorShiftMs = 0;
    var lastStrobeMs = 0;
    var lastBassStrobeMs = 0;
    var colorIdx = 0;
    var bassStrobeColor = {h: 0, s: 0, v: 0};
    var bassStrobeColorRgb = [0, 0, 0];
    var lastWidth = 0;
    runtimeGeometryKey = "";
    runtimeIdentityKey = "";
    var lastEventFrameSignature = "";

    function zeroStrip(n) {
        var out = new Array(n);
        for (var i = 0; i < n; i++) out[i] = {h: 0, s: 0, v: 0};
        return out;
    }

    function gradientStops() {
        return (algo.colors && algo.colors.length > 0)
            ? algo.colors : DEFAULT_HSV_STOPS;
    }

    function resetDynamicState(width) {
        strobeOverlay = zeroStrip(width);
        bassStrobeOverlay = zeroStrip(width);
        percussiveStrobeOverlay = new Array(width).fill(0).map(function() { return [0, 0, 0]; });
        percussiveBassOverlay = new Array(width).fill(0).map(function() { return [0, 0, 0]; });
        onsetsQueued = 0;
        elapsedMs = 0;
        lastColorShiftMs = 0;
        lastStrobeMs = 0;
        lastBassStrobeMs = 0;
        colorIdx = 0;
        bassStrobeColor = HSVUtil.gradientAt(gradientStops(), colorIdx);
        bassStrobeColorRgb = HSVUtil.hsvToRgb(bassStrobeColor.h, bassStrobeColor.s, bassStrobeColor.v);
        lastEventFrameSignature = "";
    }

    function ensure(width, height, audio) {
        var geometryKey = width + "x" + height;
        var previousIdentity = runtimeIdentityKey;
        var nextIdentity = audio ? HSVUtil.audioIdentityKey(audio, previousIdentity) : previousIdentity;
        var geometryChanged = runtimeGeometryKey !== geometryKey || lastWidth !== width || strobeOverlay.length !== width;
        var identityChanged = nextIdentity !== runtimeIdentityKey;
        if (geometryChanged || identityChanged)
            resetDynamicState(width);
        runtimeIdentityKey = nextIdentity;
        runtimeGeometryKey = geometryKey;
        lastWidth = width;
    }

    function scaleInPlace(strip, factor) {
        for (var i = 0; i < strip.length; i++)
            strip[i].v *= factor;
    }

    function scaleRgbInPlace(strip, factor) {
        for (var i = 0; i < strip.length; i++) {
            strip[i][0] *= factor;
            strip[i][1] *= factor;
            strip[i][2] *= factor;
        }
    }

    function consumedEventDeltas(audio) {
        var zero = {onset: 0, kick: 0, beat: 0, bar: 0};
        var delta = audio && audio.events && audio.events.delta ? audio.events.delta : null;
        if (!delta) return zero;
        if (!isFinite(audio.frameSequence)) {
            return {
                onset: Math.max(0, Math.floor(delta.onset || 0)),
                kick: Math.max(0, Math.floor(delta.kick || 0)),
                beat: Math.max(0, Math.floor(delta.beat || 0)),
                bar: Math.max(0, Math.floor(delta.bar || 0))
            };
        }
        var signature = [
            audio.sourceId, audio.profileId, audio.sourceEpoch, audio.configRevision, audio.frameSequence
        ].join(":");
        if (signature === lastEventFrameSignature) return zero;
        lastEventFrameSignature = signature;
        return {
            onset: Math.max(0, Math.floor(delta.onset || 0)),
            kick: Math.max(0, Math.floor(delta.kick || 0)),
            beat: Math.max(0, Math.floor(delta.beat || 0)),
            bar: Math.max(0, Math.floor(delta.bar || 0))
        };
    }

    algo.rgbMapStepCount = function(width, height) { return 1; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step, audio) {
        if (algo.presetMode === "Reference") return referenceMap(width, height, audio);
        ensure(width, height, audio);
        var map = HSVUtil.createMap(width, height);
        if (!audio) return map;
        var percussiveMode = algo.presetMode === "Percussive RGB";
        var deltas = consumedEventDeltas(audio);
        var seconds = HSVUtil.audioSeconds(audio);
        var frameScale = seconds * 50;
        elapsedMs += seconds * 1000;

        if (elapsedMs - lastColorShiftMs > Math.max(0, Math.min(1, parseFloat(algo.color_shift_delay))) * 1000.0) {
            colorIdx = (colorIdx + Math.max(0, Math.min(0.25, parseFloat(algo.color_step)))) % 1.0;
            bassStrobeColor = HSVUtil.gradientAt(gradientStops(), colorIdx);
            bassStrobeColorRgb = HSVUtil.hsvToRgb(bassStrobeColor.h, bassStrobeColor.s, bassStrobeColor.v);
            lastColorShiftMs = elapsedMs;
        }

        var bassDecay = 1.0 - Math.max(0, Math.min(1, parseFloat(algo.bass_strobe_decay_rate)));
        if (percussiveMode) {
            var kickHit = audio.version >= 6
                ? (deltas.kick > 0 || !!audio.kickFired)
                : !!audio.beatFired;
            if (kickHit && elapsedMs - lastBassStrobeMs > BASS_REFRACTORY_MS && bassDecay > 0) {
                for (var b = 0; b < width; b++)
                    percussiveBassOverlay[b] = bassStrobeColorRgb.slice();
                lastBassStrobeMs = elapsedMs;
            }

            if (deltas.onset > 0) onsetsQueued += deltas.onset;
            else if (!audio.events && audio.onset && elapsedMs - lastStrobeMs > 0) onsetsQueued++;
            if (seconds > 0 && onsetsQueued > 0) {
                onsetsQueued--;
                var strobeWidth = Math.min(Math.max(0, Math.min(1000, algo.strobe_width)), width);
                if (strobeWidth > 0) {
                    var lengthDiff = width - strobeWidth;
                    var position = lengthDiff === 0 ? 0 : Math.floor(Math.random() * (width - strobeWidth));
                    var color = HSVUtil.parseHexRgb(algo.strobe_color || "#ffffff");
                    for (var s = position; s < position + strobeWidth; s++)
                        percussiveStrobeOverlay[s] = color.slice();
                }
                lastStrobeMs = elapsedMs;
            }

            for (var x = 0; x < width; x++) {
                var rgbMix = [
                    percussiveBassOverlay[x][0] + percussiveStrobeOverlay[x][0],
                    percussiveBassOverlay[x][1] + percussiveStrobeOverlay[x][1],
                    percussiveBassOverlay[x][2] + percussiveStrobeOverlay[x][2]
                ];
                var hsvMix = HSVUtil.rgbToHsv(rgbMix[0], rgbMix[1], rgbMix[2]);
                for (var y = 0; y < height; y++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = hsvMix.h;
                    map[i3 + 1] = hsvMix.s;
                    map[i3 + 2] = hsvMix.v;
                }
            }
            scaleRgbInPlace(percussiveStrobeOverlay, Math.pow(1.0 - Math.max(0, Math.min(1, parseFloat(algo.strobe_decay_rate))), frameScale));
            scaleRgbInPlace(percussiveBassOverlay, Math.pow(bassDecay, frameScale));
            return map;
        }

        var bassTriggerFired = false;
        if (algo.presetBassTrigger === "Tempo Beat") bassTriggerFired = !!audio.beatFired || deltas.beat > 0;
        else if (algo.presetBassTrigger === "Onset") bassTriggerFired = !!audio.onset || deltas.onset > 0;
        else if (algo.presetBassTrigger === "Kick") bassTriggerFired = audio.version >= 6 ? (!!audio.kickFired || deltas.kick > 0) : !!audio.beatFired;
        else bassTriggerFired = audio.version >= 6 ? (!!audio.kickFired || deltas.kick > 0) : !!audio.onset;
        if (bassTriggerFired && elapsedMs - lastBassStrobeMs > BASS_REFRACTORY_MS && bassDecay) {
            for (var b = 0; b < width; b++)
                bassStrobeOverlay[b] = {h: bassStrobeColor.h, s: bassStrobeColor.s, v: bassStrobeColor.v};
            lastBassStrobeMs = elapsedMs;
        }

        if (deltas.onset > 0) {
            onsetsQueued += deltas.onset;
            lastStrobeMs = elapsedMs;
        } else if (!audio.events && audio.onset && elapsedMs - lastStrobeMs > 0) {
            onsetsQueued++;
            lastStrobeMs = elapsedMs;
        }

        while (onsetsQueued > 0) {
            onsetsQueued--;
            var strobeWidth = Math.min(Math.max(0, Math.min(1000, algo.strobe_width)), width);
            var lengthDiff = width - strobeWidth;
            var position = lengthDiff === 0 ? 0 : Math.floor(Math.random() * (width - strobeWidth));
            var bandColor = (algo.colors && algo.colors.length >= 3) ? algo.colors[0] : {h: 0, s: 0, v: 1};
            var domIdx = (audio.mid > audio.low && audio.mid >= audio.high) ? 1 : (audio.high > audio.low) ? 2 : 0;
            var domPower = Math.max(audio.low, audio.mid, audio.high);
            var domHsv = (domPower >= 0.05 && algo.colors && algo.colors.length >= 3) ? algo.colors[domIdx] : bandColor;
            var t2 = DOMINANT_TINT;
            var scol = {
                h: bandColor.h + (domHsv.h - bandColor.h) * t2,
                s: bandColor.s + (domHsv.s - bandColor.s) * t2,
                v: bandColor.v + (domHsv.v - bandColor.v) * t2
            };
            for (var s = position; s < position + strobeWidth; s++)
                strobeOverlay[s] = {h: scol.h, s: scol.s, v: scol.v};
        }

        for (var x = 0; x < width; x++) {
            var bv = bassStrobeOverlay[x].v;
            var sv = strobeOverlay[x].v;
            var totalV = bv + sv;
            if (totalV > 0.001) {
                var h, sat;
                if (sv > bv) {
                    h = strobeOverlay[x].h; sat = strobeOverlay[x].s;
                    var t = bv / totalV;
                    var dh = bassStrobeOverlay[x].h - h;
                    if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                    h += t * dh;
                    h = h - Math.floor(h);
                    sat = sat * (1 - t) + bassStrobeOverlay[x].s * t;
                } else {
                    h = bassStrobeOverlay[x].h; sat = bassStrobeOverlay[x].s;
                    if (sv > 0.001) {
                        var t = sv / totalV;
                        var dh = strobeOverlay[x].h - h;
                        if (dh > 0.5) dh -= 1; else if (dh < -0.5) dh += 1;
                        h += t * dh;
                        h = h - Math.floor(h);
                        sat = sat * (1 - t) + strobeOverlay[x].s * t;
                    }
                }
                var v = Math.min(1, totalV);
                for (var y = 0; y < height; y++) {
                    var i3 = (y * width + x) * 3;
                    map[i3] = h; map[i3 + 1] = sat; map[i3 + 2] = v;
                }
            }
        }

        scaleInPlace(strobeOverlay, Math.pow(1.0 - Math.max(0, Math.min(1, parseFloat(algo.strobe_decay_rate))), frameScale));
        scaleInPlace(bassStrobeOverlay, Math.pow(bassDecay, frameScale));
        return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
