/*
  Q Light Controller Plus
  TimeUtils.js

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * Returns a string from the provided time in milliseconds.
 * It builds a stripped version of the time where
 * - hours are not displayed if equal to 0
 * - milliseconds are not displayed if equal to 0
 * The format is hh:mm:ss.xx
 */

function msToString(ms)
{
    var h = Math.floor(ms / 3600000);
    ms -= (h * 3600000);

    var m = Math.floor(ms / 60000);
    ms -= (m * 60000);

    var s = Math.floor(ms / 1000);
    ms -= (s * 1000);

    var finalTime = "";
    if (h) {
        finalTime += ((h < 10) ? "0" + h : h) + ":";
    }
    finalTime += ((m < 10) ? "0" + m : m) + ":";
    finalTime += ((s < 10) ? "0" + s : s);
    if (ms) {
        finalTime += ".";
        if (ms < 10) finalTime += "00";
        else if (ms < 100) finalTime += "0";

        finalTime += parseInt(ms);
    }
    return finalTime;
}

function beatsToString(beats, division)
{
    var bar = Math.floor(beats / division);
    var beat = Math.floor(beats - (bar * division));
    return "" + bar + "." + beat;
}

/**
 * Returns a string from the provided time in milliseconds,
 * considering the requested precision to display miliseconds.
 *
 * It always returns a full string in the format: hh:mm:ss.xx
 * Precision can be 1 (during playback) or 2 (when stopped)
 */
function msToStringWithPrecision(ms, precision)
{
    var h = Math.floor(ms / 3600000);
    ms -= (h * 3600000);

    var m = Math.floor(ms / 60000);
    ms -= (m * 60000);

    var s = Math.floor(ms / 1000);
    ms -= (s * 1000);

    var finalTime = "";
    finalTime += ((h < 10) ? "0" + h : h) + ":";
    finalTime += ((m < 10) ? "0" + m : m) + ":";
    finalTime += ((s < 10) ? "0" + s : s);
    if (precision === 1)
    {
        finalTime += "." + ((ms < 10) ? "0" : parseInt(ms / 100));
    }
    else
    {
        if (ms < 10) {
            finalTime += ".00";
        } else if (ms < 100) {
            finalTime += ".0" + parseInt(ms / 10);
        } else {
            finalTime += "." + parseInt(ms / 10);
        }
    }

    return finalTime;
}

/**
  * Returns a time value from the given string
  * in the QLC+ format.
  * Time example: 2h34m12s870ms
  * Beats example: 5 1/4
  * Returns -2 on infinite time string
  */
function qlcStringToTime(str, type)
{
    if (str === "" || str === "0") {
        return 0;
    } else if (str === "∞") {
        return -2;
    }
    var finalTime = 0;

    var currStr = "";

    if (type === 0 /*Function.Time */)
    {
        for (var i = 0; i < str.length; i++)
        {
            if (str[i] >= "0" && str[i] <= "9")
            {
                currStr += str[i];
            }
            else if (str[i] === "h")
            {
                console.log("Hours: " + currStr);
                finalTime += parseInt(currStr) * 1000 * 60 * 60;
                currStr = "";
            }
            else if (str[i] === "m" && str[i + 1] === "s")
            {
                console.log("Millisecs: " + currStr);
                finalTime += parseInt(currStr);
                break;
            }
            else if (str[i] === "m")
            {
                console.log("minutes: " + currStr);
                finalTime += parseInt(currStr) * 1000 * 60;
                currStr = "";
            }
            else if (str[i] === "s")
            {
                console.log("seconds: " + currStr);
                finalTime += parseInt(currStr) * 1000;
                currStr = "";
            }
        }
        if (finalTime === 0) {
            finalTime += parseInt(currStr);
        }
    }
    else if (type === 1 /* Function.Beats */)
    {
        var tokens = str.split(" ");

        // Handle fraction-only input like "14/16", "3/4", "1/16" (no whole part).
        // parseInt("1/16") returns 1, which would mis-parse the fraction as a whole beat.
        if (tokens[0].indexOf("/") >= 0)
        {
            var fParts = tokens[0].split("/");
            var fNum = parseInt(fParts[0]);
            var fDen = parseInt(fParts[1]);
            if (!isNaN(fNum) && fNum >= 0 && !isNaN(fDen)
                && (fDen === 1 || fDen === 2 || fDen === 4 || fDen === 8 || fDen === 16))
            {
                var sixteenths = fNum * (16 / fDen);
                var wholeBeats = Math.floor(sixteenths / 16);
                var remSixteenths = sixteenths - (wholeBeats * 16);
                // ms-per-sixteenth table (matches the ladder below: round(n * 62.5))
                var subBeatMs = [0, 63, 125, 188, 250, 313, 375, 438,
                                 500, 563, 625, 688, 750, 813, 875, 938];
                return wholeBeats * 1000 + subBeatMs[remSixteenths];
            }
            return NaN;
        }

        var wholePart = parseInt(tokens[0]);
        if (isNaN(wholePart) || wholePart < 0)
            return NaN;
        finalTime = wholePart * 1000;

        if (tokens.length > 1)
        {
            var frac = tokens[1];
            if (frac === "1/16") { finalTime += 63; }
            else if (frac === "1/8") { finalTime += 125; }
            else if (frac === "3/16") { finalTime += 188; }
            else if (frac === "1/4") { finalTime += 250; }
            else if (frac === "5/16") { finalTime += 313; }
            else if (frac === "3/8") { finalTime += 375; }
            else if (frac === "7/16") { finalTime += 438; }
            else if (frac === "1/2") { finalTime += 500; }
            else if (frac === "9/16") { finalTime += 563; }
            else if (frac === "5/8") { finalTime += 625; }
            else if (frac === "11/16") { finalTime += 688; }
            else if (frac === "3/4") { finalTime += 750; }
            else if (frac === "13/16") { finalTime += 813; }
            else if (frac === "7/8") { finalTime += 875; }
            else if (frac === "15/16") { finalTime += 938; }
        }
    }

    return finalTime;
}

function timeToQlcString(value, type)
{
    if (value === 0)
    {
        return "0";
    }
    else if (value === -2)
    {
        return "∞";
    }
    var timeString = "";

    if (type === 0 /* QLCFunction.Time */)
    {
        var h = Math.floor(value / 3600000);
        value -= (h * 3600000);

        var m = Math.floor(value / 60000);
        value -= (m * 60000);

        var s = Math.floor(value / 1000);
        value -= (s * 1000);

        //console.log("h: " + h + ", m: " + m + ", s: " + s + ", value: " + value)

        if (h)
        {
            timeString += h + "h";
        }
        if (m)
        {
            timeString += ((m < 10 && h) ? "0" + m : m) + "m";
        }
        if (s)
        {
            timeString += ((s < 10 && m) ? "0" + s : s) + "s";
        }

        if (value)
        {
            if (value < 10 && timeString.length)
            {
                timeString = timeString + "00" + value + "ms";
            }
            else if (value < 100 && timeString.length)
            {
                timeString = timeString + "0" + value + "ms";
            }
            else
            {
                timeString = timeString + value + "ms";
            }
        }
    }
    else if (type === 1 /* QLCFunction.Beats */)
    {
        if (value < 63)
        {
            return value;
        }

        var beats = Math.floor(value / 1000);
        if (beats > 0)
        {
            timeString = "" + beats;
        }
        value -= (beats * 1000);

        if (value === 63) { timeString += " 1/16"; }
        else if (value === 125) { timeString += " 1/8"; }
        else if (value === 188) { timeString += " 3/16"; }
        else if (value === 250) { timeString += " 1/4"; }
        else if (value === 313) { timeString += " 5/16"; }
        else if (value === 375) { timeString += " 3/8"; }
        else if (value === 438) { timeString += " 7/16"; }
        else if (value === 500) { timeString += " 1/2"; }
        else if (value === 563) { timeString += " 9/16"; }
        else if (value === 625) { timeString += " 5/8"; }
        else if (value === 688) { timeString += " 11/16"; }
        else if (value === 750) { timeString += " 3/4"; }
        else if (value === 813) { timeString += " 13/16"; }
        else if (value === 875) { timeString += " 7/8"; }
        else if (value === 938) { timeString += " 15/16"; }
        else if (value > 0) { timeString += " " + value; } // off-grid fallback
    }

    //console.log("Final time string: " + timeString)

    return timeString;
}

/**
  * Return a value in milliseconds, for the given
  * position in pixels and the given timescale,
  * where tickSize corresponds to 1 second on a 1.0 timescale factor
  */
function posToMs(x, timescale, tickSize)
{
    if (tickSize <= 0)
        return 0;
    // tickSize : 1000 * timescale = x : result
    return parseInt(x * (1000 * timescale) / tickSize);
}

/** Return a value in beats for the given
  * position in pixels
  */
function posToBeat(x, tickSize, beatsDivision)
{
    if (!beatsDivision)
        return 0
    return Math.round(x / (tickSize / beatsDivision)) * 1000
}

/** Return a value in milliseconds for the given position
  * translated into a beat-based timeline, considering
  * ticksize and BPM number and division
  */
function posToBeatMs(x, tickSize, bpmNumber, beatsDivision)
{
    if (tickSize <= 0 || bpmNumber <= 0 || beatsDivision <= 0)
        return 0;
    // (bpmNumber / beatsDivision) * tickSize : 60000 = x : currentTime
    return (x * 60000) / ((bpmNumber / beatsDivision) * tickSize);
}

/**
  * Return a value in pixels, for the given
  * time in milliseconds and the given timescale,
  * where tickSize corresponds to 1 second on a 1.0 timescale factor
  */
function timeToSize(time, timescale, tickSize)
{
    if (timescale <= 0)
        return 0;
    return ((time * tickSize) / 1000) / timescale;
}

/** Return a value in pixel, for a time
    based on BPM and the tick size */
function timeToBeatPosition(currentTime, tickSize, bpmNumber, beatsDivision)
{
    if (beatsDivision <= 0 || bpmNumber <= 0)
        return 0;
    // (bpmNumber / beatsDivision) * tickSize : 60000 = x : currentTime
    return (bpmNumber / beatsDivision) * tickSize * (currentTime / 60000);
}

function beatsToSize(time, tickSize, beatsDivision)
{
    if (!beatsDivision)
        return 0;
    return (tickSize / beatsDivision) * (time / 1000);
}

/**
  * Return a value in pixels representing
  * a time in milliseconds over a beat-based timeline
  * where tickSize corresponds to a bar (e.g. 2, 3 or 4 beats)
  */
function timeToBeatSize(time, bpmNumber, beatsDivision, tickSize)
{
    if (bpmNumber <= 0 || beatsDivision <= 0)
        return 0;
    var barDuration = (60000 / bpmNumber) * beatsDivision;
    // tickSize : barDuration = x : time
    return (tickSize * time) / barDuration;
}

/**
  * Return a value in pixels representing a "beats as ms" value
  * (1000 units per beat, as stored by a beat tempo Function/ShowFunction)
  * over a Time (milliseconds) based timeline, at the given BPM.
  */
function beatsToTimeSize(beatsMs, bpmNumber, timescale, tickSize)
{
    if (!bpmNumber)
        return 0;
    return timeToSize(beatsMsToMs(beatsMs, bpmNumber), timescale, tickSize);
}

/** A "beats as ms" value (1000 units per beat) in real milliseconds at the given BPM */
function beatsMsToMs(beatsMs, bpmNumber)
{
    return (beatsMs / 1000) * (60000 / bpmNumber);
}

/** Real milliseconds as "beats as ms" (1000 units per beat) at the given BPM */
function msToBeatsMs(ms, bpmNumber)
{
    return ms * bpmNumber / 60;
}

/**
  * Inverse of beatsToTimeSize: return a "beats as ms" value (1000 units
  * per beat) for the given pixel position over a Time (milliseconds)
  * based timeline, at the given BPM.
  */
function posToBeatsMsOnTimeline(x, timescale, tickSize, bpmNumber)
{
    if (!bpmNumber)
        return 0;
    var realMs = posToMs(x, timescale, tickSize);
    return (realMs / (60000 / bpmNumber)) * 1000;
}


/**
  * The musical grid of a Show: the length of a beat in milliseconds, the
  * beats of a bar and the time of a downbeat. VDJ Beat uses the song's beat
  * grid, the BPM markers the Show's tempo and meter from zero. null when the
  * Show has no tempo or meter.
  */
function musicalGrid(vdjBeat, vdjGridValid, vdjBeatPeriodMs, vdjGridAnchorMs, bpmNumber, beatsDivision)
{
    if (vdjBeat)
        return (vdjGridValid && vdjBeatPeriodMs > 0)
                ? { beatMs: vdjBeatPeriodMs, beatsPerBar: 4, anchorMs: vdjGridAnchorMs } : null
    if (bpmNumber > 0 && beatsDivision > 0)
        return { beatMs: 60000 / bpmNumber, beatsPerBar: beatsDivision, anchorMs: 0 }
    return null
}

/** The display and seek bound: the ruler, the cursor and requestSeek are int */
var selectionNavigationBound = 2147483647

/** The owner's selection as plain times and, when the snapshot carries a view,
  * as pixels: { starts[] ms, maxRuler, left, right, tail, rows } or a reason */
function selectionNavigationTarget(snapshot)
{
    var ruler = snapshot.ruler
    var none = { reason: qsTr("Select clips or recorded events first") }
    var tempo = { reason: qsTr("Beat-based items need a Show tempo") }
    if (snapshot.showId < 0 || snapshot.domain === "none")
        return none
    if (!ruler.msRuler && !(ruler.bpm > 0))
        return tempo
    var toRuler = function(ms) { return ruler.msRuler ? ms : msToBeatsMs(ms, ruler.bpm) }
    var target = { starts: [], maxRuler: 0, left: Infinity, right: -Infinity, tail: 0, rows: null }
    if (snapshot.domain === "recordings")
    {
        var samples = snapshot.recordings.samples
        if (samples.length === 0)
            return none
        samples.forEach(function(sample) {
            target.starts.push(sample.ms)
            target.maxRuler = Math.max(target.maxRuler, toRuler(sample.ms))
            target.left = Math.min(target.left, sample.x)
            target.right = Math.max(target.right, sample.x)
        })
        target.tail = snapshot.recordings.tailPx || 0
        target.rows = snapshot.recordings.rows || null
        return target
    }
    var items = snapshot.clips.items
    if (items.length === 0)
        return none
    if (items.some(function(item) { return item.isBeats && !(ruler.bpm > 0) }))
        return tempo
    var earliest = null
    items.forEach(function(item) {
        var ms = function(value) { return item.isBeats ? beatsMsToMs(value, ruler.bpm) : value }
        var start = ms(item.startTime)
        target.starts.push(start)
        target.maxRuler = Math.max(target.maxRuler, ruler.msRuler || !item.isBeats
            ? toRuler(ms(item.startTime + item.duration)) : item.startTime + item.duration)
        target.left = Math.min(target.left, item.x)
        target.right = Math.max(target.right, item.x + item.width)
        if (item.y !== undefined)
        {
            var rows = target.rows
            target.rows = rows === null ? { y: item.y, height: item.height }
                : { y: Math.min(rows.y, item.y), height: Math.max(rows.y + rows.height, item.y + item.height)
                                                          - Math.min(rows.y, item.y) }
            if (earliest === null || start < earliest.start)
                earliest = { start: start, y: item.y }
        }
    })
    if (target.rows !== null)
        target.rows.firstRowY = earliest.y
    return target
}

/**
  * The plan of the two selection navigation actions for a plain snapshot of the
  * editor: Fit (a view: scale, offset, vertical scroll and ruler reach) and Go
  * (a seek time in ms), each available or with the reason it is not. Pure: equal
  * snapshots give equal plans. Fit numbers are null when the snapshot has no view.
  */
function planSelectionNavigation(snapshot)
{
    var bound = selectionNavigationBound
    var ruler = snapshot.ruler
    var target = selectionNavigationTarget(snapshot)
    var fit = { available: false, reason: target.reason || "", timeScale: null, xViewOffset: null,
                contentY: null, rulerEnd: null }
    var go = { available: false, reason: target.reason || "", ms: null }
    if (!target.reason)
    {
        var start = Math.min.apply(null, target.starts)
        if (snapshot.access.readOnly)
            // PerformFsm: Live 2, Suspended 3 follow the external clock
            go.reason = snapshot.access.performState === 2 || snapshot.access.performState === 3
                ? qsTr("Following external clock (Perform)") : qsTr("Read only in Perform mode")
        else if (start > bound)
            go.reason = qsTr("Selection start is beyond the seekable range")
        else
            go = { available: true, reason: "", ms: start }

        if (target.maxRuler > bound)
            fit.reason = qsTr("Selection is beyond the timeline display range")
        else
            fit.available = true
    }
    if (fit.available && snapshot.view)
    {
        var view = snapshot.view
        var r = view.unitPx
        var minUnits = !ruler.msRuler ? Math.max(1, ruler.beatsDivision) * 1000
            : ruler.vdjBeat && ruler.vdjGridValid && ruler.vdjBeatPeriodMs > 0 ? ruler.vdjBeatPeriodMs * 4 : 2000
        var span = target.right - target.left
        var pad = Math.max(0.1 * span, (minUnits * r - span) / 2)
        var avail = Math.max(1, view.width - target.tail)
        var k0 = avail / (span + 2 * pad)
        var scale = Math.fround(Math.max(view.minScale, ruler.msRuler ? view.timeScale / k0 : view.timeScale * k0))
        var k = ruler.msRuler ? view.timeScale / scale : scale / view.timeScale
        var rulerEnd = Math.min(bound, Math.max(view.revealedRulerEnd,
            Math.ceil((target.right + pad) / r + target.tail / (r * k))))
        var contentWidth = (Math.min(bound, Math.max(view.rulerDuration, rulerEnd)) + 300000) * r * k
        fit.timeScale = scale
        fit.rulerEnd = rulerEnd
        // a window narrower than the view (zoom-in floor) is centred; a wider one keeps its start
        var centring = Math.min(0, ((span + 2 * pad) * k - avail) / 2)
        fit.xViewOffset = Math.max(0, Math.min((target.left - pad) * k + centring, contentWidth - view.width))
        var rows = target.rows, y = view.y, maxY = Math.max(0, view.contentHeight - view.height)
        if (rows && !(rows.y >= y && rows.y + rows.height <= y + view.height))
            y = rows.height > view.height ? rows.firstRowY
                : rows.y < y ? rows.y : rows.y + rows.height - view.height
        fit.contentY = Math.max(0, Math.min(y, maxY))
    }
    return Object.freeze({ fit: Object.freeze(fit), go: Object.freeze(go) })
}

/** Whether the owner's selection is inside the view of a snapshot taken after a Fit:
  * view.x is the lanes' observed scroll, view.headerX the ruler's, and they must agree */
function selectionNavigationFitResult(snapshot)
{
    var target = selectionNavigationTarget(snapshot)
    var view = snapshot.view
    if (target.reason)
        return Object.freeze({ ok: false, reason: target.reason })
    if (view.headerX !== undefined && Math.abs(view.headerX - view.x) > 0.5)
        return Object.freeze({ ok: false, reason: qsTr("The lanes and the ruler are not aligned") })
    if (target.left < view.x - 0.5 || target.right + target.tail > view.x + view.width + 0.5)
        return Object.freeze({ ok: false, reason: qsTr("Selection is wider than the timeline at maximum zoom out") })
    var rows = target.rows
    if (rows && (view.height <= 0 || rows.y < view.y - 0.5 || rows.y + rows.height > view.y + view.height + 0.5))
        return Object.freeze({ ok: false, reason: qsTr("Selected rows cannot be shown at this editor height") })
    return Object.freeze({ ok: true, reason: "" })
}

/**
 * Return the average time between two taps given by a list of tap times.
 * It caculates the linear regression of the recorded tap times. The slope of the resulting 
 * linear function represents the average time between two taps.
 */
function calculateBPMByTapIntervals(tapHistory)
{
    var tapHistorySorted = []

    // reduce size to only 16 taps
    while (tapHistory.length > 16) tapHistory.splice(0,1)

    // copy tap history to sort it
    tapHistorySorted = tapHistory.slice()
    tapHistorySorted.sort()

    // Find the median time between taps, assume that the tempo is +-40% of this
    var tapHistoryMedian = tapHistorySorted[Math.floor(tapHistorySorted.length/2)]
    
    // init needed variables
    var n = 1, tapx = 0, tapy = 0, sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0
    
    for (var i = 0; i < tapHistory.length; i++)
    {
        var intervalMs = tapHistory[i]
        n++
        // Divide by tapHistoryMedian to determine if a tap was skipped during input
        tapx += Math.floor((tapHistoryMedian/2 + intervalMs) / tapHistoryMedian)
        tapy += intervalMs
        sum_x += tapx
        sum_y += tapy
        sum_xx += tapx * tapx
        sum_xy += tapx * tapy                 
    }

    return (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x)
}
