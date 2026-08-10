/*
  Q Light Controller Plus
  jsthread_p.h

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

#ifndef JSTHREAD_P_H
#define JSTHREAD_P_H

#include <QSemaphore>
#include <QThread>

#if defined(QT_QML_LIB)
#include <QJSEngine>

/**
 * Token-identical copy of the JSThread definition in rgbscriptv4.cpp.
 *
 * rgbscriptv4.cpp is kept byte-identical to upstream, so the class cannot be
 * relocated into a shared header. Repeating the definition verbatim in another
 * translation unit is well-formed under the one-definition rule as long as the
 * two definitions consist of the same token sequence; keep them in sync.
 */
class JSThread final : public QThread
{
public:
    QJSEngine *engine;
    QSemaphore ready;
    void run() override
    {
        engine = new QJSEngine();
        ready.release(1);
        exec();
        delete engine;
    }
};

#endif // QT_QML_LIB

#endif // JSTHREAD_P_H
