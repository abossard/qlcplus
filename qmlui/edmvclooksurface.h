/*
  Q Light Controller Plus
  edmvclooksurface.h

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

#ifndef EDMVCLOOKSURFACE_H
#define EDMVCLOOKSURFACE_H

#include "edmstructuredirector.h"

#include <QHash>
#include <QMetaObject>

class VirtualConsole;
class VCButton;

/** The Virtual Console's buttons as look buttons; Toggle buttons only */
class EdmVcLookSurface : public EdmLookSurface
{
    Q_OBJECT

public:
    EdmVcLookSurface(VirtualConsole *console, QObject *parent = nullptr);

    bool isOn(quint32 widgetId) const override;
    void set(quint32 widgetId, bool on) override;
    void watch(const QList<quint32> &widgetIds) override;

private:
    VCButton *button(quint32 widgetId) const;

    VirtualConsole *m_console;
    QList<QMetaObject::Connection> m_connections;
};

#endif
