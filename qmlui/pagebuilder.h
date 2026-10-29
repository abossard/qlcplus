/*
  Q Light Controller Plus
  pagebuilder.h

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

#ifndef PAGEBUILDER_H
#define PAGEBUILDER_H

#include <QRectF>
#include <QSizeF>
#include <QSet>

#include "pageplanner.h"

class VirtualConsole;
class VCWidget;
class VCFrame;
class Doc;

/** Turns a planned page into Virtual Console widgets: a frame per section,
 *  arranged in a flow, every slot a slider, an XY pad, buttons or a matrix
 *  effect. Faders, pads and Click & Go pickers drive the channels directly;
 *  a button runs a Scene of its choice, a matrix effect an RGBMatrix over a
 *  fixture group. */
class PageBuilder final
{
public:
    struct Result
    {
        /** The section frames built */
        QList<VCWidget *> frames;
        /** How many sections of the page they are: the rest did not fit
         *  below them and goes on another console page */
        int sections = 0;
        /** The size the frames take */
        QSizeF size;
    };

    /** Build the sections of $page into $target inside $area (logical units
     *  of $target), in page order, as many as fit the area. A first section
     *  taller than the area is built anyway */
    static Result build(VirtualConsole *vc, Doc *doc, VCFrame *target,
                        const PagePlanner::Page &page, const QRectF &area);

    /** The functions and fixture groups of $doc, to tell later what a build made */
    struct Made
    {
        QSet<quint32> functions;
        QSet<quint32> groups;
    };
    static Made made(const Doc *doc);

    /** Delete the functions and fixture groups made since $before, recorded
     *  for undo like any deletion */
    static void deleteMadeSince(Doc *doc, const Made &before);
};

#endif
