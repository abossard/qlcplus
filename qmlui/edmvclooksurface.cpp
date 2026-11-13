/*
  Q Light Controller Plus
  edmvclooksurface.cpp

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

#include "edmvclooksurface.h"
#include "virtualconsole.h"
#include "vcbutton.h"
#include "showcommandrecorder.h"

EdmVcLookSurface::EdmVcLookSurface(VirtualConsole *console, QObject *parent)
    : EdmLookSurface(parent)
    , m_console(console)
{
}

VCButton *EdmVcLookSurface::button(quint32 widgetId) const
{
    VCButton *b = qobject_cast<VCButton *>(m_console->widget(widgetId));
    return b != nullptr && b->actionType() == VCButton::Toggle ? b : nullptr;
}

bool EdmVcLookSurface::isOn(quint32 widgetId) const
{
    // a look a Collection or a cue list runs (Monitoring) is on as well
    const VCButton *b = button(widgetId);
    return b != nullptr && b->state() != VCButton::Inactive;
}

void EdmVcLookSurface::set(quint32 widgetId, bool on)
{
    // no press while the console is edited or on a disabled button; a release always goes
    // through. A button on a frame page not shown is still a look.
    VCButton *b = button(widgetId);
    if (b == nullptr || (on && (m_console->editMode() || b->isDisabled())))
        return;
    // not the DJ's input: the recorder records none of it, the debug panel shows it
    ShowCommandRecorder::traceDirectRequest(b, on ? 1 : 0);
    // the desired state, whoever runs the look now; a Toggle press would flip it
    b->applyRecordedState(on);
}

void EdmVcLookSurface::watch(const QList<quint32> &widgetIds)
{
    for (const QMetaObject::Connection &c : m_connections)
        disconnect(c);
    m_connections.clear();

    for (quint32 id : widgetIds)
    {
        if (const VCButton *b = button(id))
            m_connections << connect(b, &VCButton::stateChanged, this, [this, id]() { emit changed(id); });
    }
}
