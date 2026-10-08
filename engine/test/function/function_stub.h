/*
  Q Light Controller
  function_stub.h

  Copyright (C) Heikki Junnila

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

#ifndef FUNCTION_STUB_H
#define FUNCTION_STUB_H

#include <functional>

#include "function.h"

class Doc;

class Function_Stub final : public Function
{
    Q_OBJECT

public:
    Function_Stub(Doc* doc);
    ~Function_Stub();

    Function* createCopy(Doc* parent, bool addToDoc = true) override;

    bool saveXML(QXmlStreamWriter *doc) const override;
    bool loadXML(QXmlStreamReader &root) override;

    void preRun(MasterTimer* timer) override;
    void write(MasterTimer* timer, QList<Universe*> universes) override;
    void postRun(MasterTimer* timer, QList<Universe*> universes) override;

public slots:
    void slotFixtureRemoved(quint32 id) override;

public:
    int m_preRunCalls;
    int m_writeCalls;
    int m_postRunCalls;

    /** Optional busy delay per write() in microseconds. Default 0 keeps the
     *  stub instantaneous, so existing tests are unchanged. Used by the
     *  MasterTimer timing-diagnostics test to inject unequal callback costs. */
    int m_writeSleepUs = 0;

    /** Optional callback run inside preRun(), i.e. while the MasterTimer is
     *  starting this function. */
    std::function<void()> m_preRunHook;

    /** Optional callback run at the end of postRun(), after Function::postRun
     *  cleanup, while the MasterTimer is still stopping this function. */
    std::function<void()> m_postRunHook;

    quint32 m_slotFixtureRemovedId;
};

#endif

