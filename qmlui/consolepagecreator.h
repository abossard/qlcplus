/*
  Q Light Controller Plus
  consolepagecreator.h

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

#ifndef CONSOLEPAGECREATOR_H
#define CONSOLEPAGECREATOR_H

#include <QObject>
#include <QRectF>
#include <QVariantList>

#include "pageplanner.h"

class VirtualConsole;
class QQuickView;
class Doc;

/** Creates Virtual Console pages that control a set of fixtures: one fixture,
 *  a group of one model or of several. Plans them with PagePlanner and builds
 *  them with PageBuilder on new pages at the end of the console. */
class ConsolePageCreator final : public QObject
{
    Q_OBJECT

public:
    ConsolePageCreator(Doc *doc, VirtualConsole *vc, QQuickView *view, QObject *parent = nullptr);

    /** Build pages for $fixtureIDs, as many as fit the visible console area.
     *  Several models get one page when it fits, else a page of what they
     *  share plus a page per model. All of it is one undo step.
     *  Returns the index of the first new page, -1 when nothing was built */
    Q_INVOKABLE int createPages(const QVariantList &fixtureIDs, const QString &title = QString());

    /** The area of a console page in the window, in console units */
    QRectF consoleArea() const;

private:
    /** Build $pages on new console pages, a page more for every part that
     *  does not fit $area. Returns the new page indices */
    QList<int> buildPages(const QList<PagePlanner::Page> &pages, const QRectF &area);

private:
    Doc *m_doc;
    VirtualConsole *m_vc;
    QQuickView *m_view;
};

#endif
