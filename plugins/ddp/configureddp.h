/*
  Q Light Controller Plus
  configureddp.h

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

#ifndef CONFIGUREDDP_H
#define CONFIGUREDDP_H

#include "ui_configureddp.h"

class DDPPlugin;

class ConfigureDDP final : public QDialog, public Ui_ConfigureDDP
{
    Q_OBJECT

public:
    ConfigureDDP(DDPPlugin* plugin, QWidget* parent = nullptr);
    virtual ~ConfigureDDP();

    /** @reimp */
    void accept() override;

private:
    void fillMappingTree();
    void showIPAlert(const QString &ip);

private:
    DDPPlugin* m_plugin;
};

#endif // CONFIGUREDDP_H
