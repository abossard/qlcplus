/*
  Q Light Controller Plus
  SliderGraph.js

  Copyright (c) QLC+ contributors

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
 * Recorded slider positions as step geometry for one drawing surface.
 * Pure: reads only its arguments.
 *
 * samples: {ids, times, positions} in stored (time, Order) order, positions 0..1
 * window: {left, right}, the surface's x range; xOf(ms): surface x of a time,
 * monotonic; plot: {top, height}, position 1 at the top; selected: {id: true}.
 *
 * A pixel column holding one sample gets a mark and a vertical step from the
 * previous value. A column holding more (several times, or a same-time tie)
 * gets one span from its minimum to its maximum, joined to the previous value,
 * no marks, and a span over just the selected values: count and order inside
 * it are not drawn. A column's value holds until the next sample's column, at
 * the position of its last sample; the last sample holds nothing.
 *
 * Returns {holds, verticals, marks, selectedSpans, simplified}, each list a
 * Float64Array of flat records: hold (x1, x2, y, selected), vertical (x, top,
 * bottom, selected), mark (x, y, selected), selected span (x, top, bottom);
 * selected is 1 or 0. Typed arrays keep the result off the JS heap, sized to
 * at most one record of a kind per drawn column.
 */
function stepGeometry(samples, window, xOf, plot, selected)
{
    var ids = samples.ids, times = samples.times, positions = samples.positions
    var n = times.length
    var firstColumn = Math.floor(window.left), lastColumn = Math.ceil(window.right) - 1

    function y(position) { return plot.top + (1 - position) * plot.height }
    // a sample on a whole-pixel right edge (the end of its object) is in the last column
    function column(i)
    {
        var x = xOf(times[i])
        return x <= window.right ? Math.min(Math.floor(x), lastColumn) : Math.floor(x)
    }
    function isSelected(i) { return selected[ids[i]] === true ? 1 : 0 }

    // the first sample at or after the window's first column
    var low = 0, high = n
    while (low < high)
    {
        var middle = (low + high) >> 1
        if (column(middle) < firstColumn)
            low = middle + 1
        else
            high = middle
    }

    // one record of each kind per drawn column, plus the hold entering the window
    var capacity = Math.max(0, Math.min(lastColumn - firstColumn + 1, n - low)) + 1
    var holds = new Float64Array(4 * capacity), verticals = new Float64Array(4 * capacity)
    var marks = new Float64Array(3 * capacity), selectedSpans = new Float64Array(3 * capacity)
    var holdEnd = 0, verticalEnd = 0, markEnd = 0, spanEnd = 0, simplified = false

    function addHold(from, to, i)
    {
        var x1 = Math.max(window.left, from), x2 = Math.min(window.right, to)
        if (x2 > x1)
        {
            holds[holdEnd++] = x1
            holds[holdEnd++] = x2
            holds[holdEnd++] = y(positions[i])
            holds[holdEnd++] = isSelected(i)
        }
    }
    function addVertical(x, top, bottom, chosen)
    {
        verticals[verticalEnd++] = x
        verticals[verticalEnd++] = top
        verticals[verticalEnd++] = bottom
        verticals[verticalEnd++] = chosen
    }

    var previous = low - 1
    if (previous >= 0 && low < n)
        addHold(column(previous) + 0.5, column(low) + 0.5, previous)

    // one projection per sample: c is sample i's column, next the column of sample end
    var i = low, c = i < n ? column(i) : 0
    while (i < n && c <= lastColumn)
    {
        var end = i + 1, next = end < n ? column(end) : 0
        while (end < n && next === c)
        {
            end++
            next = end < n ? column(end) : 0
        }
        var x = c + 0.5
        if (end - i === 1)
        {
            if (previous >= 0 && positions[previous] !== positions[i])
                addVertical(x, y(Math.max(positions[previous], positions[i])),
                            y(Math.min(positions[previous], positions[i])), isSelected(i))
            marks[markEnd++] = x
            marks[markEnd++] = y(positions[i])
            marks[markEnd++] = isSelected(i)
        }
        else
        {
            simplified = true
            var min = previous >= 0 ? positions[previous] : positions[i], max = min
            var selectedMin = Infinity, selectedMax = -Infinity
            for (var k = i; k < end; k++)
            {
                min = Math.min(min, positions[k])
                max = Math.max(max, positions[k])
                if (isSelected(k))
                {
                    selectedMin = Math.min(selectedMin, positions[k])
                    selectedMax = Math.max(selectedMax, positions[k])
                }
            }
            addVertical(x, y(max), y(min), 0)
            if (selectedMax >= selectedMin)
            {
                selectedSpans[spanEnd++] = x
                selectedSpans[spanEnd++] = y(selectedMax)
                selectedSpans[spanEnd++] = y(selectedMin)
            }
        }
        if (end < n)
            addHold(x, next + 0.5, end - 1)
        previous = end - 1
        i = end
        c = next
    }
    return { holds: holds.subarray(0, holdEnd), verticals: verticals.subarray(0, verticalEnd),
             marks: marks.subarray(0, markEnd), selectedSpans: selectedSpans.subarray(0, spanEnd),
             simplified: simplified }
}

/**
 * The recorded position range of the samples, {min, max}; null without
 * samples. Pure; one pass, so callers keep it per samples snapshot.
 */
function sampleRange(samples)
{
    var positions = samples.positions, n = positions.length
    if (n === 0)
        return null
    var min = positions[0], max = positions[0]
    for (var i = 1; i < n; i++)
    {
        min = Math.min(min, positions[i])
        max = Math.max(max, positions[i])
    }
    return { min: min, max: max }
}

/**
 * The sample nearest to time: the earlier one when two are as near, the last
 * in order among equal times. {id, time, position}, null without samples.
 * Pure; a binary search over the sorted times.
 */
function nearestSample(samples, time)
{
    var times = samples.times, n = times.length
    if (n === 0)
        return null
    var low = 0, high = n
    while (low < high)
    {
        var middle = (low + high) >> 1
        if (times[middle] < time)
            low = middle + 1
        else
            high = middle
    }
    var nearest = low === n ? n - 1
                : low === 0 ? 0
                : time - times[low - 1] <= times[low] - time ? low - 1 : low
    while (nearest + 1 < n && times[nearest + 1] === times[nearest])
        nearest++
    return { id: samples.ids[nearest], time: times[nearest], position: samples.positions[nearest] }
}
