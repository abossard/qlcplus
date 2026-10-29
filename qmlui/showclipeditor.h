/*
  Q Light Controller Plus
  showclipeditor.h

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

#ifndef SHOWCLIPEDITOR_H
#define SHOWCLIPEDITOR_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class Doc;
class QQuickView;
class ShowClipAutomation;
class ShowFunction;
class ShowManager;

/**
 * The Show Manager's view and edits of clip automation, and the way into a
 * nested Show and back.
 *
 * A Show clip, and any clip that carries automation, starts its Function at
 * its in-point: trimming its left edge moves the in-point and re-bases the
 * automation, so the clip plays the same song positions, levels and
 * dominance at the same Show times. Other clips trim as before. Edits are
 * refused while the Show Manager is read-only (Perform); a trim is one undo
 * step that names its Show.
 *
 * Opening a nested Show remembers the Show it was opened from. Any other
 * change of the shown Show forgets the way back; a deleted Show leaves it.
 */
class ShowClipEditor final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY pathChanged)
    Q_PROPERTY(QString parentShowName READ parentShowName NOTIFY pathChanged)

public:
    ShowClipEditor(QQuickView *view, Doc *doc, ShowManager *manager);

    /** Trimming the left edge of sf moves its in-point */
    Q_INVOKABLE bool trimsInPoint(ShowFunction *sf) const;

    /** How far, in sf's own unit, its left edge may move earlier; -1 without limit */
    Q_INVOKABLE int startExtensionLimit(ShowFunction *sf) const;

    /** Move the left edge of sf to startTime, in its own unit, keeping its
     *  end. An extension stops at startExtensionLimit(). Refused while
     *  read-only, on overlap, when no time would be left and, for an
     *  in-point, while the Show plays: its runner keeps the automation it
     *  started the clip with */
    Q_INVOKABLE bool trimStart(ShowFunction *sf, int startTime);

    /** What to draw over sf, in its own unit from the clip start: boundaries
     *  (segment starts inside the clip), envelope ({clipMs, level}), shaded
     *  (spans {fromMs, toMs} it is not dominant) and inPoint (-1 without
     *  segments) */
    Q_INVOKABLE QVariantMap overlay(ShowFunction *sf) const;

    /** Show the Show that sf plays, back in the event loop, remembering the
     *  shown one. False when sf does not play a Show; a refused change of
     *  Show, or one the shown Show changed before, stays where it is */
    Q_INVOKABLE bool openNested(ShowFunction *sf);

    /** Show again the Show the current one was opened from */
    Q_INVOKABLE void goBack();

    bool canGoBack() const;
    QString parentShowName() const;

    /** The undo payload: a clip's Show, start, duration and automation as
     *  XML. Applying it sets them on that clip in that Show, whichever Show
     *  is shown; Gone when the Show or the clip is (history passes the step).
     *  History waits, like a trim, while the Show plays */
    enum class ClipEdit { Applied, Waits, Gone };
    static QString clipXml(quint32 showId, const ShowFunction *sf);
    static ClipEdit applyClip(Doc *doc, quint32 clipId, const QString &xml, bool history);

signals:
    void pathChanged();

private:
    /** Show showId, keeping the way back; false when refused */
    bool select(quint32 showId);

    /** sf's automation as the engine plays it: a function that is not a
     *  Show plays from the in-point on at rate 1 */
    ShowClipAutomation played(ShowFunction *sf) const;

    Doc *m_doc;
    ShowManager *m_manager;
    /** The Shows the shown one was opened from, outermost first */
    QVector<quint32> m_path;
    bool m_navigating = false;
};

#endif
