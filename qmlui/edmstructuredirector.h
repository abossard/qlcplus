/*
  Q Light Controller Plus
  edmstructuredirector.h

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

#ifndef EDMSTRUCTUREDIRECTOR_H
#define EDMSTRUCTUREDIRECTOR_H

#include "edmstructure.h"
#include "edmstructuremapping.h"

#include <QHash>
#include <QObject>

/** The look buttons the director presses: the Virtual Console, or a fake in tests */
class EdmLookSurface : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual bool isOn(quint32 widgetId) const = 0;
    /** Press or release; only called when the state must change */
    virtual void set(quint32 widgetId, bool on) = 0;
    /** The widgets whose state changes are reported */
    virtual void watch(const QList<quint32> &widgetIds) = 0;

signals:
    /** A watched widget changed state, whoever caused it */
    void changed(quint32 widgetId);
};

/** What the director remembers between events */
struct EdmLookState
{
    bool overridden = false;           // the DJ changed a look: hands off until the next trigger
};

/** What happened to the active deck */
struct EdmLookEvent
{
    bool activeDeck = false;
    bool sectionStart = false;
    bool jumped = false;               // the playhead went backwards
    bool sameSection = false;          // ... and landed in the section it was in: a loop, not a trigger
    bool becameActive = false;
    QString label;                     // the section now playing
    QList<quint32> mappedOn;           // the mapped look buttons that are on now
};

struct EdmLookDecision
{
    QList<quint32> release;            // released first: every other look that is on
    quint32 press = kNoLookWidget;
    EdmLookState state;
};

/** The look to show: calculation only. The mapped buttons are one group: the target is on, the rest off. */
EdmLookDecision edmLookDecide(const EdmStructureMapping &mapping, const EdmLookState &state, const EdmLookEvent &event);

/** The DJ changed a button the director did not touch */
EdmLookState edmLookExternalChange(const EdmStructureMapping &mapping, const EdmLookState &state, quint32 widgetId);

/**
 * Presses the Virtual Console look button mapped to the section the active
 * deck is in. A section start, a jump back and a change of the active deck
 * show that section's look; a button the DJ changes holds the director until
 * the next of those.
 */
class EdmStructureDirector : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool autoEnabled READ autoEnabled WRITE setAutoEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool overridden READ overridden NOTIFY statusChanged)
    Q_PROPERTY(int followedDeck READ followedDeck NOTIFY statusChanged)          // 0: none
    Q_PROPERTY(QString followedLabel READ followedLabel NOTIFY statusChanged)

public:
    EdmStructureDirector(EdmStructure *structure, EdmStructureMapping *mapping, EdmLookSurface *surface,
                         QObject *parent = nullptr);

    bool autoEnabled() const { return m_mapping->autoEnabled(); }
    void setAutoEnabled(bool enabled) { m_mapping->setAutoEnabled(enabled); }
    bool overridden() const { return m_state.overridden; }
    int followedDeck() const { return m_deck; }
    QString followedLabel() const { return m_label; }

signals:
    void statusChanged();

private slots:
    void slotDeckAdvanced(int deck, const EdmAnalysis &analysis, const EdmSongPosition &position,
                          const EdmCrossings &crossings, bool activeDeck);
    void slotDeckLost(int deck, bool activeDeck);
    void slotWidgetChanged(quint32 widgetId);

private:
    void apply(const EdmLookDecision &decision);

    EdmStructureMapping *m_mapping;
    EdmLookSurface *m_surface;
    EdmLookState m_state;
    QHash<quint32, bool> m_expected;   // widget -> the state the director asked for and has not seen yet
    QHash<quint32, bool> m_known;      // widget -> on when last reported; a report that keeps it is no change
    int m_deck = 0;
    int m_section = -1;                // the followed deck's section at the last event
    QString m_label;
};

#endif
