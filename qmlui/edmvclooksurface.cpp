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

EdmVcLookSurface::EdmVcLookSurface(VirtualConsole *console, QObject *parent)
    : EdmLookSurface(parent)
    , m_console(console)
{
}

VCButton *EdmVcLookSurface::button(quint32 widgetId) const
{
    return qobject_cast<VCButton *>(m_console->widget(widgetId));
}

bool EdmVcLookSurface::isOn(quint32 widgetId) const
{
    const VCButton *b = button(widgetId);
    return b != nullptr && b->state() == VCButton::Active;
}

void EdmVcLookSurface::set(quint32 widgetId, bool on)
{
    if (VCButton *b = button(widgetId))
        b->requestStateChange(on);
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
