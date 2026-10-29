/*
  Q Light Controller Plus
  pageplanner.h

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

#ifndef PAGEPLANNER_H
#define PAGEPLANNER_H

#include <QColor>
#include <QSize>
#include <QList>
#include <QString>

#include "fixturecontrols.h"

class Doc;

/** Plans console pages for a set of fixtures: which widgets, which
 *  channels each one drives, how they group into sections. Pure: reads the
 *  fixtures, creates nothing. One fixture or a hundred of mixed models go
 *  through the same path. */
class PagePlanner final
{
public:
    /** What to do when a group holds several models */
    enum Strategy
    {
        Intersection,  ///< Only the slots every model has, merged
        Union,         ///< Shared slots merged, the rest in a frame per model
        MasterAndModels ///< A page of the shared slots, then one page per model
    };

    /** What to do with the heads of a multi-head fixture */
    enum HeadMode
    {
        MergeHeads,   ///< One control drives the same slot of every head
        HeadStrips    ///< The merged control plus one control per head
    };

    struct Options
    {
        Strategy strategy = Union;
        HeadMode heads = HeadStrips;
        /** HeadStrips falls back to MergeHeads above this many heads */
        int maxHeadStrips = 16;
    };

    struct Channel
    {
        quint32 fixture;
        quint32 channel;
        bool operator==(const Channel &other) const
        { return fixture == other.fixture && channel == other.channel; }
    };

    struct Head
    {
        quint32 fixture;
        int head;
    };

    struct Value
    {
        quint32 fixture;
        quint32 channel;
        uchar value;
    };

    /** One meaning, like "Red" or "Shutter open", in the values each
     *  model needs for it */
    struct Choice
    {
        QString label;
        /** Invalid when the choice is not a colour */
        QColor color;
        /** A gobo picture, empty when there is none */
        QString picture;
        QList<Value> values;
    };

    /** One widget to be */
    struct Slot
    {
        /** Equal keys are the same control, whatever the model */
        QString key;
        QString label;
        FixtureControls::Kind kind;
        /** Every channel the widget drives: coarse channels and their fines */
        QList<Channel> channels;
        /** PanTilt only: the heads the pad moves */
        QList<Head> heads;
        /** Presets that mean the same on every model, matched by capability
         *  presets and colours rather than by DMX values. When set, the slot
         *  is a set of buttons and $channels is empty */
        QList<Choice> choices;
        /** Choices only: the one button flashes while it is held */
        bool flash = false;
        /** When valid: an RGB matrix effect over $heads in a grid of this size */
        QSize matrixGrid;
    };

    struct Section
    {
        QString title;
        /** The model of a part, empty for shared slots */
        QString model;
        QList<Slot> items;
        /** Per-head slots, laid out as a strip of small controls */
        bool strip = false;
        /** Strip only: heads per row, the width of the fixture's physical
         *  layout; 0 lets the strip flow */
        int columns = 0;
        /** The slots of this function only one model has, a part per model */
        QList<Section> parts;
    };

    struct Page
    {
        QString title;
        QList<Section> sections;
    };

    static QList<Page> plan(const Doc *doc, const QList<quint32> &fixtureIDs,
                            const QString &title, const Options &options);

    /** The section a slot key belongs to, in page order */
    static QString sectionOf(const QString &key);
};

#endif
